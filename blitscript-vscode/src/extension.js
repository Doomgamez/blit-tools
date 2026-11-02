'use strict';

const vscode = require('vscode');
const { loadApi, cheatsheetPath } = require('./host');
const { Diagnostics } = require('./diagnostics');
const { registerProviders } = require('./providers');
const { registerProjectCommands } = require('./projectCommands');
const { ProjectSession } = require('./project/session');
const { SidebarProvider } = require('./ui/sidebar');
const { ApiTree } = require('./ui/apiTree');

function lintOpenDocuments(diagnostics) {
    vscode.workspace.textDocuments.forEach((document) => diagnostics.lint(document));
}

function registerCommands(context, state, diagnostics) {
    return [
        vscode.commands.registerCommand('blitscript.compile', async () => {
            const editor = vscode.window.activeTextEditor;

            if (!editor || editor.document.languageId !== 'blitscript') {
                vscode.window.showInformationMessage('BlitScript: open a .bs file to compile it.');
                return null;
            }

            await editor.document.save();
            return diagnostics.compile(editor.document, true);
        }),

        vscode.commands.registerCommand('blitscript.openCheatsheet', async () => {
            const file = cheatsheetPath();

            if (!file) {
                vscode.window.showInformationMessage('BlitScript: no docs/cheatsheet.txt in this workspace.');
                return;
            }

            await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(file));
        }),

        vscode.commands.registerCommand('blitscript.reloadApi', () => {
            state.api = loadApi(context);
            lintOpenDocuments(diagnostics);
            vscode.window.showInformationMessage(`BlitScript: ${Object.keys(state.api.functions).length} engine functions loaded.`);
        }),
    ];
}

function activate(context) {
    const state = { api: loadApi(context) };
    const diagnostics = new Diagnostics(state);
    const session = new ProjectSession();
    const sidebar = new SidebarProvider(context, session);
    const apiTree = new ApiTree(state);

    context.subscriptions.push(
        diagnostics,
        session,
        session.onDidChange(() => sidebar.update()),
        vscode.window.registerWebviewViewProvider('blitEngine.project', sidebar),
        vscode.window.registerTreeDataProvider('blitEngine.api', apiTree),
        vscode.workspace.onDidChangeWorkspaceFolders(() => sidebar.update()),
        ...registerProviders(state),
        ...registerCommands(context, state, diagnostics),
        ...registerProjectCommands(context, state, session),
        vscode.workspace.onDidOpenTextDocument((document) => diagnostics.lint(document)),
        vscode.workspace.onDidChangeTextDocument((event) => diagnostics.schedule(event.document)),
        vscode.workspace.onDidCloseTextDocument((document) => diagnostics.lintCollection.delete(document.uri)),
        vscode.workspace.onDidSaveTextDocument((document) => {
            if (document.languageId === 'blitscript' && vscode.workspace.getConfiguration('blitscript').get('compileOnSave')) {
                diagnostics.compile(document, false);
            }
        }),
        vscode.workspace.onDidChangeConfiguration((event) => {
            if (event.affectsConfiguration('blitscript')) {
                state.api = loadApi(context);
                apiTree.refresh();
                lintOpenDocuments(diagnostics);
            }
        }),
    );

    lintOpenDocuments(diagnostics);
    return { state, diagnostics, session };
}

function deactivate() {}

module.exports = { activate, deactivate };
