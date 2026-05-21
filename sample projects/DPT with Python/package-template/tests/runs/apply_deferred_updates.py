# apply_deferred_updates.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Add records to databases.

The commit, backout, txn_per_record and deferred arguments control the
environment of the add records run.

deferred: True - index updates are deferred and transactions are disabled.
                commit, backout, and txn_per_record are ignored.
txn_per_record: True - each record is added in a separate transaction.
commit: True - transactions are explicitly committed.
                If commit is False a record is not added in a separate
                transaction.
backout: True - transactions are explicitly backed out.
                Default is False.
                If backout is False a record is not added, and then backed
                out, in a separate transaction.

If commit and backout are both False the default action, by default commit,
occurs when the database is closed.

"""
import os
    
import dpt_database


def _apply_deferred_updates(
    name,
    definition,
    records=(),
    commit=True,
    backout=False,
    txn_per_record=True,
    deferred=False,
    multistep=False,
    directory=None,
    forgivingness=None,
):
    """Apply deferred updates to database with definition in directory/name.

    records, commit, backout, txn_per_record, deferred and multistep are
    ignored.  The database is opened with deferred set False and multistep
    set True; and the database context's ApplyDeferredUpdates() method is
    called.

    forgivingness is number of records allowed out of sort order.  The
    default is -1, meaning no limit, despite what DPTDocs/language.html
    says on the matter.

    """
    if forgivingness is None:
        forgivingness = -1
    if directory is None:
        directory = dpt_database.directory_with_bitness()
    directory = os.path.join(directory, name)
    database = dpt_database.DPTDatabase(
        directory, deferred=False, multistep=True, filedefs=definition
    )
    database.create()
    for context in database.contexts.values():
        context.ApplyDeferredUpdates(forgivingness)
    return database


def keep_deferred_updates(*args, **kwargs):
    """Delegate to _apply_deferred_updates then close database."""
    _apply_deferred_updates(*args, **kwargs).close_database()


def apply_deferred_updates(*args, **kwargs):
    """Delegate to _apply_deferred_updates then delete database."""
    _apply_deferred_updates(*args, **kwargs).delete()
