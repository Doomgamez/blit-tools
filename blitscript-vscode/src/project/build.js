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

// written after a build that worked. the programs can't tell: make only relinks the ones whose sources changed
const engineStamp = (paths) => path.join(paths.engineBuild, 'engine.built');

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
        { kind: 'write', title: 'Noting the engine build', path: engineStamp(paths), text: `${new Date().toISOString()}\n` },
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

// levels from the Blit level editor (blit-tools/blit-editor) in levels/, each becomes <name>.lvl.glb in .blit/stage,
// is packed into Engine.blfs, and LoadLevel "name.blvl" finds it
function levelMaps(project) {
    return listFiles(projectPaths(project).levels).filter((file) => /\.blvl$/i.test(file));
}

const compiledLevel = (paths, map) => path.join(paths.stage, `${path.basename(map).replace(/\.blvl$/i, '')}.lvl.glb`);

// the archive is flat: the first file with a name wins, so a project can replace an engine shader
function packList(project) {
    const paths = projectPaths(project);
    const compiled = path.join(paths.stage, 'main.blit');
    const manifest = path.join(paths.stage, 'manifest.txt');
    const own = listFiles(paths.assets).filter((file) => path.basename(file).toLowerCase() !== 'manifest.txt');
    // only the maps that still exist: a deleted map's old .lvl.glb stays out
    const levels = levelMaps(project).map((map) => compiledLevel(paths, map)).filter((file) => fs.existsSync(file));
    const taken = new Set(own.map((file) => path.basename(file).toLowerCase()));
    const fromEngine = listFiles(path.join(project.engine, 'assets', 'shaders')).concat(path.join(project.engine, 'blitfs', 'arial.ttf'))
        .filter((file) => !taken.has(path.basename(file).toLowerCase()));

    return [compiled, manifest, ...own, ...levels, ...fromEngine];
}

// a project keeps its own build of the engine, which goes out of date when the engine repository moves on
function engineChanged(project, paths) {
    const stamp = engineStamp(paths);

    if (!fs.existsSync(stamp)) {
        return true;
    }

    const built = fs.statSync(stamp).mtimeMs;
    const sources = ['src', 'blitscript', 'blitfs', 'cmake', path.join('tools', 'blitlevel')].flatMap((folder) => listFiles(path.join(project.engine, folder)))
        .filter((file) => /\.(c|h|cmake)$/i.test(file))
        .concat(path.join(project.engine, 'CMakeLists.txt'));

    return sources.some((file) => fs.existsSync(file) && fs.statSync(file).mtimeMs > built);
}

// the engine build puts every DLL the game needs next to it, an engine from before that has none
function engineComplete(project) {
    const paths = projectPaths(project);
    const programs = [paths.game, paths.compiler, paths.packer];

    // an engine that has the level compiler builds it, a build from before it is rebuilt
    if (fs.existsSync(path.join(project.engine, 'tools', 'blitlevel'))) {
        programs.push(paths.mapCompiler);
    }

    if (!programs.every((file) => fs.existsSync(file))) {
        return false;
    }

    if (process.platform === 'win32' && !fs.existsSync(path.join(paths.engineBuild, 'SDL2.dll'))) {
        return false;
    }

    return !engineChanged(project, paths);
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
    const levelLibrary = path.join(project.engine, 'blitscript', 'level.bs');
    const steps = [
        { kind: 'folder', title: 'Preparing the build folder', path: paths.output },
        { kind: 'folder', title: 'Preparing the stage folder', path: paths.stage },
        { kind: 'copy', title: 'Updating engine.bs', from: path.join(project.engine, 'blitscript', 'engine.bs'), to: path.join(scripts, 'engine.bs') },
    ];

    // INCLUDE "level.bs" finds it next to the scripts, like engine.bs. older engines don't have it
    if (fs.existsSync(levelLibrary)) {
        steps.push({ kind: 'copy', title: 'Updating level.bs', from: levelLibrary, to: path.join(scripts, 'level.bs') });
    }

    const level2d = path.join(project.engine, 'blitscript', 'level2d.bs');

    if (fs.existsSync(level2d)) {
        steps.push({ kind: 'copy', title: 'Updating level2d.bs', from: level2d, to: path.join(scripts, 'level2d.bs') });
    }

    steps.push({ kind: 'run', title: 'Compiling scripts', command: paths.compiler, args: ['-i', paths.main, '-o', path.join(paths.stage, 'main.blit')], cwd: scripts });

    // level textures live in assets/textures, loose ones in assets/ work too
    for (const map of levelMaps(project)) {
        steps.push({
            kind: 'run',
            title: `Compiling level ${path.basename(map)}`,
            command: paths.mapCompiler,
            args: ['-i', map, '-o', compiledLevel(paths, map), '-t', path.join(paths.assets, 'textures'), '-t', paths.assets],
            cwd: project.root,
        });
    }

    steps.push(
        { kind: 'write', title: 'Writing the manifest', path: path.join(paths.stage, 'manifest.txt'), text: 'Main-Script: main.blit\n' },
        { kind: 'remove', title: 'Removing the old archive', path: paths.archive },
    );

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

module.exports = { buildPath, findToolchain, engineSteps, projectSteps, packList, listFiles, engineComplete, runtimeFiles, levelMaps };
