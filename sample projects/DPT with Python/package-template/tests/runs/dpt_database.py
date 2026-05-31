# dpt_database.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Provide class which defines, creates and deletes a database."""
import sys
import os
import shutil
    
from dpt_dbms import dptapi

import filespec

# See documentation for platform module.
is_64bits = sys.maxsize > 2 ** 32


def directory_with_bitness():
    """Return directory with 'bitness' of Python interpreter appended.

    Used by 'run_test_*' modules to get a default directory name.

    """
    directory = os.environ.get(
        "TEMP",
        os.environ.get("TMP",os.environ.get("HOME")),
    )
    return os.path.join(
        directory, "dpt_dbms_test" + ("64" if is_64bits else "32"),
    )


class DPTDatabase:
    """Define database."""

    def __init__(
        self,
        directory,
        deferred=False,
        multistep=False,
        load=False,
        unload=False,
        filedefs=None,
        du_fixed_length=-1,  # OpenContext_DUMulti du_parm3 argument.
        du_format_options=None,  # OpenContext_DUMulti du_parm4 argument.
    ):
        """Create DPT definition for a database in directory from filedefs.

        if bool(multistep) is True and bool(deferred) is True a multistep
        deferred update is done.
        if bool(multistep) is True and bool(load) is False and bool(unload)
        is False the run is assumed to be applying sorted deferred updates
        from a multistep deferred update run.
        load is ignored if bool(deferred) is True.
        unload is ignored if bool(deferred) is True.
        unload is ignored if bool(load) is True.

        du_parm3 and du_format_options are ignored uless bool(deferred)
        and bool(multistep) are True.

        """
        self.directory = directory
        self.deferred = deferred
        if self.deferred:
            load = False
            unload = False
        self.load = load
        self.unload = unload
        if load or unload:
            multistep = False
        self.multistep = multistep
        if filedefs is None:
            filedefs = {}
        self.filespec = filespec.FileSpec(**filedefs)
        self.du_fixed_length = du_fixed_length
        if du_format_options is None:
            du_format_options = dptapi.DU_FORMAT_DEFAULT
        self.du_format_options = du_format_options

        # Because a loop on self.filespec is done.
        # Multiple files in the file specification is fine in itself.
        # The limit is the "TAPEN" and "TAPEA" allocation names used
        # in multi-step deferred update.
        # Should not be needed now: 'tape' file name generation adjusted.
        #if self.multistep and len(self.filespec) != 1:
        #    raise RuntimeError(
        #        "".join(
        #            (
        #                "Cannot do multi-step deferred update on ",
        #                "multiple files in one go",
        #            )
        #        )
        #    )

        self.filespec.set_file_directory(self.directory)
        self.database_services = None
        self.sequential_file_services = None
        self.allocated = set()
        self.contexts = {}
        self.dptsys = os.path.join(self.directory, 'sys')

    def create(self):
        """Create the DPT database defined in self.filespec"""
        parms = os.path.join(self.dptsys, 'parms.ini')
        msgctl = os.path.join(self.dptsys, 'msgctl.ini')
        audit = os.path.join(self.dptsys, 'audit.txt')

        # Use CONSOLE rather than sysprint because SYSPRNT is not fully
        # released unless Python is restarted as a new command line command.
        # To see CONSOLE run this script by python.exe rather than pythonw.exe
        sysprint = "CONSOLE"

        if not os.path.exists(self.directory):
            os.makedirs(self.directory)
        if not os.path.exists(self.dptsys):
            os.makedirs(self.dptsys)
            
        # Set parms for normal, single-step deferred update, multi-step
        # deferred update, unload or load mode.
        if self.deferred:
            pf = open(parms, 'w')
            try:
                pf.write("RCVOPT=X'00' " + os.linesep)
                pf.write("MAXBUF=99 " + os.linesep)
            finally:
                pf.close()
        elif self.load:
            pf = open(parms, 'w')
            try:
                pf.write("RCVOPT=X'00' " + os.linesep)
                pf.write("MAXBUF=99 " + os.linesep)
            finally:
                pf.close()
        elif self.unload:
            pf = open(parms, 'w')
            try:
                pf.write("MAXBUF=99 " + os.linesep)
            finally:
                pf.close()
        elif os.path.exists(parms):
            os.remove(parms) # assume delete will work here

        # Ensure checkpoint.ckp file and #SEQTEMP folder are created in correct
        # folder (change current working directory).
        pycwd = os.getcwd()
        os.chdir(self.dptsys)
        ds = dptapi.APIDatabaseServices(
            sysprint,
            'pyapitest',
            parms,
            msgctl,
            audit)
        try:
            os.chdir(pycwd)
        except:
            ds.Destroy()
            del ds
            raise
        self.database_services = ds
        del ds

        # Allocate, create, and open context on file, followed by initialize
        # file and create fields if it does not yet exist.
        try:
            if self.deferred:
                if self.multistep:
                    dsoc = self.database_services.OpenContext_DUMulti
                else:
                    dsoc = self.database_services.OpenContext_DUSingle
            else:
                dsoc = self.database_services.OpenContext
            for key, value in self.filespec.items():
                disp = os.path.exists(value[filespec.FILE])
                if not disp:
                    if self.deferred:
                        raise RuntimeError(
                            "Cannot do deferred update on non-existent file"
                        )
                    if self.unload:
                        raise RuntimeError(
                            "Cannot do unload on non-existent file"
                        )
                    if self.load:
                        raise RuntimeError(
                            "Cannot do load on non-existent file"
                        )
                    if self.multistep:
                        raise RuntimeError(
                            "".join(
                                (
                                    "Cannot apply sorted deferred updates ",
                                    "to non-existent file",
                                )
                            )
                        )
                if disp:
                    self.database_services.Allocate(
                        value[filespec.DDNAME],
                        value[filespec.FILE],
                        dptapi.FILEDISP_OLD,
                    )
                else:
                    self.database_services.Allocate(
                        value[filespec.DDNAME],
                        value[filespec.FILE],
                        dptapi.FILEDISP_NEW,
                    )
                    records_per_page = value[
                        filespec.FILEDESC
                    ][filespec.BRECPPG]
                    table_b_size, extra = divmod(
                        value[
                            filespec.DEFAULT_RECORDS
                        ],
                        records_per_page,
                    )
                    if extra:
                        table_b_size += 1
                    self.database_services.Create(value[filespec.DDNAME],
                              table_b_size,
                              records_per_page,
                              -1,
                              -1,
                              table_b_size * value[
                                  filespec.BTOD_FACTOR
                              ] + value[filespec.BTOD_CONSTANT],
                              -1,
                              -1,
                              dptapi.FILEORG_UNORD_RRN)
                self.allocated.add(key)
                cs = dptapi.APIContextSpecification(value[filespec.DDNAME])
                if self.multistep:
                    dirname = os.path.dirname(value[filespec.FILE])
                    tapedir = os.path.join(dirname, "tapefiles")

                    # Defer index updates or apply deferred index updates?
                    if self.deferred:
                        filedisp = dptapi.FILEDISP_NEW
                        try:
                            os.mkdir(tapedir)
                        except FileExistsError:
                            pass
                    else:
                        filedisp = dptapi.FILEDISP_OLD

                    basename = os.path.basename(value[filespec.FILE])
                    filename = os.path.splitext(basename)[0]
                    tapea = os.path.join(tapedir, filename + "_tapea.txt")
                    tapen = os.path.join(tapedir, filename + "_tapen.txt")
                    sfs = self.database_services.SeqServs()
                    sfs.Allocate("TAPEA", tapea, filedisp)
                    sfs.Allocate("TAPEN", tapen, filedisp)
                    self.sequential_file_services = sfs

                    # Defer index updates or apply deferred index updates?
                    if self.deferred:
                        oc = dsoc(
                            cs,
                            "TAPEN",
                            "TAPEA",
                            self.du_fixed_length,
                            self.du_format_options,
                        )
                    else:
                        oc = dsoc(cs)

                else:
                    oc = dsoc(cs)
                if not disp:
                    oc.Initialize()
                    for field, attributes in value[filespec.FIELDS].items():
                        fa = dptapi.APIFieldAttributes()
                        if attributes[filespec.ORD]:
                            fa.SetOrderedFlag()
                            fa.SetSplitPct(attributes[filespec.SPT])
                            if attributes[filespec.ONM]:
                                fa.SetOrdNumFlag()
                            if attributes[filespec.INV]:
                                fa.SetInvisibleFlag()
                        if attributes[filespec.FLT]:
                            fa.SetFloatFlag()
                        if attributes[filespec.UAE]:
                            fa.SetUpdateAtEndFlag()
                        oc.DefineField(field, fa)
                        del fa
                self.contexts[key] = oc
                del cs, oc
        except:
            self.delete()
            raise

    def free(self):
        """Free any allocated dpt files."""
        if self.database_services is None:
            return
        for key in self.allocated:
            self.database_services.Free(self.filespec[key][filespec.DDNAME])
        self.allocated.clear()

    def close_contexts(self):
        """Close any contexts that are open."""
        if self.database_services is None:
            return
        # for value in self.contexts.values():
        #    self.database_services.CloseContext(value)
        self.contexts.clear()
        self.database_services.CloseAllContexts(force=True)

        # Commented code follows multi-step example in DPT documentation,
        # but file is still in use.  Python problem or restriction?
        #if self.multistep and self.sequential_file_services:
        #    self.sequential_file_services.Free("TAPEA")
        #    self.sequential_file_services.Free("TAPEN")

    def __delete(self, delete_directory=False):
        """Delete DPT database services and directory if delete_directory."""
        if self.database_services is None:
            return
        self.close_contexts()
        self.free()
        pycwd = os.getcwd()
        os.chdir(self.dptsys)
        self.database_services.Destroy()
        try:
            os.chdir(pycwd)
            if delete_directory:
                self.delete_database_directory()
        finally:
            self.database_services = None

    def delete(self):
        """Delete DPT database services and database directory."""
        self.__delete(delete_directory=True)

    def delete_database_directory(self):
        """Delete database directory.

        Introduced when multi-step deferred update test runs added.

        """
        shutil.rmtree(self.directory)

    def close_database(self):
        """Delete DPT database services but not database directory."""
        self.__delete(delete_directory=False)

    def add_record(self, context, record=()):
        """Store record in context with fields in given order.

        The record argument must be iterable with items containing two
        elements: field and value.

        """
        template = dptapi.APIStoreRecordTemplate()
        for field, value in record:
            template.Append(field, dptapi.APIFieldValue(value))
        context.StoreRecord(template)
