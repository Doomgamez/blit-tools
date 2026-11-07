'use strict';

// the shorthand blitscript/blitmacros.c turns into plain instructions before compiling: expressions, assignments
// without SET, calls as lines, IF / ELSEIF / ELSE / ENDIF, WHILE / ENDWHILE, RETURN with an expression

const language = require('./language');

const BLOCK_WORDS = ['IF', 'ELSEIF', 'ELSE', 'ENDIF', 'WHILE', 'ENDWHILE'];
const LOGIC_WORDS = ['AND', 'OR', 'NOT', 'TRUE', 'FALSE', 'NULL'];
const SET_EXPRESSION = /^(\s*)(SET|LOCAL)(\s+)([A-Za-z_][A-Za-z0-9_.]*)\s*=(?!=)/i;
const ASSIGNMENT = /^(\s*)([A-Za-z_][A-Za-z0-9_.]*)\s*(\+\+|--|\+=|-=|\*=|\/=|%=|=(?!=))/;
const CALL_LINE = /^(\s*)([A-Za-z_][A-Za-z0-9_.]*)\s*\(/;

// the line without a // comment, keeping strings whole
function code(source) {
    let inString = false;

    for (let i = 0; i < source.length; i++) {
        if (source[i] === '"' && source[i - 1] !== '\\') {
            inString = !inString;
        } else if (!inString && source[i] === '/' && source[i + 1] === '/') {
            return source.slice(0, i);
        }
    }

    return source;
}

// what kind of macro line it is and where its expression starts, null for any other line
function classify(line) {
    const { tokens } = line;

    if (tokens.length === 0 || tokens[0].quoted) {
        return null;
    }

    const source = code(line.source);
    const word = tokens[0].text.toUpperCase();

    if (BLOCK_WORDS.includes(word)) {
        const elseIf = word === 'ELSE' && tokens[1] && tokens[1].text.toUpperCase() === 'IF';
        const after = elseIf ? tokens[1].end : tokens[0].end;
        return { kind: elseIf ? 'ELSEIF' : word, token: tokens[0], expression: after, source };
    }

    let match = SET_EXPRESSION.exec(source);

    if (match) {
        const start = match[1].length + match[2].length + match[3].length;
        return { kind: 'assign', target: { text: match[4], start, end: start + match[4].length }, expression: match[0].length, source };
    }

    if (word === 'RETURN' && tokens.length > 2) {
        return { kind: 'return', token: tokens[0], expression: tokens[0].end, source };
    }

    if (language.isKeyword(word)) {
        return null;
    }

    if ((match = ASSIGNMENT.exec(source))) {
        const start = match[1].length;
        return { kind: 'assign', target: { text: match[2], start, end: start + match[2].length }, expression: match[0].length, source };
    }

    if ((match = CALL_LINE.exec(source))) {
        return { kind: 'call', token: tokens[0], expression: match[1].length, source };
    }

    return null;
}

// the names an expression reads, and the functions it calls, with where they are in the line
function names(macro) {
    const reads = [];
    const calls = [];
    const text = macro.source;
    let i = macro.expression;

    while (i < text.length) {
        const c = text[i];

        if (c === '"') {
            i++;
            while (i < text.length && text[i] !== '"') {
                i += text[i] === '\\' ? 2 : 1;
            }
            i++;
        } else if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(text[i + 1] || ''))) {
            while (i < text.length && /[A-Za-z0-9_.]/.test(text[i])) {
                i++;
            }
        } else if (/[A-Za-z_]/.test(c)) {
            const start = i;
            while (i < text.length && /[A-Za-z0-9_.]/.test(text[i])) {
                i++;
            }

            const name = text.slice(start, i);
            const isCall = /^\s*\(/.test(text.slice(i));

            if (isCall) {
                calls.push({ text: name, start, end: i });
            } else if (!LOGIC_WORDS.includes(name.toUpperCase())) {
                reads.push({ text: name, start, end: i });
            }
        } else {
            i++;
        }
    }

    return { reads, calls };
}

// IF and WHILE are closed, in order with the loops around them, and not across the end of a function
function checkMacroBlocks(document, fail) {
    const open = [];
    let inFunction = false;

    document.lines.forEach((line, index) => {
        const { tokens } = line;

        if (tokens.length === 0 || tokens[0].quoted) {
            return;
        }

        const first = tokens[0];
        const word = first.text.toUpperCase();
        const macro = classify(line);
        const kind = macro && macro.kind;
        const top = open[open.length - 1];
        const closes = (what, by) => {
            if (!top || top.kind !== what) {
                fail(index, first, top ? `${by}, but the ${top.kind} on line ${top.line + 1} isn't closed yet` : `${by} without ${what}`);
                return false;
            }
            return true;
        };

        if (kind === 'IF' || kind === 'WHILE') {
            open.push({ kind, line: index, elseSeen: false });
        } else if (kind === 'ELSEIF' || kind === 'ELSE') {
            if (closes('IF', kind) && top.elseSeen) {
                fail(index, first, 'Nothing can come after ELSE but ENDIF');
            } else if (top && kind === 'ELSE') {
                top.elseSeen = true;
            }
        } else if (kind === 'ENDIF' || kind === 'ENDWHILE') {
            if (closes(kind === 'ENDIF' ? 'IF' : 'WHILE', kind)) {
                open.pop();
            }
        } else if (word === 'FOR' || word === 'FOREACH') {
            open.push({ kind: 'FOR', line: index });
        } else if (word === 'ENDFOR' && top) {
            if (top.kind === 'FOR') {
                open.pop();
            } else {
                fail(index, first, `ENDFOR, but the ${top.kind} on line ${top.line + 1} isn't closed yet`);
            }
        } else if (word === 'FN' || word === 'METHOD') {
            inFunction = true;
        } else if (word === 'END' && inFunction) {
            inFunction = false;
            const unclosed = open.filter((block) => block.kind !== 'FOR');
            unclosed.forEach((block) => fail(block.line, document.lines[block.line].tokens[0], `This ${block.kind} isn't closed before END`));
            open.length = 0;
        }
    });

    open.filter((block) => block.kind !== 'FOR')
        .forEach((block) => fail(block.line, document.lines[block.line].tokens[0], `This ${block.kind} has no END${block.kind}`));
}

module.exports = { classify, names, checkMacroBlocks, BLOCK_WORDS };
