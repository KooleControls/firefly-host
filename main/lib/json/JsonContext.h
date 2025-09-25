#pragma once
#include "IStreamWriter.h"
#include "JsonStreamWriter.h"

class JsonObjectWriter;
class JsonArrayWriter;

class JsonContext {
    bool first = true;

protected:
    Stream& stream;
    JsonStreamWriter writer;

    explicit JsonContext(Stream& s)
        : stream(s), writer(s) {}

    void writeComma()       { if (first) first = false; else stream.write(",", 1); }
    void writeColon()       { stream.write(":", 1); }
    void writeBeginObject() { stream.write("{", 1); }
    void writeEndObject()   { stream.write("}", 1); }
    void writeBeginArray()  { stream.write("[", 1); }
    void writeEndArray()    { stream.write("]", 1); }
    void writeNull()        { stream.write("null", 4); }
};
