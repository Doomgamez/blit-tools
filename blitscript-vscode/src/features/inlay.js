'use strict';

const syntax = require('../core/syntax');
const { findFunction } = require('../core/program');

function parameters(program, name, api) {
    const script = findFunction(program, name);

    if (script) {
        return script;
    }

    return api.functions[name.toLowerCase()] || null;
}

// shows the parameter name in front of each argument of a CALL
function inlayHints(program, document, firstLine, lastLine, api) {
    const out = [];

    for (let index = firstLine; index <= lastLine && index < document.lines.length; index++) {
        const { tokens } = document.lines[index];

        if (tokens.length < 3 || tokens[0].text.toUpperCase() !== 'CALL') {
            continue;
        }

        const fn = parameters(program, tokens[1].text, api);

        if (!fn) {
            continue;
        }

        syntax.callArguments(tokens).forEach((arg, position) => {
            const param = syntax.parameterAt(fn, position);

            // a variable named like the parameter says it already
            if (param && arg.first.text !== param.name) {
                out.push({ line: index, character: arg.first.start, label: `${param.name}:` });
            }
        });
    }

    return out;
}

module.exports = { inlayHints };
