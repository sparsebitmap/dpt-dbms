#rmstorerecordssapi.py
#Copyright 2009 Roger Marsh
#See www.dptoolkit.com for details of DPT
#License: DPToolkit license

"""Sample code that does deferred updates in single-step mode.

self._fullpath is used because APIDatabaseFileContext does not have
a GetFullFilePath method like DatabaseFileContext. self._fullpath is
set by self.SetDefault() which creates the APIDatabaseFileContext
and unset by self.EndDeferredUpdates(). Maybe the SWIG interface
should extend APIDatabaseFileContext to get at GetFullFilePath.
"""

import os
import os.path

import dptdb.dptapi as dptapi


class rmStoreRecordError(Exception):
    pass


class rmStoreRecord(object):

    """Manage the dptapi.StoreRecord calls in single-step deferred update.

    Style for using this class
    
    For all files
        SetDefault
    Arbitrary sequence of StoreRecord calls

    Repeat as needed.
    The same Set... method must be called for all files.
    """

    def __init__(self, deferfolder):

        self._opencontext = None
        self._deferfolder = deferfolder
        self._fullpath = None

        self._fieldvalue = dptapi.APIFieldValue()
        self._putrecordcopy = dptapi.APIStoreRecordTemplate()

    def DoDeferredUpdates(self,  dbserv):
        """Apply deferred updates"""
        if not isinstance(self._opencontext, dptapi.APIDatabaseFileContext):
            return
            
        # Close DPT file and delete folders and files created
        # by this method.
        shortname = self._opencontext.GetShortName()
        dbserv.CloseContext(self._opencontext)
        dbserv.Free(shortname)

    def EndDeferredUpdates(self, dbserv):
        """Reset instance state to allow further deferred updates. """
        self._opencontext = None
        # See note at start of module
        self._fullpath = None

    def SetDefault(self, dbserv, shortname, fullpath):
        """Configure for Single Step deferred update."""
        if self._opencontext != None:
            return
        
        # See note at start of module
        self._fullpath = fullpath

        self._Open(dbserv, shortname, fullpath)

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

    def _Open(self, dbserv, shortname, fullpath):
        """Open DPT file for deferred update."""
        if self._opencontext != None:
            return

        dbserv.Allocate(
            shortname,
            fullpath,
            dptapi.FILEDISP_COND)
        cs = dptapi.APIContextSpecification(shortname)
        
        self._opencontext = dbserv.OpenContext_DUSingle(cs)
