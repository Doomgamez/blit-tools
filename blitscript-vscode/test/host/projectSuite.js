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
