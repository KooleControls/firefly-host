#pragma once
#include "JsonContext.h"

class JsonArrayWriter : public JsonContext {
    friend class JsonContext;
    explicit JsonArrayWriter(Stream& s) : JsonContext(s) { writeBeginArray(); }
    ~JsonArrayWriter() { writeEndArray(); }

public:
    void value(int64_t v) { writeComma(); writer.writeInt(v); }
    void value(uint64_t v) { writeComma(); writer.writeUInt(v); }
    void value(const char* v) { writeComma(); writer.writeString(v); }
    void value(bool v) { writeComma(); writer.writeBool(v); }
    void fieldData(const uint8_t* data, size_t len) {        writeComma(); writer.writeData(data, len);    }
    void valueNull() { writeComma(); writeNull(); }

    template<typename FUNC>
    void withObject(FUNC callback);

    template<typename FUNC>
    void withArray(FUNC callback);

    template <typename FUNC>
    static void create(Stream& stream, FUNC callback);
};

#include "JsonWriters.inl"
