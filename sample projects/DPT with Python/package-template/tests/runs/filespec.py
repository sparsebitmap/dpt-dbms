# filespec.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Provide FileSpec creation behaviour common to all file specifications.

Example database specifications are in the samples directory.

"""
import os

BSIZE = "bsize"
BRECPPG = "brecppg"
DSIZE = "dsize"
BRESERVE = "breserve"
BREUSE = "breuse"
DRESERVE = "dreserve"
DPGSRES = "dpgsres"
BTOD_FACTOR = "btod_factor"
EO = 0
RRN = 36
DEFAULT_RECORDS = "default_records"
FILEDESC = "filedesc"
FILEORG = "fileorg"
DEFAULT_INITIAL_NUMBER_OF_RECORDS = 200
FILE = "file"
DDNAME = "ddname"
FIELDS = "fields"
BTOD_CONSTANT = "btod_constant"
FLT = "float"
INV = "invisible"
UAE = "update_at_end"
ORD = "ordered"
ONM = "ordnum"
SPT = "splitpct"
DEFAULT = -1
MANDATORY_FILEATTS = {
    BSIZE: (int, type(None)),
    BRECPPG: int,
    DSIZE: (int, type(None)),
    FILEORG: int,
}
FILEATTS = {
    BSIZE: None,
    BRECPPG: None,
    BRESERVE: DEFAULT,
    BREUSE: DEFAULT,
    DSIZE: None,
    DRESERVE: DEFAULT,
    DPGSRES: DEFAULT,
    FILEORG: None,
}
SUPPORTED_FILEORGS = (EO, RRN)

# INV, ONM and SPT are ignored if ORD is False.
# ONM qualifies ORD as numeric or character if ORD is True.
FIELDATTS = {
    FLT: bool,
    INV: bool,
    UAE: bool,
    ORD: bool,
    ONM: bool,
    SPT: int,
}
DEFAULT_FIELDATTS = {
    FLT: False,
    INV: False,
    UAE: False,
    ORD: False,
    ONM: False,
    SPT: 50,
}


class FileSpecError(Exception):
    """Exception for FileSpec class."""


class FileSpec(dict):
    """Create FileSpec from database specification in **kargs.

    Derived from solentware_base.core.filespec with all non-DPT constructs
    removed.  In particular the definition of one field as primary and the
    rest as secondary is not present because this implemented the way DPT
    fields represented Berkeley DB primary and secondary databases.

    """

    @staticmethod
    def dpt_dd(file_def):
        """Return a standard DD name for DPT from file_def."""
        if len(file_def) > 8:
            raise FileSpecError(
                str(file_def).join(
                    ("'", "' too long - more than 8 characters")
                )
            )
        return file_def.upper()

    @staticmethod
    def dpt_dsn(file_def):
        """Return a standard filename (DSN name) for DPT from file_def."""
        return "".join((file_def.lower(), ".dpt"))

    @staticmethod
    def field_name(field_def):
        """Return standard fieldname to be the implementation resource name."""
        return "".join((field_def[0].upper(), field_def[1:]))

    def __init__(self, **kargs):
        """Define defaults for essential parameters for DPT database engine.

        **kargs=<file specifications>

        DPT makes one file per item in kargs containing non-ordered and ordered
        fields.

        """
        super().__init__(**kargs)

        for k, value in self.items():
            records = value.setdefault(
                DEFAULT_RECORDS, DEFAULT_INITIAL_NUMBER_OF_RECORDS
            )
            filedesc = value.setdefault(FILEDESC, {})
            brecppg = filedesc.setdefault(BRECPPG, 10)
            filedesc.setdefault(FILEORG, RRN)
            btod_factor = value.setdefault(BTOD_FACTOR, 8)
            bsize = records // brecppg
            if bsize * brecppg < records:
                bsize += 1
            value[FILEDESC][BSIZE] = bsize
            value[FILEDESC][DSIZE] = int(round(bsize * btod_factor))
            value.setdefault(BTOD_CONSTANT, 0)

        # Validate the specification, which may have been expanded in the
        # preceding section.
        pathnames = {}
        for name, specification in self.items():
            if not isinstance(specification, dict):
                msg = " ".join(
                    ["Specification for", repr(name), "must be a dictionary"]
                )
                raise FileSpecError(msg)
            if FIELDS not in specification:
                msg = " ".join(
                    [
                        "Field definitions must be present in",
                        "specification for primary fields",
                    ]
                )
                raise FileSpecError(msg)
            try:
                os.path.join(specification.get(FILE))
            except TypeError as exc:
                msg = " ".join(
                    ["File name for", name, "must be a valid path name"]
                )
                raise FileSpecError(msg) from exc
            filedesc = specification.get(FILEDESC)
            if not isinstance(filedesc, dict):
                msg = " ".join(
                    ["Description of file", name, "must be a dictionary"]
                )
                raise FileSpecError(msg)
            for attr in MANDATORY_FILEATTS:
                if attr not in filedesc:
                    msg = " ".join(
                        [
                            "Attribute",
                            repr(attr),
                            "for file",
                            name,
                            "must be present",
                        ]
                    )
                    raise FileSpecError(msg)
            for attr in filedesc:
                if attr not in FILEATTS:
                    msg = " ".join(
                        [
                            "Attribute",
                            repr(attr),
                            "for file",
                            name,
                            "is not allowed",
                        ]
                    )
                    raise FileSpecError(msg)

                if attr not in MANDATORY_FILEATTS:
                    if not isinstance(filedesc[attr], int):
                        msg = " ".join(
                            [
                                "Attribute",
                                repr(attr),
                                "for file",
                                name,
                                "must be a number",
                            ]
                        )
                        raise FileSpecError(msg)
                elif not isinstance(filedesc[attr], MANDATORY_FILEATTS[attr]):
                    msg = " ".join(
                        [
                            "Attribute",
                            repr(attr),
                            "for file",
                            name,
                            "is not correct type",
                        ]
                    )
                    raise FileSpecError(msg)
            if filedesc.get(FILEORG, None) not in SUPPORTED_FILEORGS:
                msg = " ".join(
                    [
                        "File",
                        name,
                        'must be "Entry Order" or',
                        '"Unordered and Reuse Record Number"',
                    ]
                )
                raise FileSpecError(msg)
            fields = specification[FIELDS]
            if not isinstance(fields, dict):
                msg = " ".join(
                    [
                        "Field description of file",
                        repr(name),
                        "must be a dictionary",
                    ]
                )
                raise FileSpecError(msg)

            # Set field description.
            for fieldname in fields:
                description = fields[fieldname]
                if description is None:
                    description = {}
                    fields[fieldname] = description
                if not isinstance(description, dict):
                    msg = " ".join(
                        [
                            "Attributes for field",
                            fieldname,
                            "in file",
                            repr(name),
                            'must be a dictionary or "None"',
                        ]
                    )
                    raise FileSpecError(msg)
                for attr in description:
                    if attr not in FIELDATTS:
                        msg = " ".join(
                            [
                                "Attribute",
                                repr(attr),
                                "for field",
                                fieldname,
                                "in file",
                                name,
                                "is not allowed",
                            ]
                        )
                        raise FileSpecError(msg)
                    if not isinstance(description[attr], FIELDATTS[attr]):
                        msg = " ".join(
                            [
                                attr,
                                "for field",
                                fieldname,
                                "in file",
                                name,
                                "is wrong type",
                            ]
                        )
                        raise FileSpecError(msg)
                    if attr == SPT:
                        if description[attr] < 0 or description[attr] > 100:
                            msg = " ".join(
                                [
                                    "Split percentage for field",
                                    fieldname,
                                    "in file",
                                    name,
                                    "is invalid",
                                ]
                            )
                            raise FileSpecError(msg)
                for attr in DEFAULT_FIELDATTS:
                    if attr not in description:
                        description[attr] = DEFAULT_FIELDATTS[attr]
            try:
                ddname = specification[DDNAME]
            except KeyError as exc:
                msg = " ".join(
                    ["Specification for", name, "must have a DD name"]
                )
                raise FileSpecError(msg) from exc
            if len(ddname) == 0:
                msg = " ".join(
                    ["DD name", repr(ddname), "for", name, "is zero length"]
                )
                raise FileSpecError(msg)
            if len(ddname) > 8:
                msg = " ".join(
                    ["DD name", ddname, "for", name, "is over 8 characters"]
                )
                raise FileSpecError(msg)
            if not ddname.isalnum():
                msg = " ".join(
                    [
                        "DD name",
                        ddname,
                        "for",
                        name,
                        "must be upper case alphanum",
                        "starting with alpha",
                    ]
                )
                raise FileSpecError(msg)
            if not ddname.isupper():
                msg = " ".join(
                    [
                        "DD name",
                        ddname,
                        "for",
                        name,
                        "must be upper case alphanum",
                        "starting with alpha",
                    ]
                )
                raise FileSpecError(msg)
            if not ddname[0].isupper():
                msg = " ".join(
                    [
                        "DD name",
                        ddname,
                        "for",
                        name,
                        "must be upper case alphanum",
                        "starting with alpha",
                    ]
                )
                raise FileSpecError(msg)
            try:

                # At Python26+ need to convert unicode to str for DPT.
                fname = str(
                    os.path.join(
                        specification.get(FILE, None),
                    )
                )

            except Exception as exc:
                msg = " ".join(
                    [
                        "Relative path name of DPT file for",
                        name,
                        "is invalid",
                    ]
                )
                raise FileSpecError(msg) from exc
            if fname in pathnames:
                msg = " ".join(
                    [
                        "File name",
                        os.path.basename(fname),
                        "linked to",
                        pathnames[fname],
                        "cannot link to",
                        name,
                    ]
                )
                raise FileSpecError(msg)

            pathnames[fname] = name

    def set_file_directory(self, directory, dptfile=None):
        """Prefix file for dptfile item with directory.

        All files are prefixed if dptfile is None.

        """
        for key, value in self.items():
            if dptfile is None or key == dptfile:
                value[FILE] = os.path.join(directory, value[FILE])
