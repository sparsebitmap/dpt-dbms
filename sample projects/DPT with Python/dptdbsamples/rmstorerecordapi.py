#rmstorerecordapi.py
#Copyright 2007 - 2008 Roger Marsh
#See www.dptoolkit.com for details of DPT
#License: DPToolkit license

"""Sample code that does deferred updates in multi-step mode.

Uses Python sort, the heapq module, and the dptapi module built using SWIG.

Assumes that exceptions are thrown for range errors on APIRoundedDouble
instances. SetNumRangeThrowOption(True) call occurs on first import of
dptapi.

self._fullpath is used because APIDatabaseFileContext does not have
a GetFullFilePath method like DatabaseFileContext. self._fullpath is
set by self.SetDefault() which creates the APIDatabaseFileContext
and unset by self.EndDeferredUpdates(). Maybe the SWIG interface
should extend APIDatabaseFileContext to get at GetFullFilePath.

Some tricks with encode and decode are needed to make this sample code work
on Python versions 2.6 and later.  They are not needed at all before 2.6 but
behave as null operations on these versions while converting between utf-8
and unicode on 2.6 and later.  It is this module alone, which deals with the
DEFAULT and NOPADNOCRLF deferred update formats, which needs the decode encode
actions.

Note that the handling of float in the DEFAULT deferred update format is
broken at 2.6 and later in the sample code and the exceptions raised are
caught and ignored.  It is the sample code at fault and correct use of io
module will probably get this deferred update to work.  Use of file rather
than io.FileIO makes this feature work at 2.6 of course.
"""

import os
import os.path
from heapq import heapify, heappop, heappush

# Not recommended for production code. Use separate versions.
try:
    # io is available at 2.6 and file is removed at 3.0
    # import as file for compatibility with 2.5 and earlier
    # 2.6 is transitional and behaves like 2.5 on float for encode decode
    from io import FileIO as file
    iofile = True
    usedecodeonfloat = False
    import sys
    if sys.version_info[:1] >= (3,):
        usedecodeonfloat = True
    del sys
except:
    # io is not available at 2.5 and earlier so use builtin file
    iofile = False
    usedecodeonfloat = False

import dptdb.dptapi as dptapi

_tapea = 'TAPEA'
_tapen = 'TAPEN'
_sfa = 'SFA'
_sfn = 'SFN'

_duDefault = dptapi.DU_FORMAT_DEFAULT
_duNoPadNoCRLF = dptapi.DU_FORMAT_NOCRLF | dptapi.DU_FORMAT_NOPAD
_validDUFormats = (_duDefault, _duNoPadNoCRLF,)


class rmStoreRecordError(Exception):
    pass


class rmStoreRecord(object):
    
    """Manage the dptapi.StoreRecord calls in multi-step deferred update.

    Style for using this class
    
    For all files
        SetDefault
    Arbitrary sequence of StoreRecord calls
    For all files
        DoDeferredUpdates
    For all files
        EndDeferredUpdates

    Repeat as needed.
    The same Set... method must be called for all files.

    The loop structure is as it is following an attempt to do something
    like single-step deferred update, prior to its introduction, in Python
    with the dptapi interface module extended using SWIG.  It turned out that
    the only way to make that work was to keep all files in deferred update
    mode until every file's deferred updates had been applied.  Thus the two
    loops rather than the obvious one loop.
    """

    def __init__(self, deferfolder, dulimit):

        self._opencontext = None
        self._deferfolder = deferfolder
        self._dulimit = dulimit
        # See note at start of module
        self._fullpath = None

        self._fieldvalue = dptapi.APIFieldValue()
        self._putrecordcopy = dptapi.APIStoreRecordTemplate()

        self._duformat = None
        
        self._visible = dict()
        self._ordered = dict()
        self._fid = dict()
        self._string = dict()
        self._float = dict()
        self._ordnum = dict()

        self._dubuffer = dict()
        self._dulength = dict()
        self._duseqfile = dict()
        self._highseqno = dict()

        self._KeyEncoder = _MakeKeyEncoder(4)

    def DoDeferredUpdates(self, dbserv, sfserv):
        """Apply deferred updates."""
        if self._duformat == _duDefault:
            self._DoDeferredUpdatesDefault(dbserv, sfserv)
            self._duformat = None
        elif self._duformat == _duNoPadNoCRLF:
            self._DoDeferredUpdatesNoPadNoCRLF(dbserv, sfserv)
            self._duformat = None

    def EndDeferredUpdates(self, dbserv, sfserv):
        """Reset instance state to allow further deferred updates."""
        if self._duformat in _validDUFormats:
            return
        self._opencontext = None
        # See note at start of module
        self._fullpath = None

    def SetDefault(self, dbserv, sfserv, shortname, fullpath):
        """Configure for DU_FORMAT_DEFAULT deferred update.

        dptapi.StoreRecord will write unsorted files to be sorted by
        Python sort and applied by dptapi.ApplyDeferredUpdates.
        """
        if self._duformat:
            return
        if self._opencontext != None:
            return
        
        # See note at start of module
        self._fullpath = fullpath

        self._duformat = _duDefault
        self._Open(dbserv, sfserv, shortname, fullpath)
        if self._opencontext == None:
            self._duformat = None
            return

    def SetNoPadNoCRLF(self, dbserv, sfserv, shortname, fullpath):
        """Configure for DU_FORMAT_NOPAD | DU_FORMAT_NOCRLF deferred update.

        dptapi.StoreRecord will write unsorted files to be sorted by
        Python sort and applied by dptapi.ApplyDeferredUpdates.
        """
        if self._duformat:
            return
        if self._opencontext != None:
            return
        
        # See note at start of module
        self._fullpath = fullpath

        self._duformat = _duNoPadNoCRLF
        self._Open(dbserv, sfserv, shortname, fullpath)
        if self._opencontext == None:
            self._duformat = None
            return

    def StoreRecord(self, fieldvaluepairs):
        """Add a record to filename using dptapi StoreRecord."""
        recordcopy = self._putrecordcopy
        fieldvalue = self._fieldvalue
        Assign = fieldvalue.Assign

        for field, value in fieldvaluepairs:
            Assign(value)
            recordcopy.Append(field, fieldvalue)

        recnum = self._opencontext.StoreRecord(recordcopy)
        recordcopy.Clear()

    # Unique number for each shortname ever used to open a DPT file
    # using any instance of rmStoreRecord.
    _seqfileno = dict()
            
    def _CreateSortedFilePath(self, field, shortname=None):
        """Create path for deferred update files if it does not exist."""
        if shortname == None:
            shortname = self._opencontext.GetShortName()
        if not os.path.exists(self._deferfolder):
            try:
                os.mkdir(self._deferfolder)
            except:
                pass
        if not os.path.exists(
            os.path.join(
                self._deferfolder,
                shortname)):
            try:
                os.mkdir(
                    os.path.join(
                        self._deferfolder,
                        shortname))
            except:
                pass
        if not os.path.exists(
            os.path.join(
                self._deferfolder,
                shortname,
                field)):
            try:
                os.mkdir(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        field))
            except:
                pass

    def _DeleteSortedFilePath(self, field, shortname=None):
        """Delete path for deferred update files if it exists and is empty."""
        if shortname == None:
            shortname = self._opencontext.GetShortName()
        try:
            os.rmdir(
                os.path.join(
                    self._deferfolder,
                    shortname,
                    field))
        except:
            pass
        try:
            os.rmdir(
                os.path.join(
                    self._deferfolder,
                    shortname))
        except:
            pass
        try:
            os.rmdir(self._deferfolder)
        except:
            pass

    def _DoDeferredUpdatesDefault(
        self,
        dbserv,
        sfserv):
        """Apply deferred updates from unsorted files.

        Written by dptapi.StoreRecord in DU_FORMAT_DEFAULT mode.
        """
        if not isinstance(self._opencontext, dptapi.APIDatabaseFileContext):
            return
        if not self._duformat == _duDefault:
            return
            
        # Get file and field details then close file.
        defer = dict()
        self._GetFieldDetails()
        shortname = self._opencontext.GetShortName()
        # See note at start of module
        fullpath = self._fullpath
        alphaseqfile = _sfa + rmStoreRecord._seqfileno[shortname]
        numericseqfile = _sfn + rmStoreRecord._seqfileno[shortname]
        
        dbserv.CloseContext(self._opencontext)
        dbserv.Free(shortname)
        sfserv.Free(alphaseqfile)
        sfserv.Free(numericseqfile)

        # Map field id representation on sequential file to field name
        # and create folders for sorted files.
        for field in self._ordered:
            if self._ordered[field]:
                self._dubuffer[field] = []
                self._dulength[field] = 0
                self._GetSortedFileNames(field, shortname)
                self._CreateSortedFilePath(field, shortname)
                fidhigh, fidlow = divmod(self._fid[field], 256)
                if fidhigh & 8:
                    fidhigh &= 7
                    fidhigh |= 128
                if fidlow & 8:
                    fidlow &= 7
                    fidhigh |= 64
                defer[(chr(fidlow) + chr(fidhigh)).encode()] = field

        # Split the sequential files into chunks and write the sorted
        # chunks to files for merging.
        # Better not to split into chunk per field to make control of
        # memory use easier (remove this comment when done).

        for f in (_sfa, _sfn):
            infile = _rmFile(
                os.path.join(
                    self._deferfolder,
                    shortname,
                    f),
                'rb')
            if f == _sfa:
                ReadDefault = infile.ReadDefaultOrdChar
            elif f == _sfn:
                ReadDefault = infile.ReadDefaultOrdNum
            try:
                data = ReadDefault()
                while data:
                    value, recno, fid, valuelen, recnofidvalue = data
                    field = defer[fid]
                    self._dubuffer[field].append((value, recno, recnofidvalue))
                    self._dulength[field] += valuelen
                    if self._dulength[field] > self._dulimit:
                        self._dubuffer[field].sort()
                        s = []
                        for durec in self._dubuffer[field]:
                            s.append(durec[-1])
                        self._dubuffer[field][:] = []
                        self._dulength[field] = 0
                        self._highseqno[field] += 1
                        seqno = str(self._highseqno[field])
                        outfile = _rmFile(
                            os.path.join(
                                self._deferfolder,
                                shortname,
                                field,
                                seqno),
                            'wb')
                        self._duseqfile[field].append(seqno)
                        try:
                            outfile.writelines(s)
                        finally:
                            outfile.close()
                    data = ReadDefault()
            finally:
                infile.close()

        for fid in defer:
            field = defer[fid]
            if len(self._dubuffer[field]):
                self._dubuffer[field].sort()
                s = []
                for durec in self._dubuffer[field]:
                    s.append(durec[-1])
                self._dubuffer[field][:] = []
                self._dulength[field] = 0
                self._highseqno[field] += 1
                seqno = str(self._highseqno[field])
                outfile = _rmFile(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        field,
                        seqno),
                    'wb')
                self._duseqfile[field].append(seqno)
                try:
                    outfile.writelines(s)
                finally:
                    outfile.close()
        
        # Merge the sorted chunks into two sequential files for
        # processing by ApplyDeferredUpdates.
        outfilealpha = file(
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(alphaseqfile)),
            'wb')
        outfilenumeric = file(
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(numericseqfile)),
            'wb')

        for fid in defer:
            field = defer[fid]
            self._GetSortedFileNames(field, shortname)
            sources = self._GetInputFiles(field, shortname)
            if self._ordnum[field]:
                outfile = outfilenumeric
                for f in sources:
                    f.ReadDefaultMerge = f.ReadDefaultOrdNum
            else:
                outfile = outfilealpha
                for f in sources:
                    f.ReadDefaultMerge = f.ReadDefaultOrdChar
            record = []
            heapify(record)
            for f in sources[:]:
                r = f.ReadDefaultMerge()
                if r == None:
                    sources[sources.index(f)].close()
                    del sources[sources.index(f)]
                else:
                    heappush(record, (r, f))
            more = len(record) > 0
            while more:
                wr, f = heappop(record)
                outfile.write(wr[-1])
                r = f.ReadDefaultMerge()
                if r == None:
                    sources[sources.index(f)].close()
                    del sources[sources.index(f)]
                    more = len(record) > 0
                else:
                    heappush(record, (r, f))

        outfilealpha.close()
        outfilenumeric.close()

        for fid in defer:
            field = defer[fid]
            for seqfile in self._duseqfile[field]:
                try:
                    os.remove(
                        os.path.join(
                            self._deferfolder,
                            shortname,
                            field,
                            seqfile))
                except:
                    pass
            try:
                os.remove(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        field))
            except:
                pass

        # Apply the deferred updates.
        dbserv.Allocate(
            shortname,
            fullpath,
            dptapi.FILEDISP_COND)
        sfserv.Allocate(
            _tapen,
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(numericseqfile)),
            dptapi.FILEDISP_OLD)
        sfserv.Allocate(
            _tapea,
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(alphaseqfile)),
            dptapi.FILEDISP_OLD)
        cs = dptapi.APIContextSpecification(shortname)
        opencontext = dbserv.OpenContext(cs)

        opencontext.ApplyDeferredUpdates(0)

        # Close DPT file and delete folders and files created
        # by this method.
        dbserv.CloseContext(opencontext)
        dbserv.Free(shortname)
        sfserv.Free(_tapea)
        sfserv.Free(_tapen)
        for f in (alphaseqfile, numericseqfile):
            try:
                os.remove(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        os.path.basename(f)))
            except:
                pass
        for f in (_sfa, _sfn):
            try:
                os.remove(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        f))
            except:
                pass
        for field in self._ordered:
            if self._ordered[field]:
                self._DeleteSortedFilePath(field, shortname)

    def _DoDeferredUpdatesNoPadNoCRLF(
        self,
        dbserv,
        sfserv):
        """Apply deferred updates from unsorted files.

        Written by dptapi.StoreRecord in DU_FORMAT_NOPAD | DU_FORMAT_NOCRLF
        mode.
        """
        if not isinstance(self._opencontext, dptapi.APIDatabaseFileContext):
            return
        if not self._duformat == _duNoPadNoCRLF:
            return
            
        # Get file and field details then close file.
        defer = dict()
        self._GetFieldDetails()
        shortname = self._opencontext.GetShortName()
        # See fullpath note at start of module
        fullpath = self._fullpath
        alphaseqfile = _sfa + rmStoreRecord._seqfileno[shortname]
        numericseqfile = _sfn + rmStoreRecord._seqfileno[shortname]
        
        dbserv.CloseContext(self._opencontext)
        dbserv.Free(shortname)
        sfserv.Free(alphaseqfile)
        sfserv.Free(numericseqfile)

        # Map field id representation on sequential file to field name
        # and create folders for sorted files.
        for field in self._ordered:
            if self._ordered[field]:
                self._dubuffer[field] = []
                self._dulength[field] = 0
                self._GetSortedFileNames(field, shortname)
                self._CreateSortedFilePath(field, shortname)
                fidhigh, fidlow = divmod(self._fid[field], 256)
                defer[(chr(fidlow) + chr(fidhigh)).encode()] = field

        # Split the sequential files into chunks and write the sorted
        # chunks to files for merging.
        # Better not to split into chunk per field to make control of
        # memory use easier (remove this comment when done).
        for f in (_sfa, _sfn):
            infile = _rmFile(
                os.path.join(
                    self._deferfolder,
                    shortname,
                    f),
                'rb')
            if f == _sfa:
                ReadNoCRLF = infile.ReadNoCRLFOrdChar
            elif f == _sfn:
                ReadNoCRLF = infile.ReadNoCRLFOrdNum
            try:
                data = ReadNoCRLF()
                while data:
                    value, recno, fid, valuelen, recnofidvalue = data
                    field = defer[fid]
                    self._dubuffer[field].append((value, recno, recnofidvalue))
                    self._dulength[field] += valuelen
                    if self._dulength[field] > self._dulimit:
                        self._dubuffer[field].sort()
                        s = []
                        for durec in self._dubuffer[field]:
                            s.append(durec[-1])
                        self._dubuffer[field][:] = []
                        self._dulength[field] = 0
                        self._highseqno[field] += 1
                        seqno = str(self._highseqno[field])
                        outfile = _rmFile(
                            os.path.join(
                                self._deferfolder,
                                shortname,
                                field,
                                seqno),
                            'wb')
                        self._duseqfile[field].append(seqno)
                        try:
                            outfile.writelines(s)
                        finally:
                            outfile.close()
                    data = ReadNoCRLF()
            finally:
                infile.close()

        for fid in defer:
            field = defer[fid]
            if len(self._dubuffer[field]):
                self._dubuffer[field].sort()
                s = []
                for durec in self._dubuffer[field]:
                    s.append(durec[-1])
                self._dubuffer[field][:] = []
                self._dulength[field] = 0
                self._highseqno[field] += 1
                seqno = str(self._highseqno[field])
                outfile = _rmFile(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        field,
                        seqno),
                    'wb')
                self._duseqfile[field].append(seqno)
                try:
                    outfile.writelines(s)
                finally:
                    outfile.close()
        
        # Merge the sorted chunks into two sequential files for
        # processing by ApplyDeferredUpdates.
        outfilealpha = file(
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(alphaseqfile)),
            'wb')
        outfilenumeric = file(
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(numericseqfile)),
            'wb')

        for fid in defer:
            field = defer[fid]
            self._GetSortedFileNames(field, shortname)
            sources = self._GetInputFiles(field, shortname)
            if self._ordnum[field]:
                outfile = outfilenumeric
                for f in sources:
                    f.ReadNoCRLFMerge = f.ReadNoCRLFOrdNum
            else:
                outfile = outfilealpha
                for f in sources:
                    f.ReadNoCRLFMerge = f.ReadNoCRLFOrdChar
            record = []
            heapify(record)
            for f in sources[:]:
                r = f.ReadNoCRLFMerge()
                if r == None:
                    sources[sources.index(f)].close()
                    del sources[sources.index(f)]
                else:
                    heappush(record, (r, f))
            more = len(record) > 0
            while more:
                wr, f = heappop(record)
                outfile.write(wr[-1])
                r = f.ReadNoCRLFMerge()
                if r == None:
                    sources[sources.index(f)].close()
                    del sources[sources.index(f)]
                    more = len(record) > 0
                else:
                    heappush(record, (r, f))

        outfilealpha.close()
        outfilenumeric.close()

        for fid in defer:
            field = defer[fid]
            for seqfile in self._duseqfile[field]:
                try:
                    os.remove(
                        os.path.join(
                            self._deferfolder,
                            shortname,
                            field,
                            seqfile))
                except:
                    pass
            try:
                os.remove(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        field))
            except:
                pass

        # Apply the deferred updates.
        dbserv.Allocate(
            shortname,
            fullpath,
            dptapi.FILEDISP_COND)
        sfserv.Allocate(
            _tapen,
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(numericseqfile)),
            dptapi.FILEDISP_OLD)
        sfserv.Allocate(
            _tapea,
            os.path.join(
                self._deferfolder,
                shortname,
                os.path.basename(alphaseqfile)),
            dptapi.FILEDISP_OLD)
        cs = dptapi.APIContextSpecification(shortname)
        opencontext = dbserv.OpenContext(cs)

        opencontext.ApplyDeferredUpdates(0)

        # Close DPT file and delete folders and files created
        # by this method.
        dbserv.CloseContext(opencontext)
        dbserv.Free(shortname)
        sfserv.Free(_tapea)
        sfserv.Free(_tapen)
        for f in (alphaseqfile, numericseqfile):
            try:
                os.remove(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        os.path.basename(f)))
            except:
                pass
        for f in (_sfa, _sfn):
            try:
                os.remove(
                    os.path.join(
                        self._deferfolder,
                        shortname,
                        f))
            except:
                pass
        for field in self._ordered:
            if self._ordered[field]:
                self._DeleteSortedFilePath(field, shortname)

    def _GetFieldDetails(self):
        """Get field details to control deferred updates."""
        name = dptapi.StdStringPtr()
        fac = self._opencontext.OpenFieldAttCursor()
        while fac.Accessible():
            name.assign(fac.Name())
            fn = name.value()
            atts = fac.Atts()
            self._fid[fn] = fac.FID()
            self._ordered[fn] = atts.IsOrdered()
            self._visible[fn] = atts.IsVisible()
            self._string[fn] = atts.IsString()
            self._float[fn] = atts.IsFloat()
            self._ordnum[fn] = atts.IsOrdNum()
            fac.Advance(1)
        self._opencontext.CloseFieldAttCursor(fac)

    def _GetInputFiles(self, field, name=None):
        """Open all defer update files for filename."""
        if name == None:
            name = self._opencontext.GetShortName()
        files = []
        for seqfile in self._duseqfile[field]:
            files.append(
                _rmFile(
                    os.path.join(
                        self._deferfolder,
                        name,
                        field,
                        seqfile),
                    'rb'))
        return files

    def _GetSortedFileNames(self, field, name=None):
        """Get names of existing deferred update files for field."""
        if name == None:
            name = self._opencontext.GetShortName()
        path = os.path.join(
            self._deferfolder,
            name,
            field)
        files = []
        if os.path.isdir(path):
            for name in os.listdir(path):
                if name.isdigit():
                    if os.path.isfile(os.path.join(path, name)):
                        files.append((int(name), name))
            files.sort()
        self._duseqfile[field] = [n[-1] for n in files]
        if len(files):
            self._highseqno[field] = files[-1][0]
        else:
            self._highseqno[field] = 0

    def _Open(self, dbserv, sfserv, shortname, fullpath):
        """Open DPT file for deferred update."""
        if self._opencontext != None:
            return

        if shortname not in rmStoreRecord._seqfileno:
            rmStoreRecord._seqfileno[shortname] = str(
                len(rmStoreRecord._seqfileno))

        sfserv.Allocate(
            _sfa + rmStoreRecord._seqfileno[shortname],
            os.path.join(
                self._deferfolder,
                shortname,
                _sfa),
            dptapi.FILEDISP_COND)
        sfserv.Allocate(
            _sfn + rmStoreRecord._seqfileno[shortname],
            os.path.join(
                self._deferfolder,
                shortname,
                _sfn),
            dptapi.FILEDISP_COND)
        dbserv.Allocate(
            shortname,
            fullpath,
            dptapi.FILEDISP_COND)
        cs = dptapi.APIContextSpecification(shortname)
        
        self._opencontext = dbserv.OpenContext_DUMulti(
            cs,
            _sfn + rmStoreRecord._seqfileno[shortname],
            _sfa + rmStoreRecord._seqfileno[shortname],
            -1,
            self._duformat)
        

class _rmFile(file):

    """Provide read capability for deferred update sequential files.

    Subclass of file providing read capability for data files in
    format (value length, record number, ..., value) where value
    is less than 256 bytes and all other items are fixed length.

    At Python version 2.6 and later io.FileIO is imported as file.  Not
    recommended for production code.  Here the trick highlights the
    similarities and differences between the Python versions.

    Note that the ReadDefaultOrdNum method does not work at Python 2.6 and
    later and has been modified to return None in these environments.  It
    is the sample code that is at fault; not Python or DPT.
    """

    nullstrbyte = ''.encode()

    def __init__(self, filename, mode='r', bufsize=-1):
        """Extend io.FileIO, or file before 2.6, to handle DPT files.

        See Python manual for file function argument definitions
        """
        super(_rmFile, self).__init__(filename, mode, bufsize)

        self._roundeddouble = dptapi.APIRoundedDouble()
        self._lineseplen = len(os.linesep)

        # Set to ReadDefaultOrdChar or ReadDefaultOrdNum
        # by _DoDeferredUpdatesDefault as needed.
        self.ReadDefaultMerge = None

        # Set to ReadNoCRLFOrdChar or ReadNoCRLFOrdNum
        # by _DoDeferredUpdatesNoPadNoCRLF as needed.
        self.ReadNoCRLFMerge = None

    def ReadDefaultOrdChar(self):
        """Return an ordered character deferred update field and value.

        The field value pair is decorated for sorting.

        Convert record number representation on sequential file to string
        such that string sort order is same as  numeric sort order for
        record number.
        """
        try:
            d = self.readline()
            if len(d) > self._lineseplen:
                if d[0] == '\x00':
                    recno = d[4:0:-1]
                else:
                    n = ord(d[0])
                    recno = (chr((ord(d[4]) - n) & 255) +
                             chr((ord(d[3]) - n) & 255) +
                             chr((ord(d[2]) - n) & 255) +
                             chr((ord(d[1]) - n) & 255))
                value = d[7:-self._lineseplen]
                return (value, recno, d[5:7], len(value), d)
            else:
                return None
        except:
            return None

    def ReadDefaultOrdNum(self):
        """Return an ordered numeric deferred update field and value.

        Note that this implementation does not work at Python 3.0 and later.
        It does not work at Python 2.6 and later as well because io module
        is available and used; but if file class were used it would work.

        The field value pair is decorated for sorting.

        Convert record number representation on sequential file to string
        such that string sort order is same as  numeric sort order for
        record number.
        """
        recnofidvalue = self.read(14)
        try:
            return (
                self._roundeddouble.pyCastToRoundedDouble(recnofidvalue[-8:]),
                recnofidvalue[3::-1],
                recnofidvalue[4:6],
                8,
                recnofidvalue)
        except RuntimeError:
            if len(recnofidvalue):
                if iofile:
                    return None
                else:
                    raise
            else:
                return None
        except:
            if iofile:
                return None
            else:
                raise

    def ReadNoCRLFOrdChar(self):
        """Return an ordered character deferred update field and value.

        The field value pair is decorated for sorting.

        Convert record number representation on sequential file to string
        such that string sort order is same as  numeric sort order for
        record number.
        """
        recno = self.read(4)
        fid = self.read(2)
        valuelen = self.read(1)
        try:
            value = self.read(ord(valuelen))
        except TypeError:
            if len(recno):
                raise
            else:
                return None
        except:
            raise
        return (value,
                recno[::-1],
                fid,
                ord(valuelen),
                self.nullstrbyte.join((recno, fid, valuelen, value)))

    def ReadNoCRLFOrdNum(self):
        """Return an ordered numeric deferred update field and value.

        The field value pair is decorated for sorting.

        Convert record number representation on sequential file to string
        such that string sort order is same as  numeric sort order for
        record number.
        """
        recno = self.read(4)
        fid = self.read(2)
        valuelen = self.read(1)
        try:
            value = self.read(ord(valuelen))
        except TypeError:
            if len(recno):
                raise
            else:
                return None
        except:
            raise
        if usedecodeonfloat:
            self._roundeddouble.Assign(value.decode())
        else:
            self._roundeddouble.Assign(value)
        return (self._roundeddouble.Data(),
                recno[::-1],
                fid,
                ord(valuelen),
                self.nullstrbyte.join((recno, fid, valuelen, value)))


_maskshift = (
    (255 << 24, 24),
    (255 << 16, 16),
    (255 << 8, 8),
    (255, 0))


def _MakeKeyEncoder(size):
    """Return function that encodes key (integer) as string length size."""
    if size not in (2, 3, 4):
        size = 4
    ms = _maskshift[-size:]
    
    def KeyEncoder(i):
        """Return encoded key. Leftmost byte is most significant."""
        si = ''
        for m, s in ms:
            si += chr((i & m) >> s)
        return si

    return KeyEncoder
