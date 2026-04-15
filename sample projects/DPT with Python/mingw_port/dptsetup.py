#! c:Python25/pythonw

# dptsetup.py

"""
Build the Python DPT API.

List of functions:

GetCommandArgs - collate command line arguments
GetSourceFiles - get list of DPT API source files
MakeTarget - Do the build by calling gmake

Notes

Run DPT API build on MSWindows using Mingw under MSYS.

The code can be used on MSWindows or on Wine under FreeBSD (and Linux).

The build works because DPT is g++ 3.n compliant and Mingw uses g++ 3.n.  Note
that Mingw project does not support the use of g++ 4.n (at November 2009).

distutils is not used because the focus is on DPT rather than the language
interface.  It should be easier to change the Makefile to build a Ruby interface
than convert distutils instructions to whatever Ruby does and so on.

"""

if __name__ == '__main__':

    import os
    import subprocess
    import sys

    dpt_relative_path = os.path.join('..', '..', '..')


    def GetCommandArgs():
        """
        Return list of options for make.

        The returned list of options is:
        make target
        '-D' options
        other '-' options
        path specifications

        Two arguments are available to revert the corresponding option to its
        g++ default from the default used in this build.
        debug - do not use the -DNDEBUG option (so asserts are included).
        no-optimize - use the default -O0 option rather than -O2 (so debug
        information is included).
        
        """
        optimize_options = set(('-O0', '-O1', '-O2', '-O3', '-Os'))
        ptp = 'PATH_TO_PYTHON='
        pptp = 'POSIX_PATH_TO_PYTHON='
        pts = 'PATH_TO_SWIG='
        ptc = 'PATH_TO_CXX='
        pv = 'PYTHON_VERSION='
        dbg = '-DNDEBUG'
        optimize = '-O3'
        t = 'python'
        d = []
        o = []
        ws = set()
        for e, a in enumerate(sys.argv):
            if e > 0:
                if a.startswith('-D'):
                    if a != '-DNDEBUG':
                        d.append(a)
                elif a in optimize_options:
                    optimize = a
                elif a.startswith('-'):
                    d.append(a)
                elif a == 'debug':
                    dbg = ''
                elif a.startswith(ptp):
                    ws.add(a)
                elif a.startswith(pptp):
                    ws.add(a)
                elif a.startswith(pts):
                    ws.add(a)
                elif a.startswith(pv):
                    ws.add(a)
                elif a.startswith(ptc):
                    if not a.endswith('/'):
                        ws.add(''.join((a, '/')))
                    else:
                        ws.add(a)
                elif a == 'no-optimize':
                    optimize = ''
                elif e == 1:
                    t = a
        if dbg:
            d.append(dbg)
        if optimize:
            o.append(optimize)
        return (t, ' '.join(d), ' '.join(o), ws)


    def GetSourceFiles(directory):
        """Return list of *.cpp source files without extention.""" 
        absdir = os.path.abspath(directory)
        files =[]
        for f in os.listdir(absdir):
            if os.path.isfile(os.path.join(absdir, f)):
                p, e = os.path.splitext(f)
                if e in ('.cpp',):
                    files.append(p)
        return files


    def MakeTarget(target, defines, options, settings):
        """
        Run the DPT API makefile.

        target - the make target.
        defines - additional or non-default -D options for compiler.
        options - additional or non-default - options for compiler.
        settings - non-default paths to compiler Python and SWIG.
        
        """
        # MSYS 'make' and FreeBSD 'gmake' are GNU 'make'.
        # FreeBSD 'make' is a *nix 'make'.
        # They have same purpose but use different syntax to express rules.
        job = ['make', target]
        for s in settings:
            job.append(s)
        job.extend([
            ''.join(('DPT_NAMES=',
                     ' '.join(GetSourceFiles(
                         os.path.join(dpt_relative_path, 'source'))))),
            ''.join(('DPTAPI_NAMES=',
                     ' '.join(GetSourceFiles(
                         os.path.join(dpt_relative_path, 'source', 'dbapi'))))),
            ''.join(('DEFINES=', defines)),
            ''.join(('OPTIONS=', options)),
            ''.join(('DPT_BASE=', '/'.join(dpt_relative_path.split('\\'))))])
        for s in settings:
            if s.startswith('PYTHON_VERSION='):
                break
        else:
            job.append(''.join((
                'PYTHON_VERSION=',
                ''.join((str(sys.version_info[0]),
                         str(sys.version_info[1]))))))
        sp = subprocess.Popen(job)
        return sp.wait()
    

    target, defines, options, settings = GetCommandArgs()

    rc = MakeTarget(target, defines, options, settings)
    if rc != 0:
        print 'Result code', rc
