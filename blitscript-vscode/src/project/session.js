'use strict';

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const vscode = require('vscode');
const { readProject, projectPaths, checkEngine, checkTools } = require('./layout');
const { buildPath, findToolchain, engineSteps, projectSteps } = require('./build');
const { runSteps, runProcess } = require('./runner');

// the open project and what's running for it, the sidebar draws itself from this
class ProjectSession {
    constructor() {
        this.output = vscode.window.createOutputChannel('Blit Engine');
        this.changed = new vscode.EventEmitter();
        this.onDidChange = this.changed.event;
        this.busy = '';
        this.game = null;
    }

    get project() {
        for (const folder of vscode.workspace.workspaceFolders || []) {
            const project = readProject(folder.uri.fsPath);

            if (project) {
                return project;
            }
        }

        return null;
    }

    status() {
        const { project } = this;

        if (!project) {
            return { project: null, busy: this.busy };
        }

        const paths = projectPaths(project);

        return {
            project: { name: project.name, engine: project.engine, tools: project.tools, main: project.main },
            engineError: checkEngine(project.engine),
            toolsError: checkTools(project.tools),
            engineBuilt: fs.existsSync(paths.game) && fs.existsSync(paths.compiler),
            packed: fs.existsSync(paths.archive),
            running: this.game !== null,
            busy: this.busy,
        };
    }

    log(text) {
        this.output.appendLine(text);
    }

    env() {
        const toolchain = findToolchain(vscode.workspace.getConfiguration('blitscript').get('toolchainPath'));
        const key = Object.keys(process.env).find((name) => name.toUpperCase() === 'PATH') || 'PATH';

        return { toolchain, env: Object.assign({}, process.env, { [key]: buildPath(process.env[key], toolchain) }) };
    }

    setBusy(text) {
        this.busy = text;
        this.changed.fire();
    }

    usable() {
        const { project } = this;

        if (!project) {
            vscode.window.showInformationMessage('Blit Engine: open a project first.');
            return null;
        }

        const error = checkEngine(project.engine);

        if (error) {
            vscode.window.showErrorMessage(`Blit Engine: ${error}`);
            return null;
        }

        return project;
    }

    async runWithStatus(steps, env) {
        if (this.busy) {
            vscode.window.showInformationMessage(`Blit Engine: ${this.busy}, wait for it to finish.`);
            return false;
        }

        this.output.show(true);

        const ok = await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification, title: 'Blit Engine' }, (progress) =>
            runSteps(steps, env, (text) => this.log(text), (title) => {
                progress.report({ message: title });
                this.setBusy(title);
            }));

        this.setBusy('');

        if (!ok) {
            vscode.window.showErrorMessage('Blit Engine: the build failed, see the output.');
        }

        return ok;
    }

    async buildEngine() {
        const project = this.usable();

        if (!project) {
            return false;
        }

        const { toolchain, env } = this.env();
        return this.runWithStatus(engineSteps(project, toolchain), env);
    }

    // the engine is only built when it's missing, that takes minutes and a project build seconds
    async build() {
        const project = this.usable();

        if (!project) {
            return false;
        }

        const { toolchain, env } = this.env();
        const paths = projectPaths(project);
        const needsEngine = !fs.existsSync(paths.game) || !fs.existsSync(paths.compiler) || !fs.existsSync(paths.packer);
        const steps = (needsEngine ? engineSteps(project, toolchain) : []).concat(projectSteps(project));

        return this.runWithStatus(steps, env);
    }

    async run() {
        if (this.game) {
            vscode.window.showInformationMessage('Blit Engine: the game is already running.');
            return true;
        }

        if (!await this.build()) {
            return false;
        }

        const { project } = this;
        const paths = projectPaths(project);
        const { env } = this.env();
        const game = spawn(paths.game, [], { cwd: paths.output, env });

        this.game = game;
        this.finished = new Promise((resolve) => {
            const done = (code) => {
                this.game = null;
                this.log(`> The game exited (${code})`);
                this.changed.fire();
                resolve(code);
            };

            game.once('error', (error) => {
                this.log(`could not start the game: ${error.message}`);
                done(-1);
            });
            game.once('close', done);
        });

        game.stdout.on('data', (data) => this.log(String(data).trimEnd()));
        game.stderr.on('data', (data) => this.log(String(data).trimEnd()));
        this.changed.fire();
        return true;
    }

    stop() {
        if (this.game) {
            this.game.kill();
        }
    }

    python() {
        return vscode.workspace.getConfiguration('blitscript').get('pythonPath') || 'python';
    }

    tools() {
        const { project } = this;
        const error = project ? checkTools(project.tools) : 'Open a project first';

        if (error) {
            vscode.window.showErrorMessage(`Blit Engine: ${error}`);
            return null;
        }

        return project.tools;
    }

    async inspectModel(file) {
        const tools = this.tools();

        if (!tools) {
            return false;
        }

        const folder = path.join(tools, 'gltf-inspector');

        this.output.show(true);
        this.log(`> Inspecting ${path.basename(file)}`);

        const { env } = this.env();
        return runProcess(this.python(), [path.join(folder, 'main.py'), file], { cwd: folder, env }, (text) => this.log(text));
    }

    openArchive() {
        const tools = this.tools();
        const { project } = this;

        if (!tools) {
            return;
        }

        const { archive } = projectPaths(project);

        if (!fs.existsSync(archive)) {
            vscode.window.showInformationMessage('Blit Engine: build the project first.');
            return;
        }

        const folder = path.join(tools, 'blitfs-viewer');
        const viewer = spawn(this.python(), [path.join(folder, 'main.py'), archive], { cwd: folder, env: this.env().env, detached: true, stdio: 'ignore' });

        viewer.on('error', (error) => vscode.window.showErrorMessage(`Blit Engine: could not start the viewer: ${error.message}`));
        viewer.unref();
    }

    dispose() {
        this.stop();
        this.output.dispose();
        this.changed.dispose();
    }
}

module.exports = { ProjectSession };
