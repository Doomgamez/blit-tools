'use strict';

// node download.js <folder with @vscode/test-electron in node_modules> <cache folder>
// fetches a private copy of VS Code for the tests and prints where its executable is,
// so they don't depend on the installed one, which may be busy updating

const path = require('path');

const [modules, cachePath] = process.argv.slice(2);
const { downloadAndUnzipVSCode } = require(path.join(modules, 'node_modules', '@vscode', 'test-electron'));

downloadAndUnzipVSCode({ version: 'stable', cachePath }).then((executable) => console.log(executable), (error) => {
    console.error(error.message);
    process.exit(1);
});
