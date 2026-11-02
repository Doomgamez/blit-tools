'use strict';

const vscode = require('vscode');
const { page } = require('./page');

const ALLOWED_COMMANDS = new Set([
    'blitscript.newProject', 'blitscript.runProject', 'blitscript.stopProject', 'blitscript.buildProject',
    'blitscript.buildEngine', 'blitscript.openArchive', 'blitscript.openCheatsheet', 'blitscript.chooseEngine',
    'blitscript.chooseTools', 'vscode.openFolder',
]);

class SidebarProvider {
    constructor(context, session) {
        this.context = context;
        this.session = session;
        this.view = null;
    }

    update() {
        if (this.view) {
            this.view.webview.postMessage({ type: 'status', status: this.session.status() });
        }
    }

    resolveWebviewView(view) {
        this.view = view;

        view.webview.options = { enableScripts: true, localResourceRoots: [vscode.Uri.joinPath(this.context.extensionUri, 'media')] };
        view.webview.html = page(view.webview, this.context.extensionUri, 'sidebar.js', '<main id="root" class="sidebar"></main>');

        view.webview.onDidReceiveMessage((message) => {
            if (message.type === 'ready') {
                this.update();
            } else if (message.type === 'command' && ALLOWED_COMMANDS.has(message.command)) {
                vscode.commands.executeCommand(message.command);
            }
        });

        view.onDidChangeVisibility(() => this.update());
        view.onDidDispose(() => { this.view = null; });
    }
}

module.exports = { SidebarProvider };
