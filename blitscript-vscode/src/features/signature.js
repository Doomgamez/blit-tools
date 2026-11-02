'use strict';

const language = require('../core/language');
const syntax = require('../core/syntax');
const { signature } = require('../core/api');
const { findFunction } = require('../core/program');
const { contextAt } = require('../core/cursor');

function describe(program, name, api) {
    const script = findFunction(program, name);

    if (script) {
        const params = script.params.map((param) => ({ label: param.name, name: param.name }));
        return { label: `${script.name}(${params.map((param) => param.label).join(', ')})`, params, source: { params: script.params }, doc: '' };
    }

    const native = api.functions[name.toLowerCase()];

    if (!native) {
        return null;
    }

    const params = native.params.map((param) => ({ label: `${param.type} ${param.name}${param.variadic ? '...' : ''}`, name: param.name }));
    return { label: signature(native), params, source: native, doc: native.doc };
}

function signatureHelp(program, document, lineIndex, character, api) {
    const context = contextAt(document, lineIndex, character);

    if (context.kind !== 'value' || !context.call || !context.call[1]) {
        return null;
    }

    const info = describe(program, context.call[1].text, api);

    if (!info) {
        return null;
    }

    const args = syntax.callArguments(context.call);
    const last = args[args.length - 1];

    // a VEC3 still waiting for its values is the argument being typed, not a finished one
    const open = last !== undefined && language.constructorArity(last.first.text) >= last.tokens.length;
    const position = open ? args.length - 1 : args.length;

    return { label: info.label, doc: info.doc, params: info.params, active: Math.min(position, Math.max(info.params.length - 1, 0)) };
}

module.exports = { signatureHelp };
