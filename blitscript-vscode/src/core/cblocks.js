'use strict';

// the lines between C and ENDC are C99, so they aren't BlitScript for anything here
// the same rules as blitscript/blitc.c: every function that isn't static can be called with CALL

const isOpening = (source) => /^\s*c\s*(\/\/.*)?$/i.test(source);
const isClosing = (source) => /^\s*endc\s*(\/\/.*)?$/i.test(source);

function findBlocks(sources) {
    const blocks = [];
    let open = null;

    sources.forEach((source, index) => {
        if (open) {
            if (isClosing(source)) {
                open.endLine = index;
                open = null;
            }
        } else if (isOpening(source)) {
            open = { line: index, endLine: -1 };
            blocks.push(open);
        }
    });

    return blocks;
}

function parameterNames(text) {
    const trimmed = text.trim();

    if (trimmed === '' || trimmed === 'void') {
        return [];
    }

    return trimmed.split(',').map((parameter) => {
        const words = parameter.match(/[A-Za-z_]\w*/g) || [];
        return words.length > 1 ? words[words.length - 1] : '';
    });
}

// the name's place in the document, so go to definition lands on it
function locate(sources, block, offset, length) {
    let line = block.line + 1;
    let start = offset;

    while (start > sources[line].length) {
        start -= sources[line].length + 1;
        line++;
    }

    return { line, token: { start, end: start + length, quoted: false } };
}

function addFunction(functions, head, headOffset, sources, block) {
    const match = /([A-Za-z_]\w*)\s*\(([^]*)\)\s*$/.exec(head);

    if (!match || /\b(static|inline)\b/.test(head.slice(0, match.index))) {
        return;
    }

    const { line, token } = locate(sources, block, headOffset + match.index, match[1].length);

    token.text = match[1];
    functions.push({
        name: match[1],
        token,
        params: parameterNames(match[2]).map((name) => ({ name, token })),
        line,
        endLine: line,
        native: true,
    });
}

function readFunctions(sources, block, functions) {
    const text = sources.slice(block.line + 1, block.endLine).join('\n');
    let depth = 0;
    let head = '';
    let headOffset = 0;

    const skipTo = (i, ending) => {
        const found = text.indexOf(ending, i);
        return found < 0 ? text.length : found;
    };

    for (let i = 0; i < text.length; i++) {
        const c = text[i];

        if (c === '/' && text[i + 1] === '/') {
            i = skipTo(i, '\n');
        } else if (c === '/' && text[i + 1] === '*') {
            i = skipTo(i + 2, '*/') + 1;
        } else if (c === '"' || c === "'") {
            for (i++; i < text.length && text[i] !== c && text[i] !== '\n'; i++) {
                if (text[i] === '\\') {
                    i++;
                }
            }
        } else if (c === '#') {
            while (i < text.length && text[i] !== '\n') {
                i += text[i] === '\\' ? 2 : 1;
            }
        } else if (c === '{') {
            if (depth === 0 && head.includes('(')) {
                addFunction(functions, head, headOffset, sources, block);
            }

            head = '';
            depth++;
        } else if (c === '}') {
            depth--;
            head = '';
        } else if (c === ';' && depth === 0) {
            head = '';
        } else if (depth === 0) {
            if (head === '') {
                headOffset = i;
            }

            head += c;
        }
    }
}

module.exports = { findBlocks, readFunctions };
