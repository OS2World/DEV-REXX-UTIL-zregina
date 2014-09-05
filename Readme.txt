This package is a zsh module, which allows Rexx programs to be executed by the
Regina Rexx interpreter within the same process as zsh itself.

The advantages of this are:
- environment variables set by VALUE( 'ENVVAR', 'newvalue', 'ENVIRONMENT' ) stay set
  after the end of the execution of the Rexx program.
- the current working directory can be set permanently by the Rexx program
- a numeric return code from a Rexx program is passed back to zsh, and can be obtained
  via the $? variable.

This module creates a new zsh builtin called "zregina". The builtin is run from zsh as:
zregina [-a] rexx.program [arguments]

The optional -a switch, runs the specified "rexx.program" as a subroutine. See the "-a"
switch documentation for the "regina" executable.

To install, download a recent version of zsh (testing done with 4.0.6), unpack the zsh
source distribtion. Change to the "Src/Modules" directory, and unpack this distribution
there. Simply build and install zsh as per its installation instructions.

Once the zregina module has been installed, add the following line to your $(HOME)/.zshrc:
zmodload -a hessling/zregina zregina
