'use strict';

// node test/project.test.js <engine repo> <blit-tools repo>
// the project logic with no editor, creating real projects in a temp folder

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const layout = require('../src/project/layout');
const { createProject, templateList } = require('../src/project/create');
const { buildPath, engineSteps, projectSteps, packList, listFiles, engineComplete, runtimeFiles } = require('../src/project/build');
const { startDetached } = require('../src/project/runner');

const [engine, tools] = process.argv.slice(2);
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'blit-project-'));

let passed = 0;
let failed = 0;

function test(name, body) {
    try {
        body();
        passed++;
    } catch (error) {
        failed++;
        console.log(`FAIL ${name}\n     ${error.message.split('\n').slice(0, 5).join('\n     ')}`);
    }
}

const values = (extra) => Object.assign({ name: 'Game', location: work, template: 'empty', engine, tools }, extra);
const read = (...parts) => fs.readFileSync(path.join(...parts), 'utf8');

test('correct values pass', () => assert.ok(layout.validate(values()).ok));
test('an empty name fails', () => assert.ok(layout.validate(values({ name: ' ' })).errors.name));
test('a path in the name fails', () => assert.ok(layout.validate(values({ name: '../evil' })).errors.name));
test('a trailing space fails on Windows', () => assert.ok(layout.validate(values({ name: 'Game ' })).errors.name));
test('reserved names fail', () => assert.ok(layout.validate(values({ name: 'CON' })).errors.name));
test('spaces and dashes are fine', () => assert.strictEqual(layout.validate(values({ name: 'My Game-2' })).errors.name, null));
test('a missing location fails', () => assert.ok(layout.validate(values({ location: path.join(work, 'nope') })).errors.location));
test('no engine folder fails', () => assert.ok(/Choose/.test(layout.validate(values({ engine: '' })).errors.engine)));
test('no tools folder fails', () => assert.ok(/Choose/.test(layout.validate(values({ tools: '' })).errors.tools)));
test('the tools folder is not an engine', () => assert.ok(/isn't the homeinvasionsdl/.test(layout.checkEngine(tools))));
test('the engine folder is not the tools', () => assert.ok(/isn't the blit-tools/.test(layout.checkTools(engine))));
test('a file is not a folder', () => assert.ok(layout.checkEngine(path.join(engine, 'CMakeLists.txt'))));

test('there are three templates', () => assert.deepStrictEqual(templateList().map((item) => item.id), ['empty', 'twoD', 'threeD']));

let root;

test('a project is created', () => {
    root = createProject(values({ name: 'Empty Game' }));
    assert.ok(fs.existsSync(path.join(root, 'scripts', 'main.bs')));
    assert.ok(fs.existsSync(path.join(root, 'scripts', 'engine.bs')));
});
test('the same name is refused the second time', () => assert.throws(() => createProject(values({ name: 'Empty Game' })), /already exists/));
test('nothing is created from bad values', () => {
    assert.throws(() => createProject(values({ name: 'Bad', engine: tools })));
    assert.ok(!fs.existsSync(path.join(work, 'Bad')));
});
test('an unknown template is refused', () => {
    assert.throws(() => createProject(values({ name: 'NoTemplate', template: 'x' })));
    assert.ok(!fs.existsSync(path.join(work, 'NoTemplate')));
});
test('folders stay out of the shared project file', () => {
    assert.deepStrictEqual(JSON.parse(read(root, layout.PROJECT_FILE)), { name: 'Empty Game', main: 'scripts/main.bs' });
    assert.strictEqual(JSON.parse(read(root, layout.LOCAL_FILE)).engine, engine);
});
test('this computer\'s file and generated files are ignored', () => {
    const ignored = read(root, '.gitignore');
    ['build/', '.blit/', layout.LOCAL_FILE, 'scripts/engine.bs'].forEach((entry) => assert.ok(ignored.includes(entry), entry));
});
test('the project reads back', () => {
    const project = layout.readProject(root);
    assert.strictEqual(project.name, 'Empty Game');
    assert.strictEqual(project.tools, tools);
});
test('a folder without a project reads as none', () => assert.strictEqual(layout.readProject(work), null));
test('a broken project file reads as none', () => {
    const folder = path.join(work, 'broken');
    fs.mkdirSync(folder);
    fs.writeFileSync(path.join(folder, layout.PROJECT_FILE), '{ nope');
    assert.strictEqual(layout.readProject(folder), null);
});
test('a clone without local folders still reads', () => {
    const folder = path.join(work, 'clone');
    fs.mkdirSync(folder);
    fs.writeFileSync(path.join(folder, layout.PROJECT_FILE), '{"name":"Clone"}');
    const project = layout.readProject(folder);
    assert.strictEqual(project.engine, '');
    assert.strictEqual(project.main, 'scripts/main.bs');
});

test('the 3D template brings its assets', () => {
    const made = createProject(values({ name: 'Walk', template: 'threeD' }));
    assert.deepStrictEqual(fs.readdirSync(path.join(made, 'assets')).sort(), ['checker.png', 'house.glb']);
    assert.ok(/LoadScene "house.glb"/.test(read(made, 'scripts', 'main.bs')));
});
test('the 2D template includes engine.bs from its own folder', () => {
    const made = createProject(values({ name: 'Flat', template: 'twoD' }));
    assert.ok(/INCLUDE "engine.bs"/.test(read(made, 'scripts', 'main.bs')));
    assert.ok(!read(made, 'scripts', 'main.bs').includes('../engine.bs'));
});

test('the engine builds inside the project, not the engine repo', () => {
    const project = layout.readProject(root);
    const configure = engineSteps(project, 'C:\\msys64\\mingw64\\bin')[0];
    assert.strictEqual(configure.args[configure.args.indexOf('-B') + 1], path.join(root, '.blit', 'engine'));
});
test('the build runs in order', () => {
    const steps = projectSteps(layout.readProject(root)).map((step) => step.kind);
    assert.deepStrictEqual(steps, ['folder', 'copy', 'run', 'write', 'remove', 'pack', 'runtime']);
});
test('a project with no engine build is incomplete', () => assert.strictEqual(engineComplete(layout.readProject(root)), false));
test('an engine from before the DLLs were copied gets rebuilt', () => {
    const project = layout.readProject(root);
    const paths = layout.projectPaths(project);

    const stamp = path.join(paths.engineBuild, 'engine.built');

    fs.mkdirSync(paths.engineBuild, { recursive: true });
    [paths.game, paths.compiler, paths.packer, stamp].forEach((file) => fs.writeFileSync(file, ''));
    assert.strictEqual(engineComplete(project), process.platform !== 'win32');

    if (process.platform === 'win32') {
        fs.writeFileSync(path.join(paths.engineBuild, 'SDL2.dll'), '');
        fs.writeFileSync(path.join(paths.engineBuild, 'libfreetype-6.dll'), '');
        assert.strictEqual(engineComplete(project), true);
        assert.deepStrictEqual(runtimeFiles(project).map((file) => path.basename(file)).sort(),
            ['HomeInvasion.exe', 'SDL2.dll', 'libfreetype-6.dll'].sort());
    }

    fs.rmSync(path.join(root, '.blit'), { recursive: true, force: true });
});
test('an engine older than its sources gets rebuilt', () => {
    const project = layout.readProject(root);
    const paths = layout.projectPaths(project);
    const stamp = path.join(paths.engineBuild, 'engine.built');
    const longAgo = new Date(2001, 1, 1);

    fs.mkdirSync(paths.engineBuild, { recursive: true });
    [paths.game, paths.compiler, paths.packer, stamp, path.join(paths.engineBuild, 'SDL2.dll')].forEach((file) => fs.writeFileSync(file, ''));
    assert.strictEqual(engineComplete(project), true);

    fs.utimesSync(stamp, longAgo, longAgo);
    assert.strictEqual(engineComplete(project), false, 'older than the sources');

    fs.rmSync(stamp);
    assert.strictEqual(engineComplete(project), false, 'no stamp, from before this check');

    // only the stamp counts: a program that make didn't need to relink stays old, and that's fine
    fs.writeFileSync(stamp, '');
    fs.utimesSync(paths.packer, longAgo, longAgo);
    assert.strictEqual(engineComplete(project), true, 'an old program');

    fs.rmSync(path.join(root, '.blit'), { recursive: true, force: true });
});
test('the engine steps end by noting the build', () => {
    const steps = engineSteps(layout.readProject(root), '');
    const last = steps[steps.length - 1];

    assert.strictEqual(last.kind, 'write');
    assert.strictEqual(path.basename(last.path), 'engine.built');
});
test('the archive holds script, manifest, shaders and the font', () => {
    const names = packList(layout.readProject(root)).map((file) => path.basename(file));
    ['main.blit', 'manifest.txt', 'basic.vert', 'basic.frag', 'arial.ttf'].forEach((name) => assert.ok(names.includes(name), name));
});
test('project assets go in and come first', () => {
    const made = layout.readProject(path.join(work, 'Walk'));
    const names = packList(made).map((file) => path.basename(file));
    assert.ok(names.indexOf('house.glb') < names.indexOf('basic.vert'));
});
test('a project shader replaces the engine one', () => {
    const made = layout.readProject(path.join(work, 'Walk'));
    fs.writeFileSync(path.join(made.root, 'assets', 'basic.frag'), '// mine');
    const files = packList(made).filter((file) => path.basename(file) === 'basic.frag');
    assert.strictEqual(files.length, 1);
    assert.ok(files[0].startsWith(made.root));
});
test('a stray manifest in assets can not fight the generated one', () => {
    const made = layout.readProject(path.join(work, 'Walk'));
    fs.writeFileSync(path.join(made.root, 'assets', 'manifest.txt'), 'Main-Script: other.blit\n');
    assert.strictEqual(packList(made).filter((file) => path.basename(file) === 'manifest.txt').length, 1);
    assert.ok(packList(made).find((file) => path.basename(file) === 'manifest.txt').includes('build'));
});
test('audio goes into the archive', () => {
    const made = layout.readProject(path.join(work, 'Walk'));
    fs.mkdirSync(path.join(made.root, 'assets', 'audio'), { recursive: true });
    fs.writeFileSync(path.join(made.root, 'assets', 'audio', 'door.ogg'), 'x');
    assert.ok(packList(made).some((file) => path.basename(file) === 'door.ogg'));
});
test('assets in subfolders are found', () => {
    const made = layout.readProject(path.join(work, 'Walk'));
    fs.mkdirSync(path.join(made.root, 'assets', 'deep', 'er'), { recursive: true });
    fs.writeFileSync(path.join(made.root, 'assets', 'deep', 'er', 'file.txt'), 'x');
    assert.ok(listFiles(path.join(made.root, 'assets')).some((file) => file.endsWith('file.txt')));
});
test('pack paths use forward slashes', () => {
    const step = projectSteps(layout.readProject(root)).find((item) => item.kind === 'pack');
    assert.ok(step.files().every((file) => !file.includes('\\')));
    assert.ok(!step.archive.includes('\\'));
});
test('a Unix shell leaves the Windows build PATH', () => {
    if (process.platform === 'win32') {
        const value = buildPath('C:\\Windows;C:\\Program Files\\Git\\usr\\bin;C:\\msys64\\usr\\bin;C:\\Program Files\\Git\\bin', 'C:\\tool');
        assert.strictEqual(value, 'C:\\tool;C:\\Windows');
    }
});

// what startDetached reported, in order, once it had time to report everything
function detached(command, args) {
    return new Promise((resolve) => {
        const reasons = [];

        startDetached(command, args, { cwd: work, env: process.env }, (reason) => reasons.push(reason));
        setTimeout(() => resolve(reasons), 1000);
    });
}

const script = (source) => ['-e', source];

async function detachedTests() {
    const check = (name, got, wanted) => test(name, () => assert.deepStrictEqual(got, wanted));

    check('a tool that works stays quiet', await detached(process.execPath, script('process.exit(0)')), []);
    check('a tool that crashes says why', await detached(process.execPath, script('console.error("first\\nNo module named fltk"); process.exit(1)')), ['No module named fltk']);
    check('a silent crash still reports', await detached(process.execPath, script('process.exit(3)')), ['it exited with code 3']);

    const missing = await detached(path.join(work, 'no-such-python.exe'), []);
    test('a missing python reports once', () => assert.ok(missing.length === 1 && /ENOENT/.test(missing[0]), JSON.stringify(missing)));
}

detachedTests().then(() => {
    fs.rmSync(work, { recursive: true, force: true });
    console.log(`${passed} passed, ${failed} failed`);
    process.exit(failed === 0 ? 0 : 1);
});
