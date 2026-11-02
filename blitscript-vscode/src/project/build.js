'use strict';

const fs = require('fs');
const path = require('path');
const { projectPaths } = require('./layout');

const WINDOWS_TOOLCHAIN = 'C:\\msys64\\mingw64\\bin';
const slashes = (file) => file.split(path.sep).join('/');

// cmake's MinGW generator refuses to run with a Unix shell on the PATH
function buildPath(currentPath, toolchain) {
    const separator = process.platform === 'win32' ? ';' : ':';
    const kept = (currentPath || '').split(separator)
        .filter((folder) => process.platform !== 'win32' || !/\\(msys64\\usr|Git\\usr|Git\\bin)/i.test(folder));

    return (toolchain ? [toolchain, ...kept] : kept).join(separator);
}

function findToolchain(configured) {
    if (configured) {
        return configured;
    }

    return process.platform === 'win32' && fs.existsSync(WINDOWS_TOOLCHAIN) ? WINDOWS_TOOLCHAIN : '';
}

function engineSteps(project, toolchain) {
    const paths = projectPaths(project);
    const configure = ['-S', project.engine, '-B', paths.engineBuild, '-DCMAKE_BUILD_TYPE=Release'];

    if (process.platform === 'win32') {
        configure.push('-G', 'MinGW Makefiles', '-DCMAKE_C_COMPILER=gcc');

        if (toolchain) {
            configure.push(`-DCMAKE_PREFIX_PATH=${slashes(path.dirname(toolchain))}`);
        }
    }

    return [
        { kind: 'run', title: 'Configuring the engine', command: 'cmake', args: configure, cwd: project.root },
        { kind: 'run', title: 'Building the engine', command: 'cmake', args: ['--build', paths.engineBuild, '-j', '8'], cwd: project.root },
    ];
}

function listFiles(folder) {
    if (!fs.existsSync(folder)) {
        return [];
    }

    return fs.readdirSync(folder, { withFileTypes: true }).sort((a, b) => (a.name < b.name ? -1 : 1)).flatMap((entry) => {
        const full = path.join(folder, entry.name);
        return entry.isDirectory() ? listFiles(full) : [full];
    });
}

// the archive is flat: the first file with a name wins, so a project can replace an engine shader
function packList(project) {
    const paths = projectPaths(project);
    const compiled = path.join(paths.output, 'main.blit');
    const manifest = path.join(paths.output, 'manifest.txt');
    const own = listFiles(paths.assets).filter((file) => path.basename(file).toLowerCase() !== 'manifest.txt');
    const taken = new Set(own.map((file) => path.basename(file).toLowerCase()));
    const fromEngine = listFiles(path.join(project.engine, 'assets', 'shaders')).concat(path.join(project.engine, 'blitfs', 'arial.ttf'))
        .filter((file) => !taken.has(path.basename(file).toLowerCase()));

    return [compiled, manifest, ...own, ...fromEngine];
}

// the engine build puts every DLL the game needs next to it, an engine from before that has none
function engineComplete(project) {
    const paths = projectPaths(project);
    const programs = [paths.game, paths.compiler, paths.packer];

    if (!programs.every((file) => fs.existsSync(file))) {
        return false;
    }

    return process.platform !== 'win32' || fs.existsSync(path.join(paths.engineBuild, 'SDL2.dll'));
}

// everything the game needs to run, so the build folder works on a computer without the toolchain
function runtimeFiles(project) {
    const paths = projectPaths(project);
    const libraries = process.platform === 'win32'
        ? fs.readdirSync(paths.engineBuild).filter((name) => name.toLowerCase().endsWith('.dll')).map((name) => path.join(paths.engineBuild, name))
        : [];

    return [paths.game, ...libraries];
}

function projectSteps(project) {
    const paths = projectPaths(project);
    const scripts = path.dirname(paths.main);
    const steps = [
        { kind: 'folder', title: 'Preparing the build folder', path: paths.output },
        { kind: 'copy', title: 'Updating engine.bs', from: path.join(project.engine, 'blitscript', 'engine.bs'), to: path.join(scripts, 'engine.bs') },
        { kind: 'run', title: 'Compiling scripts', command: paths.compiler, args: ['-i', paths.main, '-o', path.join(paths.output, 'main.blit')], cwd: scripts },
        { kind: 'write', title: 'Writing the manifest', path: path.join(paths.output, 'manifest.txt'), text: 'Main-Script: main.blit\n' },
        { kind: 'remove', title: 'Removing the old archive', path: paths.archive },
    ];

    // blitfs_pack stores the file name after the last forward slash
    return steps.concat({
        kind: 'pack',
        title: 'Packing Engine.blfs',
        packer: paths.packer,
        archive: slashes(paths.archive),
        cwd: paths.output,
        files: () => packList(project).map(slashes),
    }, {
        kind: 'runtime',
        title: 'Copying the game and its DLLs',
        folder: paths.output,
        files: () => runtimeFiles(project),
    });
}

module.exports = { buildPath, findToolchain, engineSteps, projectSteps, packList, listFiles, engineComplete, runtimeFiles };
