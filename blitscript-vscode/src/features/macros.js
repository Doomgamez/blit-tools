'use strict';

const { collect } = require('../core/program');

const A = '${1:a}';
const B = '${2:b}';

// IF ... ENDIF, the compiler turns it into CMP and a jump (blitscript/blitmacros.c)
function conditional(label, detail, operator) {
    return {
        label,
        detail,
        labels: [],
        body: () => [`IF ${A} ${operator} ${B}`, '    $0', 'ENDIF'],
    };
}

const MACROS = [
    conditional('if', 'if a == b', '=='),
    conditional('ifne', 'if a != b', '!='),
    conditional('iflt', 'if a < b', '<'),
    conditional('ifle', 'if a <= b', '<='),
    conditional('ifgt', 'if a > b', '>'),
    conditional('ifge', 'if a >= b', '>='),
    {
        label: 'ifelse',
        detail: 'if a == b, otherwise',
        labels: [],
        body: () => [`IF ${A} == ${B}`, '    $3', 'ELSE', '    $0', 'ENDIF'],
    },
    {
        label: 'while',
        detail: 'while a < b',
        labels: [],
        body: () => [`WHILE ${A} < ${B}`, '    $0', 'ENDWHILE'],
    },
    {
        label: 'for',
        detail: 'for i from 0 up to count',
        labels: [],
        body: () => ['FOR ${1:i} 0 ${2:count}', '    $0', 'ENDFOR'],
    },
    {
        label: 'foreach',
        detail: 'for every value of an array or a list',
        labels: [],
        body: () => ['FOREACH ${1:value} ${2:values}', '    $0', 'ENDFOR'],
    },
    {
        label: 'class',
        detail: 'a class with a field and an Init',
        labels: [],
        body: () => ['CLASS ${1:Name}', '    FIELD ${2:position} ${3:VEC3 0 0 0}', '', '    METHOD Init', '        $0', '    END', 'ENDCLASS'],
    },
    {
        label: 'cblock',
        detail: 'C99 between C and ENDC, its functions work with CALL',
        labels: [],
        body: () => ['C', 'int ${1:Name}(int ${2:value})', '{', '    $0', '}', 'ENDC'],
    },
    {
        // nothing in the shorthand runs a block before checking, so this one is a label and a jump
        label: 'repeat',
        detail: 'run a block count times, at least once',
        labels: ['loop'],
        body: ([loop]) => ['${1:count} = ${2:10}', `LBL ${loop}`, '    $0', '    SUB $1 1', `    JNE ${loop}`],
    },
    {
        label: 'ifkey',
        detail: 'if a key is held down',
        labels: [],
        body: () => ['IF IsKeyDown(${1:KEY_SPACE})', '    $0', 'ENDIF'],
    },
    {
        label: 'ifpressed',
        detail: 'if a key went down this frame',
        labels: [],
        body: () => ['IF IsKeyPressed(${1:KEY_SPACE})', '    $0', 'ENDIF'],
    },
    {
        label: 'nextscene',
        detail: 'go to the next scene, back to the first after the last',
        labels: [],
        body: () => ['ChangeScene((GetCurrentScene() + 1) % GetSceneCount())$0'],
    },
];

// a macro can be used any number of times, so its labels can't be fixed names
function freeLabel(base, taken) {
    let name = base;

    for (let number = 2; taken.has(name.toLowerCase()); number++) {
        name = `${base}${number}`;
    }

    taken.add(name.toLowerCase());
    return name;
}

function preview(snippet) {
    return snippet.replace(/\$\{\d+:([^}]*)\}/g, '$1').replace(/\$\d+/g, (match) => (match === '$1' ? 'i' : '...'));
}

function macroItems(program) {
    const used = collect(program, 'labels').map((label) => label.name.toLowerCase());

    return MACROS.map((macro) => {
        const taken = new Set(used);
        const snippet = macro.body(macro.labels.map((label) => freeLabel(label, taken))).join('\n');

        return {
            label: macro.label,
            kind: 'snippet',
            detail: macro.detail,
            documentation: `\`\`\`blitscript\n${preview(snippet)}\n\`\`\``,
            snippet,
            sortText: `0${macro.label}`,
        };
    });
}

module.exports = { macroItems };
