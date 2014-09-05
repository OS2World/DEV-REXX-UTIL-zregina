/*
 * zregina.c - a Regina Rexx module for zsh
 *            Runs a Rexx program in the same process as zsh, thus enabling environment
 *            variables set within the Rexx program to "stick", and similarly, changes
 *            to the current working directory also "stick".
 *
 * Copyright (c) 2002 Mark Hessling
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Mark Hessling or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Mark Hessling and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Mark Hessling and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Mark Hessling and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zregina.mdh"
#include "zregina.pro"
#define INCL_RXSYSEXIT
#include "rexxsaa.h"
#include <dlfcn.h>

typedef APIRET APIENTRY RREXXSTART(LONG,PRXSTRING,PSZ,PRXSTRING,PSZ,LONG,PRXSYSEXIT,PSHORT,PRXSTRING );
typedef APIRET APIENTRY RREXXREGISTEREXITEXE(PSZ,PFN,PUCHAR ) ;
typedef APIRET APIENTRY RREXXDEREGISTEREXIT(PSZ,PSZ ) ;

RexxExitHandler exit_handler;

static RREXXSTART *pRexxStart = NULL;
static RREXXREGISTEREXITEXE *pRexxRegisterExitExe = NULL;
static RREXXDEREGISTEREXIT *pRexxDeregisterExit = NULL;
static void *addr = NULL;

static struct builtin bintab[] = {
    BUILTIN("zregina", 0, bin_zregina, 0, -1, 0, "a", NULL),
};

#ifdef HPUXDYNAMIC
# define REGINA_MODULE "libregina.sl"
#else
# define REGINA_MODULE "libregina.so"
#endif

LONG
exit_handler(
    LONG ExitNumber,    /* code defining the exit function    */
    LONG Subfunction,   /* code defining the exit subfunction */
    PEXIT ParmBlock      /* function dependent control block   */
    )
{
    RXENVSET_PARM *penvset;
    RXCWDSET_PARM *pcwdset;
    int rc = RXEXIT_NOT_HANDLED;
    char *tmp;

    switch( ExitNumber ) {
        case RXENV:
            switch( Subfunction ) {
                case RXENVSET:
                    penvset = (RXENVSET_PARM *)ParmBlock;
                    pushheap();
#ifdef DEBUG
                    fprintf(stderr, "Setting [%s] to [%s]\n", penvset->rxenv_name.strptr, penvset->rxenv_value.strptr );
#endif
                    tmp = zhalloc( penvset->rxenv_name.strlength+penvset->rxenv_value.strlength+20 );
                    sprintf( tmp, "\\builtin export %s=%s", penvset->rxenv_name.strptr, bslashquote(penvset->rxenv_value.strptr, NULL, 0 ) );
                    execstring( tmp, 1, 0 );
                    popheap();
                    rc = RXEXIT_HANDLED;
                    break;
                case RXCWDSET:
                    pcwdset = (RXCWDSET_PARM *)ParmBlock;
                    pushheap();
#ifdef DEBUG
                    fprintf(stderr, "Setting CWD to [%s]\n", pcwdset->rxcwd_value.strptr );
#endif
                    tmp = zhalloc( pcwdset->rxcwd_value.strlength+20 );
                    sprintf( tmp, "\\builtin cd %s", bslashquote( pcwdset->rxcwd_value.strptr, NULL, 0 ) );
                    execstring( tmp, 1, 0 );
                    popheap();
                    rc = RXEXIT_HANDLED;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return rc;
}

/**/
static int
bin_zregina(char *nam, char **args, char *ops, int func)
{
    long i = 0, argc, ArgCount = 0;
    int rc = 0, runtype;
    SHORT macrorc;
    char *filename = *args;
    RXSYSEXIT Exits[2];
    RXSTRING *Arguments = NULL;
    RXSTRING ArgList;
    char **tmpargs = args;

    ArgList.strptr = NULL;
    ArgList.strlength = 0;
    /*
     * If -a switch, use seperate arguments and call as RXSUBROUTINE
     */
    if ( ops['a'] )
        runtype = RXSUBROUTINE;
    else
        runtype = RXCOMMAND;

#ifdef DEBUG
    printf("\nArguments:");
    for ( argc = 0; *tmpargs; argc++, tmpargs++) {
        printf(", %s", *tmpargs);
    }
#else
    /*
     * Count the number of arguments
     */
    for ( argc = 0; *tmpargs; argc++, tmpargs++) ;
#endif

    Exits[0].sysexit_name = "ZSH";
    Exits[0].sysexit_code = RXENV;
    Exits[1].sysexit_code = RXENDLST;
    /* 
     * Get number of arguments to the Rexx program
     * First Argument is filename.
     */
    ArgCount = argc - 1;

    /* 
     * Build an array of arguments if any. 
     */
    if ( runtype == RXSUBROUTINE ) {
        if ( ArgCount > 0 )
        {
            Arguments = (RXSTRING *)zalloc( ArgCount * sizeof( RXSTRING ) );
            for ( i = 1; i < argc; i++ ) {
                MAKERXSTRING( Arguments[i-1], args[i], strlen( args[i] ) );
            }
        }
        else {
            Arguments = NULL;
        }
    }
    else {
        if ( ArgCount > 0 ) {
            int len=0;
    
            for ( i = 1; i < argc; i++ ) {
                len += strlen( (char *)args[i] );
            }
            ArgList.strptr = (char *)zalloc( len + 1 + ArgCount);
            strcpy( ArgList.strptr, "" );
            for ( i = 1; i < argc; i++ ) {
                strcat( ArgList.strptr, args[i] );
                if ( i != argc )
                    strcat( ArgList.strptr, " " );
            }
            ArgList.strlength = ArgCount + len - 1;
            ArgCount = 1;
        }
        else {
            ArgList.strptr = NULL;
            ArgList.strlength = 0;
        }
    }

    if ( pRexxStart ) {
        rc = (*pRexxStart)( ArgCount,
                            (runtype==RXSUBROUTINE) ? Arguments : &ArgList,
                            filename, 
                            NULL, 
                            "ZSH", 
                            runtype,
                            Exits, 
                            &macrorc, 
                            NULL );
    }
    else {
        printf("\nNo address for Rexxstart()");
    }
    if ( runtype == RXSUBROUTINE ) {
        if ( Arguments )
            free( Arguments );
    }
    else {
        if ( ArgList.strptr )
            free( ArgList.strptr );
    }
#ifdef DEBUG
    printf("\nName: %s rc: %d macrorc: %d\n", nam, rc, macrorc);
#endif
    return macrorc;
}

/**/
int
setup_(Module m)
{
#ifdef DEBUG
    printf("The zregina module has now been set up.\n");
    fflush(stdout);
#endif
    return 0;
}

/**/
int
boot_(Module m)
{
    int rc = 0;

#ifdef DEBUG
    printf("The zregina module has now been booted. Loading address of RexxStart()\n");
    fflush(stdout);
#endif
    addr = dlopen( "libregina.so", RTLD_LAZY );
    if ( addr ) {
        pRexxStart = (RREXXSTART *)dlsym( addr, "RexxStart" );
        if ( pRexxStart == NULL ) {
            printf("Can't find RexxStart in libregina.so: %s\n", dlerror() );
            dlclose( addr );
            return 1;
        }
        else {
            pRexxRegisterExitExe = (RREXXREGISTEREXITEXE *)dlsym( addr, "RexxRegisterExitExe" );
            if ( pRexxStart == NULL ) {
                printf("Can't find RexxRegisterExitExe in libregina.so: %s\n", dlerror() );
                dlclose( addr );
                return 1;
            }
            else {
                pRexxDeregisterExit = (RREXXDEREGISTEREXIT *)dlsym( addr, "RexxDeregisterExit" );
                if ( pRexxStart == NULL ) {
                    printf("Can't find RexxDeregisterExit in libregina.so: %s\n", dlerror() );
                    dlclose( addr );
                    return 1;
                }
            }
        }
    }
    else {
        printf("Can't find libregina.so: %s\n", dlerror() );
        return 1;
    }
    rc = (*pRexxRegisterExitExe)( "ZSH", (PFN)exit_handler, NULL );

#ifdef DEBUG
    printf("Loaded libregina.so OK rc=%d\n", rc );
#endif
    return !(addbuiltins(m->nam, bintab, sizeof(bintab)/sizeof(*bintab)));
}

/**/
int
cleanup_(Module m)
{
    if ( pRexxDeregisterExit ) {
       (*pRexxDeregisterExit)( "ZSH", NULL );
    }
    if ( addr ) {
        dlclose( addr );
    }
    deletebuiltins(m->nam, bintab, sizeof(bintab)/sizeof(*bintab));
    return 0;
}

/**/
int
finish_(Module m)
{
    printf("Thank you for using the zregina module.  Have a nice day.\n");
    fflush(stdout);
    return 0;
}
