'use strict';

// node test/core.test.js <engine repo>
// tests the language core with no editor, on made-up scripts and on the engine's real ones

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { tokenize } = require('../src/core/tokenizer');
const { parseDocument } = require('../src/core/document');
const { loadProgram } = require('../src/core/program');
const { parseCheatsheet, parseConstants } = require('../src/core/api');
const { lint } = require('../src/core/linter');
const { callArguments } = require('../src/core/syntax');
const { parseCompilerOutput } = require('../src/core/compilerOutput');
const { contextAt, classify } = require('../src/core/cursor');
const { complete } = require('../src/features/completion');
const { hover } = require('../src/features/hover');
const { signatureHelp } = require('../src/features/signature');
const { definition, references, canRename } = require('../src/features/navigation');
const { symbols, foldingRanges } = require('../src/features/outline');
const { colors, colorText } = require('../src/features/colors');
const { inlayHints } = require('../src/features/inlay');

const engine = process.argv[2];
const cheatsheet = fs.readFileSync(path.join(engine, 'docs', 'cheatsheet.txt'), 'utf8');
const header = fs.readFileSync(path.join(engine, 'blitscript', 'engine.bs'), 'utf8');
const api = { functions: parseCheatsheet(cheatsheet), constants: parseConstants(header) };

let passed = 0;
let failed = 0;

function test(name, body) {
    try {
        body();
        passed++;
    } catch (error) {
        failed++;
        console.log(`FAIL ${name}\n     ${error.message.split('\n').slice(0, 6).join('\n     ')}`);
    }
}

function memoryFiles(sources) {
    const paths = Object.keys(sources).map((name) => path.resolve('/virtual', name));

    return {
        read: (file) => (sources[path.basename(file)] === undefined ? null : sources[path.basename(file)]),
        siblings: () => paths,
    };
}

function open(source, extra) {
    const files = memoryFiles(Object.assign({ 'test.bs': source }, extra));
    const program = loadProgram(path.resolve('/virtual/test.bs'), files);
    return { program, model: program.documents.get(program.entry), files };
}

const lintText = (source, extra, options) => lint(open(source, extra).program, api, options);
const messages = (source, extra, options) => lintText(source, extra, options).map((item) => item.message);
const texts = (line) => tokenize(line).tokens.map((token) => token.text);

test('commas are whitespace', () => assert.deepStrictEqual(texts('CALL a 1,2, 3'), ['CALL', 'a', '1', '2', '3']));
test('a comment can start in the middle of a word', () => assert.deepStrictEqual(texts('SET a 1//x'), ['SET', 'a', '1']));
test('// in a string is not a comment', () => assert.deepStrictEqual(texts('CALL log "a//b" c'), ['CALL', 'log', 'a//b', 'c']));
test('an escaped quote stays in the string', () => assert.strictEqual(tokenize('SET a "x\\"y" z').tokens.length, 4));
test('an open string is noticed', () => assert.ok(tokenize('SET a "oops').unterminated));
test('token positions are right', () => assert.deepStrictEqual(tokenize('  JMP top').tokens.map((token) => [token.start, token.end]), [[2, 5], [6, 9]]));

test('the cheatsheet parses completely', () => {
    const declared = cheatsheet.split(/\r?\n/).filter((line) => /^ {4}\w+\s+\w+\(.*\);/.test(line)).length;
    assert.strictEqual(Object.keys(api.functions).length, declared);
    assert.ok(declared > 150);
});
test('constants parse', () => assert.strictEqual(api.constants.RED.value, 'COLOR 230 41 55 255'));
test('variadic parameters are recognized', () => assert.ok(api.functions.log.params[0].variadic));
test('function names ignore case in the table', () => assert.ok(api.functions.drawcube));
test('parameter types with two words survive', () => assert.strictEqual(api.functions.log.params[0].type, 'any'));

test('a constructor is one argument', () => {
    const args = callArguments(tokenize('CALL DrawCube VEC3 0 1 0, VEC3 1 1 1, RED -> x').tokens);
    assert.strictEqual(args.length, 3);
    assert.strictEqual(args[0].tokens.length, 4);
});

const CLEAN = 'INCLUDE "engine.bs"\nMAIN LBL main\nSET a 1\nLBL top\nCALL DrawCube VEC3 0 1 0, VEC3 1 1 1, RED\nSUB a 1\nJNE top\nEND\n';

test('a correct script is clean', () => assert.deepStrictEqual(messages(CLEAN, { 'engine.bs': header }), []));
test('unknown instruction', () => assert.ok(messages('MAIN LBL m\nFOO 1\nEND').some((m) => /Unknown instruction FOO/.test(m))));
test('keywords ignore case', () => assert.deepStrictEqual(messages('main lbl m\nset a 1\nadd a 1\nend'), []));
test('unknown label', () => assert.ok(messages('MAIN LBL m\nJMP nope\nEND').some((m) => /Unknown label nope/.test(m))));
test('labels ignore case', () => assert.deepStrictEqual(messages('MAIN LBL m\nLBL Top\nJMP TOP\nEND').filter((m) => /label/i.test(m)), []));
test('duplicate label', () => assert.ok(messages('MAIN LBL m\nLBL a\nLBL A\nEND').some((m) => /Duplicate label/.test(m))));
test('two MAINs', () => assert.ok(messages('MAIN LBL a\nMAIN LBL b\nEND').some((m) => /Only one MAIN/.test(m))));
test('RET outside a function', () => assert.ok(messages('MAIN LBL m\nRET\nEND').some((m) => /RET outside/.test(m))));
test('RET inside a function is fine', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL f 1\nEND\nFN f x\nRET\nEND').filter((m) => /RET/.test(m)), []));
test('nested FN', () => assert.ok(messages('MAIN LBL m\nEND\nFN a\nFN b\nEND').some((m) => /Nested FN/.test(m))));
test('missing include', () => assert.ok(messages('INCLUDE "gone.bs"\nMAIN LBL m\nEND').some((m) => /Can't find gone.bs/.test(m))));
test('open string', () => assert.ok(messages('MAIN LBL m\nCALL log "oops\nEND').some((m) => /not closed/.test(m))));
test('unknown native warns', () => assert.ok(messages('MAIN LBL m\nCALL Nope\nEND').some((m) => /Unknown function Nope/.test(m))));
test('custom natives are allowed', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL Mine\nEND', {}, { customNatives: ['mine'] }), []));
test('natives ignore case', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL waitframe\nEND'), []));
test('too few native arguments', () => assert.ok(messages('MAIN LBL m\nCALL DrawCube VEC3 0 0 0\nEND').some((m) => /takes 3 argument/.test(m))));
test('too many native arguments', () => assert.ok(messages('MAIN LBL m\nCALL WaitFrame 1\nEND').some((m) => /takes 0 argument/.test(m))));
test('variadic natives take any number', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL log 1 2 3 4 5\nEND'), []));
test('wrong script function arguments', () => assert.ok(messages('MAIN LBL m\nCALL f 1 2\nEND\nFN f x\nEND').some((m) => /f takes 1/.test(m))));
test('string given to a number', () => assert.ok(messages('MAIN LBL m\nCALL SetVSync "yes"\nEND').some((m) => /this is a string/.test(m))));
test('number given to a string', () => assert.ok(messages('MAIN LBL m\nCALL LoadModel 5 -> h\nEND').some((m) => /this is a number/.test(m))));
test('a result from a void function', () => assert.ok(messages('MAIN LBL m\nCALL WaitFrame -> x\nEND').some((m) => /returns nothing/.test(m))));
test('variable never set', () => assert.ok(messages('MAIN LBL m\nCALL log nope\nEND').some((m) => /nope is never set/.test(m))));
test('variables are case sensitive', () => assert.ok(messages('MAIN LBL m\nSET a 1\nCALL log A\nEND').some((m) => /A is never set/.test(m))));
test('a parameter counts as set', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL f 1\nEND\nFN f x\nCALL log x\nEND'), []));
test('a parameter is not set outside its function', () => assert.ok(messages('MAIN LBL m\nCALL f 1\nCALL log x\nEND\nFN f x\nEND').some((m) => /x is never set/.test(m))));
test('call results and READ count as set', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL GetFPS -> f\nSET l 1 2\nREAD s l 1\nCALL log f s\nEND'), []));
test('the check can be turned off', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL log nope\nEND', {}, { unknownVariables: false }), []));
test('exposed variables are allowed', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL log v\nEND', {}, { exposedVariables: ['v'] }), []));
test('unary minus is explained', () => assert.ok(messages('MAIN LBL m\nCONST K 1\nSET a 1\nMUL a -K\nEND').some((m) => /no unary minus/.test(m))));
test('negative numbers are numbers', () => assert.deepStrictEqual(messages('MAIN LBL m\nSET a -1.5e3\nMUL a -0x10\nEND'), []));
test('number suffixes', () => assert.deepStrictEqual(messages('MAIN LBL m\nSET a 1u\nSET b 2i64\nSET c 3u64\nSET d 2.5d\nSET e 3f\nEND'), []));
test('dead code is marked and only that', () => {
    const items = lintText('MAIN LBL m\nJMP a\nSET b 1\nLBL a\nSET c 1\nEND').filter((item) => item.unnecessary);
    assert.deepStrictEqual(items.map((item) => item.line), [2]);
});
test('code after END of a function is alive', () => assert.strictEqual(lintText('MAIN LBL m\nEND\nFN f\nRET\nEND\nFN g\nSET a 1\nEND').filter((item) => item.unnecessary).length, 0));
test('a file made to be included sees its includer', () => {
    const fragment = 'CALL log value\nJMP mainLoop\n';
    const main = 'MAIN LBL m\nSET value 1\nLBL mainLoop\nINCLUDE "test.bs"\nEND\n';
    assert.deepStrictEqual(messages(fragment, { 'game.bs': main }), []);
});
test('include cycles end', () => assert.ok(Array.isArray(messages('INCLUDE "other.bs"\nMAIN LBL m\nEND', { 'other.bs': 'INCLUDE "test.bs"\n' }))));

function labels(items) {
    return items.map((item) => item.label);
}

test('statement completion', () => {
    const { program, model, files } = open('\n');
    assert.ok(labels(complete(program, model, 0, 0, api, files)).includes('CALL'));
});
test('CALL completes natives and script functions', () => {
    const { program, model, files } = open('MAIN LBL m\nCALL \nEND\nFN mine\nEND');
    const items = labels(complete(program, model, 1, 5, api, files));
    assert.ok(items.includes('DrawCube') && items.includes('mine'));
});
test('native completion fills in parameters', () => {
    const { program, model, files } = open('MAIN LBL m\nCALL \nEND');
    const item = complete(program, model, 1, 5, api, files).find((entry) => entry.label === 'SetVSync');
    assert.strictEqual(item.snippet, 'SetVSync ${1:enabled}');
});
test('jumps complete labels', () => {
    const { program, model, files } = open('MAIN LBL m\nLBL top\nJMP \nEND');
    assert.deepStrictEqual(labels(complete(program, model, 2, 4, api, files)).sort(), ['m', 'top']);
});
test('completing a half typed word', () => assert.strictEqual(contextAt(open('MAIN LBL m\nJMP to\nEND').model, 1, 6).kind, 'label'));
test('arguments complete values', () => {
    const { program, model, files } = open('MAIN LBL m\nSET speed 1\nCALL log \nEND');
    const items = labels(complete(program, model, 2, 9, api, files));
    assert.ok(items.includes('speed') && items.includes('RED') && items.includes('VEC3') && items.includes('EAX'));
});
test('after -> comes a variable', () => assert.strictEqual(contextAt(open('MAIN LBL m\nCALL GetFPS -> \nEND').model, 1, 15).kind, 'variableName'));
test('no completion in comments', () => assert.strictEqual(contextAt(open('// hello\n').model, 0, 5).kind, 'none'));
test('no completion in strings', () => assert.strictEqual(contextAt(open('CALL log "hel"\n').model, 0, 12).kind, 'none'));
test('INCLUDE completes files', () => {
    const { program, model, files } = open('INCLUDE ""\n', { 'engine.bs': header });
    assert.deepStrictEqual(labels(complete(program, model, 0, 9, api, files)), ['engine.bs']);
});

test('hover on a native', () => {
    const { program, model } = open('MAIN LBL m\nCALL SetVSync true\nEND');
    assert.ok(/void SetVSync\(bool enabled\)/.test(hover(program, model, 1, 7, api).contents));
});
test('hover on a keyword', () => assert.ok(/CMP a b/.test(hover(open('CMP a b').program, open('CMP a b').model, 0, 1, api).contents)));
test('hover on a constant', () => {
    const { program, model } = open('INCLUDE "engine.bs"\nMAIN LBL m\nCALL log RED\nEND', { 'engine.bs': header });
    assert.ok(/CONST RED COLOR 230 41 55 255/.test(hover(program, model, 2, 10, api).contents));
});
test('hover on a variable shows where it was set', () => {
    const { program, model } = open('MAIN LBL m\nSET speed 4.5\nCALL log speed\nEND');
    assert.ok(/SET speed 4.5/.test(hover(program, model, 2, 11, api).contents));
});
test('hover on a parameter', () => {
    const { program, model } = open('MAIN LBL m\nEND\nFN f x\nCALL log x\nEND');
    assert.ok(/Parameter of `f`/.test(hover(program, model, 3, 9, api).contents));
});

test('signature help tracks the argument', () => {
    const { program, model } = open('MAIN LBL m\nCALL DrawCube VEC3 0 1 0, \nEND');
    const help = signatureHelp(program, model, 1, 26, api);
    assert.ok(/DrawCube/.test(help.label));
    assert.strictEqual(help.active, 1);
});
test('signature help stays on a half typed constructor', () => {
    const { program, model } = open('MAIN LBL m\nCALL DrawCube VEC3 0 1 0, VEC3 1\nEND');
    assert.strictEqual(signatureHelp(program, model, 1, 33, api).active, 1);
});
test('signature help moves on after a finished constructor', () => {
    const { program, model } = open('MAIN LBL m\nCALL DrawCube VEC3 0 1 0, VEC3 1 1 1, \nEND');
    assert.strictEqual(signatureHelp(program, model, 1, 38, api).active, 2);
});
test('signature help on the first argument', () => {
    const { program, model } = open('MAIN LBL m\nCALL DrawCube \nEND');
    assert.strictEqual(signatureHelp(program, model, 1, 14, api).active, 0);
});
test('signature help for script functions', () => {
    const { program, model } = open('MAIN LBL m\nCALL f 1 \nEND\nFN f a b\nEND');
    assert.strictEqual(signatureHelp(program, model, 1, 9, api).active, 1);
});

test('go to a label', () => {
    const { program, model } = open('MAIN LBL m\nLBL top\nJMP TOP\nEND');
    assert.strictEqual(definition(program, model, 2, 5).line, 1);
});
test('go to a function', () => {
    const { program, model } = open('MAIN LBL m\nCALL f\nEND\nFN f\nEND');
    assert.strictEqual(definition(program, model, 1, 5).line, 3);
});
test('go to a variable', () => {
    const { program, model } = open('MAIN LBL m\nSET a 1\nCALL log a\nEND');
    assert.strictEqual(definition(program, model, 2, 9).line, 1);
});
test('go to a parameter', () => {
    const { program, model } = open('MAIN LBL m\nEND\nFN f x\nCALL log x\nEND');
    assert.strictEqual(definition(program, model, 3, 9).line, 2);
});
test('go to a constant in an include', () => {
    const { program, model } = open('INCLUDE "engine.bs"\nMAIN LBL m\nCALL log RED\nEND', { 'engine.bs': header });
    assert.ok(/engine\.bs$/.test(definition(program, model, 2, 10).file));
});
test('go to an included file', () => {
    const { program, model } = open('INCLUDE "other.bs"\nMAIN LBL m\nEND', { 'other.bs': '// x\n' });
    assert.ok(/other\.bs$/.test(definition(program, model, 0, 11).file));
});
test('references of a label ignore case and skip strings', () => {
    const { program, model } = open('MAIN LBL m\nLBL top\nJMP TOP\nJE top\nCALL log "top"\nEND');
    assert.strictEqual(references(program, model, 1, 5).length, 3);
});
test('references of a variable are case sensitive', () => {
    const { program, model } = open('MAIN LBL m\nSET a 1\nSET A 2\nADD a 1\nCALL log a\nEND');
    assert.strictEqual(references(program, model, 1, 4).length, 3);
});
test('a parameter stays inside its function', () => {
    const { program, model } = open('MAIN LBL m\nSET x 1\nCALL f x\nEND\nFN f x\nCALL log x\nEND');
    assert.strictEqual(references(program, model, 5, 9).length, 2);
    assert.strictEqual(references(program, model, 1, 4).length, 2);
});
test('natives and keywords can not be renamed', () => {
    const { program, model } = open('MAIN LBL m\nCALL WaitFrame\nEND');
    assert.strictEqual(canRename(program, model, 1, 7), null);
    assert.strictEqual(canRename(program, model, 1, 1), null);
});
test('generated constants can not be renamed', () => {
    const { program, model } = open('INCLUDE "engine.bs"\nMAIN LBL m\nCALL log RED\nEND', { 'engine.bs': header });
    assert.strictEqual(canRename(program, model, 2, 10), null);
});
test('your own constants can be renamed', () => {
    const { program, model } = open('CONST SPEED 2\nMAIN LBL m\nSET a SPEED\nEND');
    assert.ok(canRename(program, model, 2, 8));
    assert.strictEqual(references(program, model, 2, 8).length, 2);
});

test('outline', () => {
    const items = symbols(parseDocument('CONST K 1\nMAIN LBL m\nLBL top\nEND\nFN f a\nEND', 't'));
    assert.deepStrictEqual(items.map((item) => [item.name, item.kind]), [['K', 'constant'], ['m', 'entry'], ['top', 'label'], ['f', 'function']]);
    assert.strictEqual(items[3].endLine, 5);
});
test('folding covers functions and comments', () => {
    const ranges = foldingRanges(parseDocument('// a\n// b\nFN f\nSET x 1\nEND\n', 't'));
    assert.deepStrictEqual(ranges.map((range) => [range.start, range.end, range.kind]).sort(), [[0, 1, 'comment'], [2, 4, 'region']]);
});
test('colors are found', () => {
    assert.deepStrictEqual(colors(parseDocument('CALL ClearBackground COLOR 24 26 32 255\n', 't')).map((c) => [c.r, c.g, c.b, c.a]), [[24, 26, 32, 255]]);
});
test('colors with variables are skipped', () => assert.strictEqual(colors(parseDocument('SET c COLOR r 26 32 255\n', 't')).length, 0));
test('out of range colors are skipped', () => assert.strictEqual(colors(parseDocument('SET c COLOR 300 26 32 255\n', 't')).length, 0));
test('color text', () => assert.strictEqual(colorText(1, 0, 0.5, 1), 'COLOR 255 0 128 255'));
test('inlay hints name the parameters', () => {
    const { program, model } = open('MAIN LBL m\nSET enabled true\nCALL DrawCube VEC3 0 0 0, VEC3 1 1 1, RED\nCALL SetVSync enabled\nEND');
    assert.deepStrictEqual(inlayHints(program, model, 0, 4, api).map((hint) => [hint.line, hint.label]),
        [[2, 'position:'], [2, 'size:'], [2, 'color:']]);
});

test('compiler errors: at file:line', () => assert.deepStrictEqual(parseCompilerOutput("Unknown label 'x' at C:/a b/c.bs:7")[0], { message: "Unknown label 'x'", file: 'C:/a b/c.bs', line: 6 }));
test('compiler errors: (file:line)', () => assert.deepStrictEqual(parseCompilerOutput('CMP expects: CMP <value>, <value> (main.bs:77)')[0], { message: 'CMP expects: CMP <value>, <value>', file: 'main.bs', line: 76 }));
test('compiler errors: at file:line: detail', () => {
    const [error] = parseCompilerOutput('Unknown instruction at main.bs:3: FOO');
    assert.strictEqual(error.file, 'main.bs');
    assert.strictEqual(error.line, 2);
});
test('compiler success lines are not errors', () => assert.deepStrictEqual(parseCompilerOutput('Compiled a.bs -> b.blit\nMain: 0x00000000\nCode size: 12 bytes'), []));

// the real scripts: a linter that cries wolf on working code gets turned off
function realFiles() {
    return {
        read: (file) => { try { return fs.readFileSync(file, 'utf8'); } catch { return null; } },
        siblings: (file) => fs.readdirSync(path.dirname(file)).filter((name) => name.endsWith('.bs')).map((name) => path.join(path.dirname(file), name)),
    };
}

for (const script of ['blitscript/main.bs', 'blitscript/hello.bs', 'blitscript/examples/2d.bs', 'blitscript/engine.bs']) {
    test(`real script is clean: ${script}`, () => {
        const items = lint(loadProgram(path.join(engine, script), realFiles()), api).filter((item) => !item.unnecessary);
        assert.deepStrictEqual(items.map((item) => `${item.line + 1}: ${item.message}`), []);
    });
}

test('classify agrees with the compiler on a real script', () => {
    const model = parseDocument(fs.readFileSync(path.join(engine, 'blitscript', 'main.bs'), 'utf8'), 'main.bs');
    for (const line of model.lines) {
        line.tokens.forEach((token, index) => assert.notStrictEqual(classify(line.tokens, index), 'unknown'));
    }
});

test('the bundled API matches the cheatsheet', () => {
    const bundled = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'data', 'api.json'), 'utf8'));
    assert.deepStrictEqual(Object.keys(bundled.functions).sort(), Object.keys(api.functions).sort());
});

console.log(`${passed} passed, ${failed} failed`);
process.exit(failed === 0 ? 0 : 1);
