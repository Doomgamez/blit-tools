'use strict';

const { tokenize } = require('./tokenizer');
const { findBlocks, readFunctions } = require('./cblocks');

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

function addClass(model, tokens, index) {
    const extended = tokens.length === 4 ? tokens[3] : undefined;
    const item = {
        name: tokens[1] ? tokens[1].text : '',
        token: tokens[1] || tokens[0],
        parentName: extended ? extended.text : '',
        parentToken: extended,
        fields: [],
        methods: [],
        line: index,
        endLine: -1,
    };

    model.classes.push(item);
    return item;
}

// a method isn't called by its name alone, so it stays out of model.functions
function addMethod(tokens, index, owner) {
    const fn = {
        name: tokens[1] ? tokens[1].text : '',
        token: tokens[1] || tokens[0],
        params: [{ name: 'this', token: tokens[0] }, ...tokens.slice(2).map((token) => ({ name: token.text, token }))],
        parameters: Math.max(tokens.length - 2, 0),
        owner,
        line: index,
        endLine: -1,
    };

    if (owner) {
        owner.methods.push(fn);
    }

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

        case 'CLASS':
            state.owner = addClass(model, tokens, index);
            break;

        case 'ENDCLASS':
            if (state.owner) {
                state.owner.endLine = index;
                state.owner = null;
            }
            break;

        case 'FIELD':
            if (state.owner && tokens[1]) {
                state.owner.fields.push({ name: tokens[1].text, token: tokens[1], line: index });
            }
            break;

        case 'METHOD':
            state.fn = addMethod(tokens, index, state.owner);
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

        case 'FOR':
        case 'FOREACH': {
            const loop = { line: index, endLine: -1 };
            model.loops.push(loop);
            state.loops.push(loop);

            if (tokens[1]) {
                write(tokens[1]);
            }
            break;
        }

        case 'ENDFOR':
            if (state.loops.length > 0) {
                state.loops.pop().endLine = index;
            }
            break;

        case 'SET':
        case 'LOCAL':
        case 'READ':
            if (tokens[1]) {
                write(tokens[1]);
            }
            break;

        case 'CALL':
        case 'NEW':
        case 'NEWOF': {
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
    const model = { path, text, lines: [], labels: [], mains: [], functions: [], constants: [], includes: [], writes: [], loops: [], classes: [], cBlocks: [] };
    const state = { fn: null, loops: [], owner: null };
    const sources = text.split(/\r?\n/);

    model.cBlocks = findBlocks(sources);

    const insideC = (index) => model.cBlocks.some((block) => index >= block.line && (block.endLine < 0 || index <= block.endLine));

    model.cBlocks.filter((block) => block.endLine >= 0).forEach((block) => readFunctions(sources, block, model.functions));

    sources.forEach((source, index) => {
        const c = insideC(index);
        const line = Object.assign(tokenize(c ? '' : source), { source, fn: state.fn, c });

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
