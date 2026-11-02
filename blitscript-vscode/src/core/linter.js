'use strict';

const language = require('./language');
const syntax = require('./syntax');
const { signature } = require('./api');
const { collect, findFunction, findConstant, findLabel } = require('./program');
const { arrowIndex } = require('./document');
const { checkLoops } = require('./loops');

const ERROR = 0;
const WARNING = 1;
const HINT = 3;

const NUMERIC = ['int', 'float', 'double', 'bool'];
const AUDIO_LOADERS = ['loadsound', 'loadmusic'];
const ASSET_LOADERS = [...AUDIO_LOADERS, 'loadtexture', 'loadmodel', 'loadscene'];
const AUDIO_FORMATS = ['wav', 'ogg', 'mp3', 'flac', 'opus', 'mod', 'xm', 's3m', 'it', 'mid', 'midi'];

function report(out, line, token, severity, message, extra) {
    out.push(Object.assign({ line, start: token.start, end: token.end, severity, message }, extra));
}

function checkDuplicates(items, describe, out, document) {
    const seen = new Map();

    for (const item of items) {
        const key = item.name.toLowerCase();

        if (seen.has(key) && item.document === document) {
            report(out, item.line, item.token, ERROR, describe(item));
        }

        seen.set(key, item);
    }
}

function checkArgumentTypes(fn, args, line, out) {
    args.forEach((arg, index) => {
        const param = syntax.parameterAt(fn, index);

        if (!param || arg.tokens.length !== 1) {
            return;
        }

        const token = arg.first;
        const type = param.type;

        if (token.quoted && (NUMERIC.includes(type) || /^(vec|color|rect)/.test(type))) {
            report(out, line, token, WARNING, `${fn.name}: ${param.name} is a ${type}, this is a string`);
        } else if (type === 'string' && language.isNumber(token.text)) {
            report(out, line, token, WARNING, `${fn.name}: ${param.name} is a string, this is a number`);
        }
    });
}

// these fail when the game runs, in a place that's easy to miss
function checkAssetArgument(native, args, line, options, out) {
    const token = args.length > 0 ? args[0].first : null;

    if (!ASSET_LOADERS.includes(native.name.toLowerCase()) || !token || !token.quoted) {
        return;
    }

    const name = token.text;
    const extension = name.includes('.') ? name.slice(name.lastIndexOf('.') + 1).toLowerCase() : '';

    if (/[\\/]/.test(name)) {
        report(out, line, token, WARNING, `The archive keeps file names only: use "${name.split(/[\\/]/).pop()}"`);
    } else if (AUDIO_LOADERS.includes(native.name.toLowerCase()) && !AUDIO_FORMATS.includes(extension)) {
        report(out, line, token, WARNING, `The engine can't play .${extension || '(no extension)'}: use ${AUDIO_FORMATS.slice(0, 4).join(', ')} or another format it knows`);
    } else if (options.assets && !options.assets.includes(name.toLowerCase())) {
        report(out, line, token, WARNING, `${name} is not in assets/, it will fail to load`);
    }
}

function checkCall(tokens, index, program, api, options, out) {
    const name = tokens[1];

    if (!name) {
        return;
    }

    const args = syntax.callArguments(tokens);
    const script = findFunction(program, name.text);

    if (script) {
        if (args.length !== script.params.length) {
            report(out, index, name, ERROR, `${script.name} takes ${script.params.length} argument(s), got ${args.length}`);
        }
        return;
    }

    const native = api.functions[name.text.toLowerCase()];

    if (!native) {
        if (!options.customNatives.some((custom) => custom.toLowerCase() === name.text.toLowerCase())) {
            report(out, index, name, WARNING, `Unknown function ${name.text}: the script stops here when it runs`,
                { code: 'unknown-function' });
        }
        return;
    }

    const count = syntax.functionArguments(native, args);

    if (count.tooFew || count.tooMany) {
        const wanted = count.variadic ? `at least ${count.required}` : `${count.required}`;
        report(out, index, name, ERROR, `${native.name} takes ${wanted} argument(s), got ${args.length}: ${signature(native)}`);
    }

    checkArgumentTypes(native, args, index, out);
    checkAssetArgument(native, args, index, options, out);

    if (arrowIndex(tokens) >= 0 && native.returns === 'void') {
        report(out, index, name, WARNING, `${native.name} returns nothing, the result is always null`);
    }
}

function isKnownVariable(name, line, program, api, written, options) {
    return written.has(name) || findConstant(program, name) || api.constants[name] ||
           (line.fn && line.fn.params.some((param) => param.name === name)) ||
           options.exposedVariables.includes(name);
}

function checkReads(line, index, program, api, written, options, out) {
    for (const token of syntax.readTokens(line.tokens)) {
        if (syntax.isLiteral(token) || isKnownVariable(token.text, line, program, api, written, options)) {
            continue;
        }

        const negated = token.text.startsWith('-') && isKnownVariable(token.text.slice(1), line, program, api, written, options);

        report(out, index, token, negated ? ERROR : WARNING, negated
            ? `BlitScript has no unary minus: ${token.text} is read as a variable name`
            : `${token.text} is never set: the script stops here when it runs`);
    }
}

function checkStatement(line, index, document, program, api, written, options, out) {
    const { tokens } = line;
    const first = tokens[0];
    const command = first.text.toUpperCase();

    if (line.unterminated) {
        report(out, index, tokens[tokens.length - 1], ERROR, 'This string is not closed');
    }

    if (first.quoted || !language.isKeyword(command)) {
        report(out, index, first, ERROR, `Unknown instruction ${first.text}`);
        return;
    }

    if (language.isJump(command)) {
        if (tokens.length !== 2) {
            report(out, index, first, ERROR, `${command} takes one label`);
        } else if (!findLabel(program, tokens[1].text)) {
            report(out, index, tokens[1], ERROR, `Unknown label ${tokens[1].text}`);
        }
    } else if (command === 'RET' && !line.fn) {
        report(out, index, first, ERROR, 'RET outside a function');
    } else if (command === 'CALL') {
        checkCall(tokens, index, program, api, options, out);
    }

    if (options.unknownVariables) {
        checkReads(line, index, program, api, written, options, out);
    }
}

// the lines the optimizer would remove, the same rule as RemoveDeadCode in blitopt.c
function checkDeadCode(document, out) {
    let dead = false;

    document.lines.forEach((line, index) => {
        if (line.tokens.length === 0) {
            return;
        }

        const command = line.tokens[0].text.toUpperCase();

        if (['LBL', 'MAIN', 'FN', 'END', 'CONST', 'INCLUDE', 'FOR', 'FOREACH', 'ENDFOR'].includes(command)) {
            dead = false;
        } else if (dead) {
            out.push({ line: index, start: line.tokens[0].start, end: line.source.length, severity: HINT,
                       message: 'Nothing can reach this line', unnecessary: true });
        } else if (['JMP', 'RET', 'BREAK', 'CONTINUE'].includes(command)) {
            dead = true;
        }
    });
}

function lint(program, api, options) {
    const settings = Object.assign({ customNatives: [], exposedVariables: [], unknownVariables: true }, options);
    const document = program.documents.get(program.entry);
    const out = [];

    if (!document) {
        return out;
    }

    const written = new Set(collect(program, 'writes').map((write) => write.name));

    document.lines.forEach((line, index) => {
        if (line.tokens.length > 0) {
            checkStatement(line, index, document, program, api, written, settings, out);
        }
    });

    document.lines.forEach((line, index) => {
        if (line.nested) {
            report(out, index, line.tokens[0], ERROR, 'Nested FN is not allowed');
        }
    });

    checkLoops(document, (index, token, message) => report(out, index, token, ERROR, message));

    for (const include of document.includes) {
        if (!include.found) {
            report(out, include.line, include.token, ERROR, `Can't find ${include.path}`);
        }
    }

    checkDuplicates(collect(program, 'labels'), (label) => `Duplicate label ${label.name}`, out, document);
    checkDuplicates(collect(program, 'functions'), (fn) => `Duplicate function ${fn.name}`, out, document);
    checkDuplicates(collect(program, 'constants').filter((constant) => constant.document === document),
        (constant) => `Duplicate constant ${constant.name}`, out, document);

    if (collect(program, 'mains').length > 1) {
        document.mains.forEach((main) => report(out, main.line, main.token, ERROR, 'Only one MAIN per program'));
    }

    checkDeadCode(document, out);
    return out.sort((a, b) => a.line - b.line || a.start - b.start);
}

module.exports = { lint, ERROR, WARNING, HINT };
