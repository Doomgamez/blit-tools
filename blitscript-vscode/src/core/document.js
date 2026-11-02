'use strict';

const { tokenize } = require('./tokenizer');

const sameName = (a, b) => a.toLowerCase() === b.toLowerCase();

function arrowIndex(tokens) {
    return tokens.findIndex((token) => !token.quoted && token.text === '->');
}

function addFunction(model, line, tokens, index) {
    const fn = {
        name: tokens[1] ? tokens[1].text : '',
        token: tokens[1] || tokens[0],
        params: tokens.slice(2).map((token) => ({ name: token.text, token })),
        line: index,
        endLine: -1,
    };

    model.functions.push(fn);
    return fn;
}

function addStatement(model, line, index, state) {
    const { tokens } = line;
    const command = tokens[0].text.toUpperCase();
    const write = (token) => model.writes.push({ name: token.text, token, line: index, fn: state.fn });

    switch (command) {
        case 'MAIN':
            if (tokens[2]) {
                const entry = { name: tokens[2].text, token: tokens[2], line: index };
                model.labels.push(entry);
                model.mains.push(entry);
            }
            break;

        case 'LBL':
            if (tokens[1]) {
                model.labels.push({ name: tokens[1].text, token: tokens[1], line: index });
            }
            break;

        case 'FN':
            state.fn = addFunction(model, line, tokens, index);
            break;

        case 'END':
            if (state.fn) {
                state.fn.endLine = index;
                state.fn = null;
            }
            break;

        case 'CONST':
            if (tokens[1]) {
                const value = tokens.slice(2).map((token) => (token.quoted ? `"${token.text}"` : token.text)).join(' ');
                model.constants.push({ name: tokens[1].text, token: tokens[1], value, line: index });
            }
            break;

        case 'INCLUDE':
            if (tokens[1]) {
                model.includes.push({ path: tokens[1].text, token: tokens[1], line: index });
            }
            break;

        case 'SET':
        case 'READ':
            if (tokens[1]) {
                write(tokens[1]);
            }
            break;

        case 'CALL': {
            const arrow = arrowIndex(tokens);
            if (arrow >= 0 && tokens[arrow + 1]) {
                write(tokens[arrow + 1]);
            }
            break;
        }

        default:
            break;
    }
}

// one source file, no includes followed yet
function parseDocument(text, path) {
    const model = { path, text, lines: [], labels: [], mains: [], functions: [], constants: [], includes: [], writes: [] };
    const state = { fn: null };

    text.split(/\r?\n/).forEach((source, index) => {
        const line = Object.assign(tokenize(source), { source, fn: state.fn });

        model.lines.push(line);

        if (line.tokens.length > 0) {
            line.nested = line.tokens[0].text.toUpperCase() === 'FN' && state.fn !== null;
            addStatement(model, line, index, state);
            line.fn = state.fn || line.fn;
        }
    });

    return model;
}

module.exports = { parseDocument, arrowIndex, sameName };
