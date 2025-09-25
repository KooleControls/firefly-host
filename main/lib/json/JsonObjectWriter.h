#pragma once
#include "JsonContext.h"

class JsonObjectWriter : public JsonContext {
    friend class JsonContext;
    explicit JsonObjectWriter(Stream& s) : JsonContext(s) { writeBeginObject(); }
    ~JsonObjectWriter() { writeEndObject(); }

public:
    void field(const char* key, int64_t v) {
        writeComma(); writer.writeString(key); writeColon(); writer.writeInt(v);
    }
    void field(const char* key, uint64_t v) {
        writeComma(); writer.writeString(key); writeColon(); writer.writeUInt(v);
    }
    void field(const char* key, const char* v) {
        writeComma(); writer.writeString(key); writeColon(); writer.writeString(v);
    }
    void field(const char* key, bool v) {
        writeComma(); writer.writeString(key); writeColon(); writer.writeBool(v);
    }
    void fieldData(const char* key, const uint8_t* data, size_t len) {
        writeComma(); writer.writeString(key); writeColon(); writer.writeData(data, len);
    }
    void fieldNull(const char* key) {
        writeComma(); writer.writeString(key); writeColon(); writeNull();
    }


    template <typename FUNC>
    void withObject(const char* key, FUNC callback);

    template <typename FUNC>
    void withArray(const char* key, FUNC callback);

    template <typename FUNC>
    static void create(Stream& stream, FUNC callback);
};

#include "JsonWriters.inl"
