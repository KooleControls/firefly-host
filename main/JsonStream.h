#pragma once
#include <cstddef>
#include <cstring>
#include <cstdio>

// ---------------- Stream Interface ----------------
class Stream {
public:
    virtual ~Stream() = default;
    virtual size_t write(const void* data, size_t len) = 0;
    virtual size_t read(void* buffer, size_t len) = 0;
    virtual void flush() = 0;
};

class StringWriter {
	Stream& stream;
public:
    explicit StringWriter(Stream& s) : stream(s) {}
    size_t write(const char* str) {
        return stream.write(str, std::strlen(str));
	}
};


// ---------------- Base ----------------
class JsonContext {
    bool first;

protected:
    Stream& stream;
    StringWriter writer;
    
    JsonContext(Stream& s) 
        : first(true)
        , stream(s)
        , writer(stream) 
    {
        
    }

    void writeComma() {
        if (first) {
            first = false;
        } else {
            writer.write(",");
        }
    }
};


// forward declarations
class JsonObjectWriter;
class JsonArrayWriter;

// ---------------- ArrayWriter ----------------
class JsonArrayWriter : public JsonContext {
public:
    explicit JsonArrayWriter(Stream& s) : JsonContext(s) {
        writer.write( "[");
    }
    ~JsonArrayWriter() { writer.write( "]"); }

    void value(int32_t v) {
        writeComma();
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", v);
		writer.write(buf);
    }

    void value(const char* v) {
        writeComma();
        writer.write( "\"");
		writer.write(v);
        writer.write( "\"");
    }

    void value(bool v) {
        writeComma();
        writer.write( v ? "true" : "false");
    }

    void valueNull() {
        writeComma();
        writer.write( "null");
    }

    template<typename FUNC>
    void withObject(FUNC callback);

    template<typename FUNC>
    void withArray(FUNC callback);
};

// ---------------- ObjectWriter ----------------
class JsonObjectWriter : public JsonContext {
public:
    explicit JsonObjectWriter(Stream& s) : JsonContext(s) {
        writer.write( "{");
    }
    ~JsonObjectWriter() { writer.write( "}"); }

    void field(const char* key, int32_t value) {
        writeComma();
        writer.write( "\"");
		writer.write(key);
        writer.write( "\":");
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "%ld", value);
        writer.write(buf);
    }

    void field(const char* key, const char* value) {
        writeComma();
        writer.write( "\"");
        writer.write(key);
        writer.write( "\":\"");
        writer.write(value);
        writer.write( "\"");
    }

    void field(const char* key, bool value) {
        writeComma();
        writer.write( "\"");
        writer.write(key);
        writer.write( "\":");
        writer.write( value ? "true" : "false");
    }

    void field(const char* key, float value) {
        writeComma();
        writer.write( "\"");
        writer.write(key);
        writer.write( "\":");
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "%f", value);
        writer.write(buf);
    }

    void fieldNull(const char* key) {
        writeComma();
        writer.write( "\"");
        writer.write(key);
        writer.write( "\":null");
    }

    template<typename FUNC>
    void withObject(const char* key, FUNC callback) {
        writeComma();
        writer.write( "\"");
        writer.write(key);
        writer.write( "\":");
        {
            JsonObjectWriter nested(stream);
            callback(nested);
        }
    }

    template<typename FUNC>
    void withArray(const char* key, FUNC callback) {
        writeComma();
        writer.write( "\"");
        writer.write(key);
        writer.write( "\":");
        {
            JsonArrayWriter nested(stream);
            callback(nested);
        }
    }
};

// now that ObjectWriter is known, implement ArrayWriter methods
template<typename FUNC>
void JsonArrayWriter::withObject(FUNC callback) {
    writeComma();
    {
        JsonObjectWriter nested(stream);
        callback(nested);
    }
}

template<typename FUNC>
void JsonArrayWriter::withArray(FUNC callback) {
    writeComma();
    {
        JsonArrayWriter nested(stream);
        callback(nested);
    }
}
