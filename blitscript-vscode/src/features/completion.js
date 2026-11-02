'use strict';

const language = require('../core/language');
const { collect } = require('../core/program');
const { contextAt } = require('../core/cursor');
const { signature } = require('../core/api');

function keywordItems() {
    return Object.keys(language.KEYWORDS).map((name) => ({
        label: name, kind: 'keyword', detail: 'instruction', documentation: language.KEYWORDS[name],
    }));
}

function nativeSnippet(fn) {
    const params = fn.params.filter((param) => !param.variadic);
    return `${fn.name}${params.map((param, index) => ` \${${index + 1}:${param.name}}`).join('')}`;
}

function functionItems(program, api) {
    const natives = Object.values(api.functions).map((fn) => ({
        label: fn.name, kind: 'native', detail: signature(fn), documentation: fn.doc, snippet: nativeSnippet(fn),
    }));
    const scripts = collect(program, 'functions').map((fn) => ({
        label: fn.name, kind: 'function', detail: `FN ${fn.name} ${fn.params.map((param) => param.name).join(' ')}`,
        snippet: `${fn.name}${fn.params.map((param, index) => ` \${${index + 1}:${param.name}}`).join('')}`,
        sortText: `0${fn.name}`,
    }));

    return scripts.concat(natives);
}

function labelItems(program) {
    return collect(program, 'labels').map((label) => ({ label: label.name, kind: 'label', detail: 'label' }));
}

function variableItems(program, line) {
    const names = new Set(collect(program, 'writes').map((write) => write.name));

    if (line.fn) {
        line.fn.params.forEach((param) => names.add(param.name));
    }

    return [...names].map((name) => ({ label: name, kind: 'variable', detail: 'variable', sortText: `0${name}` }));
}

function valueItems(program, line, api) {
    const constants = new Map();

    for (const constant of Object.values(api.constants)) {
        constants.set(constant.name, constant.value);
    }

    for (const constant of collect(program, 'constants')) {
        constants.set(constant.name, constant.value);
    }

    const items = [...constants].map(([name, value]) => ({ label: name, kind: 'constant', detail: value, sortText: `1${name}` }));
    const constructors = Object.keys(language.CONSTRUCTORS).map((name) => ({
        label: name, kind: 'constructor', detail: `${language.CONSTRUCTORS[name]} values`, sortText: `2${name}`,
    }));
    const registers = language.REGISTERS.map((name) => ({ label: name, kind: 'register', detail: 'register', sortText: `3${name}` }));
    const words = language.VALUE_WORDS.map((name) => ({ label: name, kind: 'keyword', detail: 'value', sortText: `2${name}` }));

    return variableItems(program, line).concat(items, constructors, registers, words);
}

function includeItems(document, files) {
    return files.siblings(document.path)
        .filter((file) => file !== document.path)
        .map((file) => ({ label: require('path').basename(file), kind: 'file', detail: 'BlitScript file' }));
}

function complete(program, document, lineIndex, character, api, files) {
    const context = contextAt(document, lineIndex, character);
    const line = document.lines[lineIndex];

    switch (context.kind) {
        case 'statement': return keywordItems();
        case 'function': return functionItems(program, api);
        case 'label': return labelItems(program);
        case 'include': return includeItems(document, files);
        case 'variableName': return variableItems(program, line);
        case 'register': return language.REGISTERS.map((name) => ({ label: name, kind: 'register', detail: 'register' }));
        case 'value': return valueItems(program, line, api);
        default: return [];
    }
}

module.exports = { complete, nativeSnippet };
