===============
DPT with Python
===============

.. contents::


Description
===========

This project provides makefiles to build a `Python`_ interface to dpt-dbms.

It is embedded within the dpt-dbms project and builds the interface from the c++ code in directories ../../include and ../../source relative to the makefile's location.

The makefiles build interfaces for 64-bit Python, but appropriate command line overrides of the relevant definitions will build interfaces for 32-bit Python.

This project contains:
   three makefiles aimed at different build environments,
   one SWIG interface definition file,
   one Python package template directory,
   an stdafx directory,
   sed_path_separator_edit file for the '-f' option to sed,
   this README.

The build environments are:
   Microsoft Windows `Visual Studio`_,
   `Msys2`_ on Microsoft Windows,
   `mingw-w64`_ on Unix-like (for example `Debian`_),
and there is one makefile for each.

Unix-like environments require wine, from `Wine`_, to be installed.

The sed_path_separator_edit file is needed by Unix-like environments.


Instructions
============

These instructions use the command name 'make' and the file name '<name>'.  When applying to the 'vs' (Microsoft Windows `Visual Studio`_) environment remember to say 'nmake', it's a different command, and 'Nmakefile', it's a different name to indicate the content is 'nmake' syntax.

The three makefile names are:
   dpt-dbms_mingw-w64_python.Makefile
   dpt-dbms_msys2_python.Makefile
   dpt-dbms_vs_python.Nmakefile
referred to as '<name>' in the instructions below.

The `Python`_ interface is built by the following three commands in sequence:
   make <name> clean
   make <name> work-directories
   make <name>
followed by
   make <name> local-install
to install the interface in the Python which built it, after seeing the build succeed and are happy with what will happen in relation to the points made in the Python Build and Install section below.

When installing, the 'make <name>' command can be omitted but leaving out either of the 'clean' or 'work-directories' commands is likely to cause problems.


Restrictions
============

At time of writing the msys2 makefile produces a 'warning: externally-managed-environment' if local-install is given but installs the interface anyway.  It is assumed the warning will become an error at some point in the future: what is said about --break-system-packages in 'Python Build and Install' below will then become relevant.


Python Versions
===============

The interface is built for Python3.12 by default.  To build for, say, Python3.16 add PYTHON_MINOR=16 to the command.  The major version number can be changed: the only likely case is covered by adding PYTHON_MAJOR=2 to the command.

Python is invoked by the py command in the Microsoft Windows `Visual Studio`_ environment.

Python is invoked by the python command in the other environments.


Python Build and Install
========================

The *.whl file created by the 'wheel' command is renamed using the values in the PYTHON_TAG, ABI_TAG, and PLATFORM_TAG, variables.  The default values are correct for 64-bit builds, with the qualification 'at time of writing' for PLATFORM_TAG.  See `Python Packaging User Guide`_ especially the 'PyPA specifications - Package Distribuion Metadata - Platform compatibility tags' section.

The build and install processes requires several Python packages to be installed: build, setuptools, pip, and wheel.  How and where these are installed, if they are not already present, is beyond the scope of this document.  See `Python Packaging User Guide`_ especially the 'PyPA specifications - Tutorials - Installing Packages' section.

The <vs> makefile needs the pythonsed package too.

The local-install argument to these makefiles will do a `user install`_.

The <msys2> makefile will need the --break-system-packages flag when installing the package into an `externally managed`_ Python environment such as the one in `Msys2`_.  Add BREAK_SYSTEM_PACKAGES=--break-system-packages to the make command to allow the local install.  `PEP 668`_ discusses externally managed environments.

In a MINGW32 shell add CHAIN=gnu BITNESS=i686 RUNTIME=msvcrt to the make command to allow installation to proceed.  The order of the CHAIN, BITNESS, and RUNTIME, arguments does not matter.


Compiler Choice
===============

The `Msys2`_ makefile expects to be run in a CLANG64 session where no additional arguments are needed.  Add COMPILER=g++ to the make command for builds with the gcc compiler in a MINGW32 session.

The `Visual Studio`_ makefile uses the clang-cl compiler by default.  Add COMPILER=cl to the nmake command for builds with the cl compiler.

The `mingw-w64`_ makefile uses the x86_64-w64-mingw32-g++-win32 compiler by default.  The structure of the compiler name indicates there are choices: all of them may produce a build in at least one environment.  (The x86_64 at the start of the name is assumed to indicate this is a 64-bit compiler.)


32-bit Builds
=============

`Msys2`_ has shells named CLANG64, MINGW32, and so forth, for the two build bit-nesses.  The correct Python comes with the shell and the names of the Python components are the same.

The `mingw-w64`_ makefile is not intended for 32-bit builds, but the environment is capable of doing them.  (The makefile might succeed on 32-bit builds without modification.)

`Visual Studio`_ has shells named 'x64 Native Tools ...' for 64-bit builds and 'x86 Native Tools ...' for 32-bit build bits.  The py command has to be told which Python is needed: add PYTHON_32_OR_64=-32 to get a 32-bit build.


History
=======

The previous version of this document is './DPTwithPython.html' which is now almost 20 years old and exists in the initial commit of `dpt-dbms`_.

The <msys2> makefile replaced the makefiles aimed at the toolchain from mingw.org (which closed down some years ago).  The <vs> makefile was added because community editions of Visual Studio seem to be a permanent feature now.  The <mingw-w64> makefile was added to support this package in Wine on Unix-like systems: Debian provides mingw-w64 and wine packages.


.. _dpt-dbms: https://github.com/sparsebitmap/dpt-dbms
.. _Visual Studio: https://visualstudio.microsoft.com
.. _Msys2: https://msys2.org
.. _mingw-w64: https://mingw-w64.org
.. _Python: https://python.org
.. _SWIG: https://swig.org
.. _Wine: https://winehq.org
.. _Python Packaging User Guide: https://packaging.python.org
.. _user install: https://packaging.python.org
.. _PEP 668: https://peps.python.org/pep-0668/
.. _Debian: https://debian.org
