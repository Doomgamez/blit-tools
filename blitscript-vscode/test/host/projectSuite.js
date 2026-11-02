'use strict';

// runs inside a real VS Code opened on a project the first suite created:
// builds the engine and the game from nothing and runs the result

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { execFile } = require('child_process');
const vscode = require('vscode');

const results = [];

async function check(name, body) {
    try {
        await body();
        results.push(`PASS ${name}`);
    } catch (error) {
        results.push(`FAIL ${name}: ${String(error.message).split('\n')[0]}`);
    }
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const execute = (command, ...args) => vscode.commands.executeCommand(command, ...args);

function capture(title, name) {
    return new Promise((resolve) => {
        const script = path.join(__dirname, 'capture.ps1');
        execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', script, title, path.join(path.dirname(process.env.RESULTS_FILE), name)], () => resolve());
    });
}

function packedNames(root, packer) {
    return new Promise((resolve) => {
        execFile(packer, ['-m', '2', '-i', path.join(root, 'build', 'Engine.blfs')], (error, stdout) => resolve(stdout || ''));
    });
}

async function run() {
    const root = vscode.workspace.workspaceFolders[0].uri.fsPath;
    const inProject = (...parts) => path.join(root, ...parts);
    const extension = vscode.extensions.all.find((item) => item.packageJSON.name === 'blitscript');

    await extension.activate();

    const { session, state } = extension.exports;

    await check('the project is recognized', () => {
        assert.strictEqual(session.project.name, 'Host Game');
        assert.strictEqual(session.status().engineError, null);
        assert.strictEqual(session.status().toolsError, null);
        assert.strictEqual(session.status().engineBuilt, false);
    });

    await check('the API comes from the project\'s engine', () => assert.ok(state.api.source.startsWith(process.env.ENGINE_FOLDER), state.api.source));

    await execute('workbench.view.extension.blitEngine');
    await sleep(2500);
    await capture('*Host Game*', 'sidebar.png');

    await check('the first build makes the engine and the game', async () => {
        assert.strictEqual(await execute('blitscript.buildProject'), true);
        assert.ok(fs.existsSync(inProject('build', 'Engine.blfs')));
        assert.strictEqual(session.status().engineBuilt, true);
    });

    await check('the engine repository stayed untouched', () => {
        assert.ok(!fs.existsSync(path.join(process.env.ENGINE_FOLDER, '.blit')));
    });

    await check('the archive has what the game needs', async () => {
        const names = await packedNames(root, process.env.PACKER || path.join(root, '.blit', 'engine', 'blitfs_pack.exe'));
        ['main.blit', 'manifest.txt', 'basic.vert', 'basic.frag', 'arial.ttf', 'house.glb', 'checker.png'].forEach((name) => assert.ok(names.includes(name), `no ${name}: ${names.slice(0, 200)}`));
    });

    await check('the game runs from the project', async () => {
        assert.strictEqual(await execute('blitscript.runProject'), true);
        const code = await Promise.race([session.finished, sleep(30000).then(() => 'timed out')]);
        assert.strictEqual(code, 0);
        assert.ok(fs.existsSync(process.env.HOMEINVASION_SCREENSHOT), 'no screenshot');
    });

    await check('the build folder runs with no toolchain', async () => {
        const output = inProject('build');
        const shot = path.join(output, 'shipped.bmp');
        const system = path.join(process.env.SystemRoot, 'System32');

        assert.ok(fs.existsSync(path.join(output, 'SDL2_ttf.dll')), 'no DLLs in build');

        const code = await new Promise((resolve) => {
            execFile(path.join(output, 'HomeInvasion.exe'), [], {
                cwd: output,
                timeout: 30000,
                env: { PATH: `${system};${process.env.SystemRoot}`, SystemRoot: process.env.SystemRoot, HOMEINVASION_SCREENSHOT: shot },
            }, (error) => resolve(error ? error.code : 0));
        });

        assert.strictEqual(code, 0);
        assert.ok(fs.existsSync(shot), 'no screenshot');
        fs.rmSync(shot);
    });

    await check('a changed script reaches the game', async () => {
        const file = inProject('scripts', 'main.bs');
        fs.writeFileSync(file, fs.readFileSync(file, 'utf8').replace('demo: WASD', 'PROJECT EDIT: WASD'));
        fs.rmSync(process.env.HOMEINVASION_SCREENSHOT, { force: true });

        const output = inProject('build', 'main.blit');
        const before = fs.statSync(output).size;

        assert.strictEqual(await execute('blitscript.buildProject'), true);
        assert.notStrictEqual(fs.statSync(output).size, before);
    });

    await check('compiling in the editor uses the project\'s compiler', async () => {
        const document = await vscode.workspace.openTextDocument(inProject('scripts', 'main.bs'));
        await vscode.window.showTextDocument(document);
        const result = await execute('blitscript.compile');
        assert.ok(result && result.ok, result ? result.raw : 'no compiler');
    });

    await check('a script error stops the build', async () => {
        const file = inProject('scripts', 'main.bs');
        const good = fs.readFileSync(file, 'utf8');

        fs.writeFileSync(file, `${good}\nFOO bar\n`);
        const ok = await execute('blitscript.buildProject');
        fs.writeFileSync(file, good);
        assert.strictEqual(ok, false);
    });

    await check('the model inspector runs from blit-tools', async () => {
        assert.strictEqual(await execute('blitscript.inspectModel', vscode.Uri.file(inProject('assets', 'house.glb'))), true);
    });

    await check('the Python tools skip the toolchain', async () => {
        const tools = path.join(path.dirname(process.env.RESULTS_FILE), 'fake-tools');
        const toolchain = 'C:\\msys64\\mingw64\\bin';
        const made = (name) => path.join(tools, `${name}.txt`);
        const record = (name) => `import sys\nopen(${JSON.stringify(made(name))}, 'w').write(sys.executable)\n`;

        for (const name of ['gltf-inspector', 'blitfs-viewer']) {
            fs.mkdirSync(path.join(tools, name), { recursive: true });
            fs.writeFileSync(path.join(tools, name, 'main.py'), record(name));
        }

        const realTools = session.tools;
        session.tools = () => tools;

        try {
            assert.strictEqual(await execute('blitscript.inspectModel', vscode.Uri.file(inProject('assets', 'house.glb'))), true);
            await execute('blitscript.openArchive');

            for (let i = 0; i < 50 && !fs.existsSync(made('blitfs-viewer')); i++) {
                await sleep(100);
            }

            for (const name of ['gltf-inspector', 'blitfs-viewer']) {
                const python = fs.readFileSync(made(name), 'utf8');
                assert.ok(!python.toLowerCase().startsWith(toolchain.toLowerCase()), `${name} ran with ${python}`);
            }
        } finally {
            session.tools = realTools;
        }
    });

    await check('a C block builds into a library next to the game', async () => {
        const file = inProject('scripts', 'main.bs');
        const library = inProject('build', 'main.natives.dll');
        const good = fs.readFileSync(file, 'utf8');

        fs.writeFileSync(file, `${good}\nC\nint Three(void) { return 3; }\nENDC\n`);
        assert.strictEqual(await execute('blitscript.buildProject'), true);
        assert.ok(fs.existsSync(library), 'no library');

        fs.writeFileSync(file, good);
        assert.strictEqual(await execute('blitscript.buildProject'), true);
        assert.ok(!fs.existsSync(library), 'an old library stayed');
    });

    await check('a mistake in a C block reaches its line', async () => {
        const file = inProject('scripts', 'main.bs');
        const good = fs.readFileSync(file, 'utf8');
        const line = good.split('\n').length + 1;

        fs.writeFileSync(file, `${good}\nC\nint Three(void) { return nope; }\nENDC\n`);

        const document = await vscode.workspace.openTextDocument(file);
        await vscode.window.showTextDocument(document);
        const result = await execute('blitscript.compile');

        fs.writeFileSync(file, good);
        assert.ok(result && !result.ok, 'it compiled');
        assert.ok(result.errors.some((error) => error.line === line && /nope/.test(error.message)), JSON.stringify(result.errors).slice(0, 200));
    });

    await check('a missing asset is flagged and clears once it exists', async () => {
        const file = inProject('scripts', 'audio.bs');
        const sound = inProject('assets', 'door.wav');

        fs.writeFileSync(file, 'MAIN LBL main\nCALL LoadSound "door.wav" -> door\nEND\n');

        const document = await vscode.workspace.openTextDocument(file);
        await vscode.window.showTextDocument(document);

        const flagged = () => vscode.languages.getDiagnostics(document.uri).some((item) => /not in assets/.test(item.message));

        for (let i = 0; i < 30 && !flagged(); i++) {
            await sleep(100);
        }

        assert.ok(flagged(), 'never flagged');

        fs.writeFileSync(sound, 'RIFF');
        await vscode.window.activeTextEditor.edit((edit) => edit.insert(new vscode.Position(2, 0), '// x\n'));

        for (let i = 0; i < 30 && flagged(); i++) {
            await sleep(100);
        }

        fs.rmSync(sound);
        assert.ok(!flagged(), 'never cleared');
    });

    await check('a moved engine folder is reported', async () => {
        const local = inProject('blit.local.json');
        const saved = fs.readFileSync(local, 'utf8');

        fs.writeFileSync(local, JSON.stringify({ engine: 'C:\\nowhere', tools: process.env.TOOLS_FOLDER }));
        const status = session.status();
        fs.writeFileSync(local, saved);
        assert.ok(status.engineError);
    });

    fs.writeFileSync(process.env.RESULTS_FILE, `${results.join('\n')}\n`);
}

module.exports = { run };
