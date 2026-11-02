'use strict';

// the same rules as Tokenize in blitscriptc.c: commas are whitespace, // starts a comment
// anywhere outside a string, even in the middle of a word

function isSeparator(c) {
    return c === ' ' || c === '\t' || c === ',' || c === '\r' || c === '\n';
}

function isComment(line, i) {
    return line[i] === '/' && line[i + 1] === '/';
}

function readString(line, start) {
    let i = start + 1;

    while (i < line.length && line[i] !== '"') {
        i += line[i] === '\\' && i + 1 < line.length ? 2 : 1;
    }

    return { end: i, closed: i < line.length };
}

function tokenize(line) {
    const tokens = [];
    let commentStart = -1;
    let unterminated = false;
    let i = 0;

    while (i < line.length) {
        while (i < line.length && isSeparator(line[i])) {
            i++;
        }

        if (i >= line.length) {
            break;
        }

        if (isComment(line, i)) {
            commentStart = i;
            break;
        }

        const start = i;

        if (line[i] === '"') {
            const string = readString(line, i);

            i = string.closed ? string.end + 1 : line.length;
            unterminated = !string.closed;
            tokens.push({ text: line.slice(start + 1, string.end), start, end: i, quoted: true });
            continue;
        }

        while (i < line.length && !isSeparator(line[i]) && !isComment(line, i)) {
            i++;
        }

        tokens.push({ text: line.slice(start, i), start, end: i, quoted: false });
    }

    return { tokens, commentStart, unterminated };
}

module.exports = { tokenize };
