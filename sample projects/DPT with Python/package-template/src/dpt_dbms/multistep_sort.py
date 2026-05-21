# multistep_sort.py
# Copyright 2026 Roger Marsh
# License: LGPL-2.1

"""Python implementation of sorts for DPT multi-step deferred update.

The sample multi-step job in the DPT documentation; 'Sizing, loading and
reorging in chapter 10 DBA funcions of DBAPI.html'; has 'invoke external
sort somehow, and wait for completion' as one of the steps.

Details of this process, including file formats, are given in the Deferred
Index Updates section of Data Loading, and Appendix 1, of dbaguide.html.

Since version 2 release 14 of DPT single step deferred updates has been
available and is the recommended way to do deferred updates.

Attempts to do single step deferred update with the DPT DBMS version 3
release 0 built on 64-bit toolchains fail when deferred updates of a
database extend over more than one segment.  Thus the Python interface
to DPT is fine on 32-bit Python but flawed on 64-bit Python.

This module provides the sorts needed to support multi-step deferred
update on 64-bit Python.  These can be used on 32-bit Python too but the
single-step process is available and should be preferred.

"""
import os
import struct

_CHUNKS_DIRECTORY = "chunks"
_MAX_VALUE_LENGTH = 255
_HEADER_LENGTH = 7  # For the formats which allow variable length values.
_MAX_TAPE_CRLF_LINE_LENGTH = (
    _MAX_VALUE_LENGTH + _HEADER_LENGTH  # For CRLF delimited formats.
)
_RECORD_NUMBERS_PER_SEGMENT = 65280  # Could import value from dptapi?
_ORIGINAL = ".original"
_TAPE_IEEE_NUMERIC_LENGTH = 14  # Fixed length values in IEEE float format.


class ValueLengthTapeA(Exception):
    """Field value more than 255 bytes."""


class ValueLengthTapeN(Exception):
    """Field value more than 255 bytes."""


class ValueLengthAllowedTapeN(Exception):
    """Field value more than 255 bytes."""


class ValueLengthAllowedFixedTapeN(Exception):
    """Field value more than 255 bytes."""


class ValueLengthIEEETapeN(Exception):
    """Field value is not 14 bytes."""


class MultistepSort:
    """Describe database and sequential files containing updates.

    location is the directory containing the sequential files generated
    by a database update in multi-step deferred update mode.

    The two sequential files are usually called 'tapea' and 'tapen' and
    the tapea and tapen arguments allow for different names.

    keep is flag saying whether to keep or discard original sequential
    files and chunks work area on completion of sort.  If keep is False
    the original sequential files are deleted when all data has been
    put in sorted chunks; and the chunks are deleted when all data has
    been merged into new sequential files with the tapea and tapen names.
    if keep is True the original sequential files are renamed with the
    extension '.original' before merging chunks into new tapea and tapen
    files.  Default is False.

    chunk_size is number of record numbers which can be put in a chunk.  A
    new chunk is started when the 'chunk_size + 1'th record number would
    otherwise be added to a chunk.  Default is None: meaning use number of
    record numbers in a segment.
    
    """
    
    def __init__(
        self, location, tapea=None, tapen=None, keep=False, chunk_size=None
    ):
        """Initialise data structures."""
        if tapea == None:
            tapea = "tapea"
        if tapen == None:
            tapen = "tapen"
        self.location = location
        self.tapea = os.path.join(location, tapea)
        self.tapen = os.path.join(location, tapen)
        self.keep = keep
        if chunk_size is None:
            chunk_size = _RECORD_NUMBERS_PER_SEGMENT
        self.chunk_size = chunk_size
        self.chunks = os.path.join(location, _CHUNKS_DIRECTORY)
        self.__record_number_count = -1 # For first chunk only.
        self.__previous_record_number = None  # For first chunk only.
        self.__fixed_value_length = None  # Used in tapen processing.
    
    def _is_chunk_full(self, record_number):
        """Return True if self.chunk_size record numbers are in a chunk.

        Consecutive entries in a chunk have the same record number or a
        higher record number in the later entry.  The two record numbers
        usually differ by 1.

        """
        if record_number != self.__previous_record_number:
            self.__record_number_count += 1
            self.__previous_record_number = record_number
        if self.__record_number_count < self.chunk_size:
            return False
        self.__record_number_count = 0  # Not -1 on reset for next chunk.
        return True
    
    def _tapea_crlf_sort_key(self, value):
        """Return key for sorting purposes."""
        return value[5:]  # Field id and value.
    
    def _tapea_crlf_chunk_sort_key(self, value):
        """Return key for sorting purposes."""
        return self._tapea_crlf_sort_key(value[0])  # Or just value[0][4:]
    
    def _tapen_crlf_sort_key(self, value):
        """Return key for sorting purposes."""
        return (value[5:7], float(value[7:]))  # (Field id, value).
    
    def _tapen_crlf_chunk_sort_key(self, value):
        """Return key for sorting purposes."""
        return self._tapen_crlf_sort_key(value[0])
    
    def _tapea_length_sort_key(self, value):
        """Return key for sorting purposes."""
        return value[4:6] + value[7:]  # Field id and value without length.
    
    def _tapea_length_chunk_sort_key(self, value):
        """Return key for sorting purposes."""
        return self._tapea_length_sort_key(value[0])
    
    def _tapen_length_sort_key(self, value):
        """Return key for sorting purposes."""
        return (value[4:6], float(value[7:]))  # (Field id, value).
    
    def _tapen_length_chunk_sort_key(self, value):
        """Return key for sorting purposes."""
        return self._tapen_length_sort_key(value[0])
    
    def _tape_ieee_sort_key(self, value):
        """Return key for sorting purposes."""
        return (
            value[4:6], struct.unpack("=d", value[6:])  # (Field id, value).
        )
    
    def _tape_ieee_chunk_sort_key(self, value):
        """Return key for sorting purposes."""
        return self._tape_ieee_sort_key(value[0])
    
    def _length_tapea_data_le_255(self, value):
        """Raise ValueLengthTape exception if value length gt 255."""
        if len(value) > _MAX_TAPE_CRLF_LINE_LENGTH:
            raise ValueLengthTapeA(
                "Field value length > " + str(_MAX_VALUE_LENGTH)
            )
        if len(value) < _HEADER_LENGTH:
            raise ValueLengthTapeA("Record less than 7 bytes in tape")
    
    def _length_tapen_data_le_255(self, value):
        """Raise ValueLengthTape exception if value length gt 255."""
        if len(value) > _MAX_TAPE_CRLF_LINE_LENGTH:
            raise ValueLengthTapeN(
                "Field value length > " + str(_MAX_VALUE_LENGTH)
            )
        if len(value) < _HEADER_LENGTH:
            raise ValueLengthTapeN("Record less than 7 bytes in tape")
    
    def _length_tapen_data_is_fixed(self, value):
        """Raise ValueLengthTape exception if value length gt 255."""
        if len(value) != self.__fixed_value_length:
            raise ValueLengthAllowedFixedTapeN(
                (
                    "Field value length ne " +
                    str(self.__fixed_value_length - _HEADER_LENGTH)
                )
            )
    
    def _write_chunk_crlf_delimited(
        self, records, sort_key, chunk_name, last_chunk=False
    ):
        """Create chunk_name in self.chunks and return full chunk name.

        Clear records after creating chunk.

        """
        records.sort(key=sort_key)
        chunk_name = os.path.join(self.chunks, chunk_name)
        with open(chunk_name, mode="wb") as file:
            file.write(b"\r\n".join(records))
            if not last_chunk:
                file.write(b"\r\n")
        records.clear()
        return chunk_name
    
    def _write_chunk_length_delimited(self, records, sort_key, chunk_name):
        """Create chunk_name in self.chunks and return full chunk name.

        Clear records after creating chunk.

        """
        records.sort(key=sort_key)
        chunk_name = os.path.join(self.chunks, chunk_name)
        with open(chunk_name, mode="wb") as file:
            file.write(b"".join(records))
        records.clear()
        return chunk_name
    
    def _create_chunks_directory(self):
        """Create chunks directory if it does not exist."""
        try:
            os.mkdir(self.chunks)
        except FileExistsError:
            pass
    
    def _sort_tape_crlf_delimited(
        self, tape, sort_key, chunk_sort_key, length_data
    ):
        """Sort data, validated by length_data, in tape file into chunks.

        tape will be self.tapea or self.tapen for formats A1 or N1 as
        defined in dbaguide.html.

        length_data will allow data up to 255 bytes for either format A1,
        or N1, but may allow just a particular size for format N1.

        """
        self._create_chunks_directory()
        records = []
        chunks = []
        with open(tape, mode="rb") as file:
            while True:
                data = file.readline().rstrip(b"\r\n")
                if not data:
                    break
                length_data(data)
                if self._is_chunk_full(data[1:5]):
                    chunks.append(
                        self._write_chunk_crlf_delimited(
                            records,
                            sort_key,
                            os.path.basename(tape) + str(len(chunks)),
                        )
                    )
                records.append(data)
        if records:
            chunks.append(
                self._write_chunk_crlf_delimited(
                    records,
                    sort_key,
                    os.path.basename(tape) + str(len(chunks)),
                    last_chunk=True,
                )
            )
        if not chunks:
            return
        if self.keep:
            os.rename(tape, tape + _ORIGINAL)
        else:
            os.remove(tape)
        with open(tape, mode="wb") as file:

            # enumerate so stream object identity does not affect sort.
            open_chunks = [
                [None, e, open(f, mode="rb")] for e, f in enumerate(chunks)
            ]

            ocl = len(open_chunks) - 1
            for oci in range(ocl + 1):
                oc = open_chunks[ocl - oci]
                oc[0] = oc[-1].readline().rstrip(b"\r\n")
                if not oc[0]:
                    oc[-1].close()
                    del open_chunks[ocl - oci]
                    del oc
                    continue
                length_data(oc[0])
            while len(open_chunks):
                open_chunks.sort(key=chunk_sort_key)
                oc = open_chunks[0]
                file.write(oc[0])
                oc[0] = oc[-1].readline().rstrip(b"\r\n")
                if not oc[0]:
                    oc[-1].close()
                    del open_chunks[0]
                    del oc
                    if len(open_chunks):
                        file.write(b"\r\n")
                    continue
                length_data(oc[0])
                file.write(b"\r\n")
        if not self.keep:
            for chunk in chunks:
                os.remove(chunk)
    
    def _sort_tape_length_delimited(
        self, tape, sort_key, chunk_sort_key, length_error
    ):
        """Sort data, validated by length_data, in tape file into chunks.

        tape will be self.tapea or self.tapen for formats A1 or N1 as
        defined in dbaguide.html.

        length_data will allow data up to 255 bytes for either format A1,
        or N1, but may allow just a particular size for format N1.

        """
        self._create_chunks_directory()
        records = []
        chunks = []
        with open(tape, mode="rb") as file:
            while True:
                head = file.read(_HEADER_LENGTH)
                if not head:
                    break
                if len(head) != _HEADER_LENGTH:
                    raise length_error("Record less than 7 bytes in tape")
                data_length = int.from_bytes(head[-1:], byteorder="little")
                data = file.read(data_length)
                if len(data) != data_length:
                    raise length_error(
                        (
                            "Data length " +
                            str(len(data)) +
                            " is not the expected " +
                            str(data_length) +
                            " during tape read"
                        )
                    )
                if self._is_chunk_full(data[0:4]):
                    chunks.append(
                        self._write_chunk_length_delimited(
                            records,
                            sort_key,
                            os.path.basename(tape) + str(len(chunks)),
                        )
                    )
                records.append(head+data)
        if records:
            chunks.append(
                self._write_chunk_length_delimited(
                    records,
                    sort_key,
                    os.path.basename(tape) + str(len(chunks)),
                )
            )
        if not chunks:
            return
        if self.keep:
            os.rename(tape, tape + _ORIGINAL)
        else:
            os.remove(tape)
        with open(tape, mode="wb") as file:

            # enumerate so stream object identity does not affect sort.
            open_chunks = [
                [None, e, open(f, mode="rb")] for e, f in enumerate(chunks)
            ]

            ocl = len(open_chunks) - 1
            for oci in range(ocl + 1):
                oc = open_chunks[ocl - oci]
                head = oc[-1].read(_HEADER_LENGTH)
                if len(head) == 0:
                    oc[-1].close()
                    del open_chunks[ocl - oci]
                    del oc
                    continue
                if len(head) != _HEADER_LENGTH:
                    raise length_error("Header less than 7 bytes start merge")
                data_length = int.from_bytes(head[-1:], byteorder="little")
                data = oc[-1].read(data_length)
                if len(data) != data_length:
                    raise length_error(
                        (
                            "Data length " +
                            str(len(data)) +
                            " is not the expected " +
                            str(data_length) +
                            " start merge"
                        )
                    )
                oc[0] = head + data
            while len(open_chunks):
                open_chunks.sort(key=chunk_sort_key)
                oc = open_chunks[0]
                file.write(oc[0])
                head = oc[-1].read(_HEADER_LENGTH)
                if len(head) == 0:
                    oc[-1].close()
                    del open_chunks[0]
                    del oc
                    continue
                if len(head) != _HEADER_LENGTH:
                    raise length_error("Header less than 7 bytes during merge")
                data_length = int.from_bytes(head[-1:], byteorder="little")
                data = oc[-1].read(data_length)
                if len(data) != data_length:
                    raise length_error(
                        (
                            "Data length " +
                            str(len(data)) +
                            " is not the expected " +
                            str(data_length) +
                            " during merge"
                        )
                    )
                oc[0] = head + data
        if not self.keep:
            for chunk in chunks:
                os.remove(chunk)
    
    def sort_tape_ieee_numeric(self, **kwargs):
        """Sort data in self.tapen file into chunks.

        kwargs is for compatibility with sort_tapen_crlf_delimited but is
        ignored.

        """
        del kwargs
        self._create_chunks_directory()
        records = []
        chunks = []
        tape = self.tapen
        with open(tape, mode="rb") as file:
            while True:
                data = file.read(_TAPE_IEEE_NUMERIC_LENGTH)
                if not data:
                    break
                if len(data) != _TAPE_IEEE_NUMERIC_LENGTH:
                    raise ValueLengthIEEETapeN(
                        (
                            "Record less than " +
                            str(_TAPE_IEEE_NUMERIC_LENGTH) +
                            " bytes in tape"
                        )
                    )
                if self._is_chunk_full(data[0:4]):
                    chunks.append(
                        self._write_chunk_length_delimited(
                            records,
                            self._tape_ieee_sort_key,
                            os.path.basename(tape) + str(len(chunks)),
                        )
                    )
                records.append(data)
        if records:
            chunks.append(
                self._write_chunk_length_delimited(
                    records,
                    self._tape_ieee_sort_key,
                    os.path.basename(tape) + str(len(chunks)),
                )
            )
        if not chunks:
            return
        if self.keep:
            os.rename(tape, tape + _ORIGINAL)
        else:
            os.remove(tape)
        with open(tape, mode="wb") as file:

            # enumerate so stream object identity does not affect sort.
            open_chunks = [
                [None, e, open(f, mode="rb")] for e, f in enumerate(chunks)
            ]

            ocl = len(open_chunks) - 1
            for oci in range(ocl + 1):
                oc = open_chunks[ocl - oci]
                oc[0] = oc[-1].read(_TAPE_IEEE_NUMERIC_LENGTH)
                if len(oc[0]) == 0:
                    oc[-1].close()
                    del open_chunks[ocl - oci]
                    del oc
                    continue
                if len(oc[0]) != _TAPE_IEEE_NUMERIC_LENGTH:
                    raise ValueLengthIEEETapeN(
                        (
                            "Record less than " +
                            str(_TAPE_IEEE_NUMERIC_LENGTH) +
                            " bytes in chunk"
                        )
                    )
            while len(open_chunks):
                open_chunks.sort(key=self._tape_ieee_chunk_sort_key)
                oc = open_chunks[0]
                file.write(oc[0])
                oc[0] = oc[-1].read(_TAPE_IEEE_NUMERIC_LENGTH)
                if len(oc[0]) == 0:
                    oc[-1].close()
                    del open_chunks[ocl - oci]
                    del oc
                    continue
                if len(oc[0]) != _TAPE_IEEE_NUMERIC_LENGTH:
                    raise ValueLengthIEEETapeN(
                        (
                            "Record less than " +
                            str(_TAPE_IEEE_NUMERIC_LENGTH) +
                            " bytes in chunk"
                        )
                    )
        if not self.keep:
            for chunk in chunks:
                os.remove(chunk)
    
    def sort_tapea_crlf_delimited(self, **kwargs):
        """Sort data, validated by length_data, in tape file into chunks.

        kwargs is for compatibility with sort_tapen_crlf_delimited but is
        ignored.

        """
        del kwargs
        self._sort_tape_crlf_delimited(
            self.tapea,
            self._tapea_crlf_sort_key,
            self._tapea_crlf_chunk_sort_key,
            self._length_tapea_data_le_255,
        )
    
    def sort_tapen_crlf_delimited(self, value_length=None):
        """Sort data, validated by length_data, in tape file into chunks."""
        if value_length is not None:
            if not isinstance(value_length, int):
                raise ValueLengthAllowedTapeN(
                    "value_length must be integer or None"
                )
            if value_length < 0 or value_length > _MAX_VALUE_LENGTH:
                raise ValueLengthAllowedTapeN(
                    "value_length must be in range 0 to 255"
                )
            self.__fixed_value_length = value_length + _HEADER_LENGTH
        else:
            self.__fixed_value_length = value_length
        self._sort_tape_crlf_delimited(
            self.tapen,
            self._tapen_crlf_sort_key,
            self._tapen_crlf_chunk_sort_key,
            self._length_tapen_data_le_255
            if value_length is None else self._length_tapen_data_is_fixed
        )
    
    def sort_tapea_length_delimited(self, **kwargs):
        """Sort data, validated by length_data, in tape file into chunks.

        kwargs is for compatibility with sort_tapen_crlf_delimited but is
        ignored.

        """
        del kwargs
        self._sort_tape_length_delimited(
            self.tapea,
            self._tapea_length_sort_key,
            self._tapea_length_chunk_sort_key,
            ValueLengthTapeA,
        )
    
    def sort_tapen_length_delimited(self, **kwargs):
        """Sort data, validated by length_data, in tape file into chunks.

        kwargs is for compatibility with sort_tapen_crlf_delimited but is
        ignored.

        """
        del kwargs
        self._sort_tape_length_delimited(
            self.tapen,
            self._tapen_length_sort_key,
            self._tapen_length_chunk_sort_key,
            ValueLengthTapeN,
        )
