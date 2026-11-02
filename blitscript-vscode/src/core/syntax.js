'use strict';

const language = require('./language');
const { arrowIndex } = require('./document');

// groups the tokens after a CALL into logical arguments: VEC3 1 2 3 is one argument
function callArguments(tokens) {
    const arrow = arrowIndex(tokens);
    const last = arrow >= 0 ? arrow : tokens.length;
    const args = [];
    let i = 2;

    while (i < last) {
        const arity = tokens[i].quoted ? undefined : language.constructorArity(tokens[i].text);
        const size = arity === undefined ? 1 : Math.min(arity + 1, last - i);

        args.push({ first: tokens[i], last: tokens[i + size - 1], tokens: tokens.slice(i, i + size) });
        i += size;
    }

    return args;
}

function functionArguments(fn, args) {
    const variadic = fn.params.length > 0 && fn.params[fn.params.length - 1].variadic;
    const required = variadic ? fn.params.length - 1 : fn.params.length;

    return { required, variadic, tooFew: args.length < required, tooMany: !variadic && args.length > required };
}

// which parameter an argument feeds, the last one repeats for variadic functions
function parameterAt(fn, index) {
    if (index < fn.params.length) {
        return fn.params[index];
    }

    const last = fn.params[fn.params.length - 1];
    return last && last.variadic ? last : undefined;
}

// the plain words of a statement that read a value, not the ones that name something
function readTokens(tokens) {
    const command = tokens[0].text.toUpperCase();

    if (['SET', 'LOCAL', 'READ', 'FOR', 'FOREACH'].includes(command)) return tokens.slice(2);
    if (command === 'MOV') return tokens.slice(2);
    if (command === 'CMP' || language.isArithmetic(command)) return tokens.slice(1);

    if (command === 'CALL') {
        const arrow = arrowIndex(tokens);
        return tokens.slice(2, arrow >= 0 ? arrow : tokens.length);
    }

    return [];
}

function isLiteral(token) {
    return token.quoted || language.isNumber(token.text) || language.isValueWord(token.text) ||
           language.isRegister(token.text) || language.constructorArity(token.text) !== undefined;
}

module.exports = { callArguments, functionArguments, parameterAt, readTokens, isLiteral };
