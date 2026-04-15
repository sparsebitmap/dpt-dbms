/* File : dptapi_python.i */
/* Copyright 2007 Roger Marsh */
/* See www.dptoolkit.com for details of DPT */
/* License: DPToolkit license */ 
/* Interface done with no change to DPT source files. */

%module dptapi
%{
#include "dptdb.h"
%}

%rename (FD_POINT) dpt::FD_POINT$;
%rename (FD_SET) dpt::FD_SET$;
%rename (FD_FILE) dpt::FD_FILE$;
%rename (FD_NOT_POINT) dpt::FD_NOT_POINT$;
%rename (__invert__) dpt::APIFindSpecification::operator!;

//Pick the operator= signatures to wrap.
//APIFieldValue and APIRoundedDouble have explicit Assign methods.
%ignore dpt::APIFieldValue::operator=;
%ignore dpt::APIRoundedDouble::operator=;

%include std_vector.i
%include std_string.i
namespace std {
    %template(IntVector) vector<int>;
    %template(StringVector) vector<std::string>;
}

%exception {
    try {
        $action
    } catch (dpt::Exception &e) {
        PyErr_SetString(PyExc_RuntimeError, const_cast<char *>(e.What().c_str()));
        return NULL;
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "DPT API unknown error");
        return NULL;
    }
}

%include "cpointer.i"
%pointer_class(bool, BoolPtr);
%pointer_class(int, IntPtr);
%pointer_class(short, ShortPtr);
%pointer_class(std::string, StdStringPtr);

%include ../apiconst.h
%include ../msgopts.h
%include ../const_file.h
%include ../const_stat.h
%include ../const_group.h
%include ../lineio.h
%include fieldatts.h
%include ctxtspec.h
%include cursor.h
%include floatnum.h
%include fieldinfo.h
%include fieldval.h
%include grpserv.h
%include msgroute.h
%include seqfile.h
%include seqserv.h
%include sortspec.h
%include findspec.h
%include recread.h
%include valdirect.h
%include valset.h
%include reccopy.h
%include record.h
%include recset.h
%include sortset.h
%include access.h
%include bmset.h
%include foundset.h
%include reclist.h
%include dbctxt.h
%include parmvr.h
%include statview.h
%include core.h
%include dbserv.h

%extend dpt::APIRoundedDouble {

    // pyCastToRoundedDouble is provided for multi-step deferred update where
    // numeric values have been written to sequential files in internal format.

    double pyCastToRoundedDouble(const std::string& binary_string) {

        $self->Assign(*(reinterpret_cast<const double*>(binary_string.data())));
        return $self->Data();

    }

    // pyCastToString is provided for multi-step deferred update where numeric
    // values are to be written to sequential files in internal format.

    std::string pyCastToString(const double& d) {

        $self->Assign(d);
        double data;
        data = $self->Data();
        return std::string(reinterpret_cast<char*>(&data), 8);

    }

};

%inline %{

    // The pyAppend<type> functions may make loading very large numbers of
    // field-value pairs much quicker than using APIStoreRecordTemplate.Append
    // and APIFieldValue.Assign directly if the decision on which to use is
    // cheap.
    // Overloading pyAppend to do this is much slower than direct use of the
    // two API methods.
    // The effect is most noticable in single-step deferred update mode.

    void pyAppendChar(
        dpt::APIStoreRecordTemplate& record,
        const std::string& name,
        dpt::APIFieldValue& field,
        const char* value) {

        field.Assign(value);
        record.Append(name, field);

    }

    void pyAppendStdString(
        dpt::APIStoreRecordTemplate& record,
        const std::string& name,
        dpt::APIFieldValue& field,
        const std::string& value) {

        field.Assign(value);
        record.Append(name, field);

    }

    void pyAppendInt(
        dpt::APIStoreRecordTemplate& record,
        const std::string& name,
        dpt::APIFieldValue& field,
        const int& value) {

        field.Assign(value);
        record.Append(name, field);

    }

    void pyAppendDouble(
        dpt::APIStoreRecordTemplate& record,
        const std::string& name,
        dpt::APIFieldValue& field,
        const double& value) {

        field.Assign(value);
        record.Append(name, field);

    }

    void pyAppendAPIRoundedDouble(
        dpt::APIStoreRecordTemplate& record,
        const std::string& name,
        dpt::APIFieldValue& field,
        const dpt::APIRoundedDouble& value) {

        field.Assign(value);
        record.Append(name, field);

    }

    void pyAppendAPIFieldValue(
        dpt::APIStoreRecordTemplate& record,
        const std::string& name,
        dpt::APIFieldValue& field,
        const dpt::APIFieldValue& value) {

        field.Assign(value);
        record.Append(name, field);

    }

%}

%pythoncode %{

'''
Raise exceptions for APIRoundedDouble numbers out of range rather than
just assign 0.0 instead.
'''
APIRoundedDouble_SetNumRangeThrowOption(True)

%}
