'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFile } = require('child_process');
const { parseCompilerOutput, parseCodeSize } = require('./core/compilerOutput');
const { buildPath } = require('./project/build');

// compiles into a temp file: checking a script should never replace a build's output
function compile(compiler, file, toolchain) {
    const stem = path.join(os.tmpdir(), `blitscript-${process.pid}-${path.basename(file)}`);
    const output = `${stem}.blit`;
    const key = Object.keys(process.env).find((name) => name.toUpperCase() === 'PATH') || 'PATH';
    // C blocks need gcc
    const env = Object.assign({}, process.env, { [key]: buildPath(process.env[key], toolchain) });

    return new Promise((resolve) => {
        execFile(compiler, ['-i', file, '-o', output], { cwd: path.dirname(file), timeout: 15000, env }, (error, stdout, stderr) => {
            for (const made of [output, `${stem}.natives.c`, `${stem}.natives.dll`, `${stem}.natives.so`]) {
                fs.rm(made, { force: true }, () => {});
            }

            // ENOENT and friends: the compiler never ran, as opposed to it finding a mistake
            if (error && typeof error.code !== 'number') {
                resolve({ ran: false, reason: error.message, errors: [] });
                return;
            }

            const text = `${stdout}\n${stderr}`;
            const errors = error ? parseCompilerOutput(text) : [];

            resolve({
                ran: true,
                ok: !error,
                errors,
                raw: text.trim(),
                codeSize: parseCodeSize(text),
                directory: path.dirname(file),
            });
        });
    });
}

module.exports = { compile };
