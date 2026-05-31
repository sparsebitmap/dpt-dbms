# file_definitions.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Define sample FileSpec instances for tests."""
import filespec as fs

# Field names used in these file definitions.
FLD = "fld"
FLDORD = "fldord"
FLDINV = "fldinv"
DATA = "data"

# File names used in these file definitions.
ONE_FILE_NO_FIELDS = "nofields"
ONE_FILE_ONE_FIELD = "onefield"
ONE_FIELD_ORDERED = "oneford"
TWO_FIELD_ONE_ORDERED = "twoford"
TWO_FIELD_ONE_INVISIBLE = "twofinv"
THREE_FIELD_ONE_INVISIBLE = "ordfinv"
DATA_DATA_ORD_INV = "ddoiflds"
VIS_INV_INV_INV = "visinv"
DATA_DATA_ORD_INV_UPPER = "ddoiufds"
SORTING = "sorting"


def one_file_no_fields(default_records=200, brecppg=50, btod_factor=1):
    """Return a no fields file definition for 200 records by default."""
    return {
        ONE_FILE_NO_FIELDS : {
            fs.DDNAME: fs.FileSpec.dpt_dd(ONE_FILE_NO_FIELDS),
            fs.FILE: fs.FileSpec.dpt_dsn(ONE_FILE_NO_FIELDS),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {},
        },
    }


def one_file_one_field(default_records=200, brecppg=50, btod_factor=1):
    """Return a one field file definition for 200 records by default."""
    return {
        ONE_FILE_ONE_FIELD : {
            fs.DDNAME: fs.FileSpec.dpt_dd(ONE_FILE_ONE_FIELD),
            fs.FILE: fs.FileSpec.dpt_dsn(ONE_FILE_ONE_FIELD),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
            },
        },
    }


def one_field_ordered(default_records=200, brecppg=50, btod_factor=1):
    """Return a one field file definition for 200 records by default.

    The field is ordered.

    """
    return {
        ONE_FIELD_ORDERED : {
            fs.DDNAME: fs.FileSpec.dpt_dd(ONE_FIELD_ORDERED),
            fs.FILE: fs.FileSpec.dpt_dsn(ONE_FIELD_ORDERED),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): {fs.ORD: True},
            },
        },
    }


def two_field_one_ordered(default_records=200, brecppg=50, btod_factor=1):
    """Return a two field file definition for 200 records by default.

    One field is ordered.

    """
    return {
        TWO_FIELD_ONE_ORDERED : {
            fs.DDNAME: fs.FileSpec.dpt_dd(TWO_FIELD_ONE_ORDERED),
            fs.FILE: fs.FileSpec.dpt_dsn(TWO_FIELD_ONE_ORDERED),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
                fs.FileSpec.field_name(FLDORD): {fs.ORD: True},
            },
        },
    }


def two_field_one_invisible(default_records=200, brecppg=50, btod_factor=1):
    """Return a one field file definition for 200 records by default.

    One field is invisible, which means it is ordered too.

    """
    return {
        TWO_FIELD_ONE_INVISIBLE : {
            fs.DDNAME: fs.FileSpec.dpt_dd(TWO_FIELD_ONE_INVISIBLE),
            fs.FILE: fs.FileSpec.dpt_dsn(TWO_FIELD_ONE_INVISIBLE),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
                fs.FileSpec.field_name(FLDINV): {fs.ORD: True, fs.INV: True},
            },
        },
    }


def three_field_one_invisible(default_records=200, brecppg=50, btod_factor=1):
    """Return a three field file definition for 200 records by default.

    One field is ordered and another is invisible (ordered too).

    """
    return {
        THREE_FIELD_ONE_INVISIBLE : {
            fs.DDNAME: fs.FileSpec.dpt_dd(THREE_FIELD_ONE_INVISIBLE),
            fs.FILE: fs.FileSpec.dpt_dsn(THREE_FIELD_ONE_INVISIBLE),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
                fs.FileSpec.field_name(FLDORD): {fs.ORD: True},
                fs.FileSpec.field_name(FLDINV): {fs.ORD: True, fs.INV: True},
            },
        },
    }


def data_data_ord_inv(default_records=200, brecppg=50, btod_factor=1):
    """Return a four field file definition for 200 records by default.

    One field is ordered and another is invisible (ordered too).
    The fourth field is another unordered field.

    """
    return {
        DATA_DATA_ORD_INV : {
            fs.DDNAME: fs.FileSpec.dpt_dd(DATA_DATA_ORD_INV),
            fs.FILE: fs.FileSpec.dpt_dsn(DATA_DATA_ORD_INV),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
                fs.FileSpec.field_name(FLDORD): {fs.ORD: True},
                fs.FileSpec.field_name(FLDINV): {fs.ORD: True, fs.INV: True},
                fs.FileSpec.field_name(DATA): None,
            },
        },
    }


def data_data_ord_inv_upper(default_records=200, brecppg=50, btod_factor=1):
    """Return a four field file definition for 200 records by default.

    One field is ordered and another is invisible (ordered too).
    The fourth field is another unordered field.

    This definition exists to demonstrate fastload is incompatible with
    field names which are not uppercase.

    """
    return {
        DATA_DATA_ORD_INV_UPPER : {
            fs.DDNAME: fs.FileSpec.dpt_dd(DATA_DATA_ORD_INV_UPPER),
            fs.FILE: fs.FileSpec.dpt_dsn(DATA_DATA_ORD_INV_UPPER),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD.upper()): None,
                fs.FileSpec.field_name(FLDORD.upper()): {fs.ORD: True},
                fs.FileSpec.field_name(FLDINV.upper()): {
                    fs.ORD: True, fs.INV: True
                },
                fs.FileSpec.field_name(DATA.upper()): None,
            },
        },
    }


def vis_inv_inv_inv(default_records=200, brecppg=50, btod_factor=1):
    """Return a four field file definition for 200 records by default.

    One field is not ordered.
    The other three fields are invisible, hence ordered, fields.

    Intended for records with in excess of 4000 index value references,
    which is seen on databases created by chesstab where the problem
    with x64 dptdb was seen.

    The number of index values must be large enough to make the DPT
    audit file show multiple 'chunks' being processed when the
    run_test_vis_inv_inv_inv module is run.

    Therefore the test directory has to be deleted manually afterwards.

    """
    return {
        VIS_INV_INV_INV : {
            fs.DDNAME: fs.FileSpec.dpt_dd(VIS_INV_INV_INV),
            fs.FILE: fs.FileSpec.dpt_dsn(VIS_INV_INV_INV),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
                fs.FileSpec.field_name(FLDORD): {fs.ORD: True, fs.INV: True},
                fs.FileSpec.field_name(FLDINV): {fs.ORD: True, fs.INV: True},
                fs.FileSpec.field_name(DATA): {fs.ORD: True, fs.INV: True},
            },
        },
    }


def sorting(default_records=200, brecppg=50, btod_factor=1):
    """Return a four field file definition for 200 records by default.

    One field is not ordered.
    The other three fields are ordered fields.

    At least one ordered field is invisible, and at least one is numeric.

    """
    return {
        SORTING : {
            fs.DDNAME: fs.FileSpec.dpt_dd(SORTING),
            fs.FILE: fs.FileSpec.dpt_dsn(SORTING),
            fs.FILEDESC: {
                fs.BRECPPG: brecppg,
                fs.FILEORG: fs.RRN,
            },
            fs.BTOD_FACTOR: btod_factor,
            fs.BTOD_CONSTANT: 30,
            fs.DEFAULT_RECORDS: default_records,
            fs.FIELDS: {
                fs.FileSpec.field_name(FLD): None,
                fs.FileSpec.field_name(FLDORD): {fs.ORD: True},
                fs.FileSpec.field_name(FLDINV): {fs.ORD: True, fs.INV: True},
                fs.FileSpec.field_name(DATA): {fs.ORD: True, fs.ONM: True},
            },
        },
    }
