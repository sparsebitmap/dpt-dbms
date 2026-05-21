# run_test_multistep_sort.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Add records to empty databases with ordered fields.

Run run_test_multistep_no_sort five times with a number of simple field
and value combinations and few records to verify the multi-step deferred
update processing sequence doing sorts without forgivingness.

"""
import multiprocessing

import add_records
import open_dpt_database
import apply_deferred_updates
import record_tuples
import multistep_sort_process


def run_test_multistep_sort(
    default_records=200,
    brecppg=50,
    btod_factor=1,
    modulus=None,
    deferred=True,
    multistep=True,
    forgivingness=None,
    du_fixed_length=-1,  # OpenContext_DUMulti du_parm3 argument.
    du_format_options=None,  # OpenContext_DUMulti du_parm4 argument.
    ignore_discard=True,
    items=None,
):
    """Run tests for all the database definitions in file_definitions.

    ignore_discard controls whether the sorted TAPE files are deleted at
    end of test run.  By default delete them, but at least two test runs
    should do what du_format_options demands.

    """
    if items is None:
        items = ()
    print(
        "enter run_test_multistep_sort",
        "(deferred is " + str(deferred) + ")",
        "adding",
        default_records,
        "records",
    )
    for name, definition, records in items:
        print("enter multi-step deferred update for", name)

        process = multiprocessing.Process(
            target=open_dpt_database.create_database,
            name="Create Database",
            args=(name, definition),
            kwargs=dict(
                default_records=default_records,
                brecppg=brecppg,
                btod_factor=btod_factor,
            ),
        )
        process.start()
        process.join()

        process = multiprocessing.Process(
            target=add_records.keep_records,
            name="Populate Database with Index Updates Deferred",
            args=(
                name,
                definition(
                    default_records=default_records,
                    brecppg=brecppg,
                    btod_factor=btod_factor,
                ),
            ),
            kwargs=dict(
                records=records,
                default_records=default_records,
                modulus=modulus,
                deferred=deferred,
                multistep=multistep,
                du_fixed_length=du_fixed_length,
                du_format_options=du_format_options,
            ),
        )
        process.start()
        process.join()

        process = multiprocessing.Process(
            target=multistep_sort_process.multistep_sort_process,
            name="Sort Deferred Updates",
            args=(
                name,
                definition(),  #  File names matter, but not sizing stuff.
            ),
            kwargs=dict(
                du_fixed_length=du_fixed_length,
                du_format_options=du_format_options,
            ),
        )
        process.start()
        process.join()

        process = multiprocessing.Process(
            target=apply_deferred_updates.keep_deferred_updates,
            name="Apply Deferred Updates",
            args=(
                name,
                definition(
                    default_records=default_records,
                    brecppg=brecppg,
                    btod_factor=btod_factor,
                ),
            ),
            kwargs=dict(forgivingness=forgivingness)
        )
        process.start()
        process.join()

        # Action decided by ignore_discard argument to see what happened
        # to the TAPE files depending on the DU_FORMAT_DISCARD flag.
        if ignore_discard:
            open_dpt_database.define_dpt_database(
                name, {}  # A null definition, {}, is ok at this point.
            ).delete_database_directory()

        print("leave multi-step deferred update for", name)
    print("done")


if __name__ == "__main__":
    multiprocessing.set_start_method("spawn")
    run_test_multistep_sort(
        forgivingness=0,
        items=record_tuples.record_generators,
    )
