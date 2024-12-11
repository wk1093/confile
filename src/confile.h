/**
 * CON file format
 * similar to JSON, but C instead of JS
 * uses a binary format for faster parsing
 * has same data types as JSON
 * but has compression, type checking, and other features
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>

#include <zlib.h>

std::vector<uint8_t> zcompress(std::vector<uint8_t> input) {

    std::vector<uint8_t> output;
    std::vector<uint8_t> buffer(64 * 1024);
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        std::cerr << "Failed to initialize deflate stream, error: " << ret << std::endl;
        return {};
    }

    stream.avail_in = input.size();
    stream.next_in = input.data();

    do {
        stream.avail_out = buffer.size();
        stream.next_out = buffer.data();

        ret = deflate(&stream, stream.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH);

        if (ret != Z_OK && ret != Z_STREAM_END) {
            std::cerr << "Failed to compress data, error: " << ret << std::endl;
            deflateEnd(&stream);
            return {};
        }

        output.insert(output.end(), buffer.begin(), buffer.begin() + (buffer.size() - stream.avail_out));
    } while (ret != Z_STREAM_END);

    deflateEnd(&stream);

    if (input.size() < output.size()) {
        std::cout << "INEFFICIENT COMPRESSION" << std::endl;
    }

    return output;
}

std::vector<uint8_t> zdecompress(std::vector<uint8_t> input) {
    std::vector<uint8_t> output;
    std::vector<uint8_t> buffer(64 * 1024);
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = input.size();
    stream.next_in = input.data();

    int ret = inflateInit(&stream);
    if (ret != Z_OK) {
        std::cerr << "Failed to initialize inflate stream, error: " << ret << std::endl;
        return {};
    }

    do {
        stream.avail_out = buffer.size();
        stream.next_out = buffer.data();

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret == Z_STREAM_END) {
            output.insert(output.end(), buffer.begin(), buffer.begin() + (buffer.size() - stream.avail_out));
            break; // End of stream
        } else if (ret != Z_OK) {
            std::cerr << "Failed to decompress data, error: " << ret << std::endl;
            inflateEnd(&stream);
            return {};
        }

        output.insert(output.end(), buffer.begin(), buffer.begin() + (buffer.size() - stream.avail_out));
    } while (stream.avail_out == 0);

    inflateEnd(&stream);

    return output;
}

enum class ConType : uint8_t {
    Null,
    Boolean,
    Integer,
    Float,
    String,
    Array,
    Object
    #define CON_TYPE_Null void
    #define CON_TYPE_Boolean bool
    #define CON_TYPE_Integer int64_t
    #define CON_TYPE_Float double
    #define CON_TYPE_String std::string
    #define CON_TYPE_Array ConArray
    #define CON_TYPE_Object ConObject
};

struct ConObject;
struct ConArray;

#define CON_CAST(value, type) (*((CON_TYPE_##type*)(value).data))

struct ConValue {
    ConType type;
    void* data;

    ConValue() : type(ConType::Null), data(nullptr) {}
    ConValue(bool value) : type(ConType::Boolean), data(new bool(value)) {}
    ConValue(int64_t value) : type(ConType::Integer), data(new int64_t(value)) {}
    ConValue(double value) : type(ConType::Float), data(new double(value)) {}
    ConValue(std::string value) : type(ConType::String), data(new std::string(value)) {}
    ConValue(ConArray* value);
    ConValue(ConObject* value);

    ConValue(const ConValue& other);


    void write(std::ostream& stream, uint64_t level=0);

    void read(std::istream& stream);
};

struct ConArray {
    std::vector<ConValue> values;

    ConValue& operator[](size_t index) {
        return values[index];
    }

    void write(std::ostream& stream, uint64_t level) {
        uint64_t size = values.size();
        stream.write((char*)&size, sizeof(uint64_t));
        for (ConValue& value : values) {
            value.write(stream, level+1);
        }
    }

    void read(std::istream& stream) {
        uint64_t size;
        stream.read((char*)&size, sizeof(uint64_t));
        for (size_t i = 0; i < size; i++) {
            ConValue value;
            value.read(stream);
            values.push_back(value);
        }
    }
};

struct ConObject {
    std::map<std::string, ConValue> values;

    ConValue& operator[](std::string key) {
        return values[key];
    }

    ConValue& operator[](const char* key) {
        return values[key];
    }

    void write(std::ostream& stream, uint64_t level) {
        uint64_t size = values.size();
        stream.write((char*)&size, sizeof(uint64_t));
        for (auto& [key, value] : values) {
            uint64_t keySize = key.size();
            stream.write((char*)&keySize, sizeof(uint64_t));
            stream.write(key.c_str(), keySize);
            value.write(stream, level+1);
        }
    }
    void read(std::istream& stream) {
        uint64_t size;
        stream.read((char*)&size, sizeof(uint64_t));
        for (size_t i = 0; i < size; i++) {
            uint64_t keySize;
            stream.read((char*)&keySize, sizeof(uint64_t));
            char* buffer = new char[keySize];
            stream.read(buffer, keySize);
            std::string key(buffer, keySize);
            ConValue value;
            value.read(stream);
            values[key] = value;
            delete[] buffer;
        }
    }
};


ConValue::ConValue(ConArray* value) : type(ConType::Array), data(new ConArray(*value)) {}
ConValue::ConValue(ConObject* value) : type(ConType::Object), data(new ConObject(*value)) {}
ConValue::ConValue(const ConValue& other) {
    type = other.type;
    switch (type) {
        case ConType::Boolean:
            data = new bool(CON_CAST(other, Boolean));
            break;
        case ConType::Integer:
            data = new int64_t(CON_CAST(other, Integer));
            break;
        case ConType::Float:
            data = new double(CON_CAST(other, Float));
            break;
        case ConType::String:
            data = new std::string(CON_CAST(other, String));
            break;
        case ConType::Array:
            data = new ConArray(CON_CAST(other, Array));
            break;
        case ConType::Object:
            data = new ConObject(CON_CAST(other, Object));
            break;
        default:
            data = nullptr;
            break;
    }
}

void ConValue::write(std::ostream& stream, uint64_t level) {

    const static uint64_t COMPRESSION_THRESHOLD = 256;
    // we might also want to only compress certain levels, for example only compress top-level data, or only a certain range of levels
    // this can also depend on the data itself
    // 0 is top-level, 1 is first level, 2 is second level, etc.
    const static uint64_t COMPRESSION_LEVEL_MIN = 0;
    const static uint64_t COMPRESSION_LEVEL_MAX = 0;

#define COMPRESS_COMPARE(size) ((size) > COMPRESSION_THRESHOLD && level >= COMPRESSION_LEVEL_MIN && level <= COMPRESSION_LEVEL_MAX)

    // we will first write the object into a buffer, and if it is too large, we will compress it
    // only if it is array, or object, and if it is larger than 64 bytes (todo: optimize this value)
    // we will write the type first
    stream.write((char*)&type, sizeof(ConType));

    // then we will write whether it is compressed or not,
    // if it is, we append the compressed byte count, then the compressed data
    // otherwise, just write the data (size isn't needed because of how the format is designed)
    switch (type) {
        case ConType::Boolean:
            stream.write((char*)data, sizeof(bool));
            break;
        case ConType::Integer:
            stream.write((char*)data, sizeof(int64_t));
            break;
        case ConType::Float:
            stream.write((char*)data, sizeof(double));
            break;
        case ConType::String: {
            std::string str = CON_CAST(*this, String);
            if (COMPRESS_COMPARE(str.size())) {
                std::vector<uint8_t> compressed = zcompress(std::vector<uint8_t>(str.begin(), str.end()));
                uint64_t size = compressed.size();
                stream.put(1);
                stream.write((char*)&size, sizeof(uint64_t));
                stream.write((char*)compressed.data(), size);
            } else {
                uint64_t size = str.size();
                stream.put(0);
                stream.write((char*)&size, sizeof(uint64_t));
                stream.write(str.c_str(), size);
            }
            break;
        }
        case ConType::Array: {
            std::stringstream buffer;
            ((ConArray*)data)->write(buffer, level);
            std::string str = buffer.str();
            if (COMPRESS_COMPARE(str.size())) {
                std::vector<uint8_t> compressed = zcompress(std::vector<uint8_t>(str.begin(), str.end()));
                
                uint64_t size = compressed.size();
                stream.put(1);
                stream.write((char*)&size, sizeof(uint64_t));
                stream.write((char*)compressed.data(), size);
            } else {
                uint64_t size = str.size();
                stream.put(0);
                stream.write(str.c_str(), size);
            }
        } break;
        case ConType::Object: {
            std::stringstream buffer;
            ((ConObject*)data)->write(buffer, level);
            std::string str = buffer.str();
            if (COMPRESS_COMPARE(str.size())) {
                std::vector<uint8_t> compressed = zcompress(std::vector<uint8_t>(str.begin(), str.end()));
                uint64_t size = compressed.size();
                stream.put(1);
                stream.write((char*)&size, sizeof(uint64_t));
                stream.write((char*)compressed.data(), size);
            } else {
                uint64_t size = str.size();
                stream.put(0);
                stream.write(str.c_str(), size);
            }
        } break;
        default:
            std::cerr << "Invalid type" << std::endl;
            std::cerr << "Type: " << (int)type << std::endl;
            break;      
    }
}

void ConValue::read(std::istream& stream) {
    data = nullptr;
    uint8_t type;
    stream.read((char*)&type, sizeof(uint8_t));
    this->type = (ConType)type;
    switch (this->type) {
        case ConType::Boolean:
            data = new bool;
            stream.read((char*)data, sizeof(bool));
            break;
        case ConType::Integer:
            data = new int64_t;
            stream.read((char*)data, sizeof(int64_t));
            break;
        case ConType::Float:
            data = new double;
            stream.read((char*)data, sizeof(double));
            break;
        case ConType::String: {
            uint8_t compressed;
            stream.read((char*)&compressed, sizeof(uint8_t));
            uint64_t size;
            stream.read((char*)&size, sizeof(uint64_t));
            char* buffer = new char[size];
            stream.read(buffer, size);
            if (compressed) {
                std::vector<uint8_t> compressed(buffer, buffer + size);
                std::vector<uint8_t> decompressed = zdecompress(compressed);
                std::string str(decompressed.begin(), decompressed.end());
                data = new std::string(str);

            } else {
                std::string str(buffer, buffer + size);
                data = new std::string(str);
            }
            delete[] buffer;
            break;
        }
        case ConType::Array: {
            uint8_t compressed;
            stream.read((char*)&compressed, sizeof(uint8_t));
            if (compressed) {
                uint64_t size;
                stream.read((char*)&size, sizeof(uint64_t));
                char* buffer = new char[size];
                stream.read(buffer, size);
                std::vector<uint8_t> compressed(buffer, buffer + size);
                std::vector<uint8_t> decompressed = zdecompress(compressed);
                std::string str(decompressed.begin(), decompressed.end());
                std::stringstream bufferStream(str);
                ConArray* arr = new ConArray;
                arr->read(bufferStream);
                data = arr;
                delete[] buffer;

            } else {
                ConArray* arr = new ConArray;
                arr->read(stream);
                data = arr;
            }
            break;
        }
        case ConType::Object: {
            uint8_t compressed;
            stream.read((char*)&compressed, sizeof(uint8_t));
            if (compressed) {
                uint64_t size;
                stream.read((char*)&size, sizeof(uint64_t));
                // char* buffer = new char[size];
                // stream.read(buffer, size);
                // std::vector<uint8_t> compressed(buffer, buffer + size);
                // for some reason this only is loading 19 of the correct compressed bytes, and the rest is random unitialized memory (I think, since it is random data, some are wierd strings (debug data probably))
                // lets test where we are in the file, and how many bytes are left in the file

                std::vector<uint8_t> compressed(size);
                stream.read((char*)compressed.data(), size);
                std::vector<uint8_t> decompressed = zdecompress(compressed);
                std::string str(decompressed.begin(), decompressed.end());
                std::stringstream bufferStream(str);
                ConObject* obj = new ConObject;
                obj->read(bufferStream);
                data = obj;
            } else {
                ConObject* obj = new ConObject;
                obj->read(stream);
                data = obj;
            }
            break;
        }
    }
}

// converting con to json

std::ostream& operator<<(std::ostream& os, ConValue& value);

std::ostream& operator<<(std::ostream& os, ConArray& arr) {
    os << "[";
    for (size_t i = 0; i < arr.values.size(); i++) {
        os << arr.values[i];
        if (i < arr.values.size() - 1) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, ConObject& obj) {
    os << "{";
    for (auto& [key, value] : obj.values) {
        os << '"' << key << "\": " << value;
        if (&obj.values[key] != &obj.values.rbegin()->second) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

std::ostream& operator<<(std::ostream& os, ConValue& value) {
    switch (value.type) {
        case ConType::Null:
            os << "null";
            break;
        case ConType::Boolean:
            os << (CON_CAST(value, Boolean) ? "true" : "false");
            break;
        case ConType::Integer:
            os << CON_CAST(value, Integer);
            break;
        case ConType::Float:
            os << CON_CAST(value, Float);
            break;
        case ConType::String:
            os << '"' << CON_CAST(value, String) << '"';
            break;
        case ConType::Array:
            os << CON_CAST(value, Array);
            break;
        case ConType::Object:
            os << CON_CAST(value, Object);
            break;
        default:
            os << "Invalid type";
            break;
    }
    return os;
}

// converting json to con

void eliminateWhitespace(std::istream& is) {
    char c;
    while (is.get(c)) {
        if (c != ' ' && c != '\n' && c != '\t' && c != '\r') {
            is.putback(c);
            break;
        }
    }
}

std::istream& operator>>(std::istream& is, ConValue& value);

std::istream& operator>>(std::istream& is, ConArray& arr) {
    char c;
    eliminateWhitespace(is);
    is >> c;
    if (c != '[') {
        is.setstate(std::ios::failbit);
        std::cout << "Failed to read array: no '['" << std::endl;
        return is;
    }
    // check if it an empty array
    eliminateWhitespace(is);
    is >> c;
    if (c == ']') {
        eliminateWhitespace(is);
        return is;
    }
    is.putback(c);
    while (true) {
        ConValue value;
        eliminateWhitespace(is);
        is >> value;
        arr.values.push_back(value);
        eliminateWhitespace(is);
        is >> c;
        if (c == ']') {
            break;
        } else if (c != ',') {
            is.setstate(std::ios::failbit);
            std::cout << "Failed to read array: no ',': '" << c << '\'' << std::endl;
            return is;
        }
    }
    return is;
}

std::istream& operator>>(std::istream& is, ConObject& obj) {
    char c;
    eliminateWhitespace(is);
    is >> c;
    if (c != '{') {
        is.setstate(std::ios::failbit);
        std::cout << "Failed to read object: no '{'" << std::endl;
        return is;
    }
    // check if it an empty object
    eliminateWhitespace(is);
    is >> c;
    if (c == '}') {
        eliminateWhitespace(is);
        return is;
    }
    is.putback(c);

    while (true) {
        std::string key;
        eliminateWhitespace(is);
        is >> c;
        if (c != '"') {
            is.setstate(std::ios::failbit);
            std::cout << "Failed to read object: no '\"'" << std::endl;
            return is;
        }
        std::getline(is, key, '"');
        eliminateWhitespace(is);
        is >> c;
        if (c != ':') {
            is.setstate(std::ios::failbit);
            std::cout << "Failed to read object: no ':'" << std::endl;
            return is;
        }
        ConValue value;
        eliminateWhitespace(is);
        is >> value;
        if (is.fail()) {
            std::cout << "Failed to read object: failed to read value" << std::endl;
            return is;
        }
        obj.values[key] = value;
        eliminateWhitespace(is);
        is >> c;
        if (c == '}') {
            break;
        } else if (c != ',') {
            is.setstate(std::ios::failbit);
            std::cout << "Failed to read object: no ',': '" << c << '\'' << std::endl;
            return is;
        }
    }
    return is;
}

std::istream& operator>>(std::istream& is, ConValue& value) {
    char c;
    eliminateWhitespace(is);
    is.get(c);
    is.putback(c);
    if (c == 'n') {
        value.type = ConType::Null;
    } else if (c == 't' || c == 'f') {
        bool b;
        eliminateWhitespace(is);
        b = c == 't';
        // use up all the characters
        for (size_t i = 0; i < (b ? 4 : 5); i++) {
            is.get(c);
        }
        value = ConValue((bool)b);
    } else if (c == '"') {
        std::string str;
        eliminateWhitespace(is);
        is.get(c);
        if (c != '"') {
            std::cerr << "Failed to read string: no '\"'" << std::endl;
            is.setstate(std::ios::failbit);
        }
        while (true) {
            is.get(c);
            if (c == '"') {
                break;
            }
            str += c;
        }
        value = ConValue(str);
    } else if (c == '[') {
        ConArray arr;
        eliminateWhitespace(is);
        is >> arr;
        value = ConValue(&arr);
    } else if (c == '{') {
        ConObject obj;
        eliminateWhitespace(is);
        is >> obj;
        value = ConValue(&obj);
    } else {
        double d;
        eliminateWhitespace(is);
        is >> d;
        if (d == (int64_t)d) {
            value = ConValue((int64_t)d);
        } else {
            value = ConValue(d);
        }
    }
    if (is.fail()) {
        std::cout << "Failed to read value?" << std::endl;
        std::cout << "c: " << c << std::endl;
        std::cout << "valtype: " << (int)value.type << std::endl;

    }
    return is;
}