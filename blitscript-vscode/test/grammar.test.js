'use strict';

// node test/grammar.test.js <folder with vscode-textmate and vscode-oniguruma in node_modules>
// runs the grammar through the same engine VS Code uses

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const modules = path.join(process.argv[2], 'node_modules');
const textmate = require(path.join(modules, 'vscode-textmate'));
const oniguruma = require(path.join(modules, 'vscode-oniguruma'));

async function loadGrammar() {
    const wasm = fs.readFileSync(path.join(modules, 'vscode-oniguruma', 'release', 'onig.wasm')).buffer;

    await oniguruma.loadWASM(wasm);

    const registry = new textmate.Registry({
        onigLib: Promise.resolve({
            createOnigScanner: (patterns) => new oniguruma.OnigScanner(patterns),
            createOnigString: (text) => new oniguruma.OnigString(text),
        }),
        // VS Code has the C grammar, here a C block just stays uncolored
        loadGrammar: async (scope) => scope !== 'source.blitscript' ? null : textmate.parseRawGrammar(
            fs.readFileSync(path.join(__dirname, '..', 'syntaxes', 'blitscript.tmLanguage.json'), 'utf8'), 'blitscript.tmLanguage.json'),
    });

    return registry.loadGrammar('source.blitscript');
}

function scopeOf(grammar, line, word) {
    const start = line.indexOf(word);
    const { tokens } = grammar.tokenizeLine(line, textmate.INITIAL);
    const token = tokens.find((item) => start >= item.startIndex && start < item.endIndex);
    return token.scopes[token.scopes.length - 1];
}

const CASES = [
    ['// a comment', 'comment', 'comment.line'],
    ['SET a 1 // after', 'after', 'comment.line'],
    ['MAIN LBL start', 'MAIN', 'keyword.control'],
    ['MAIN LBL start', 'start', 'entity.name.function'],
    ['LBL frame', 'frame', 'entity.name.label'],
    ['    lbl frame', 'lbl', 'keyword.control'],
    ['JMP frame', 'JMP', 'keyword.control.flow'],
    ['    JNE skip', 'skip', 'entity.name.label'],
    ['FN greet name times', 'greet', 'entity.name.function'],
    ['FN greet name times', 'times', 'variable.parameter'],
    ['CONST SPEED 4.5', 'SPEED', 'variable.other.constant'],
    ['CALL DrawCube pos', 'DrawCube', 'entity.name.function'],
    ['CALL GetFPS -> fps', '->', 'keyword.operator.arrow'],
    ['SET speed 4.5', 'SET', 'keyword.other'],
    ['LOCAL speed 4.5', 'LOCAL', 'keyword.other'],
    ['LOCAL speed 4.5', 'speed', 'variable.other'],
    ['FOR i 0 10', 'FOR', 'keyword.control.flow'],
    ['FOR i 0 10', 'i', 'variable.other'],
    ['FOR i 0 10', '10', 'constant.numeric'],
    ['    FOREACH enemy enemies', 'FOREACH', 'keyword.control.flow'],
    ['    FOREACH enemy enemies', 'enemy', 'variable.other'],
    ['ENDFOR', 'ENDFOR', 'keyword.control.flow'],
    ['    BREAK', 'BREAK', 'keyword.control.flow'],
    ['    continue', 'continue', 'keyword.control.flow'],
    ['SET speed 4.5', 'speed', 'variable.other'],
    ['SET speed 4.5', '4.5', 'constant.numeric'],
    ['ADD a 1', 'ADD', 'keyword.operator.arithmetic'],
    ['SET a -1.5e3', '-1.5e3', 'constant.numeric'],
    ['SET a 42u64', '42u64', 'constant.numeric'],
    ['SET a 0xFF', '0xFF', 'constant.numeric.hex'],
    ['SET a true', 'true', 'constant.language'],
    ['MOV EAX 5', '5', 'constant.numeric'],
    ['ADD a EAX', 'EAX', 'variable.language.register'],
    ['SET v VEC3 0 1 0', 'VEC3', 'storage.type'],
    ['SET c COLOR 1 2 3 4', 'COLOR', 'storage.type'],
    ['CALL log RED', 'RED', 'variable.other.constant'],
    ['CALL log "text"', 'text', 'string.quoted'],
    ['CALL log "a\\nb"', '\\n', 'constant.character.escape'],
    ['CALL TextFormat "%.1f" a', '%.1f', 'constant.other.placeholder'],
    ['CALL log "// not a comment"', 'not', 'string.quoted'],
    ['INCLUDE "engine.bs"', 'INCLUDE', 'keyword.control.import'],
    ['INCLUDE "engine.bs"', 'engine', 'string.quoted'],
    ['END', 'END', 'keyword.control'],
    ['CLASS Boss EXTENDS Enemy', 'CLASS', 'storage.type.class'],
    ['CLASS Boss EXTENDS Enemy', 'Boss', 'entity.name.type.class'],
    ['CLASS Boss EXTENDS Enemy', 'EXTENDS', 'storage.modifier.extends'],
    ['CLASS Boss EXTENDS Enemy', 'Enemy', 'entity.other.inherited-class'],
    ['ENDCLASS', 'ENDCLASS', 'storage.type.class'],
    ['    FIELD health 100', 'FIELD', 'storage.modifier'],
    ['    FIELD health 100', 'health', 'variable.other.property'],
    ['    FIELD health 100', '100', 'constant.numeric'],
    ['    METHOD Hit amount', 'METHOD', 'storage.type.function'],
    ['    METHOD Hit amount', 'Hit', 'entity.name.function'],
    ['    METHOD Hit amount', 'amount', 'variable.parameter'],
    ['NEW Enemy 5 -> e', 'NEW', 'keyword.other.new'],
    ['NEW Enemy 5 -> e', 'Enemy', 'entity.name.type.class'],
    ['RETURN x', 'RETURN', 'keyword.control'],
    ['C', 'C', 'keyword.control'],
    ['C // some code', 'C', 'keyword.control'],
    ['SET this.health 5', 'this', 'variable.language.this'],
];

// a rule that only includes a grammar nobody has gets dropped, the block has to survive the C extension being off
function blockScopes(grammar) {
    let stack = textmate.INITIAL;

    return ['SET a 1', 'C', 'int x;', 'ENDC', 'SET b 2'].map((line) => {
        const { tokens, ruleStack } = grammar.tokenizeLine(line, stack);

        stack = ruleStack;
        return tokens[0].scopes[tokens[0].scopes.length - 1];
    });
}

loadGrammar().then((grammar) => {
    let failed = 0;

    const block = blockScopes(grammar);
    const wanted = ['keyword.other.blitscript', 'keyword.control.blitscript', 'source.c', 'keyword.control.blitscript', 'keyword.other.blitscript'];

    if (JSON.stringify(block) !== JSON.stringify(wanted)) {
        failed++;
        console.log(`FAIL a C block: got ${block.join(', ')}`);
    }

    for (const [line, word, scope] of CASES) {
        const found = scopeOf(grammar, line, word);

        if (!found.startsWith(scope)) {
            failed++;
            console.log(`FAIL ${line} [${word}]: wanted ${scope}, got ${found}`);
        }
    }

    console.log(`${CASES.length - failed} passed, ${failed} failed`);
    assert.strictEqual(failed, 0);
});
