'use strict';

const path = require('path');
const vscode = require('vscode');
const { loadProgram } = require('./core/program');
const { lint } = require('./core/linter');
const { compile } = require('./compiler');
const { files, findCompiler, projectAssetNames } = require('./host');

const LINT_DELAY = 250;

const SEVERITIES = [
    vscode.DiagnosticSeverity.Error,
    vscode.DiagnosticSeverity.Warning,
    vscode.DiagnosticSeverity.Information,
    vscode.DiagnosticSeverity.Hint,
];

function lintOptions() {
    const settings = vscode.workspace.getConfiguration('blitscript.lint');

    return {
        unknownVariables: settings.get('unknownVariables'),
        customNatives: settings.get('customNatives'),
        exposedVariables: settings.get('exposedVariables'),
        assets: projectAssetNames(),
    };
}

function toDiagnostic(item) {
    const range = new vscode.Range(item.line, item.start, item.line, item.end);
    const diagnostic = new vscode.Diagnostic(range, item.message, SEVERITIES[item.severity]);

    diagnostic.source = 'BlitScript';

    if (item.unnecessary) {
        diagnostic.tags = [vscode.DiagnosticTag.Unnecessary];
    }

    return diagnostic;
}

function compilerDiagnostic(error, document) {
    const line = document && error.line < document.lineCount ? document.lineAt(Math.max(error.line, 0)) : null;
    const range = line ? line.range : new vscode.Range(Math.max(error.line, 0), 0, Math.max(error.line, 0), 1);
    const diagnostic = new vscode.Diagnostic(range, error.message, vscode.DiagnosticSeverity.Error);

    diagnostic.source = 'blitscriptc';
    return diagnostic;
}

class Diagnostics {
    constructor(state) {
        this.state = state;
        this.lintCollection = vscode.languages.createDiagnosticCollection('blitscript');
        this.compileCollection = vscode.languages.createDiagnosticCollection('blitscriptc');
        this.timers = new Map();
        this.status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 10);
        this.status.command = 'blitscript.compile';
    }

    schedule(document) {
        const key = document.uri.toString();

        clearTimeout(this.timers.get(key));
        this.timers.set(key, setTimeout(() => this.lint(document), LINT_DELAY));
    }

    lint(document) {
        if (document.languageId !== 'blitscript' || document.uri.scheme !== 'file') {
            return;
        }

        const program = loadProgram(document.uri.fsPath, files);
        this.lintCollection.set(document.uri, lint(program, this.state.api, lintOptions()).map(toDiagnostic));
    }

    showResult(document, result) {
        const byFile = new Map();

        for (const error of result.errors) {
            const file = path.resolve(result.directory, error.file);

            if (!byFile.has(file)) {
                byFile.set(file, []);
            }

            const open = vscode.workspace.textDocuments.find((item) => item.uri.fsPath.toLowerCase() === file.toLowerCase());
            byFile.get(file).push(compilerDiagnostic(error, open));
        }

        this.compileCollection.clear();

        for (const [file, diagnostics] of byFile) {
            this.compileCollection.set(vscode.Uri.file(file), diagnostics);
        }

        // a failure the output doesn't place on a line would otherwise look like nothing happened
        if (!result.ok && byFile.size === 0) {
            this.compileCollection.set(document.uri, [compilerDiagnostic({ line: 0, message: result.raw || 'blitscriptc failed' }, document)]);
        }
    }

    async compile(document, explicit) {
        const compiler = findCompiler();

        if (!compiler) {
            if (explicit) {
                vscode.window.showWarningMessage('BlitScript: blitscriptc not found, set blitscript.compilerPath.');
            }
            this.status.hide();
            return null;
        }

        const result = await compile(compiler, document.uri.fsPath);

        if (!result.ran) {
            this.status.text = '$(warning) BlitScript: compiler did not run';
            this.status.tooltip = result.reason;
            this.status.show();

            if (explicit) {
                vscode.window.showErrorMessage(`BlitScript: could not run blitscriptc: ${result.reason}`);
            }
            return result;
        }

        this.showResult(document, result);
        this.status.text = result.ok ? `$(check) BlitScript: ${result.codeSize} bytes` : '$(error) BlitScript: failed';
        this.status.tooltip = result.raw;
        this.status.show();
        return result;
    }

    dispose() {
        this.timers.forEach((timer) => clearTimeout(timer));
        this.lintCollection.dispose();
        this.compileCollection.dispose();
        this.status.dispose();
    }
}

module.exports = { Diagnostics };
