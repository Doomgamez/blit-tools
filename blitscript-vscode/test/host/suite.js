'use strict';

// runs inside a real VS Code, started by test/host/run.ps1, and writes what happened to RESULTS_FILE

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

async function waitFor(condition, label) {
    for (let i = 0; i < 100; i++) {
        const value = await condition();

        if (value) {
            return value;
        }

        await sleep(100);
    }

    throw new Error(`timed out: ${label}`);
}

function hoverText(hovers) {
    return hovers.map((item) => item.contents.map((part) => part.value || String(part)).join('\n')).join('\n');
}

const execute = (command, ...args) => vscode.commands.executeCommand(command, ...args);

async function run() {
    const root = vscode.workspace.workspaceFolders[0].uri.fsPath;
    const uriOf = (name) => vscode.Uri.file(path.join(root, 'blitscript', name));
    const document = await vscode.workspace.openTextDocument(uriOf('test.bs'));
    const editor = await vscode.window.showTextDocument(document);
    const at = (line, character) => new vscode.Position(line, character);

    await check('.bs files open as BlitScript', () => assert.strictEqual(document.languageId, 'blitscript'));

    const extension = vscode.extensions.all.find((item) => item.packageJSON.name === 'blitscript');

    await check('the extension activates', async () => {
        await extension.activate();
        assert.ok(extension.isActive);
    });

    await check('the workspace cheatsheet is used', () => {
        const { state } = extension.exports;
        assert.ok(/cheatsheet\.txt$/.test(state.api.source), state.api.source);
        assert.ok(Object.keys(state.api.functions).length > 150);
    });

    await check('statement completion', async () => {
        const list = await execute('vscode.executeCompletionItemProvider', document.uri, at(9, 0));
        assert.ok(list.items.some((item) => item.label === 'CALL'));
    });

    await check('CALL completes natives as snippets', async () => {
        const list = await execute('vscode.executeCompletionItemProvider', document.uri, at(4, 5));
        const item = list.items.find((entry) => entry.label === 'DrawCube');
        assert.ok(item, 'no DrawCube');
        assert.ok(item.insertText instanceof vscode.SnippetString);
    });

    await check('hover on a native', async () => {
        const text = hoverText(await execute('vscode.executeHoverProvider', document.uri, at(4, 8)));
        assert.ok(/DrawCube\(vec3 position/.test(text), text);
    });

    await check('hover on a keyword', async () => {
        const text = hoverText(await execute('vscode.executeHoverProvider', document.uri, at(6, 1)));
        assert.ok(/flags/.test(text), text);
    });

    await check('signature help', async () => {
        const help = await execute('vscode.executeSignatureHelpProvider', document.uri, at(4, 32));
        assert.ok(help && /DrawCube/.test(help.signatures[0].label));
        assert.strictEqual(help.activeParameter, 1);
    });

    await check('go to a label', async () => {
        const [target] = await execute('vscode.executeDefinitionProvider', document.uri, at(7, 5));
        assert.strictEqual((target.targetRange || target.range).start.line, 3);
    });

    await check('go to a constant in an included file', async () => {
        const [target] = await execute('vscode.executeDefinitionProvider', document.uri, at(4, 39));
        assert.ok(/engine\.bs$/.test((target.targetUri || target.uri).fsPath));
    });

    await check('references', async () => {
        const found = await execute('vscode.executeReferenceProvider', document.uri, at(3, 5));
        assert.strictEqual(found.length, 2);
    });

    await check('rename edits every use', async () => {
        const edit = await execute('vscode.executeDocumentRenameProvider', document.uri, at(3, 5), 'renamed');
        assert.strictEqual(edit.get(document.uri).length, 2);
    });

    await check('renaming a native is refused', async () => {
        await assert.rejects(Promise.resolve(execute('vscode.prepareRename', document.uri, at(4, 8))));
    });

    await check('outline', async () => {
        const symbols = await execute('vscode.executeDocumentSymbolProvider', document.uri);
        assert.deepStrictEqual(symbols.map((item) => item.name), ['main', 'top', 'helper']);
    });

    await check('folding', async () => {
        const ranges = await execute('vscode.executeFoldingRangeProvider', document.uri);
        assert.ok(ranges.some((range) => range.start === 12 && range.end === 14));
    });

    await check('color swatches', async () => {
        const found = await execute('vscode.executeDocumentColorProvider', document.uri);
        assert.strictEqual(found.length, 1);
        assert.ok(Math.abs(found[0].color.red - 24 / 255) < 0.001);
    });

    await check('a clean file has no problems', async () => {
        await sleep(600);
        const items = vscode.languages.getDiagnostics(document.uri).filter((item) => !item.tags);
        assert.deepStrictEqual(items.map((item) => item.message), []);
    });

    await check('lint updates while typing', async () => {
        await editor.edit((edit) => edit.insert(at(5, 0), 'JMP nowhere\n'));
        const items = await waitFor(() => {
            const found = vscode.languages.getDiagnostics(document.uri).filter((item) => /Unknown label nowhere/.test(item.message));
            return found.length ? found : null;
        }, 'lint');
        assert.strictEqual(items[0].range.start.line, 5);
        await execute('undo');
    });

    await check('the lint clears after the fix', () => waitFor(() =>
        !vscode.languages.getDiagnostics(document.uri).some((item) => /nowhere/.test(item.message)), 'cleared'));

    await check('compile command reports the size', async () => {
        const result = await execute('blitscript.compile');
        assert.ok(result.ran, result.reason);
        assert.ok(result.ok, result.raw);
        assert.ok(result.codeSize > 0);
    });

    const broken = await vscode.workspace.openTextDocument(uriOf('broken.bs'));
    await vscode.window.showTextDocument(broken);

    await check('compile on save puts the error on its line', async () => {
        await vscode.window.activeTextEditor.edit((edit) => edit.insert(new vscode.Position(0, 0), '// saved\n'));
        await broken.save();

        const found = await waitFor(() => {
            const items = vscode.languages.getDiagnostics(broken.uri).filter((item) => item.source === 'blitscriptc');
            return items.length ? items : null;
        }, 'compiler error');

        assert.strictEqual(found[0].range.start.line, 3);
        assert.ok(/CMP expects/.test(found[0].message), found[0].message);
    });

    await check('the cheatsheet command opens it', async () => {
        await execute('blitscript.openCheatsheet');
        assert.ok(/cheatsheet\.txt$/.test(vscode.window.activeTextEditor.document.fileName));
    });

    await check('the API tree and sidebar are registered', async () => {
        await execute('workbench.view.extension.blitEngine');
        await sleep(1000);
    });

    await check('the wizard opens', async () => {
        await execute('blitscript.newProject');
        await sleep(3000);
        const tabs = vscode.window.tabGroups.all.flatMap((group) => group.tabs.map((tab) => tab.label));
        assert.ok(tabs.includes('New Blit Engine Project'), tabs.join(', '));
    });

    await capture('*Extension Development Host*', 'wizard.png');

    await check('a project is created from the wizard values', async () => {
        const made = await execute('blitscript.createProject', {
            name: 'Host Game', location: process.env.PROJECTS_FOLDER, template: 'threeD',
            engine: process.env.ENGINE_FOLDER, tools: process.env.TOOLS_FOLDER,
        });
        assert.ok(fs.existsSync(path.join(made, 'blit.project.json')));
    });

    await check('bad folders are refused', async () => {
        await assert.rejects(Promise.resolve(execute('blitscript.createProject', {
            name: 'Bad Game', location: process.env.PROJECTS_FOLDER, template: 'empty',
            engine: process.env.TOOLS_FOLDER, tools: process.env.TOOLS_FOLDER,
        })));
        assert.ok(!fs.existsSync(path.join(process.env.PROJECTS_FOLDER, 'Bad Game')));
    });

    fs.writeFileSync(process.env.RESULTS_FILE, `${results.join('\n')}\n`);
}

// a failed picture isn't a failed test, the file just won't be there to look at
function capture(title, name) {
    return new Promise((resolve) => {
        const script = path.join(__dirname, 'capture.ps1');
        execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', script, title, path.join(path.dirname(process.env.RESULTS_FILE), name)], () => resolve());
    });
}

module.exports = { run };
