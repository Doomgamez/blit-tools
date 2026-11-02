'use strict';

const vscode = require('vscode');
const { signature } = require('../core/api');
const { nativeSnippet } = require('../features/completion');

function functionsByCategory(api) {
    const groups = new Map();

    for (const fn of Object.values(api.functions)) {
        if (!groups.has(fn.category)) {
            groups.set(fn.category, []);
        }

        groups.get(fn.category).push(fn);
    }

    return groups;
}

// browses the engine's functions, clicking one writes the CALL into the open script
class ApiTree {
    constructor(state) {
        this.state = state;
        this.changed = new vscode.EventEmitter();
        this.onDidChangeTreeData = this.changed.event;
    }

    refresh() {
        this.changed.fire();
    }

    getChildren(element) {
        if (!element) {
            return [...functionsByCategory(this.state.api).keys()].map((category) => ({ category }));
        }

        return element.category ? functionsByCategory(this.state.api).get(element.category).map((fn) => ({ fn })) : [];
    }

    getTreeItem(element) {
        if (element.category) {
            return new vscode.TreeItem(element.category, vscode.TreeItemCollapsibleState.Collapsed);
        }

        const item = new vscode.TreeItem(element.fn.name);

        item.description = element.fn.doc;
        item.tooltip = new vscode.MarkdownString(`\`${signature(element.fn)}\`\n\n${element.fn.doc}`);
        item.iconPath = new vscode.ThemeIcon('symbol-function');
        item.contextValue = 'apiFunction';
        item.command = { command: 'blitscript.insertFunction', title: 'Insert', arguments: [element.fn] };
        return item;
    }
}

async function insertFunction(fn) {
    const editor = vscode.window.activeTextEditor;

    if (!editor || editor.document.languageId !== 'blitscript') {
        vscode.window.showInformationMessage(`${signature(fn)} — open a .bs file to insert it.`);
        return false;
    }

    const arrow = fn.returns === 'void' ? '' : ' -> ${0:result}';
    return editor.insertSnippet(new vscode.SnippetString(`CALL ${nativeSnippet(fn)}${arrow}`));
}

async function searchApi(state) {
    const items = Object.values(state.api.functions).map((fn) => ({ label: fn.name, description: fn.category, detail: fn.doc, fn }));
    const picked = await vscode.window.showQuickPick(items, { placeHolder: 'Find an engine function', matchOnDescription: true, matchOnDetail: true });

    if (picked) {
        await insertFunction(picked.fn);
    }
}

module.exports = { ApiTree, insertFunction, searchApi };
