'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const vscode = require('vscode');
const { page } = require('./page');
const { WIZARD_BODY } = require('./wizardBody');
const { validate } = require('../project/layout');
const { createProject, templateList } = require('../project/create');

const REMEMBERED = 'blitscript.projectFolders';

const BROWSE_TITLES = {
    location: 'Create the project in',
    engine: 'Select the homeinvasionsdl folder',
    tools: 'Select the blit-tools folder',
};

// the last folders used, or where the repositories usually live
function startingValues(context) {
    const remembered = context.globalState.get(REMEMBERED) || {};
    const repositories = path.join(os.homedir(), 'Documents', 'GitHub');
    const guess = (name) => {
        const folder = path.join(repositories, name);
        return fs.existsSync(folder) ? folder : '';
    };

    return {
        name: '',
        location: remembered.location || (fs.existsSync(repositories) ? repositories : ''),
        engine: remembered.engine || guess('homeinvasionsdl'),
        tools: remembered.tools || guess('blit-tools'),
    };
}

async function pickFolder(title) {
    const picked = await vscode.window.showOpenDialog({ canSelectFolders: true, canSelectFiles: false, canSelectMany: false, title });
    return picked ? picked[0].fsPath : null;
}

async function offerToOpen(root) {
    const open = 'Open Project';
    const answer = await vscode.window.showInformationMessage(`Created ${path.basename(root)}.`, open);

    if (answer === open) {
        await vscode.commands.executeCommand('vscode.openFolder', vscode.Uri.file(root));
    }
}

function handleMessage(panel, context, message) {
    const post = (reply) => panel.webview.postMessage(reply);

    switch (message.type) {
        case 'ready':
            post({ type: 'init', templates: templateList(), values: startingValues(context) });
            break;

        case 'validate':
            post({ type: 'validation', result: validate(message.values) });
            break;

        case 'browse':
            pickFolder(BROWSE_TITLES[message.field]).then((value) => {
                if (value) {
                    post({ type: 'field', field: message.field, value });
                }
            });
            break;

        case 'cancel':
            panel.dispose();
            break;

        case 'create':
            try {
                const root = createProject(message.values);
                const { location, engine, tools } = message.values;

                context.globalState.update(REMEMBERED, { location, engine, tools });
                panel.dispose();
                offerToOpen(root);
            } catch (error) {
                post({ type: 'failed', message: `Could not create the project: ${error.message}` });
            }
            break;

        default:
            break;
    }
}

let current = null;

function openWizard(context) {
    if (current) {
        current.reveal();
        return;
    }

    current = vscode.window.createWebviewPanel('blitNewProject', 'New Blit Engine Project', vscode.ViewColumn.One, {
        enableScripts: true,
        localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, 'media')],
    });

    current.webview.html = page(current.webview, context.extensionUri, 'wizard.js', WIZARD_BODY);
    current.webview.onDidReceiveMessage((message) => handleMessage(current, context, message));
    current.onDidDispose(() => { current = null; });
}

module.exports = { openWizard, pickFolder };
