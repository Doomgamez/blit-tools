'use strict';

const language = require('../core/language');
const { signature } = require('../core/api');
const { collect, findFunction, findConstant, findLabel } = require('../core/program');
const { tokenAt, classify } = require('../core/cursor');

const code = (text) => `\`\`\`blitscript\n${text}\n\`\`\``;

function functionHover(program, name, api) {
    const script = findFunction(program, name);

    if (script) {
        return code(`FN ${script.name} ${script.params.map((param) => param.name).join(' ')}`.trim());
    }

    const native = api.functions[name.toLowerCase()];
    return native ? `${code(signature(native))}\n\n${native.doc}\n\n*${native.category}*` : null;
}

function constantHover(program, name, api) {
    const constant = findConstant(program, name) || api.constants[name];
    return constant ? code(`CONST ${constant.name} ${constant.value}`) : null;
}

function variableHover(program, name, line) {
    if (line.fn && line.fn.params.some((param) => param.name === name)) {
        return `Parameter of \`${line.fn.name}\``;
    }

    const write = collect(program, 'writes').find((item) => item.name === name);

    if (!write) {
        return null;
    }

    const source = write.document.lines[write.line].source.trim();
    return `${code(source)}\n\nFirst set on line ${write.line + 1}`;
}

function hover(program, document, lineIndex, character, api) {
    const found = tokenAt(document, lineIndex, character);

    if (!found) {
        return null;
    }

    const { line, index, token } = found;
    let contents = null;

    switch (classify(line.tokens, index)) {
        case 'keyword':
            contents = language.KEYWORDS[token.text.toUpperCase()] || null;
            break;

        case 'function':
            contents = functionHover(program, token.text, api);
            break;

        case 'constant':
            contents = constantHover(program, token.text, api);
            break;

        case 'label': {
            const label = findLabel(program, token.text);
            contents = label ? `Label on line ${label.line + 1}` : null;
            break;
        }

        case 'constructor':
            contents = `${token.text.toUpperCase()}: ${language.constructorArity(token.text)} values`;
            break;

        case 'register':
            contents = '32-bit register';
            break;

        case 'variable':
            contents = constantHover(program, token.text, api) || variableHover(program, token.text, line);
            break;

        default:
            break;
    }

    return contents ? { contents, start: token.start, end: token.end } : null;
}

module.exports = { hover };
