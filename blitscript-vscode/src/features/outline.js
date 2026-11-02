'use strict';

function symbols(document) {
    const out = [];

    for (const main of document.mains) {
        out.push({ name: main.name, detail: 'MAIN', kind: 'entry', line: main.line, endLine: main.line, token: main.token });
    }

    for (const fn of document.functions) {
        out.push({
            name: fn.name,
            detail: fn.params.map((param) => param.name).join(' '),
            kind: 'function',
            line: fn.line,
            endLine: fn.endLine >= 0 ? fn.endLine : fn.line,
            token: fn.token,
        });
    }

    for (const item of document.classes) {
        out.push({
            name: item.name,
            detail: item.parentName ? `extends ${item.parentName}` : 'class',
            kind: 'class',
            line: item.line,
            endLine: item.endLine >= 0 ? item.endLine : item.line,
            token: item.token,
        });

        for (const method of item.methods) {
            out.push({
                name: `${item.name}.${method.name}`,
                detail: method.params.slice(1).map((param) => param.name).join(' '),
                kind: 'method',
                line: method.line,
                endLine: method.endLine >= 0 ? method.endLine : method.line,
                token: method.token,
            });
        }
    }

    for (const label of document.labels) {
        if (!document.mains.some((main) => main.line === label.line)) {
            out.push({ name: label.name, detail: 'label', kind: 'label', line: label.line, endLine: label.line, token: label.token });
        }
    }

    for (const constant of document.constants) {
        out.push({ name: constant.name, detail: constant.value, kind: 'constant', line: constant.line, endLine: constant.line, token: constant.token });
    }

    return out.sort((a, b) => a.line - b.line);
}

// function bodies, loops, runs of comment lines and what sits under a label in a loop
function foldingRanges(document) {
    const ranges = [];

    for (const fn of document.functions) {
        if (fn.endLine > fn.line) {
            ranges.push({ start: fn.line, end: fn.endLine, kind: 'region' });
        }
    }

    const blocks = [...document.classes.flatMap((item) => [item, ...item.methods]), ...document.cBlocks];

    for (const block of blocks) {
        if (block.endLine > block.line) {
            ranges.push({ start: block.line, end: block.endLine, kind: 'region' });
        }
    }

    for (const loop of document.loops) {
        if (loop.endLine > loop.line) {
            ranges.push({ start: loop.line, end: loop.endLine, kind: 'region' });
        }
    }

    let commentStart = -1;

    document.lines.forEach((line, index) => {
        const comment = line.tokens.length === 0 && line.commentStart >= 0;

        if (comment && commentStart < 0) {
            commentStart = index;
        } else if (!comment && commentStart >= 0) {
            if (index - 1 > commentStart) {
                ranges.push({ start: commentStart, end: index - 1, kind: 'comment' });
            }
            commentStart = -1;
        }
    });

    return ranges;
}

module.exports = { symbols, foldingRanges };
