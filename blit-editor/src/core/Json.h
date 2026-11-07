#pragma once

// a small JSON tree for the editor's files: levels, projects and settings
// objects keep their key order, so files saved by the editor diff cleanly
//
//   Json.cpp       the tree, JsonRead.cpp the parser, JsonWrite.cpp the writer

#include <string>
#include <utility>
#include <vector>

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct Json
{
    JsonType type = JsonType::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Json> items;                            // Array
    std::vector<std::pair<std::string, Json>> fields;   // Object

    Json() = default;
    Json(bool value) : type(JsonType::Bool), boolean(value) {}
    Json(double value) : type(JsonType::Number), number(value) {}
    Json(int value) : type(JsonType::Number), number(value) {}
    Json(float value) : type(JsonType::Number), number(value) {}
    Json(const char* value) : type(JsonType::String), text(value) {}
    Json(const std::string& value) : type(JsonType::String), text(value) {}

    bool IsObject() const { return type == JsonType::Object; }
    bool IsArray() const { return type == JsonType::Array; }

    // the value of a key, nullptr when it isn't an object or has no such key
    const Json* Find(const std::string& key) const;
    // the value of a key, made (and the value turned into an object) when it isn't there
    Json& operator[](const std::string& key);
    // appends to an array, turning the value into one
    Json& Push(const Json& value);

    // a key's value if it has the right type, else the fallback
    std::string Text(const std::string& key, const std::string& fallback = "") const;
    double Number(const std::string& key, double fallback = 0.0) const;
    bool Bool(const std::string& key, bool fallback = false) const;
    // a key's array of numbers into out[0..count), what's missing keeps its value
    void Numbers(const std::string& key, float* out, int count) const;

    // the value as a property string: strings as they are, numbers short, arrays joined with spaces
    std::string AsPropertyText() const;
};

Json JsonArray();
Json JsonObject();
// an array of count numbers
Json JsonNumbers(const float* values, int count);

// a number in its shortest exact form: 1, 0.25, 1e-07
std::string JsonNumberText(double number);

// false, with what and where in error, when the text isn't JSON
bool JsonParse(const std::string& text, Json& out, std::string& error);
// indented with two spaces, short arrays of numbers stay on one line
std::string JsonWrite(const Json& value);
// the writer's steps
void JsonWriteValue(std::string& out, const Json& value, int indent);
void JsonWriteString(std::string& out, const std::string& text);
bool JsonIsShortArray(const Json& value);

// a whole file, false if it can't be read or isn't JSON
bool JsonReadFile(const std::string& path, Json& out, std::string& error);
bool JsonWriteFile(const std::string& path, const Json& value);
