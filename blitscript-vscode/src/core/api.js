'use strict';

// reads docs/cheatsheet.txt and blitscript/engine.bs, both are generated from the engine's
// own function table, so what they say is exactly what the running engine has

const FUNCTION_LINE = /^ {4}(\w+)\s+(\w+)\((.*?)\);\s*(?:\/\/\s*(.*))?$/;
const CATEGORY_LINE = /^ {4}\/\/ (.+)$/;

function parseParameter(text) {
    const words = text.trim().split(/\s+/);
    const name = words.pop();

    return {
        type: words.join(' ') || 'any',
        name: name.replace(/\.\.\.$/, ''),
        variadic: name.endsWith('...'),
    };
}

function parseParameters(text) {
    return text.trim() === '' ? [] : text.split(',').map(parseParameter);
}

function parseCheatsheet(text) {
    const functions = {};
    let category = '';

    for (const line of text.split(/\r?\n/)) {
        const header = CATEGORY_LINE.exec(line);
        const match = FUNCTION_LINE.exec(line);

        if (header) {
            category = header[1];
        } else if (match) {
            functions[match[2].toLowerCase()] = {
                name: match[2],
                returns: match[1],
                params: parseParameters(match[3]),
                doc: match[4] || '',
                category,
            };
        }
    }

    return functions;
}

function parseConstants(text) {
    const constants = {};
    let category = '';

    for (const line of text.split(/\r?\n/)) {
        const header = /^\/\/ (\S.*)$/.exec(line);
        const match = /^CONST\s+(\S+)\s+(.+?)\s*$/.exec(line);

        if (match) {
            constants[match[1]] = { name: match[1], value: match[2], category };
        } else if (header) {
            category = header[1];
        }
    }

    return constants;
}

function signature(fn) {
    const params = fn.params.map((param) => `${param.type} ${param.name}${param.variadic ? '...' : ''}`).join(', ');
    return `${fn.returns} ${fn.name}(${params})`;
}

module.exports = { parseCheatsheet, parseConstants, signature };
