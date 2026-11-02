'use strict';

const language = require('./language');

// the same rules as blitscript/blitloops.c, where these stop the build
const MAX_DEPTH = 16;

const isName = (token) => !token.quoted && /^[A-Za-z_][A-Za-z0-9_]*$/.test(token.text);
const isZero = (token) => language.isNumber(token.text) && !/0x/i.test(token.text) && parseFloat(token.text) === 0;

function checkFor(tokens, at) {
    if (tokens.length < 4 || tokens.length > 5 || !isName(tokens[1])) {
        at(tokens[0], 'FOR expects: FOR variable start end [step]');
        return;
    }

    const text = tokens.slice(2).find((token) => token.quoted);

    if (text) {
        at(text, 'FOR needs numbers, not text');
    } else if (tokens[4] && isZero(tokens[4])) {
        at(tokens[4], 'A FOR step of 0 never ends');
    }
}

function checkForEach(tokens, at) {
    if (tokens.length < 3 || !isName(tokens[1])) {
        at(tokens[0], 'FOREACH expects: FOREACH variable values');
    }
}

function checkLoops(document, fail) {
    const open = [];
    let inFunction = false;

    document.lines.forEach((line, index) => {
        const { tokens } = line;

        if (tokens.length === 0 || tokens[0].quoted) {
            return;
        }

        const first = tokens[0];
        const command = first.text.toUpperCase();
        const at = (token, message) => fail(index, token, message);

        if (command === 'FOR' || command === 'FOREACH') {
            (command === 'FOR' ? checkFor : checkForEach)(tokens, at);

            if (open.length >= MAX_DEPTH) {
                at(first, `Loops can nest ${MAX_DEPTH} deep`);
            } else {
                open.push({ line: index, token: first });
            }
        } else if (command === 'ENDFOR') {
            if (tokens.length !== 1) {
                at(tokens[1], 'ENDFOR takes nothing after it');
            }

            if (open.length === 0) {
                at(first, 'ENDFOR without a FOR or FOREACH');
            }

            open.pop();
        } else if (command === 'BREAK' || command === 'CONTINUE') {
            if (tokens.length !== 1) {
                at(tokens[1], `${command} takes nothing after it`);
            } else if (open.length === 0) {
                at(first, `${command} outside a loop`);
            }
        } else if (command === 'FN') {
            if (open.length > 0) {
                at(first, 'FN inside a loop');
            }

            inFunction = true;
        } else if (command === 'END' && inFunction) {
            // in main END stops the script and can sit in a loop, here it closes the function
            if (open.length > 0) {
                at(first, `END before the ENDFOR of the loop on line ${open[open.length - 1].line + 1}`);
            }

            open.length = 0;
            inFunction = false;
        }
    });

    open.forEach((loop) => fail(loop.line, loop.token, 'This loop has no ENDFOR'));
}

module.exports = { checkLoops };
