'use strict';

const vscode = require('vscode');
const { writeLocal } = require('./project/layout');
const { createProject } = require('./project/create');
const { openWizard, pickFolder } = require('./ui/wizard');
const { insertFunction, searchApi } = require('./ui/apiTree');

function chooseFolder(session, key, title) {
    return async () => {
        const { project } = session;
        const folder = project ? await pickFolder(title) : null;

        if (folder) {
            writeLocal(project.root, { [key]: folder });
            session.changed.fire();
        }
    };
}

async function pickModel(session) {
    const { project } = session;
    const defaultUri = project ? vscode.Uri.file(project.root) : undefined;
    const picked = await vscode.window.showOpenDialog({ canSelectMany: false, defaultUri, filters: { 'glTF models': ['glb', 'gltf'] } });

    return picked ? picked[0] : null;
}

function registerProjectCommands(context, state, session) {
    const command = (name, handler) => vscode.commands.registerCommand(name, handler);

    return [
        command('blitscript.newProject', () => openWizard(context)),
        command('blitscript.createProject', (values) => createProject(values)),
        command('blitscript.buildProject', () => session.build()),
        command('blitscript.runProject', () => session.run()),
        command('blitscript.stopProject', () => session.stop()),
        command('blitscript.buildEngine', () => session.buildEngine()),
        command('blitscript.openArchive', () => session.openArchive()),
        command('blitscript.chooseEngine', chooseFolder(session, 'engine', 'Select the homeinvasionsdl folder')),
        command('blitscript.chooseTools', chooseFolder(session, 'tools', 'Select the blit-tools folder')),
        command('blitscript.insertFunction', insertFunction),
        command('blitscript.searchApi', () => searchApi(state)),
        command('blitscript.inspectModel', async (uri) => {
            const model = uri || await pickModel(session);
            return model ? session.inspectModel(model.fsPath) : false;
        }),
    ];
}

module.exports = { registerProjectCommands };
