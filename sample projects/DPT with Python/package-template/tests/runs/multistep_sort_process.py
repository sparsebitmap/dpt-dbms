# multistep_sort_process.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Sort multi-step deferred update 'tapea' and tapen' files."""
import os

import dpt_dbms.multistep_sort
import dpt_dbms.dptapi

import open_dpt_database
import filespec


def multistep_sort_process(
    name, definition, du_fixed_length=-1, du_format_options=None,
):
    """Sort the 'tapea' and 'tapen' files for name in definition.

    du_fixed_length and du_format_options indicate how the deferred
    update 'tape' files are formatted.

    The du_fixed_length argument is ignored if DU_FORMAT_NOPAD is given
    as an option in the du_format_options argument.  Otherwise the file
    will become physically inconsistent if the du_fixed_length argument
    is less than the length of a numeric field value being applied in
    the apply deferred updates stage.

    The du_fixed_length argument is relevant for 'tapen' files only.

    The du_fixed_length default, -1, means accept whatever data length
    is implied by the 'tapen' file format.

    """
    database = open_dpt_database.define_dpt_database(
        name,
        definition,
    )
    location = os.path.join(database.directory, "tapefiles")
    file = os.path.splitext(
        os.path.basename(database.filespec[name][filespec.FILE])
    )[0]
    tapea = file + "_tapea.txt"
    tapen = file + "_tapen.txt"
    multistep = dpt_dbms.multistep_sort.MultistepSort(
        location, tapea=tapea, tapen=tapen
    )

    # Default action.
    # Equivalent to DU_FORMAT_DEFAULT where neither DU_FORMAT_NOCRLF nor
    # DU_FORMAT_NOPAD flags are set.
    # The DU_FORMAT_DISCARD flag may be set: but this is about what to do
    # with the sorted files after they have been applied to database.
    if du_format_options is None:
        multistep.sort_tapea_crlf_delimited()
        if du_fixed_length < 0:
            multistep.sort_tape_ieee_numeric()
        else:
            multistep.sort_tapen_crlf_delimited(value_length=du_fixed_length)
        return

    # TapeA action.
    if du_format_options & dpt_dbms.dptapi.DU_FORMAT_NOCRLF:
        multistep.sort_tapea_length_delimited()
    else:
        multistep.sort_tapea_crlf_delimited()

    # TapeN action when DU_FORMAT_NOCRLF flag set.
    if du_format_options & dpt_dbms.dptapi.DU_FORMAT_NOCRLF:
        if du_format_options & dpt_dbms.dptapi.DU_FORMAT_NOPAD:
            multistep.sort_tapen_length_delimited()
        elif du_fixed_length < 0:
            multistep.sort_tape_ieee_numeric()
        else:
            multistep.sort_tapen_length_delimited(value_length=du_fixed_length)
        return

    # TapeN action when DU_FORMAT_NOPAD flag set without DU_FORMAT_NOCRLF.
    if du_format_options & dpt_dbms.dptapi.DU_FORMAT_NOPAD:
        multistep.sort_tapen_crlf_delimited()
        return

    # TapeN action when DU_FORMAT_NOPAD flag not set.
    if du_fixed_length < 0:
        multistep.sort_tape_ieee_numeric()
    else:
        multistep.sort_tapen_crlf_delimited(value_length=du_fixed_length)
