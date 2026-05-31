# multistep_sort.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Unittests for multistep_sort module."""

import unittest
import os
import shutil

import dpt_dbms
from dpt_dbms import multistep_sort


class ModuleContents(unittest.TestCase):

    def test_module_contents_01(self):
        contents = [
            a for a in dir(multistep_sort)
            if not a.startswith("__") and not a.endswith("__")
        ]
        self.assertEqual(
            contents,
            [
                "MultistepSort",
                "ValueLengthAllowedFixedTapeN",
                "ValueLengthAllowedTapeN",
                "ValueLengthIEEETapeN",
                "ValueLengthTapeA",
                "ValueLengthTapeN",
                "_CHUNKS_DIRECTORY",
                "_HEADER_LENGTH",
                "_MAX_TAPE_CRLF_LINE_LENGTH",
                "_MAX_VALUE_LENGTH",
                "_ORIGINAL",
                "_RECORD_NUMBERS_PER_SEGMENT",
                "_TAPE_IEEE_NUMERIC_LENGTH",
                "os",
                "struct",
            ]
        )

    def test_module_constants_values_01(self):
        ae = self.assertEqual
        ae(multistep_sort._CHUNKS_DIRECTORY , "chunks")
        ae(multistep_sort._HEADER_LENGTH , 7)
        ae(multistep_sort._MAX_TAPE_CRLF_LINE_LENGTH , 262)
        ae(multistep_sort._MAX_VALUE_LENGTH , 255)
        ae(multistep_sort._ORIGINAL , ".original")
        ae(multistep_sort._RECORD_NUMBERS_PER_SEGMENT , 65280)
        ae(multistep_sort._TAPE_IEEE_NUMERIC_LENGTH , 14)


class MultistepSort___init__(unittest.TestCase):

    def test___init___01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"__init__\(\) takes from 2 to 6 positional arguments ",
                    "but 7 were given",
                )
            ),
            multistep_sort.MultistepSort,
            *(None, None, None, None, None, None),
        )

    def test___init___02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"__init__\(\) missing 1 required positional argument: ",
                    "'location'",
                )
            ),
            multistep_sort.MultistepSort,
        )

    def test___init___03(self):
        mss = multistep_sort.MultistepSort(
            "t", tapea="a", tapen="n", keep=True, chunk_size=30000
        )
        ae = self.assertEqual
        ae(mss.location, "t")
        ae(mss.tapea, os.path.join("t", "a"))
        ae(mss.tapen, os.path.join("t", "n"))
        ae(mss.keep, True)
        ae(mss.chunk_size, 30000)
        ae(mss.chunks, os.path.join("t", "chunks"))
        ae(getattr(mss, "_MultistepSort__record_number_count"), -1)
        ae(getattr(mss, "_MultistepSort__previous_record_number"), None)
        ae(getattr(mss, "_MultistepSort__fixed_value_length"), None)
        ae(
            set(mss.__dict__.keys()),
            {
                "location",
                "tapea",
                "tapen",
                "keep",
                "chunk_size",
                "chunks",
                "_MultistepSort__record_number_count",
                "_MultistepSort__previous_record_number",
                "_MultistepSort__fixed_value_length",
             }
        )

    def test___init___04(self):
        mss = multistep_sort.MultistepSort("t")
        ae = self.assertEqual
        ae(mss.location, "t")
        ae(mss.tapea, os.path.join("t", "tapea"))
        ae(mss.tapen, os.path.join("t", "tapen"))
        ae(mss.keep, False)
        ae(mss.chunk_size, 65280)
        ae(mss.chunks, os.path.join("t", "chunks"))
        ae(getattr(mss, "_MultistepSort__record_number_count"), -1)
        ae(getattr(mss, "_MultistepSort__previous_record_number"), None)
        ae(getattr(mss, "_MultistepSort__fixed_value_length"), None)
        ae(
            set(mss.__dict__.keys()),
            {
                "location",
                "tapea",
                "tapen",
                "keep",
                "chunk_size",
                "chunks",
                "_MultistepSort__record_number_count",
                "_MultistepSort__previous_record_number",
                "_MultistepSort__fixed_value_length",
             }
        )


class MultistepSort__is_chunk_full(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__is_chunk_full_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_is_chunk_full\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._is_chunk_full,
            *(None, None),
        )

    def test__is_chunk_full_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_is_chunk_full\(\) missing 1 required ",
                    "positional argument: 'record_number'",
                )
            ),
            self.mss._is_chunk_full,
        )

    def test__is_chunk_full_03(self):
        self.assertEqual(self.mss._is_chunk_full("anything"), False)

    def test__is_chunk_full_04(self):
        setattr(self.mss, "_MultistepSort__previous_record_number", "this")
        self.assertEqual(self.mss._is_chunk_full("this"), False)

    def test__is_chunk_full_05(self):
        setattr(
            self.mss,
            "_MultistepSort__record_number_count",
            self.mss.chunk_size-1,
        )
        setattr(self.mss, "_MultistepSort__previous_record_number", "this")
        self.assertEqual(self.mss._is_chunk_full("this"), False)

    def test__is_chunk_full_06(self):
        setattr(
            self.mss,
            "_MultistepSort__record_number_count",
            self.mss.chunk_size,
        )
        setattr(self.mss, "_MultistepSort__previous_record_number", "this")
        self.assertEqual(self.mss._is_chunk_full("next"), True)

    def test__is_chunk_full_07(self):
        setattr(
            self.mss,
            "_MultistepSort__record_number_count",
            self.mss.chunk_size-2,
        )
        setattr(self.mss, "_MultistepSort__previous_record_number", "this")
        self.assertEqual(self.mss._is_chunk_full("next"), False)

    def test__is_chunk_full_08(self):
        setattr(
            self.mss,
            "_MultistepSort__record_number_count",
            self.mss.chunk_size+1,
        )
        setattr(self.mss, "_MultistepSort__previous_record_number", "this")
        self.assertEqual(self.mss._is_chunk_full("next"), True)


class MultistepSort__tapea_crlf_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapea_crlf_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_crlf_sort_key\(\) takes 2 positional arguments ",
                    "but 3 were given",
                )
            ),
            self.mss._tapea_crlf_sort_key,
            *(None, None),
        )

    def test__tapea_crlf_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_crlf_sort_key\(\) missing 1 required positional ",
                    "argument: 'value'",
                )
            ),
            self.mss._tapea_crlf_sort_key,
        )

    def test__tapea_crlf_sort_key_03(self):
        self.assertEqual(self.mss._tapea_crlf_sort_key(b"abcdefg"), b"fg")


class MultistepSort__tapea_crlf_chunk_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapea_crlf_chunk_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_crlf_chunk_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tapea_crlf_chunk_sort_key,
            *(None, None),
        )

    def test__tapea_crlf_chunk_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_crlf_chunk_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tapea_crlf_chunk_sort_key,
        )

    def test__tapea_crlf_chunk_sort_key_03(self):
        self.assertEqual(
            self.mss._tapea_crlf_chunk_sort_key((b"abcdefg", None)), b"fg"
        )


class MultistepSort__tapen_crlf_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapen_crlf_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_crlf_sort_key\(\) takes 2 positional arguments ",
                    "but 3 were given",
                )
            ),
            self.mss._tapen_crlf_sort_key,
            *(None, None),
        )

    def test__tapen_crlf_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_crlf_sort_key\(\) missing 1 required positional ",
                    "argument: 'value'",
                )
            ),
            self.mss._tapen_crlf_sort_key,
        )

    def test__tapen_crlf_sort_key_03(self):
        self.assertEqual(
            self.mss._tapen_crlf_sort_key(b"abcdefg3"), (b"fg", 3.0)
        )


class MultistepSort__tapen_crlf_chunk_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapen_crlf_chunk_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_crlf_chunk_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tapen_crlf_chunk_sort_key,
            *(None, None),
        )

    def test__tapen_crlf_chunk_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_crlf_chunk_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tapen_crlf_chunk_sort_key,
        )

    def test__tapen_crlf_chunk_sort_key_03(self):
        self.assertEqual(
            self.mss._tapen_crlf_chunk_sort_key(
                (b"abcdefg3", None)), (b"fg", 3.0)
        )


class MultistepSort__tapea_length_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapea_length_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_length_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tapea_length_sort_key,
            *(None, None),
        )

    def test__tapea_length_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_length_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tapea_length_sort_key,
        )

    def test__tapea_length_sort_key_03(self):
        self.assertEqual(
            self.mss._tapea_length_sort_key(b"abcdefghi"), b"efhi"
        )


class MultistepSort__tapea_length_chunk_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapea_length_chunk_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_length_chunk_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tapea_length_chunk_sort_key,
            *(None, None),
        )

    def test__tapea_length_chunk_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapea_length_chunk_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tapea_length_chunk_sort_key,
        )

    def test__tapea_length_chunk_sort_key_03(self):
        self.assertEqual(
            self.mss._tapea_length_chunk_sort_key((b"abcdefghi", None)),
            b"efhi",
        )


class MultistepSort__tapen_length_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapen_length_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_length_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tapen_length_sort_key,
            *(None, None),
        )

    def test__tapen_length_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_length_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tapen_length_sort_key,
        )

    def test__tapen_length_sort_key_03(self):
        self.assertEqual(
            self.mss._tapen_length_sort_key(b"abcdefg3"), (b"ef", 3.0)
        )


class MultistepSort__tapen_length_chunk_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tapen_length_chunk_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_length_chunk_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tapen_length_chunk_sort_key,
            *(None, None),
        )

    def test__tapen_length_chunk_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tapen_length_chunk_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tapen_length_chunk_sort_key,
        )

    def test__tapen_length_chunk_sort_key_03(self):
        self.assertEqual(
            self.mss._tapen_length_chunk_sort_key((b"abcdefg3", None)),
            (b"ef", 3.0),
        )


class MultistepSort__tape_ieee_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tape_ieee_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tape_ieee_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tape_ieee_sort_key,
            *(None, None),
        )

    def test__tape_ieee_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tape_ieee_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tape_ieee_sort_key,
        )

    def test__tape_ieee_sort_key_03(self):
        self.assertEqual(
            self.mss._tape_ieee_sort_key(b"abcdef     ghi"),
            (b"ef", (5.837238083554008e+199,))
        )


class MultistepSort__tape_ieee_chunk_sort_key(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__tape_ieee_chunk_sort_key_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tape_ieee_chunk_sort_key\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._tape_ieee_chunk_sort_key,
            *(None, None),
        )

    def test__tape_ieee_chunk_sort_key_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_tape_ieee_chunk_sort_key\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._tape_ieee_chunk_sort_key,
        )

    def test__tape_ieee_chunk_sort_key_03(self):
        self.assertEqual(
            self.mss._tape_ieee_chunk_sort_key((b"abcdef     ghi", None)),
            (b"ef", (5.837238083554008e+199,))
        )


class MultistepSort__length_tapea_data_le_255(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__length_tapea_data_le_255_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_length_tapea_data_le_255\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._length_tapea_data_le_255,
            *(None, None),
        )

    def test__length_tapea_data_le_255_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_length_tapea_data_le_255\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._length_tapea_data_le_255,
        )

    def test__length_tapea_data_le_255_03(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthTapeA,
            r"Field value length > 255",
            self.mss._length_tapea_data_le_255,
            b"a" * 263,
        )

    def test__length_tapea_data_le_255_04(self):
        self.assertEqual(
            self.mss._length_tapea_data_le_255(b"a" * 262), None
        )


class MultistepSort__length_tapen_data_le_255(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__length_tapen_data_le_255_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_length_tapen_data_le_255\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._length_tapen_data_le_255,
            *(None, None),
        )

    def test__length_tapen_data_le_255_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_length_tapen_data_le_255\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._length_tapen_data_le_255,
        )

    def test__length_tapen_data_le_255_03(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthTapeN,
            r"Field value length > 255",
            self.mss._length_tapen_data_le_255,
            b"a" * 263,
        )

    def test__length_tapen_data_le_255_04(self):
        self.assertEqual(
            self.mss._length_tapen_data_le_255(b"a" * 262), None
        )


class MultistepSort__length_tapen_data_is_fixed(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")
        setattr(self.mss, "_MultistepSort__fixed_value_length", 13)

    def test__length_tapen_data_is_fixed_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_length_tapen_data_is_fixed\(\) takes 2 positional ",
                    "arguments but 3 were given",
                )
            ),
            self.mss._length_tapen_data_is_fixed,
            *(None, None),
        )

    def test__length_tapen_data_is_fixed_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_length_tapen_data_is_fixed\(\) missing 1 required ",
                    "positional argument: 'value'",
                )
            ),
            self.mss._length_tapen_data_is_fixed,
        )

    def test__length_tapen_data_is_fixed_03(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthAllowedFixedTapeN,
            r"Field value length ne 6",
            self.mss._length_tapen_data_is_fixed,
            b"a" * 12,
        )

    def test__length_tapen_data_is_fixed_04(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthAllowedFixedTapeN,
            r"Field value length ne 6",
            self.mss._length_tapen_data_is_fixed,
            b"a" * 14,
        )

    def test__length_tapen_data_is_fixed_05(self):
        self.assertEqual(
            self.mss._length_tapen_data_is_fixed(b"a" * 13), None
        )


class MultistepSort__write_chunk_crlf_delimited_01(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__write_chunk_crlf_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_write_chunk_crlf_delimited\(\) takes from 4 to 5 positional ",
                    "arguments but 6 were given",
                )
            ),
            self.mss._write_chunk_crlf_delimited,
            *(None, None, None, None, None),
        )

    def test__write_chunk_crlf_delimited_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_write_chunk_crlf_delimited\(\) missing 3 required positional ",
                    "arguments: 'records', 'sort_key', and 'chunk_name'",
                )
            ),
            self.mss._write_chunk_crlf_delimited,
        )

    def test__write_chunk_crlf_delimited_03(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_write_chunk_crlf_delimited\(\) got an unexpected keyword ",
                    "argument 'extra'",
                )
            ),
            self.mss._write_chunk_crlf_delimited,
            *(None, None, None),
            **dict(last_chunk=True, extra=False),
        )


class SortFiles(unittest.TestCase):

    def setUp(self):

        # Most dpt-dbms runs have to be done on MS Windows but unittests
        # for multistep_sort, on it's own, can be run on BSD etc.
        self.testroot = os.path.join(
            os.environ.get("TEMP") or os.environ.get("HOME"),
            "multistep_sort",
        )

        os.mkdir(self.testroot)
        self.mss = multistep_sort.MultistepSort(
            self.testroot, chunk_size=3
        )

    def tearDown(self):
        if os.path.exists(self.testroot):
            shutil.rmtree(self.testroot)


class MultistepSort__write_chunk_crlf_delimited_02(SortFiles):

    def test__write_chunk_crlf_delimited_04(self):
        ae = self.assertEqual
        records = []
        ae(
            self.mss._write_chunk_crlf_delimited(records, str, self.mss.tapea),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"\r\n")
        self.assertEqual(records, [])

    def test__write_chunk_crlf_delimited_05(self):
        ae = self.assertEqual
        records = [b"aa"]
        self.assertEqual(
            self.mss._write_chunk_crlf_delimited(records, str, self.mss.tapea),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"aa\r\n")
        self.assertEqual(records, [])

    def test__write_chunk_crlf_delimited_06(self):
        ae = self.assertEqual
        records = [b"bbbbbbbbbbb", b"aaaaaaaaaaa"]
        ae(
            self.mss._write_chunk_crlf_delimited(
                records, self.mss._tapea_crlf_sort_key, self.mss.tapea
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"aaaaaaaaaaa\r\nbbbbbbbbbbb\r\n")
        ae(records, [])

    def test__write_chunk_crlf_delimited_07(self):
        ae = self.assertEqual
        records = []
        ae(
            self.mss._write_chunk_crlf_delimited(
                records, str, self.mss.tapea, last_chunk=True
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"")
        self.assertEqual(records, [])

    def test__write_chunk_crlf_delimited_08(self):
        ae = self.assertEqual
        records = [b"aa"]
        self.assertEqual(
            self.mss._write_chunk_crlf_delimited(
                records, str, self.mss.tapea, last_chunk=True
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"aa")
        self.assertEqual(records, [])

    def test__write_chunk_crlf_delimited_09(self):
        ae = self.assertEqual
        records = [b"bbbbbbbbbbb", b"aaaaaaaaaaa"]
        ae(
            self.mss._write_chunk_crlf_delimited(
                records,
                self.mss._tapea_crlf_sort_key,
                self.mss.tapea,
                last_chunk=True,
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"aaaaaaaaaaa\r\nbbbbbbbbbbb")
        ae(records, [])


class MultistepSort__write_chunk_length_delimited_01(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__write_chunk_length_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_write_chunk_length_delimited\(\) takes 4 positional ",
                    "arguments but 5 were given",
                )
            ),
            self.mss._write_chunk_length_delimited,
            *(None, None, None, None),
        )

    def test__write_chunk_length_delimited_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_write_chunk_length_delimited\(\) missing 3 required ",
                    "positional arguments: 'records', 'sort_key', and ",
                    "'chunk_name'",
                )
            ),
            self.mss._write_chunk_length_delimited,
        )


class MultistepSort__write_chunk_length_delimited_02(SortFiles):

    def test__write_chunk_length_delimited_04(self):
        ae = self.assertEqual
        records = []
        ae(
            self.mss._write_chunk_length_delimited(
                records, str, self.mss.tapea
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"")
        ae(records, [])

    def test__write_chunk_length_delimited_05(self):
        ae = self.assertEqual
        records = [b"aa"]
        ae(
            self.mss._write_chunk_length_delimited(
                records, str, self.mss.tapea
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"aa")
        ae(records, [])

    def test__write_chunk_length_delimited_06(self):
        ae = self.assertEqual
        records = [b"bbbbbb\x04bbbb", b"aaaaaa\x04aaaa"]
        ae(
            self.mss._write_chunk_length_delimited(
                records, self.mss._tapea_crlf_sort_key, self.mss.tapea
            ),
            self.mss.tapea,
        )
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"aaaaaa\x04aaaabbbbbb\x04bbbb")
        ae(records, [])


class MultistepSort__create_chunks_directory(SortFiles):

    def test__create_chunks_directory_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_create_chunks_directory\(\) takes 1 positional ",
                    "argument but 2 were given",
                )
            ),
            self.mss._create_chunks_directory,
            *(None,),
        )

    def test__create_chunks_directory_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_create_chunks_directory\(\) got an unexpected ",
                    "keyword argument 'extra'",
                )
            ),
            self.mss._create_chunks_directory,
            **dict(extra=None),
        )

    def test__create_chunks_directory_03(self):
        ae = self.assertEqual
        ae(os.path.exists(self.mss.chunks), False)
        ae(self.mss._create_chunks_directory(), None)
        ae(os.path.exists(self.mss.chunks), True)
        ae(self.mss._create_chunks_directory(), None)
        ae(os.path.exists(self.mss.chunks), True)


class MultistepSort__sort_tape_crlf_delimited_01(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__sort_tape_crlf_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_sort_tape_crlf_delimited\(\) takes 5 positional ",
                    "arguments but 6 were given",
                )
            ),
            self.mss._sort_tape_crlf_delimited,
            *(None, None, None, None, None),
        )

    def test__sort_tape_crlf_delimited_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_sort_tape_crlf_delimited\(\) missing 4 required ",
                    "positional arguments: 'tape', 'sort_key', ",
                    "'chunk_sort_key', and 'length_data'",
                )
            ),
            self.mss._sort_tape_crlf_delimited,
        )


class MultistepSort__sort_tape_crlf_delimited_02(SortFiles):

    def test__sort_tape_crlf_delimited_03(self):
        ae = self.assertEqual
        records = [
            b"ggggggg",
            b"hhhhhhh",
        ]
        with open(self.mss.tapea, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.mss._sort_tape_crlf_delimited(
            self.mss.tapea,
            self.mss._tapea_crlf_sort_key,
            self.mss._tapea_crlf_chunk_sort_key,
            self.mss._length_tapea_data_le_255
        )
        records.sort()
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"\r\n".join(records))

    def test__sort_tape_crlf_delimited_04(self):
        ae = self.assertEqual
        records = [
            b"aaaaaaaa",
            b"bbbbbbbb",
            b"cccccccc",
            b"ffffffff",
            b"dddddddd",
            b"eeeeeeee",
            b"iiiiiiii",
            b"hhhhhhhh",
            b"gggggggg",
            b"llllllll",
            b"mmmmmmmm",
            b"kkkkkkkk",
            b"pppppppp",
            b"nnnnnnnn",
            b"oooooooo",
        ]
        with open(self.mss.tapea, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.mss._sort_tape_crlf_delimited(
            self.mss.tapea,
            self.mss._tapea_crlf_sort_key,
            self.mss._tapea_crlf_chunk_sort_key,
            self.mss._length_tapea_data_le_255
        )
        records.sort()
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"\r\n".join(records))


class MultistepSort__sort_tape_length_delimited_01(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test__sort_tape_length_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_sort_tape_length_delimited\(\) takes 5 positional ",
                    "arguments but 6 were given",
                )
            ),
            self.mss._sort_tape_length_delimited,
            *(None, None, None, None, None),
        )

    def test__sort_tape_length_delimited_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"_sort_tape_length_delimited\(\) missing 4 required ",
                    "positional arguments: 'tape', 'sort_key', ",
                    "'chunk_sort_key', and 'length_error'",
                )
            ),
            self.mss._sort_tape_length_delimited,
        )


class MultistepSort__sort_tape_length_delimited_02(SortFiles):

    def test__sort_tape_length_delimited_03(self):
        ae = self.assertEqual
        records = [
            b"gggggg\x00",
            b"hhhhhh\x00",
        ]
        with open(self.mss.tapea, mode="wb") as file:
            file.write(b"".join(records))
        self.mss._sort_tape_length_delimited(
            self.mss.tapea,
            self.mss._tapea_length_sort_key,
            self.mss._tapea_length_chunk_sort_key,
            multistep_sort.ValueLengthTapeA
        )
        records.sort()
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"".join(records))

    def test__sort_tape_length_delimited_04(self):
        ae = self.assertEqual
        records = [
            b"aaaaaa\x01a",
            b"bbbbbb\x01b",
            b"cccccc\x01c",
            b"ffffff\x01f",
            b"dddddd\x01d",
            b"eeeeee\x01e",
            b"iiiiii\x01i",
            b"hhhhhh\x01h",
            b"gggggg\x01g",
            b"llllll\x01l",
            b"mmmmmm\x01m",
            b"kkkkkk\x01k",
            b"pppppp\x01p",
            b"nnnnnn\x01n",
            b"oooooo\x01o",
        ]
        with open(self.mss.tapea, mode="wb") as file:
            file.write(b"".join(records))
        self.mss._sort_tape_length_delimited(
            self.mss.tapea,
            self.mss._tapea_length_sort_key,
            self.mss._tapea_length_chunk_sort_key,
            multistep_sort.ValueLengthTapeA
        )
        records.sort()
        with open(self.mss.tapea, mode="rb") as file:
            ae(file.read(), b"".join(records))


class MultistepSort_sort_tape_ieee_numeric_01(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test_sort_tape_ieee_numeric_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"sort_tape_ieee_numeric\(\) takes 1 positional ",
                    "argument but 2 were given",
                )
            ),
            self.mss.sort_tape_ieee_numeric,
            *(None,),
        )


class MultistepSort_sort_tape_ieee_numeric_02(SortFiles):

    def test__sort_tape_length_delimited_03(self):
        ae = self.assertEqual
        records = [
            b"gggggg0a0a0a0a",
            b"hhhhhh0b0b0b0b",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"".join(records))
        self.mss.sort_tape_ieee_numeric()
        records.sort()
        with open(self.mss.tapen, mode="rb") as file:
            ae(file.read(), b"".join(records))

    def test__sort_tape_length_delimited_04(self):
        ae = self.assertEqual
        records = [
            b"aaaaaa0a0a0a0a",
            b"bbbbbb0b0b0b0b",
            b"cccccc0c0c0c0c",
            b"ffffff0f0f0f0f",
            b"dddddd0d0d0d0d",
            b"eeeeee0e0e0e0e",
            b"iiiiii0i0i0i0i",
            b"hhhhhh0h0h0h0h",
            b"gggggg0g0g0g0g",
            b"llllll0b0a0a0b",  # b"0l0l0l0l" does not convert to float.
            b"mmmmmm0b0c0c0b",
            b"kkkkkk0b0d0d0b",
            b"pppppp0b0f0e0b",
            b"nnnnnn0b0e0f0b",
            b"oooooo0b0g0h0b",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"".join(records))
        self.mss.sort_tape_ieee_numeric()
        records.sort()
        with open(self.mss.tapen, mode="rb") as file:
            ae(file.read(), b"".join(records))


class MultistepSort_sort_tapea_crlf_delimited(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test_sort_tapea_crlf_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"sort_tapea_crlf_delimited\(\) takes 1 positional ",
                    "argument but 2 were given",
                )
            ),
            self.mss.sort_tapea_crlf_delimited,
            *(None,),
        )


class MultistepSort_sort_tapen_crlf_delimited_01(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test_sort_tapen_crlf_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"sort_tapen_crlf_delimited\(\) takes from 1 to 2 ",
                    "positional arguments but 3 were given",
                )
            ),
            self.mss.sort_tapen_crlf_delimited,
            *(None, None),
        )

    def test_sort_tapen_crlf_delimited_02(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"sort_tapen_crlf_delimited\(\) got an unexpected ",
                    "keyword argument 'extra'",
                )
            ),
            self.mss.sort_tapen_crlf_delimited,
            **dict(value_length=5, extra=None),
        )

    def test_sort_tapen_crlf_delimited_03(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthAllowedTapeN,
            "value_length must be integer or None",
            self.mss.sort_tapen_crlf_delimited,
            **dict(value_length=5.0),
        )

    def test_sort_tapen_crlf_delimited_04(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthAllowedTapeN,
            "value_length must be in range 0 to 255",
            self.mss.sort_tapen_crlf_delimited,
            **dict(value_length=256),
        )

    def test_sort_tapen_crlf_delimited_05(self):
        self.assertRaisesRegex(
            multistep_sort.ValueLengthAllowedTapeN,
            "value_length must be in range 0 to 255",
            self.mss.sort_tapen_crlf_delimited,
            **dict(value_length=-1),
        )


class MultistepSort_sort_tapen_crlf_delimited_02(SortFiles):

    def test_sort_tapen_crlf_delimited_06(self):
        ae = self.assertEqual
        records = [
            b"ggggggg2",
            b"hhhhhhh3",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.mss.sort_tapen_crlf_delimited()
        records.sort()
        with open(self.mss.tapen, mode="rb") as file:
            ae(file.read(), b"\r\n".join(records))

    def test_sort_tapen_crlf_delimited_07(self):
        ae = self.assertEqual
        records = [
            b"aaaaaaa1",
            b"bbbbbbb3",
            b"ccccccc2",
            b"fffffff4",
            b"ddddddd5",
            b"eeeeeee4.5",
            b"iiiiiii6",
            b"hhhhhhh9",
            b"ggggggg8",
            b"lllllll7",
            b"mmmmmmm6.7",
            b"kkkkkkk6.3",
            b"ppppppp10",
            b"nnnnnnn12",
            b"ooooooo11",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.mss.sort_tapen_crlf_delimited()
        records.sort()
        with open(self.mss.tapen, mode="rb") as file:
            ae(file.read(), b"\r\n".join(records))


class MultistepSort_sort_tapen_crlf_delimited_03(SortFiles):

    def test_sort_tapen_crlf_delimited_08(self):
        ae = self.assertEqual
        records = [
            b"ggggggg",
            b"hhhhhhh",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.assertRaisesRegex(
            ValueError,
            "could not convert string to float: b''",
            self.mss.sort_tapen_crlf_delimited,
            **dict(value_length=0),
        )

    def test_sort_tapen_crlf_delimited_09(self):
        ae = self.assertEqual
        records = [
            b"ggggggg2",
            b"hhhhhhh",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.assertRaisesRegex(
            multistep_sort.ValueLengthAllowedFixedTapeN,
            "Field value length ne 1",
            self.mss.sort_tapen_crlf_delimited,
            **dict(value_length=1),
        )

    def test_sort_tapen_crlf_delimited_10(self):
        ae = self.assertEqual
        records = [
            b"ggggggg2",
            b"hhhhhhh3",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.mss.sort_tapen_crlf_delimited(value_length=1)
        records.sort()
        with open(self.mss.tapen, mode="rb") as file:
            ae(file.read(), b"\r\n".join(records))

    def test_sort_tapen_crlf_delimited_11(self):
        ae = self.assertEqual
        records = [
            b"aaaaaaa  1",
            b"bbbbbbb3  ",
            b"ccccccc 2 ",
            b"fffffff  4",
            b"ddddddd  5",
            b"eeeeeee4.5",
            b"iiiiiii  6",
            b"hhhhhhh9  ",
            b"ggggggg  8",
            b"lllllll  7",
            b"mmmmmmm6.7",
            b"kkkkkkk6.3",
            b"ppppppp 10",
            b"nnnnnnn12 ",
            b"ooooooo 11",
        ]
        with open(self.mss.tapen, mode="wb") as file:
            file.write(b"\r\n".join(records))
        self.mss.sort_tapen_crlf_delimited(value_length=3)
        records.sort()
        with open(self.mss.tapen, mode="rb") as file:
            ae(file.read(), b"\r\n".join(records))


class MultistepSort_sort_tapea_length_delimited(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test_sort_tapea_length_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"sort_tapea_length_delimited\(\) takes 1 positional ",
                    "argument but 2 were given",
                )
            ),
            self.mss.sort_tapea_length_delimited,
            *(None,),
        )


class MultistepSort_sort_tapen_length_delimited(unittest.TestCase):

    def setUp(self):
        self.mss = multistep_sort.MultistepSort("t")

    def test_sort_tapen_length_delimited_01(self):
        self.assertRaisesRegex(
            TypeError,
            "".join(
                (
                    r"sort_tapen_length_delimited\(\) takes 1 positional ",
                    "argument but 2 were given",
                )
            ),
            self.mss.sort_tapen_length_delimited,
            *(None,),
        )


if __name__ == "__main__":
    runner = unittest.TextTestRunner
    loader = unittest.defaultTestLoader.loadTestsFromTestCase
    runner().run(loader(ModuleContents))
    runner().run(loader(MultistepSort___init__))
    runner().run(loader(MultistepSort__is_chunk_full))
    runner().run(loader(MultistepSort__tapea_crlf_sort_key))
    runner().run(loader(MultistepSort__tapea_crlf_chunk_sort_key))
    runner().run(loader(MultistepSort__tapen_crlf_sort_key))
    runner().run(loader(MultistepSort__tapen_crlf_chunk_sort_key))
    runner().run(loader(MultistepSort__tapea_length_sort_key))
    runner().run(loader(MultistepSort__tapea_length_chunk_sort_key))
    runner().run(loader(MultistepSort__tapen_length_sort_key))
    runner().run(loader(MultistepSort__tapen_length_chunk_sort_key))
    runner().run(loader(MultistepSort__tape_ieee_sort_key))
    runner().run(loader(MultistepSort__tape_ieee_chunk_sort_key))
    runner().run(loader(MultistepSort__length_tapea_data_le_255))
    runner().run(loader(MultistepSort__length_tapen_data_le_255))
    runner().run(loader(MultistepSort__length_tapen_data_is_fixed))
    runner().run(loader(MultistepSort__write_chunk_crlf_delimited_01))
    runner().run(loader(MultistepSort__write_chunk_crlf_delimited_02))
    runner().run(loader(MultistepSort__write_chunk_length_delimited_01))
    runner().run(loader(MultistepSort__write_chunk_length_delimited_02))
    runner().run(loader(MultistepSort__create_chunks_directory))
    runner().run(loader(MultistepSort__sort_tape_crlf_delimited_01))
    runner().run(loader(MultistepSort__sort_tape_crlf_delimited_02))
    runner().run(loader(MultistepSort__sort_tape_length_delimited_01))
    runner().run(loader(MultistepSort__sort_tape_length_delimited_02))
    runner().run(loader(MultistepSort_sort_tape_ieee_numeric_01))
    runner().run(loader(MultistepSort_sort_tape_ieee_numeric_02))
    runner().run(loader(MultistepSort_sort_tapea_crlf_delimited))
    runner().run(loader(MultistepSort_sort_tapen_crlf_delimited_01))
    runner().run(loader(MultistepSort_sort_tapen_crlf_delimited_02))
    runner().run(loader(MultistepSort_sort_tapen_crlf_delimited_03))
    runner().run(loader(MultistepSort_sort_tapea_length_delimited))
    runner().run(loader(MultistepSort_sort_tapen_length_delimited))
