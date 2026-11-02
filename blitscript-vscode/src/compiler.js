'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFile } = require('child_process');
const { parseCompilerOutput, parseCodeSize } = require('./core/compilerOutput');

// compiles into a temp file: checking a script should never replace a build's output
function compile(compiler, file) {
    const output = path.join(os.tmpdir(), `blitscript-${process.pid}-${path.basename(file)}.blit`);

    return new Promise((resolve) => {
        execFile(compiler, ['-i', file, '-o', output], { cwd: path.dirname(file), timeout: 15000 }, (error, stdout, stderr) => {
            fs.rm(output, { force: true }, () => {});

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
