'use strict';

const fs = require('fs');
const path = require('path');
const vscode = require('vscode');
const { parseCheatsheet, parseConstants } = require('./core/api');
const { readProject, projectPaths, executable } = require('./project/layout');
const { listFiles } = require('./project/build');

function samePath(a, b) {
    return process.platform === 'win32' ? a.toLowerCase() === b.toLowerCase() : a === b;
}

// reads files for the core: what's open in an editor wins over what's on disk
const files = {
    read(file) {
        const open = vscode.workspace.textDocuments.find((document) => samePath(document.uri.fsPath, file));

        if (open) {
            return open.getText();
        }

        try {
            return fs.readFileSync(file, 'utf8');
        } catch {
            return null;
        }
    },

    siblings(file) {
        const folder = path.dirname(file);

        try {
            return fs.readdirSync(folder).filter((name) => name.endsWith('.bs')).map((name) => path.join(folder, name));
        } catch {
            return [];
        }
    },
};

function findInWorkspace(relativePaths) {
    for (const folder of vscode.workspace.workspaceFolders || []) {
        for (const relative of relativePaths) {
            const candidate = path.join(folder.uri.fsPath, relative);

            if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) {
                return candidate;
            }
        }
    }

    return null;
}

function currentProject() {
    for (const folder of vscode.workspace.workspaceFolders || []) {
        const project = readProject(folder.uri.fsPath);

        if (project) {
            return project;
        }
    }

    return null;
}

// a game project keeps nothing of the engine, so its engine folder answers these
function inEngine(relative) {
    const project = currentProject();
    const candidate = project && project.engine ? path.join(project.engine, relative) : null;

    return candidate && fs.existsSync(candidate) ? candidate : null;
}

function cheatsheetPath() {
    const configured = vscode.workspace.getConfiguration('blitscript').get('cheatsheetPath');
    return configured || inEngine('docs/cheatsheet.txt') || findInWorkspace(['docs/cheatsheet.txt']);
}

// the workspace's own cheatsheet wins, so new engine functions show up without updating the extension
function loadApi(context) {
    const bundled = JSON.parse(fs.readFileSync(path.join(context.extensionPath, 'data', 'api.json'), 'utf8'));
    const cheatsheet = cheatsheetPath();
    const header = inEngine('blitscript/engine.bs') || findInWorkspace(['blitscript/engine.bs', 'engine.bs']);
    const api = { functions: bundled.functions, constants: bundled.constants, source: 'bundled' };

    try {
        if (cheatsheet) {
            api.functions = parseCheatsheet(fs.readFileSync(cheatsheet, 'utf8'));
            api.source = cheatsheet;
        }

        if (header) {
            api.constants = parseConstants(fs.readFileSync(header, 'utf8'));
        }
    } catch (error) {
        console.error(`BlitScript: could not read the API: ${error.message}`);
    }

    return api;
}

// the names a script can load in this project, undefined outside a project where nobody can tell
function projectAssetNames() {
    const project = currentProject();

    return project ? listFiles(projectPaths(project).assets).map((file) => path.basename(file).toLowerCase()) : undefined;
}

function findCompiler() {
    const configured = vscode.workspace.getConfiguration('blitscript').get('compilerPath');
    const name = executable('blitscriptc');
    const project = currentProject();

    if (configured) {
        return configured;
    }

    if (project) {
        const built = projectPaths(project).compiler;
        return fs.existsSync(built) ? built : null;
    }

    return findInWorkspace([`build/${name}`, `blitscriptc/${name}`, name]);
}

module.exports = { files, loadApi, findCompiler, cheatsheetPath, projectAssetNames };
