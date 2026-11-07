#include "core/Json.h"

#include <cstdio>

void JsonWriteString(std::string& out, const std::string& text)
{
    out += '"';

    for (unsigned char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += (char)c;
        }
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c == '\r') out += "\\r";
        else if (c < 0x20)
        {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
        }
        else
        {
            out += (char)c;
        }
    }

    out += '"';
}

// arrays of a few numbers (positions, planes, colours) read best on one line
bool JsonIsShortArray(const Json& value)
{
    if (value.items.size() > 6)
    {
        return false;
    }

    for (const Json& item : value.items)
    {
        if (item.type != JsonType::Number)
        {
            return false;
        }
    }

    return true;
}

void JsonWriteValue(std::string& out, const Json& value, int indent)
{
    std::string inner(indent + 2, ' ');
    std::string outer(indent, ' ');

    switch (value.type)
    {
    case JsonType::Null: out += "null"; break;
    case JsonType::Bool: out += value.boolean ? "true" : "false"; break;
    case JsonType::Number: out += JsonNumberText(value.number); break;
    case JsonType::String: JsonWriteString(out, value.text); break;

    case JsonType::Array:
        if (value.items.empty())
        {
            out += "[]";
        }
        else if (JsonIsShortArray(value))
        {
            out += '[';

            for (size_t i = 0; i < value.items.size(); i++)
            {
                out += i ? ", " : "";
                JsonWriteValue(out, value.items[i], indent);
            }

            out += ']';
        }
        else
        {
            out += "[\n";

            for (size_t i = 0; i < value.items.size(); i++)
            {
                out += inner;
                JsonWriteValue(out, value.items[i], indent + 2);
                out += i + 1 < value.items.size() ? ",\n" : "\n";
            }

            out += outer + "]";
        }
        break;

    case JsonType::Object:
        if (value.fields.empty())
        {
            out += "{}";
            break;
        }

        out += "{\n";

        for (size_t i = 0; i < value.fields.size(); i++)
        {
            out += inner;
            JsonWriteString(out, value.fields[i].first);
            out += ": ";
            JsonWriteValue(out, value.fields[i].second, indent + 2);
            out += i + 1 < value.fields.size() ? ",\n" : "\n";
        }

        out += outer + "}";
        break;
    }
}

std::string JsonWrite(const Json& value)
{
    std::string out;
    JsonWriteValue(out, value, 0);
    return out;
}
