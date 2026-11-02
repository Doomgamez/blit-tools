'use strict';

// blitscriptc prints one error line, ending in "at file:line", "(file:line)" or "at file:line: detail"
const LOCATION = /^(.*?)(?:\s+at\s+|\s*\()(.+?):(\d+)(?:\)|: .*)?\s*$/;

// gcc, about a C block: its lines are the script's
const C_ERROR = /^(.+?):(\d+):\d+: (?:fatal )?error: (.*)$/;

function parseCompilerOutput(output) {
    const errors = [];

    for (const line of output.split(/\r?\n/)) {
        const fromC = C_ERROR.exec(line);

        if (fromC) {
            errors.push({ message: fromC[3], file: fromC[1], line: Number(fromC[2]) - 1 });
            continue;
        }

        const match = LOCATION.exec(line);

        if (match) {
            errors.push({ message: line.replace(/\s*\(.+?:\d+\)\s*$/, '').replace(/\s+at\s+.+?:\d+/, '').trim() || line, file: match[2], line: Number(match[3]) - 1 });
        }
    }

    return errors;
}

function parseCodeSize(output) {
    const match = /Code size: (\d+) bytes/.exec(output);
    return match ? Number(match[1]) : null;
}

module.exports = { parseCompilerOutput, parseCodeSize };
