'use strict';

const { collect } = require('./program');
const { sameName, arrowIndex } = require('./document');
const { callArguments, isDotted } = require('./syntax');

const NAME = /^[A-Za-z_][A-Za-z0-9_]*$/;
const MAX_CHAIN = 64;

// the same rules as blitscript/blitclass.c, where these stop the build

function parentOf(classes, item) {
    return item.parentName ? classes.find((other) => sameName(other.name, item.parentName)) : undefined;
}

function hasField(classes, item, name) {
    for (let current = item, steps = 0; current && steps < MAX_CHAIN; current = parentOf(classes, current), steps++) {
        if (current.fields.some((field) => field.name === name)) {
            return true;
        }
    }

    return false;
}

function findMethod(classes, item, name, parameters) {
    for (let current = item, steps = 0; current && steps < MAX_CHAIN; current = parentOf(classes, current), steps++) {
        const method = current.methods.find((candidate) => sameName(candidate.name, name) &&
            (parameters === undefined || candidate.parameters === parameters));

        if (method) {
            return method;
        }
    }

    return undefined;
}

function checkStructure(document, fail) {
    let inClass = false;
    let inMethod = false;
    let inFunction = false;

    document.lines.forEach((line, index) => {
        const { tokens } = line;

        if (tokens.length === 0 || tokens[0].quoted) {
            return;
        }

        const first = tokens[0];
        const command = first.text.toUpperCase();
        const at = (message, token = first) => fail(index, token, message);

        if (command === 'ENDC') {
            at('ENDC without a C');
        } else if (command === 'CLASS') {
            const extended = tokens.length === 4 && tokens[2].text.toUpperCase() === 'EXTENDS' && NAME.test(tokens[3].text);

            if (inClass) at('CLASS inside a CLASS');
            else if (inFunction) at('CLASS inside a function');
            else if ((tokens.length !== 2 && !extended) || !NAME.test(tokens[1].text)) at('CLASS expects: CLASS Name [EXTENDS Parent]');

            inClass = true;
        } else if (command === 'ENDCLASS') {
            if (!inClass) at('ENDCLASS without a CLASS');
            else if (inMethod) at('ENDCLASS before the END of a method');
            else if (tokens.length !== 1) at('ENDCLASS takes nothing after it', tokens[1]);

            inClass = false;
        } else if (command === 'FIELD') {
            if (!inClass || inMethod) at('FIELD outside a CLASS');
            else if (tokens.length < 2 || tokens[1].quoted || !NAME.test(tokens[1].text)) at('FIELD expects: FIELD name [value]');
        } else if (command === 'METHOD') {
            if (!inClass) at('METHOD outside a CLASS');
            else if (inMethod) at('METHOD inside a method');
            else if (tokens.length < 2 || tokens[1].quoted || !NAME.test(tokens[1].text)) at('METHOD expects: METHOD name [parameters]');

            const bad = tokens.slice(2).find((token) => token.quoted || !NAME.test(token.text) || token.text.toLowerCase() === 'this');

            if (bad) at(`${bad.text} can't be a parameter`, bad);

            inMethod = true;
            inFunction = true;
        } else if (command === 'FN') {
            if (inClass) at('FN inside a CLASS, use METHOD');

            inFunction = true;
        } else if (command === 'END' && inFunction) {
            inFunction = false;
            inMethod = false;
        } else if (inClass && !inMethod) {
            at('Only FIELD and METHOD go in a CLASS');
        } else if (command === 'RETURN' && !inFunction) {
            at('RETURN outside a function');
        }
    });

    if (inClass) {
        const item = document.classes[document.classes.length - 1];

        if (item) {
            fail(item.line, item.token, `CLASS ${item.name} has no ENDCLASS`);
        }
    }
}

function checkDeclarations(document, classes, fail) {
    for (const item of document.classes) {
        const parent = parentOf(classes, item);
        const seenFields = new Set();
        const seenMethods = new Set();
        const others = classes.filter((other) => sameName(other.name, item.name));

        if (others.length > 1 && others[0] !== item) {
            fail(item.line, item.token, `There are two classes called ${item.name}`);
        }

        if (item.parentName && !parent) {
            fail(item.line, item.parentToken, `${item.name} extends ${item.parentName}, there's no such class`);
        }

        let steps = 0;

        for (let current = item; current; current = parentOf(classes, current)) {
            if (++steps > classes.length) {
                fail(item.line, item.token, `${item.name} extends itself`);
                break;
            }
        }

        for (const field of item.fields) {
            if (seenFields.has(field.name)) {
                fail(field.line, field.token, `${item.name} already has a field called ${field.name}`);
            } else if (parent && steps <= classes.length && hasField(classes, parent, field.name)) {
                fail(field.line, field.token, `${item.name} repeats the field ${field.name} of a class it extends`);
            }

            seenFields.add(field.name);
        }

        for (const method of item.methods) {
            const key = method.name.toLowerCase();

            if (seenMethods.has(key)) {
                fail(method.line, method.token, `${item.name} already has a method called ${method.name}`);
            }

            seenMethods.add(key);
        }
    }
}

function checkNew(tokens, index, classes, fail) {
    const arrow = arrowIndex(tokens);

    if (tokens.length < 2 || arrow < 0 || !tokens[arrow + 1] || tokens[arrow + 2]) {
        fail(index, tokens[0], 'NEW expects: NEW Class [values] -> variable');
        return;
    }

    const item = classes.find((candidate) => sameName(candidate.name, tokens[1].text));

    if (!item) {
        fail(index, tokens[1], `There's no class called ${tokens[1].text}`);
        return;
    }

    const init = findMethod(classes, item, 'Init');
    const wanted = init ? init.parameters : 0;
    const given = callArguments(tokens).length;

    if (wanted !== given) {
        fail(index, tokens[1], `${item.name} takes ${wanted} values when it's made, this gives ${given}`);
    }
}

function checkMethodCall(tokens, index, line, classes, fail) {
    const name = tokens[1];
    const parts = name.text.split('.');
    const method = parts[parts.length - 1];
    const count = callArguments(tokens).length;
    const owner = line.fn && line.fn.owner;

    if (parts[0].toLowerCase() === 'super') {
        const parent = owner ? parentOf(classes, owner) : undefined;

        if (!parent) {
            fail(index, name, 'super is for a method of a class that extends another');
        } else if (parts.length !== 2 || !findMethod(classes, parent, method, count)) {
            fail(index, name, `${parent.name} has no method ${method} with ${count} parameters to call`);
        }

        return;
    }

    if (!classes.some((item) => findMethod(classes, item, method, count))) {
        fail(index, name, `No class has a method ${method} with ${count} parameters`);
    }
}

function checkFields(tokens, line, index, classes, fail) {
    const owner = line.fn && line.fn.owner;

    if (!owner) {
        return;
    }

    const isCall = tokens[0].text.toUpperCase() === 'CALL';

    tokens.slice(1).forEach((token, position) => {
        if (!isDotted(token) || (isCall && position === 0 && token.text.split('.').length === 2)) {
            return;
        }

        const parts = token.text.split('.');

        if (parts[0] === 'this' && !hasField(classes, owner, parts[1])) {
            fail(index, token, `${owner.name} has no field ${parts[1]}`);
        }
    });
}

function checkClasses(program, document, fail) {
    const classes = collect(program, 'classes');

    checkStructure(document, fail);
    checkDeclarations(document, classes, fail);

    for (const block of document.cBlocks) {
        if (block.endLine < 0) {
            fail(block.line, { start: 0, end: document.lines[block.line].source.length }, 'This C block has no ENDC');
        }
    }

    document.lines.forEach((line, index) => {
        const { tokens } = line;

        if (tokens.length === 0 || tokens[0].quoted) {
            return;
        }

        const command = tokens[0].text.toUpperCase();

        if (command === 'NEW') {
            checkNew(tokens, index, classes, fail);
        } else if (command === 'CALL' && tokens[1] && isDotted(tokens[1])) {
            checkMethodCall(tokens, index, line, classes, fail);
        } else if (['LOCAL', 'FOR', 'FOREACH'].includes(command) && tokens[1] && isDotted(tokens[1])) {
            fail(index, tokens[1], `${command} needs a plain variable, not ${tokens[1].text}`);
        }

        checkFields(tokens, line, index, classes, fail);
    });
}

module.exports = { checkClasses };
