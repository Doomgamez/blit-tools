'use strict';

// node test/mutation.js <engine repo>
// breaks the code on purpose in a temp copy: if the tests still pass, they aren't testing it

const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const root = path.join(__dirname, '..');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'blitscript-mutation-'));

const MUTATIONS = [
    ['src/core/tokenizer.js', "c === ','", "c === ';'", 'commas stop being whitespace'],
    ['src/core/tokenizer.js', "line[i + 1] === '/'", "line[i + 1] === '*'", 'comments stop working'],
    ['src/core/document.js', 'a.toLowerCase() === b.toLowerCase()', 'a === b', 'labels become case sensitive'],
    ['src/core/language.js', 'VEC3: 3', 'VEC3: 2', 'VEC3 takes the wrong number of values'],
    ['src/core/linter.js', 'count.tooFew || count.tooMany', 'count.tooMany', 'too few arguments pass'],
    ['src/core/linter.js', "['LBL', 'MAIN', 'FN', 'END', 'CONST', 'INCLUDE']", "['MAIN', 'FN', 'END', 'CONST', 'INCLUDE']", 'code after a label counts as dead'],
    ['src/core/program.js', 'addIncluders(program, key, files);', '', 'included files lose their includer'],
    ['src/features/navigation.js', 'return line.fn === symbol.scope;', 'return true;', 'renaming a parameter leaks out of its function'],
    ['src/features/navigation.js', '/engine\\.bs$/i.test(constant.document.path)', 'false', 'generated constants can be renamed'],
    ['src/features/signature.js', 'open ? args.length - 1 : args.length', 'args.length', 'half typed constructors count as finished'],
    ['src/features/colors.js', 'Number(token.text) <= 255', 'true', 'colors over 255 are accepted'],
    ['src/core/compilerOutput.js', 'Number(match[3]) - 1', 'Number(match[3])', 'compiler errors land one line off'],
];

function copy() {
    fs.rmSync(work, { recursive: true, force: true });
    fs.cpSync(path.join(root, 'src'), path.join(work, 'src'), { recursive: true });
    fs.cpSync(path.join(root, 'test'), path.join(work, 'test'), { recursive: true });
    fs.cpSync(path.join(root, 'data'), path.join(work, 'data'), { recursive: true });
}

let survived = 0;

for (const [file, from, to, description] of MUTATIONS) {
    copy();

    const target = path.join(work, file);
    const source = fs.readFileSync(target, 'utf8');

    if (!source.includes(from)) {
        console.log(`STALE    ${description}: the code it breaks is gone`);
        survived++;
        continue;
    }

    fs.writeFileSync(target, source.replace(from, to));

    const result = spawnSync(process.execPath, [path.join(work, 'test', 'core.test.js'), process.argv[2]], { encoding: 'utf8' });

    if (result.status === 0) {
        console.log(`SURVIVED ${description}`);
        survived++;
    } else {
        console.log(`caught   ${description}`);
    }
}

fs.rmSync(work, { recursive: true, force: true });
console.log(`${MUTATIONS.length - survived} of ${MUTATIONS.length} caught`);
process.exit(survived === 0 ? 0 : 1);
