'use strict';

const language = require('./language');
const { arrowIndex } = require('./document');

function tokenAt(document, lineIndex, character) {
    const line = document.lines[lineIndex];

    if (!line) {
        return null;
    }

    const index = line.tokens.findIndex((token) => character >= token.start && character <= token.end);
    return index >= 0 ? { line, index, token: line.tokens[index] } : null;
}

// what a token is, from where it stands in its statement
function classify(tokens, index) {
    const token = tokens[index];
    const command = tokens[0].text.toUpperCase();

    if (index === 0) return token.quoted ? 'unknown' : 'keyword';
    if (command === 'INCLUDE') return 'include';
    if (token.quoted) return 'string';
    if (command === 'LBL' || language.isJump(command)) return 'label';
    if (command === 'MAIN') return index === 2 ? 'label' : 'keyword';
    if (command === 'FN') return index === 1 ? 'function' : 'variable';
    if (command === 'METHOD') return index === 1 ? 'method' : 'variable';
    if (command === 'CLASS' || command === 'ENDCLASS') return 'class';
    if (command === 'FIELD' && index === 1) return 'field';
    if (command === 'NEW' && index === 1) return 'class';
    if (command === 'CALL' && index === 1 && token.text.includes('.')) return 'method';
    if (command === 'CONST') return index === 1 ? 'constant' : 'value';
    if (command === 'CALL' && index === 1) return 'function';
    if (token.text === '->') return 'arrow';
    if (language.isNumber(token.text) || language.isValueWord(token.text)) return 'value';
    if (language.isRegister(token.text)) return 'register';
    if (language.constructorArity(token.text) !== undefined) return 'constructor';
    return 'variable';
}

// what is being typed at the cursor, used for completion and signature help
function contextAt(document, lineIndex, character) {
    const line = document.lines[lineIndex];

    if (!line || (line.commentStart >= 0 && character > line.commentStart)) {
        return { kind: 'none' };
    }

    const before = line.tokens.filter((token) => token.end < character || (token.start < character && token.end >= character));
    const typing = before.length > 0 && before[before.length - 1].end >= character && !/[\s,]/.test(line.source[character - 1] || ' ');
    const finished = typing ? before.slice(0, -1) : before;
    const last = before[before.length - 1];

    if (last && last.quoted && character < last.end) {
        return { kind: before[0].text.toUpperCase() === 'INCLUDE' ? 'include' : 'none' };
    }

    if (finished.length === 0) {
        return { kind: 'statement' };
    }

    const command = finished[0].text.toUpperCase();

    if (['CLASS', 'ENDCLASS', 'METHOD'].includes(command) || (command === 'FIELD' && finished.length === 1)) return { kind: 'none' };
    if (command === 'NEW' && arrowIndex(finished) >= 0) return { kind: 'variableName' };
    if (command === 'CONST' || command === 'FN' || command === 'INCLUDE') return { kind: finished.length === 1 && command === 'INCLUDE' ? 'include' : 'none' };
    if (command === 'LBL' || (command === 'MAIN' && finished.length >= 2)) return { kind: 'none' };
    if (command === 'MAIN') return { kind: 'mainLabel' };
    if (language.isJump(command)) return { kind: finished.length === 1 ? 'label' : 'none' };
    if (command === 'CALL' && finished.length === 1) return { kind: 'function' };

    if (command === 'CALL' && arrowIndex(finished) >= 0) {
        return { kind: 'variableName' };
    }

    if (['SET', 'LOCAL', 'READ', 'FOR', 'FOREACH'].includes(command) && finished.length === 1) {
        return { kind: 'variableName' };
    }

    if (command === 'MOV' && finished.length === 1) {
        return { kind: 'register' };
    }

    return { kind: 'value', call: command === 'CALL' ? finished : null };
}

module.exports = { tokenAt, classify, contextAt };
