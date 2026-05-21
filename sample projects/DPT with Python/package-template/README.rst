==========================================
DPT database API wrappers built using SWIG
==========================================

.. contents::


Description
===========

This package provides `Python`_ applications with the database API used by DPT.

DPT is a multi-user database system for Microsoft Windows.

This package contains two modules: a `Python`_ interface to the DPT API built using `SWIG`_ and multistep_sort which is an implementation of the sorts needed by DPT's multi-step deferred update process for efficient updates.

Documentation is at '../../doc' relative to this file in the download from `dpt-dbms`_.

There is no separate documentation for `Python`_.

There are no source distributions (*.tar.gz files), only wheels.  The source is at '../', with c++ source at '../../', relative to this file in the download.


Installation Instructions
=========================


Install the package by 

   'nmake -f dpt-dbms_vs-python.Nmakefile local-install'

in a x64 Native Tools shell for `Visual Studio`_ clang-cl builds,

or by

   'make -f dpt-dbms_msys2_python.Makefile local-install'

in a `Msys2`_ CLANG64 shell for `Msys2`_ clang++ builds,

or by

   'py -m pip install --user --no-index --find-links <path> dpt3.0_dptdb'

in a Windows PowerShell (see 'py' documentation for selecting `Python`_ version),

or by

   'python -m pip install --user --no-index --find-links <path> dpt3.0_dptdb'

in a `Msys2`_ CLANG64 shell.


Other Compilers
===============

Add 'COMPILER=cl' to the nmake invocation for builds with the `Visual Studio`_ cl compiler.

Add 'COMPILER=g++' to the make invocation for builds with the `Msys2`_ g++ compiler in a `Msys2`_ UCRT64 shell.


32-bit Builds
=============

Run the nmake in a x86 Native Tools shell for `Visual Studio`_ builds.

Run the make in a `Msys2`_ MINGW32 shell for `Msys2`_ builds.


Sorting for Multistep Deferred Update
=====================================

DPT documentation recommends deferred update is done with OpenContextDUSingle which handles sorting itself.

The multistep_sort module is added alongside the dptapi module built by `SWIG`_ to support deferred updates done with OpenContextDUMulti.

Any sort function which can handle the file format is acceptable.


History
=======

The previous version of this document is '../DPTwithPython.html' which is now almost 20 years old and exists in the initial commit of `dpt-dbms`_.


.. _dpt-dbms: https://github.com/sparsebitmap/dpt-dbms
.. _Visual Studio: https://visualstudio.microsoft.com
.. _Msys2: https://msys2.org
.. _Python: https://python.org
.. _SWIG: https://swig.org
.. _Wine: https://winehq.org
