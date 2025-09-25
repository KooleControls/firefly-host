#pragma once
#include "JsonObjectWriter.h"
#include "JsonArrayWriter.h"

// JsonObjectWriter impls
template<typename FUNC>
void JsonObjectWriter::withObject(const char* key, FUNC callback) {
    writeComma();
    writer.writeString(key);
    writeColon();
    JsonObjectWriter::create(stream, callback);
}

template<typename FUNC>
void JsonObjectWriter::withArray(const char* key, FUNC callback) {
    writeComma();
    writer.writeString(key);
    writeColon();
    JsonArrayWriter::create(stream, callback);
}

template<typename FUNC>
void JsonObjectWriter::create(Stream& stream, FUNC callback) {
    JsonObjectWriter root(stream);
    callback(root);
}

// JsonArrayWriter impls
template<typename FUNC>
void JsonArrayWriter::withObject(FUNC callback) {
    writeComma();
    JsonObjectWriter::create(stream, callback);
}

template<typename FUNC>
void JsonArrayWriter::withArray(FUNC callback) {
    writeComma();
    JsonArrayWriter::create(stream, callback);
}

template<typename FUNC>
void JsonArrayWriter::create(Stream& stream, FUNC callback) {
    JsonArrayWriter root(stream);
    callback(root);
}
