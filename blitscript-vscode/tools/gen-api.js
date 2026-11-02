'use strict';

// node tools/gen-api.js <path to the engine repo>
// refreshes data/api.json, the API the extension falls back on outside the engine's workspace

const fs = require('fs');
const path = require('path');
const { parseCheatsheet, parseConstants } = require('../src/core/api');

const engine = process.argv[2];

if (!engine) {
    console.error('usage: node tools/gen-api.js <engine repo>');
    process.exit(1);
}

const functions = parseCheatsheet(fs.readFileSync(path.join(engine, 'docs', 'cheatsheet.txt'), 'utf8'));
const constants = parseConstants(fs.readFileSync(path.join(engine, 'blitscript', 'engine.bs'), 'utf8'));
const output = path.join(__dirname, '..', 'data', 'api.json');

fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, `${JSON.stringify({ functions, constants }, null, 1)}\n`);
console.log(`${Object.keys(functions).length} functions, ${Object.keys(constants).length} constants`);
