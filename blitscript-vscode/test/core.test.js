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
const loading = (line, options) => messages(`MAIN LBL m\n${line}\nEND`, {}, options);

test('a known audio file is fine', () => assert.deepStrictEqual(loading('CALL LoadSound "door.ogg" -> s', { assets: ['door.ogg'] }), []));
test('asset names ignore case', () => assert.deepStrictEqual(loading('CALL LoadMusic "Theme.MP3" -> m', { assets: ['theme.mp3'] }), []));
test('an audio file missing from assets', () => assert.ok(loading('CALL LoadSound "door.ogg" -> s', { assets: ['other.ogg'] }).some((m) => /not in assets/.test(m))));
test('outside a project assets are not checked', () => assert.deepStrictEqual(loading('CALL LoadSound "door.ogg" -> s'), []));
test('an unsupported audio format', () => assert.ok(loading('CALL LoadSound "door.aac" -> s').some((m) => /can't play \.aac/.test(m))));
test('a folder in an asset name', () => assert.ok(loading('CALL LoadMusic "music/theme.ogg" -> m').some((m) => /use "theme.ogg"/.test(m))));
test('textures are checked too', () => assert.ok(loading('CALL LoadTexture "a.png" -> t', { assets: [] }).some((m) => /not in assets/.test(m))));
test('shaders are checked too', () => assert.ok(loading('CALL LoadShader "a.frag" -> s', { assets: [] }).some((m) => /not in assets/.test(m))));
test('a shader that exists is fine', () => assert.deepStrictEqual(loading('CALL LoadShader "A.frag" -> s', { assets: ['a.frag'] }), []));
test('an image name is fine for a texture', () => assert.deepStrictEqual(loading('CALL LoadTexture "a.png" -> t', { assets: ['a.png'] }), []));
test('a variable file name is not judged', () => assert.deepStrictEqual(loading('SET n "x"\nCALL LoadSound n -> s', { assets: [] }), []));
test('every audio function is in the API', () => {
    ['loadsound', 'playsound', 'playsoundat', 'loopsound', 'loadmusic', 'playmusic', 'fadeoutmusic', 'setmastervolume', 'setaudiolistener']
        .forEach((name) => assert.ok(api.functions[name], name));
});

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

const macro = (source, name) => {
    const { program, model, files } = open(source);
    return complete(program, model, source.split('\n').length - 1, 0, api, files).find((item) => item.label === name);
};

test('macros come first at the start of a line', () => {
    const { program, model, files } = open('\n');
    const names = complete(program, model, 0, 0, api, files).sort((a, b) => (a.sortText || a.label).localeCompare(b.sortText || b.label)).map((item) => item.label);
    assert.ok(names.indexOf('if') < names.indexOf('ADD'));
});
test('a macro inserts real BlitScript', () => assert.ok(/^CMP .*\nJNE endif\n[\s\S]*LBL endif$/.test(macro('\n', 'if').snippet)));
test('a macro avoids a label in use', () => assert.ok(/JNE endif2/.test(macro('MAIN LBL m\nLBL endif\nEND\n', 'if').snippet)));
test('a macro avoids labels in any case', () => assert.ok(/JNE endif2/.test(macro('MAIN LBL m\nLBL ENDIF\nEND\n', 'if').snippet)));
test('a macro skips every used number', () => assert.ok(/JNE endif3/.test(macro('MAIN LBL m\nLBL endif\nLBL endif2\nEND\n', 'if').snippet)));
test('a macro sees labels in included files', () => {
    const files = memoryFiles({ 'test.bs': 'INCLUDE "other.bs"\n', 'other.bs': 'LBL endif\n' });
    const program = loadProgram(path.resolve('/virtual/test.bs'), files);
    const item = complete(program, program.documents.get(program.entry), 1, 0, api, files).find((entry) => entry.label === 'if');
    assert.ok(/JNE endif2/.test(item.snippet));
});
test('macros are only offered for statements', () => {
    const { program, model, files } = open('MAIN LBL m\nCALL \nEND');
    assert.ok(!complete(program, model, 1, 5, api, files).some((item) => item.kind === 'snippet'));
});
test('the preview shows plain code', () => {
    const { documentation } = macro('\n', 'for');
    assert.ok(/FOR i 0 count/.test(documentation) && /ENDFOR/.test(documentation) && !documentation.includes('${'));
});

const loopErrors = (body) => messages(`MAIN LBL m\nSET n 3\n${body}\nEND`);
test('a whole loop is fine', () => assert.deepStrictEqual(loopErrors('FOR i 0 n\nFOREACH v 1 2\nCONTINUE\nENDFOR\nBREAK\nENDFOR'), []));
test('the keywords work in any case', () => assert.deepStrictEqual(loopErrors('for i 0 n\nbreak\nendfor'), []));
test('a loop variable counts as set', () => assert.deepStrictEqual(loopErrors('FOR i 0 n\nCALL log i\nENDFOR\nFOREACH v 1 2\nCALL log v\nENDFOR'), []));
test('LOCAL sets a variable', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL f\nEND\nFN f\nLOCAL a 1\nCALL log a\nEND'), []));
test('a FOR reads its values', () => assert.ok(loopErrors('FOR i 0 nope\nENDFOR').some((m) => /nope is never set/.test(m))));
test('a FOREACH reads its values', () => assert.ok(loopErrors('FOREACH v nope\nENDFOR').some((m) => /nope is never set/.test(m))));
test('a loop without ENDFOR', () => assert.ok(loopErrors('FOR i 0 n').includes('This loop has no ENDFOR')));
test('ENDFOR without a loop', () => assert.ok(loopErrors('ENDFOR').includes('ENDFOR without a FOR or FOREACH')));
test('BREAK outside a loop', () => assert.ok(loopErrors('BREAK').includes('BREAK outside a loop')));
test('CONTINUE outside a loop', () => assert.ok(loopErrors('CONTINUE').includes('CONTINUE outside a loop')));
test('a label is not a loop for BREAK', () => assert.ok(loopErrors('LBL a\nBREAK\nJMP a').includes('BREAK outside a loop')));
test('ENDFOR takes nothing', () => assert.ok(loopErrors('FOR i 0 n\nENDFOR x').includes('ENDFOR takes nothing after it')));
test('FOR needs an end', () => assert.ok(loopErrors('FOR i 3\nENDFOR').some((m) => /FOR expects/.test(m))));
test('FOR can have a step', () => assert.deepStrictEqual(loopErrors('FOR i 0 n 2\nENDFOR'), []));
test('FOR has at most a step', () => assert.ok(loopErrors('FOR i 0 n 1 9\nENDFOR').some((m) => /FOR expects/.test(m))));
test('a number is no loop variable', () => assert.ok(loopErrors('FOR 5 0 n\nENDFOR').some((m) => /FOR expects/.test(m))));
test('FOR wants numbers', () => assert.ok(loopErrors('FOR i "a" n\nENDFOR').includes('FOR needs numbers, not text')));
test('a step of 0', () => assert.ok(loopErrors('FOR i 0 n 0.0\nENDFOR').includes('A FOR step of 0 never ends')));
test('a hex step is not 0', () => assert.deepStrictEqual(loopErrors('FOR i 0 n 0x10\nENDFOR'), []));
test('a variable step is fine', () => assert.deepStrictEqual(loopErrors('FOR i 0 n n\nENDFOR'), []));
test('FOREACH needs values', () => assert.ok(loopErrors('FOREACH v\nENDFOR').some((m) => /FOREACH expects/.test(m))));
test('loops nest 16 deep', () => {
    const nested = (count) => `${'FOR i 0 n\n'.repeat(count)}${'ENDFOR\n'.repeat(count)}`;
    assert.deepStrictEqual(loopErrors(nested(16)), []);
    assert.ok(loopErrors(nested(17)).includes('Loops can nest 16 deep'));
});
test('no FN inside a loop', () => assert.ok(messages('MAIN LBL m\nFOR i 0 3\nFN f\nEND\nENDFOR\nEND').includes('FN inside a loop')));
test('a function can not end inside a loop', () => {
    const found = messages('MAIN LBL m\nEND\nFN f\nFOR i 0 3\nEND\nFN g\nENDFOR\nEND');
    assert.ok(found.includes('END before the ENDFOR of the loop on line 4'));
    assert.ok(found.includes('ENDFOR without a FOR or FOREACH'));
});
test('END can sit in a loop in main', () => assert.deepStrictEqual(loopErrors('FOR i 0 n\nEND\nENDFOR'), []));
const ENEMY = 'CLASS Enemy\nFIELD health 100\nFIELD position VEC3 0 0 0\nMETHOD Init hp\nSET this.health hp\nEND\nMETHOD Hit amount\nSUB this.health amount\nRETURN this.health\nEND\nENDCLASS\n';
const BOSS = 'CLASS Boss EXTENDS Enemy\nFIELD armor 5\nMETHOD Init\nSET this.armor 9\nCALL super.Hit 1\nEND\nENDCLASS\n';
const inMain = (body, classes = ENEMY + BOSS) => messages(`MAIN LBL m\n${body}\nEND\n${classes}`);
test('a class is fine', () => assert.deepStrictEqual(inMain('NEW Enemy 5 -> e\nCALL e.Hit 1\nCALL e.Hit 1 -> left\nCALL log e.health left'), []));
test('fields and parts of vectors read and store', () => assert.deepStrictEqual(inMain('NEW Enemy 5 -> e\nSET e.position.y 3\nADD e.health 1\nCMP e.position.x 2\nCALL Vector3Add e.position e.position -> e.position'), []));
test('a variable holding a vector has parts', () => assert.deepStrictEqual(messages('MAIN LBL m\nSET v VEC3 1 2 3\nSET v.x 5\nCALL log v.y\nEND'), []));
test('a field of something never set', () => assert.ok(inMain('CALL log nope.health').some((m) => /nope is never set/.test(m))));
test('storing into a field of something never set', () => assert.ok(inMain('SET nope.health 1').some((m) => /nope is never set/.test(m))));
test('a method of something never set', () => assert.ok(inMain('CALL nope.Hit 1').some((m) => /nope is never set/.test(m))));
test('NEW sets its variable', () => assert.ok(!inMain('NEW Boss -> b\nCALL log b').some((m) => /never set/.test(m))));
test('NEW reads its values', () => assert.ok(inMain('NEW Enemy nope -> e').some((m) => /nope is never set/.test(m))));
test('RETURN reads its value', () => assert.ok(messages('MAIN LBL m\nEND\nFN f\nRETURN nope\nEND').some((m) => /nope is never set/.test(m))));
test('this is known in a method', () => assert.deepStrictEqual(inMain('NEW Boss -> b'), []));
test('an unknown class', () => assert.ok(inMain('NEW Zombie -> z').includes("There's no class called Zombie")));
test('NEW without a variable', () => assert.ok(inMain('NEW Boss').some((m) => /NEW expects/.test(m))));
test('NEW counts the values of Init', () => assert.ok(inMain('NEW Enemy -> e').includes("Enemy takes 1 values when it's made, this gives 0")));
test('a VEC3 is one value for NEW', () => assert.deepStrictEqual(inMain('NEW P VEC3 1 2 3 -> p', 'CLASS P\nMETHOD Init at\nEND\nENDCLASS\n'), []));
test('Init is inherited', () => assert.ok(inMain('NEW Kid 5 -> k', 'CLASS P\nMETHOD Init v\nEND\nENDCLASS\nCLASS Kid EXTENDS P\nENDCLASS\n').length === 0));
test('NEWOF with a class name in a variable', () => assert.deepStrictEqual(inMain('SET kind "Enemy"\nNEWOF kind 5 -> e\nCALL log e'), []));
test('NEWOF sets its variable and reads its class', () => assert.ok(inMain('NEWOF nope -> e').some((m) => /nope is never set/.test(m))));
test('NEWOF without a variable', () => assert.ok(inMain('NEWOF "Enemy" 5').some((m) => /NEWOF expects/.test(m))));
test('NEWOF with a count no class takes', () => assert.ok(inMain('NEWOF "Enemy" 1 2 3 -> e').some((m) => /No class takes 3 values/.test(m))));
test('NEWOF without classes', () => assert.ok(inMain('NEWOF "Enemy" -> e', '').some((m) => /has none/.test(m))));
test('a method no class has', () => assert.ok(inMain('NEW Boss -> b\nCALL b.Nope').includes('No class has a method Nope with 0 parameters')));
test('a method with the wrong count', () => assert.ok(inMain('NEW Boss -> b\nCALL b.Hit').includes('No class has a method Hit with 0 parameters')));
test('methods ignore case', () => assert.deepStrictEqual(inMain('NEW Boss -> b\nCALL b.hit 1'), []));
test('this. checks fields', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nFIELD a\nMETHOD f\nCALL log this.b\nEND\nENDCLASS').includes('A has no field b')));
test('this. sees inherited fields', () => assert.deepStrictEqual(inMain('', `${ENEMY}CLASS Kid EXTENDS Enemy\nMETHOD f\nCALL log this.health\nEND\nENDCLASS\n`), []));
test('fields are case sensitive', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nFIELD a\nMETHOD f\nCALL log this.A\nEND\nENDCLASS').includes('A has no field A')));
test('super needs a parent', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nMETHOD f\nCALL super.f\nEND\nENDCLASS').some((m) => /super is for/.test(m))));
test('super needs the method', () => assert.ok(inMain('', `${ENEMY}CLASS Kid EXTENDS Enemy\nMETHOD f\nCALL super.Nope\nEND\nENDCLASS\n`).some((m) => /Enemy has no method Nope/.test(m))));
test('a class without ENDCLASS', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nFIELD a').includes('CLASS A has no ENDCLASS')));
test('ENDCLASS alone', () => assert.ok(messages('MAIN LBL m\nEND\nENDCLASS').includes('ENDCLASS without a CLASS')));
test('ENDCLASS in a method', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nMETHOD f\nENDCLASS').includes('ENDCLASS before the END of a method')));
test('FIELD outside', () => assert.ok(messages('MAIN LBL m\nEND\nFIELD a').includes('FIELD outside a CLASS')));
test('METHOD outside', () => assert.ok(messages('MAIN LBL m\nEND\nMETHOD a\nEND').includes('METHOD outside a CLASS')));
test('code in a class', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nSET a 1\nENDCLASS').includes('Only FIELD and METHOD go in a CLASS')));
test('FN in a class', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nFN f\nEND\nENDCLASS').includes('FN inside a CLASS, use METHOD')));
test('two classes', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A\nENDCLASS\nCLASS a\nENDCLASS').includes('There are two classes called a')));
test('an unknown parent', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A EXTENDS Z\nENDCLASS').some((m) => /A extends Z/.test(m))));
test('a class extending itself', () => assert.ok(messages('MAIN LBL m\nEND\nCLASS A EXTENDS B\nENDCLASS\nCLASS B EXTENDS A\nENDCLASS').some((m) => /extends itself/.test(m))));
test('a repeated field', () => assert.ok(inMain('', 'CLASS A\nFIELD a\nFIELD a\nENDCLASS\n').includes('A already has a field called a')));
test('a field of a parent repeated', () => assert.ok(inMain('', `${ENEMY}CLASS Kid EXTENDS Enemy\nFIELD health\nENDCLASS\n`).some((m) => /repeats the field health/.test(m))));
test('a repeated method', () => assert.ok(inMain('', 'CLASS A\nMETHOD f\nEND\nMETHOD F\nEND\nENDCLASS\n').includes('A already has a method called F')));
test('this is no parameter', () => assert.ok(inMain('', 'CLASS A\nMETHOD f this\nEND\nENDCLASS\n').some((m) => /this can't be a parameter/.test(m))));
test('RETURN outside a function', () => assert.ok(messages('MAIN LBL m\nRETURN 1\nEND').includes('RETURN outside a function')));
test('RETURN in a function is fine', () => assert.deepStrictEqual(messages('MAIN LBL m\nCALL f -> x\nCALL log x\nEND\nFN f\nRETURN 1\nEND'), []));
test('a field is no loop variable', () => assert.ok(inMain('NEW Boss -> b\nFOR b.armor 0 3\nENDFOR').some((m) => /needs a plain variable/.test(m))));
test('FOREACH over a field is fine', () => assert.deepStrictEqual(inMain('NEW Boss -> b\nFOREACH v b.armor\nCALL log v\nENDFOR'), []));
test('a loop in a method', () => assert.deepStrictEqual(inMain('', 'CLASS A\nMETHOD f\nFOR i 0 3\nENDFOR\nEND\nENDCLASS\n'), []));
test('an open loop in a method', () => assert.ok(inMain('', 'CLASS A\nMETHOD f\nFOR i 0 3\nEND\nENDCLASS\n').some((m) => /END before the ENDFOR/.test(m))));
test('classes in an include', () => {
    const items = messages('INCLUDE "c.bs"\nMAIN LBL m\nNEW Enemy 1 -> e\nCALL e.Hit 2\nEND', { 'c.bs': ENEMY });
    assert.deepStrictEqual(items, []);
});
test('code after RETURN is dead', () => assert.deepStrictEqual(lintText('MAIN LBL m\nEND\nFN f\nRETURN 1\nSET a 1\nEND').filter((item) => item.unnecessary).map((item) => item.line), [4]));
test('classes and methods are in the outline', () => {
    const names = symbols(open(`MAIN LBL m\nEND\n${ENEMY}`).model).map((item) => item.name);
    assert.ok(names.includes('Enemy') && names.includes('Enemy.Hit'));
});
test('classes and methods fold', () => {
    const ranges = foldingRanges(open(`MAIN LBL m\nEND\n${ENEMY}`).model).map((range) => range.start);
    assert.ok(ranges.includes(2) && ranges.includes(5));
});
test('hover on a class keyword', () => {
    const { program, model } = open('NEW Enemy -> e');
    assert.ok(/NEW Class/.test(hover(program, model, 0, 1, api).contents));
});
test('the class macro', () => assert.ok(/CLASS Name/.test(macro('\n', 'class').documentation)));
test('a variable name after NEW ->', () => {
    const { model } = open('NEW Enemy -> ');
    assert.strictEqual(contextAt(model, 0, 13).kind, 'variableName');
});

const C_BLOCK = 'C\n#include <stdio.h>\n// not { a function ( \nstatic int helper(int x) { return x; }\nconst char* text = "a { (";\nint Add(int a, int b)\n{\n    if (a) { return helper(b); }\n    return a + b;\n}\n/* int Commented(int a) { } */\ntypedef struct P { int a; } P;\nvoid None(void) { }\nVec3 Twice(Vec3 v, const char* name) { return v; }\nENDC\n';
test('C is not read as BlitScript', () => assert.deepStrictEqual(messages(`MAIN LBL m\nCALL Add 1 2 -> x\nCALL log x\nEND\n${C_BLOCK}`), []));
test('a function from a C block counts its arguments', () => assert.ok(messages(`MAIN LBL m\nCALL Add 1\nEND\n${C_BLOCK}`).includes('Add takes 2 argument(s), got 1')));
test('void means no arguments', () => assert.ok(messages(`MAIN LBL m\nCALL None 1\nEND\n${C_BLOCK}`).includes('None takes 0 argument(s), got 1')));
test('a VEC3 is one C argument', () => assert.deepStrictEqual(messages(`MAIN LBL m\nCALL Twice VEC3 1 2 3, "a"\nEND\n${C_BLOCK}`), []));
test('static C functions stay in C', () => assert.ok(messages(`MAIN LBL m\nCALL helper 1\nEND\n${C_BLOCK}`).some((m) => /Unknown function helper/.test(m))));
test('a commented C function does not exist', () => assert.ok(messages(`MAIN LBL m\nCALL Commented 1\nEND\n${C_BLOCK}`).some((m) => /Unknown function Commented/.test(m))));
test('C blocks work in an include', () => assert.deepStrictEqual(messages('INCLUDE "c.bs"\nMAIN LBL m\nCALL Add 1 2\nEND', { 'c.bs': C_BLOCK }), []));
test('a lower case block', () => assert.deepStrictEqual(messages('MAIN LBL m\nEND\nc // note\nint a(void) { return 1; }\nendc'), []));
test('a C block without ENDC', () => assert.ok(messages('MAIN LBL m\nEND\nC\nint a(void);').includes('This C block has no ENDC')));
test('ENDC alone', () => assert.ok(messages('MAIN LBL m\nEND\nENDC').includes('ENDC without a C')));
test('go to a C function', () => {
    const source = `MAIN LBL m\nCALL Add 1 2\nEND\n${C_BLOCK}`;
    const { program, model } = open(source);
    const found = definition(program, model, 1, 6, api);
    assert.strictEqual(found.line, source.split('\n').findIndex((line) => line.startsWith('int Add')));
    assert.strictEqual(found.start, 4);
});
test('C blocks fold', () => assert.ok(foldingRanges(open(`MAIN LBL m\nEND\n${C_BLOCK}`).model).some((range) => range.start === 2)));
test('nothing completes inside C', () => assert.strictEqual(contextAt(open(`${C_BLOCK}`).model, 1, 3).kind, 'none'));
test('gcc errors land on the script line', () => {
    const [error] = parseCompilerOutput("C:/game/main.bs:6:12: error: 'nope' undeclared (first use in this function)\nThe C blocks did not compile");
    assert.deepStrictEqual([error.file, error.line, error.message], ['C:/game/main.bs', 5, "'nope' undeclared (first use in this function)"]);
});
test('a missing C header', () => assert.strictEqual(parseCompilerOutput('a.bs:4:10: fatal error: x.h: No such file or directory')[0].line, 3));
test('gcc warnings are not errors', () => assert.deepStrictEqual(parseCompilerOutput('a.bs:4:10: warning: unused'), []));

test('ONCE at the top is fine', () => assert.deepStrictEqual(messages('// a comment\n\nONCE\nMAIN LBL m\nEND'), []));
test('ONCE in any case', () => assert.deepStrictEqual(messages('once\nMAIN LBL m\nEND'), []));
test('ONCE after a statement', () => assert.ok(messages('MAIN LBL m\nONCE\nEND').includes('ONCE goes above everything else in a file')));
test('ONCE takes nothing', () => assert.ok(messages('ONCE x\nMAIN LBL m\nEND').includes('ONCE takes nothing after it')));
test('a C block is not a statement for ONCE', () => assert.deepStrictEqual(messages('C\nint a(void) { return 1; }\nENDC\nONCE\nMAIN LBL m\nEND'), []));

const dead = (body) => lintText(`MAIN LBL m\nSET n 3\n${body}\nEND`).filter((item) => item.unnecessary).map((item) => item.line);
test('code after BREAK is dead', () => assert.deepStrictEqual(dead('FOR i 0 n\nBREAK\nSET a 1\nENDFOR'), [4]));
test('code after CONTINUE is dead', () => assert.deepStrictEqual(dead('FOR i 0 n\nCONTINUE\nSET a 1\nENDFOR'), [4]));
test('ENDFOR makes code live again', () => assert.deepStrictEqual(dead('FOR i 0 n\nBREAK\nENDFOR\nSET a 1'), []));
test('loops fold', () => {
    const ranges = foldingRanges(open('MAIN LBL m\nFOR i 0 3\nFOREACH v 1 2\nENDFOR\nENDFOR\nEND').model);
    assert.deepStrictEqual(ranges.map((range) => [range.start, range.end]).sort(), [[1, 4], [2, 3]]);
});
test('a loop variable is a variable name while typing', () => {
    const { model } = open('FOR ');
    assert.strictEqual(contextAt(model, 0, 4).kind, 'variableName');
});
test('hover on a loop keyword', () => {
    const { program, model } = open('FOREACH v 1 2');
    assert.ok(/FOREACH variable values/.test(hover(program, model, 0, 2, api).contents));
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
