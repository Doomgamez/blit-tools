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
        loadGrammar: async () => textmate.parseRawGrammar(
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
];

loadGrammar().then((grammar) => {
    let failed = 0;

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
