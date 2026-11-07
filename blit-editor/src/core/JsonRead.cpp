#include "core/Json.h"

#include <cstdlib>

// where the parser is in the text, and the first thing that went wrong
struct JsonReader
{
    const std::string& text;
    size_t at = 0;
    std::string error;

    explicit JsonReader(const std::string& source) : text(source) {}

    bool Fail(const std::string& message)
    {
        if (error.empty())
        {
            int line = 1;

            for (size_t i = 0; i < at && i < text.size(); i++)
            {
                line += text[i] == '\n';
            }

            error = "line " + std::to_string(line) + ": " + message;
        }

        return false;
    }

    void SkipSpace()
    {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' || text[at] == '\r'))
        {
            at++;
        }
    }

    bool Literal(const char* word)
    {
        size_t length = std::char_traits<char>::length(word);

        if (text.compare(at, length, word) != 0)
        {
            return Fail(std::string("expected ") + word);
        }

        at += length;
        return true;
    }

    void AppendUtf8(std::string& out, unsigned code)
    {
        if (code < 0x80)
        {
            out += (char)code;
        }
        else if (code < 0x800)
        {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
        }
        else
        {
            out += (char)(0xE0 | (code >> 12));
            out += (char)(0x80 | ((code >> 6) & 0x3F));
            out += (char)(0x80 | (code & 0x3F));
        }
    }

    bool String(std::string& out)
    {
        at++; // the opening quote

        while (at < text.size() && text[at] != '"')
        {
            char c = text[at++];

            if (c != '\\')
            {
                out += c;
                continue;
            }

            if (at >= text.size())
            {
                break;
            }

            c = text[at++];

            switch (c)
            {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u':
                if (at + 4 > text.size())
                {
                    return Fail("a \\u escape needs 4 hex digits");
                }

                AppendUtf8(out, (unsigned)std::strtoul(text.substr(at, 4).c_str(), nullptr, 16));
                at += 4;
                break;
            default: out += c; break;
            }
        }

        if (at >= text.size())
        {
            return Fail("a string isn't closed");
        }

        at++;
        return true;
    }

    bool Number(Json& out)
    {
        const char* start = text.c_str() + at;
        char* end = nullptr;
        double value = std::strtod(start, &end);

        if (end == start)
        {
            return Fail("expected a value");
        }

        at += (size_t)(end - start);
        out = Json(value);
        return true;
    }

    bool Value(Json& out, int depth)
    {
        if (depth > 128)
        {
            return Fail("nested too deep");
        }

        SkipSpace();

        if (at >= text.size())
        {
            return Fail("unexpected end of file");
        }

        char c = text[at];

        if (c == '{') return Object(out, depth);
        if (c == '[') return Array(out, depth);
        if (c == '"') { out = Json(""); return String(out.text); }
        if (c == 't') { out = Json(true); return Literal("true"); }
        if (c == 'f') { out = Json(false); return Literal("false"); }
        if (c == 'n') { out = Json(); return Literal("null"); }
        return Number(out);
    }

    bool Array(Json& out, int depth)
    {
        out = JsonArray();
        at++;
        SkipSpace();

        if (at < text.size() && text[at] == ']')
        {
            at++;
            return true;
        }

        for (;;)
        {
            Json item;

            if (!Value(item, depth + 1))
            {
                return false;
            }

            out.items.push_back(std::move(item));
            SkipSpace();

            if (at < text.size() && text[at] == ',') { at++; continue; }
            if (at < text.size() && text[at] == ']') { at++; return true; }
            return Fail("expected , or ] in an array");
        }
    }

    bool Object(Json& out, int depth)
    {
        out = JsonObject();
        at++;
        SkipSpace();

        if (at < text.size() && text[at] == '}')
        {
            at++;
            return true;
        }

        for (;;)
        {
            std::string key;
            Json value;

            SkipSpace();

            if (at >= text.size() || text[at] != '"')
            {
                return Fail("expected a key in quotes");
            }

            if (!String(key))
            {
                return false;
            }

            SkipSpace();

            if (at >= text.size() || text[at] != ':')
            {
                return Fail("expected : after a key");
            }

            at++;

            if (!Value(value, depth + 1))
            {
                return false;
            }

            out.fields.emplace_back(std::move(key), std::move(value));
            SkipSpace();

            if (at < text.size() && text[at] == ',') { at++; continue; }
            if (at < text.size() && text[at] == '}') { at++; return true; }
            return Fail("expected , or } in an object");
        }
    }
};

bool JsonParse(const std::string& text, Json& out, std::string& error)
{
    JsonReader reader(text);

    if (!reader.Value(out, 0))
    {
        error = reader.error;
        out = Json();
        return false;
    }

    reader.SkipSpace();

    if (reader.at != text.size())
    {
        reader.Fail("something after the end of the document");
        error = reader.error;
        return false;
    }

    return true;
}
