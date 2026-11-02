'use strict';

const { collect, findFunction, findConstant, findLabel } = require('../core/program');
const { tokenAt, classify } = require('../core/cursor');
const { sameName } = require('../core/document');

const location = (document, line, token) => ({ file: document.path, line, start: token.start, end: token.end });

function isParameter(line, name) {
    return Boolean(line.fn) && line.fn.params.some((param) => param.name === name);
}

function definition(program, document, lineIndex, character) {
    const found = tokenAt(document, lineIndex, character);

    if (!found) {
        return null;
    }

    const { line, index, token } = found;
    const kind = classify(line.tokens, index);

    if (kind === 'include') {
        const include = document.includes.find((item) => item.line === lineIndex);
        return include && include.found ? { file: include.resolved, line: 0, start: 0, end: 0 } : null;
    }

    if (kind === 'label') {
        const label = findLabel(program, token.text);
        return label ? location(label.document, label.line, label.token) : null;
    }

    if (kind === 'function') {
        const fn = findFunction(program, token.text);
        return fn ? location(fn.document, fn.line, fn.token) : null;
    }

    if (kind === 'constant' || kind === 'variable') {
        const constant = findConstant(program, token.text);

        if (constant) {
            return location(constant.document, constant.line, constant.token);
        }

        if (isParameter(line, token.text)) {
            const param = line.fn.params.find((item) => item.name === token.text);
            return location(document, line.fn.line, param.token);
        }

        const write = collect(program, 'writes').find((item) => item.name === token.text);
        return write ? location(write.document, write.line, write.token) : null;
    }

    return null;
}

// the symbol under the cursor, what counts as "the same name" depends on its kind
function symbolAt(program, document, lineIndex, character) {
    const found = tokenAt(document, lineIndex, character);

    if (!found) {
        return null;
    }

    const { line, index, token } = found;
    let kind = classify(line.tokens, index);

    if (kind === 'variable' && findConstant(program, token.text)) {
        kind = 'constant';
    }

    if (kind === 'function' && !findFunction(program, token.text)) {
        return null;
    }

    if (kind === 'variable') {
        return { kind, name: token.text, scope: isParameter(line, token.text) ? line.fn : null, token };
    }

    return ['label', 'function', 'constant'].includes(kind) ? { kind, name: token.text, scope: null, token } : null;
}

function matches(symbol, tokens, index, line) {
    const token = tokens[index];

    if (token.quoted) {
        return false;
    }

    let kind = classify(tokens, index);

    if (symbol.kind === 'constant' && kind === 'variable') {
        kind = 'constant';
    }

    if (kind !== symbol.kind) {
        return false;
    }

    if (symbol.kind === 'label' || symbol.kind === 'function') {
        return sameName(token.text, symbol.name);
    }

    if (token.text !== symbol.name) {
        return false;
    }

    if (symbol.scope) {
        return line.fn === symbol.scope;
    }

    return symbol.kind !== 'variable' || !isParameter(line, token.text);
}

function references(program, document, lineIndex, character) {
    const symbol = symbolAt(program, document, lineIndex, character);
    const out = [];

    if (!symbol) {
        return out;
    }

    for (const file of program.documents.values()) {
        file.lines.forEach((line, index) => {
            line.tokens.forEach((token, tokenIndex) => {
                if (matches(symbol, line.tokens, tokenIndex, line)) {
                    out.push(location(file, index, token));
                }
            });
        });
    }

    return out;
}

// engine.bs is generated, a rename there would be lost on the next make api_docs
function canRename(program, document, lineIndex, character) {
    const symbol = symbolAt(program, document, lineIndex, character);

    if (!symbol) {
        return null;
    }

    if (symbol.kind === 'constant') {
        const constant = findConstant(program, symbol.name);

        if (!constant || /engine\.bs$/i.test(constant.document.path)) {
            return null;
        }
    }

    return { start: symbol.token.start, end: symbol.token.end, name: symbol.name };
}

module.exports = { definition, references, canRename };
