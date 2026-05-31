# open_dpt_database.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Open DPT databases defined in file_definitions module."""
import os
    
import dpt_database


def open_dpt_database(*args, **kwargs):
    """Return database for definition in directory/name.

    See define_dpt_database for arguments.

    The database is created if it does not exist.

    An OpenContext for each database file is available in self.contexts
    dict keyed by a file name from the file_definitions module used in
    the file_definition function which defines the database.

    """
    database = define_dpt_database(*args, **kwargs)
    database.create()
    return database


def create_database(
    name, definition, default_records=200, brecppg=50, btod_factor=1
):
    """Create then close a database and return None.

    Initially provided for convenient use wia multiprocessing.

    """
    open_dpt_database(
        name,
        definition(
            default_records=default_records,
            brecppg=brecppg,
            btod_factor=btod_factor,
        ),
    ).close_database()


# Split out of open_dpt_database when multi-step deferred update test
# runs introduced: but probably best split out anyway.
def define_dpt_database(
    name,
    definition,
    directory=None,
    deferred=False,
    multistep=False,
    load=False,
    unload=False,
    du_fixed_length=-1,  # OpenContext_DUMulti du_parm3 argument.
    du_format_options=None,  # OpenContext_DUMulti du_parm4 argument.
):
    """Return database for definition in directory/name.

    Introduced when multi-step deferred update test runs added.

    """
    if directory is None:
        directory = dpt_database.directory_with_bitness()
    directory = os.path.join(directory, name)
    database = dpt_database.DPTDatabase(
        directory,
        deferred=deferred,
        load=load,
        multistep=multistep,
        unload=unload,
        filedefs=definition,
        du_fixed_length=du_fixed_length,
        du_format_options=du_format_options,
    )
    return database
