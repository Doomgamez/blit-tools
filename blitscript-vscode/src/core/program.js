'use strict';

const path = require('path');
const { parseDocument, sameName } = require('./document');

function resolveInclude(fromFile, includePath) {
    return path.isAbsolute(includePath) ? path.normalize(includePath) : path.resolve(path.dirname(fromFile), includePath);
}

// files is the host: read(file) returns text or null, siblings(file) lists the .bs files next to it
function addFile(program, file, files) {
    const key = path.normalize(file);

    if (program.documents.has(key)) {
        return program.documents.get(key);
    }

    const text = files.read(key);

    if (text === null || text === undefined) {
        return null;
    }

    const document = parseDocument(text, key);

    program.documents.set(key, document);

    for (const include of document.includes) {
        include.resolved = resolveInclude(key, include.path);
        include.found = addFile(program, include.resolved, files) !== null;
    }

    return document;
}

// a file made to be included uses the labels and variables of whoever includes it, so
// sibling files that include this one count as part of its program
function addIncluders(program, entry, files) {
    for (const sibling of files.siblings(entry)) {
        const text = program.documents.has(sibling) ? null : files.read(sibling);

        if (text && /^\s*INCLUDE\b/im.test(text)) {
            const includes = parseDocument(text, sibling).includes
                .some((include) => resolveInclude(sibling, include.path) === entry);

            if (includes) {
                addFile(program, sibling, files);
            }
        }
    }
}

function loadProgram(entry, files) {
    const key = path.normalize(entry);
    const program = { entry: key, documents: new Map() };

    addFile(program, key, files);
    addIncluders(program, key, files);
    return program;
}

function collect(program, field) {
    const out = [];

    for (const document of program.documents.values()) {
        for (const item of document[field]) {
            out.push(Object.assign({ document }, item));
        }
    }

    return out;
}

const findLabel = (program, name) => collect(program, 'labels').find((label) => sameName(label.name, name));
const findFunction = (program, name) => collect(program, 'functions').find((fn) => sameName(fn.name, name));
const findConstant = (program, name) => collect(program, 'constants').find((constant) => constant.name === name);

module.exports = { loadProgram, collect, findLabel, findFunction, findConstant, resolveInclude };
