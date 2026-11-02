'use strict';

const { collect } = require('../core/program');

const A = '${1:a}';
const B = '${2:b}';

// BlitScript jumps over a block, so each condition uses the opposite jump: if a < b skips with JGE
function conditional(label, detail, skipJump) {
    return {
        label,
        detail,
        labels: ['endif'],
        body: ([end]) => [`CMP ${A} ${B}`, `${skipJump} ${end}`, '    $0', `LBL ${end}`],
    };
}

const MACROS = [
    conditional('if', 'if a == b', 'JNE'),
    conditional('ifne', 'if a != b', 'JE'),
    conditional('iflt', 'if a < b', 'JGE'),
    conditional('ifle', 'if a <= b', 'JG'),
    conditional('ifgt', 'if a > b', 'JLE'),
    conditional('ifge', 'if a >= b', 'JL'),
    {
        label: 'ifelse',
        detail: 'if a == b, otherwise',
        labels: ['otherwise', 'endif'],
        body: ([otherwise, end]) => [`CMP ${A} ${B}`, `JNE ${otherwise}`, '    $3', `    JMP ${end}`, `LBL ${otherwise}`, '    $0', `LBL ${end}`],
    },
    {
        label: 'while',
        detail: 'while a < b',
        labels: ['loop', 'endloop'],
        body: ([loop, end]) => [`LBL ${loop}`, `    CMP ${A} ${B}`, `    JGE ${end}`, '    $0', `    JMP ${loop}`, `LBL ${end}`],
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
        label: 'repeat',
        detail: 'run a block count times, at least once',
        labels: ['loop'],
        body: ([loop]) => ['SET ${1:count} ${2:10}', `LBL ${loop}`, '    $0', '    SUB $1 1', `    JNE ${loop}`],
    },
    {
        label: 'ifkey',
        detail: 'if a key is held down',
        labels: ['endif'],
        body: ([end]) => ['CALL IsKeyDown ${1:KEY_SPACE} -> ${2:down}', 'CMP $2 true', `JNE ${end}`, '    $0', `LBL ${end}`],
    },
    {
        label: 'ifpressed',
        detail: 'if a key went down this frame',
        labels: ['endif'],
        body: ([end]) => ['CALL IsKeyPressed ${1:KEY_SPACE} -> ${2:pressed}', 'CMP $2 true', `JNE ${end}`, '    $0', `LBL ${end}`],
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
