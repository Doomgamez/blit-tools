'use strict';

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

function runProcess(command, args, options, log) {
    return new Promise((resolve) => {
        let child;

        try {
            child = spawn(command, args, { cwd: options.cwd, env: options.env });
        } catch (error) {
            log(`could not start ${command}: ${error.message}`);
            resolve(false);
            return;
        }

        child.stdout.on('data', (data) => log(String(data).trimEnd()));
        child.stderr.on('data', (data) => log(String(data).trimEnd()));
        child.on('error', (error) => {
            log(`could not start ${command}: ${error.message}`);
            resolve(false);
        });
        child.on('close', (code) => resolve(code === 0));
    });
}

async function runStep(step, env, log) {
    switch (step.kind) {
        case 'run':
            return runProcess(step.command, step.args, { cwd: step.cwd, env }, log);

        case 'folder':
            fs.mkdirSync(step.path, { recursive: true });
            return true;

        case 'copy':
            fs.copyFileSync(step.from, step.to);
            return true;

        case 'write':
            fs.writeFileSync(step.path, step.text);
            return true;

        case 'remove':
            fs.rmSync(step.path, { force: true });
            return true;

        case 'runtime':
            for (const file of step.files()) {
                fs.copyFileSync(file, path.join(step.folder, path.basename(file)));
            }
            return true;

        case 'pack':
            for (const file of step.files()) {
                if (!await runProcess(step.packer, ['-m', '0', '-i', file, '-o', step.archive], { cwd: step.cwd, env }, () => {})) {
                    log(`could not pack ${file}`);
                    return false;
                }
            }
            return true;

        default:
            throw new Error(`unknown build step ${step.kind}`);
    }
}

// stops at the first failure, onStep gets the title of each step as it starts
async function runSteps(steps, env, log, onStep) {
    for (const step of steps) {
        onStep(step.title);
        log(`> ${step.title}`);

        try {
            if (!await runStep(step, env, log)) {
                log(`failed: ${step.title}`);
                return false;
            }
        } catch (error) {
            log(`failed: ${step.title}: ${error.message}`);
            return false;
        }
    }

    return true;
}

module.exports = { runSteps, runProcess };
