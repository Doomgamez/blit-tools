#include "core/Json.h"

#include <cstdio>

#include "core/Files.h"

const Json* Json::Find(const std::string& key) const
{
    if (type != JsonType::Object)
    {
        return nullptr;
    }

    for (const auto& field : fields)
    {
        if (field.first == key)
        {
            return &field.second;
        }
    }

    return nullptr;
}

Json& Json::operator[](const std::string& key)
{
    if (type != JsonType::Object)
    {
        *this = JsonObject();
    }

    for (auto& field : fields)
    {
        if (field.first == key)
        {
            return field.second;
        }
    }

    fields.emplace_back(key, Json());
    return fields.back().second;
}

Json& Json::Push(const Json& value)
{
    if (type != JsonType::Array)
    {
        *this = JsonArray();
    }

    items.push_back(value);
    return items.back();
}

std::string Json::Text(const std::string& key, const std::string& fallback) const
{
    const Json* value = Find(key);
    return value && value->type == JsonType::String ? value->text : fallback;
}

double Json::Number(const std::string& key, double fallback) const
{
    const Json* value = Find(key);
    return value && value->type == JsonType::Number ? value->number : fallback;
}

bool Json::Bool(const std::string& key, bool fallback) const
{
    const Json* value = Find(key);
    return value && value->type == JsonType::Bool ? value->boolean : fallback;
}

void Json::Numbers(const std::string& key, float* out, int count) const
{
    const Json* value = Find(key);

    for (int i = 0; value && value->IsArray() && i < count && i < (int)value->items.size(); i++)
    {
        if (value->items[i].type == JsonType::Number)
        {
            out[i] = (float)value->items[i].number;
        }
    }
}

std::string JsonNumberText(double number)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%.9g", number);
    return text;
}

std::string Json::AsPropertyText() const
{
    switch (type)
    {
    case JsonType::Bool: return boolean ? "true" : "false";
    case JsonType::Number: return JsonNumberText(number);
    case JsonType::String: return text;
    case JsonType::Array:
    {
        std::string out;

        for (const Json& item : items)
        {
            out += (out.empty() ? "" : " ") + item.AsPropertyText();
        }

        return out;
    }
    default: return "";
    }
}

Json JsonArray()
{
    Json value;
    value.type = JsonType::Array;
    return value;
}

Json JsonObject()
{
    Json value;
    value.type = JsonType::Object;
    return value;
}

Json JsonNumbers(const float* values, int count)
{
    Json array = JsonArray();

    for (int i = 0; i < count; i++)
    {
        array.items.push_back(Json((double)values[i]));
    }

    return array;
}

bool JsonReadFile(const std::string& path, Json& out, std::string& error)
{
    std::string text;

    if (!ReadTextFile(path, text))
    {
        error = "can't read " + path;
        return false;
    }

    if (!JsonParse(text, out, error))
    {
        error = path + ": " + error;
        return false;
    }

    return true;
}

bool JsonWriteFile(const std::string& path, const Json& value)
{
    return WriteTextFile(path, JsonWrite(value) + "\n");
}
