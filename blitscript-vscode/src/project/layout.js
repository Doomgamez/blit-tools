'use strict';

const fs = require('fs');
const path = require('path');

const PROJECT_FILE = 'blit.project.json';   // name and entry script, belongs in git
const LOCAL_FILE = 'blit.local.json';       // this computer's folders, doesn't

const ENGINE_MARKERS = ['CMakeLists.txt', 'src/Engine/Engine.h', 'blitscript/blitscriptc.c', 'blitscript/engine.bs',
    'blitfs/arial.ttf', 'assets/shaders/basic.vert'];
const TOOLS_MARKERS = ['gltf-inspector/main.py', 'blitfs-viewer/main.py'];

function missingMarkers(folder, markers) {
    return markers.filter((marker) => !fs.existsSync(path.join(folder, marker)));
}

function checkFolder(folder, markers, describe) {
    if (!folder || !folder.trim()) {
        return `Choose the ${describe} folder`;
    }

    if (!fs.existsSync(folder) || !fs.statSync(folder).isDirectory()) {
        return 'This folder doesn\'t exist';
    }

    const missing = missingMarkers(folder, markers);
    return missing.length ? `This isn't the ${describe} folder: no ${missing[0]}` : null;
}

const checkEngine = (folder) => checkFolder(folder, ENGINE_MARKERS, 'homeinvasionsdl');
const checkTools = (folder) => checkFolder(folder, TOOLS_MARKERS, 'blit-tools');

function checkName(name) {
    if (!name || !name.trim()) {
        return 'Give the project a name';
    }

    if (!/^[A-Za-z0-9][A-Za-z0-9 _-]*$/.test(name) || name.endsWith(' ')) {
        return 'Use letters, numbers, spaces, - and _';
    }

    return /^(con|prn|aux|nul|com\d|lpt\d)$/i.test(name) ? 'Windows reserves this name' : null;
}

function checkLocation(location, name) {
    if (!location || !location.trim()) {
        return 'Choose where to create it';
    }

    if (!fs.existsSync(location) || !fs.statSync(location).isDirectory()) {
        return 'This folder doesn\'t exist';
    }

    if (!checkName(name) && fs.existsSync(path.join(location, name))) {
        return `${name} already exists here`;
    }

    return null;
}

function validate(values) {
    const errors = {
        name: checkName(values.name),
        location: checkLocation(values.location, values.name),
        engine: checkEngine(values.engine),
        tools: checkTools(values.tools),
    };

    return { errors, ok: Object.values(errors).every((error) => error === null) };
}

function readJson(file) {
    try {
        return JSON.parse(fs.readFileSync(file, 'utf8'));
    } catch {
        return null;
    }
}

function readProject(root) {
    const project = readJson(path.join(root, PROJECT_FILE));

    if (!project) {
        return null;
    }

    const local = readJson(path.join(root, LOCAL_FILE)) || {};

    return {
        root,
        name: project.name || path.basename(root),
        main: project.main || 'scripts/main.bs',
        engine: local.engine || '',
        tools: local.tools || '',
    };
}

function writeLocal(root, values) {
    const current = readJson(path.join(root, LOCAL_FILE)) || {};

    fs.writeFileSync(path.join(root, LOCAL_FILE), `${JSON.stringify(Object.assign(current, values), null, 4)}\n`);
}

const executable = (name) => (process.platform === 'win32' ? `${name}.exe` : name);

function projectPaths(project) {
    const engineBuild = path.join(project.root, '.blit', 'engine');

    return {
        engineBuild,
        output: path.join(project.root, 'build'),
        // what goes into Engine.blfs but isn't the game itself: main.blit, manifest.txt, compiled levels
        stage: path.join(project.root, '.blit', 'stage'),
        main: path.join(project.root, project.main),
        assets: path.join(project.root, 'assets'),
        levels: path.join(project.root, 'levels'),
        compiler: path.join(engineBuild, executable('blitscriptc')),
        packer: path.join(engineBuild, executable('blitfs_pack')),
        mapCompiler: path.join(engineBuild, executable('blitlevel')),
        game: path.join(engineBuild, executable('HomeInvasion')),
        shipped: path.join(project.root, 'build', executable('HomeInvasion')),
        archive: path.join(project.root, 'build', 'Engine.blfs'),
    };
}

module.exports = {
    PROJECT_FILE, LOCAL_FILE, checkEngine, checkTools, validate, readProject, writeLocal, projectPaths, executable,
};
