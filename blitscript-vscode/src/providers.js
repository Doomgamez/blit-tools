'use strict';

const vscode = require('vscode');
const { loadProgram } = require('./core/program');
const { parseDocument } = require('./core/document');
const { files } = require('./host');
const { complete } = require('./features/completion');
const { hover } = require('./features/hover');
const { signatureHelp } = require('./features/signature');
const { definition, references, canRename } = require('./features/navigation');
const { symbols, foldingRanges } = require('./features/outline');
const { colors, colorText } = require('./features/colors');
const { inlayHints } = require('./features/inlay');

const SELECTOR = { language: 'blitscript' };

const COMPLETION_KINDS = {
    keyword: vscode.CompletionItemKind.Keyword,
    native: vscode.CompletionItemKind.Function,
    function: vscode.CompletionItemKind.Method,
    label: vscode.CompletionItemKind.Reference,
    variable: vscode.CompletionItemKind.Variable,
    constant: vscode.CompletionItemKind.Constant,
    constructor: vscode.CompletionItemKind.Struct,
    register: vscode.CompletionItemKind.Field,
    file: vscode.CompletionItemKind.File,
};

const SYMBOL_KINDS = {
    entry: vscode.SymbolKind.Event,
    function: vscode.SymbolKind.Function,
    label: vscode.SymbolKind.Key,
    constant: vscode.SymbolKind.Constant,
};

// the model of a document and of everything it includes, an unsaved file has no includes to follow
function open(document) {
    if (document.uri.scheme !== 'file') {
        const model = parseDocument(document.getText(), document.uri.toString());
        return { program: { entry: model.path, documents: new Map([[model.path, model]]) }, model };
    }

    const program = loadProgram(document.uri.fsPath, files);
    return { program, model: program.documents.get(program.entry) };
}

function toLocation(item) {
    return new vscode.Location(vscode.Uri.file(item.file), new vscode.Range(item.line, item.start, item.line, item.end));
}

function completionProvider(state) {
    return {
        provideCompletionItems(document, position) {
            const { program, model } = open(document);

            return complete(program, model, position.line, position.character, state.api, files).map((item) => {
                const completion = new vscode.CompletionItem(item.label, COMPLETION_KINDS[item.kind]);

                completion.detail = item.detail;
                completion.documentation = item.documentation ? new vscode.MarkdownString(item.documentation) : undefined;
                completion.sortText = item.sortText;
                completion.insertText = item.snippet ? new vscode.SnippetString(item.snippet) : item.label;
                return completion;
            });
        },
    };
}

function hoverProvider(state) {
    return {
        provideHover(document, position) {
            const { program, model } = open(document);
            const result = hover(program, model, position.line, position.character, state.api);

            return result ? new vscode.Hover(new vscode.MarkdownString(result.contents),
                new vscode.Range(position.line, result.start, position.line, result.end)) : null;
        },
    };
}

function signatureProvider(state) {
    return {
        provideSignatureHelp(document, position) {
            const { program, model } = open(document);
            const result = signatureHelp(program, model, position.line, position.character, state.api);

            if (!result) {
                return null;
            }

            const info = new vscode.SignatureInformation(result.label, result.doc);
            info.parameters = result.params.map((param) => new vscode.ParameterInformation(param.label));

            const help = new vscode.SignatureHelp();
            help.signatures = [info];
            help.activeSignature = 0;
            help.activeParameter = result.active;
            return help;
        },
    };
}

const definitionProvider = {
    provideDefinition(document, position) {
        const { program, model } = open(document);
        const result = definition(program, model, position.line, position.character);

        return result ? toLocation(result) : null;
    },
};

const referenceProvider = {
    provideReferences(document, position) {
        const { program, model } = open(document);
        return references(program, model, position.line, position.character).map(toLocation);
    },
};

const renameProvider = {
    prepareRename(document, position) {
        const { program, model } = open(document);
        const result = canRename(program, model, position.line, position.character);

        if (!result) {
            throw new Error('This name can\'t be renamed');
        }

        return { range: new vscode.Range(position.line, result.start, position.line, result.end), placeholder: result.name };
    },

    provideRenameEdits(document, position, newName) {
        const { program, model } = open(document);
        const edit = new vscode.WorkspaceEdit();

        for (const item of references(program, model, position.line, position.character)) {
            edit.replace(vscode.Uri.file(item.file), new vscode.Range(item.line, item.start, item.line, item.end), newName);
        }

        return edit;
    },
};

const symbolProvider = {
    provideDocumentSymbols(document) {
        return symbols(parseDocument(document.getText(), document.uri.toString())).map((item) => {
            const range = new vscode.Range(item.line, 0, item.endLine, Number.MAX_SAFE_INTEGER);
            const selection = new vscode.Range(item.line, item.token.start, item.line, item.token.end);

            return new vscode.DocumentSymbol(item.name, item.detail, SYMBOL_KINDS[item.kind], document.validateRange(range), selection);
        });
    },
};

const foldingProvider = {
    provideFoldingRanges(document) {
        return foldingRanges(parseDocument(document.getText(), document.uri.toString())).map((range) => new vscode.FoldingRange(
            range.start, range.end, range.kind === 'comment' ? vscode.FoldingRangeKind.Comment : vscode.FoldingRangeKind.Region));
    },
};

const colorProvider = {
    provideDocumentColors(document) {
        return colors(parseDocument(document.getText(), document.uri.toString())).map((color) => new vscode.ColorInformation(
            new vscode.Range(color.line, color.start, color.line, color.end),
            new vscode.Color(color.r / 255, color.g / 255, color.b / 255, color.a / 255)));
    },

    provideColorPresentations(color) {
        return [new vscode.ColorPresentation(colorText(color.red, color.green, color.blue, color.alpha))];
    },
};

function inlayProvider(state) {
    return {
        provideInlayHints(document, range) {
            if (!vscode.workspace.getConfiguration('blitscript').get('inlayHints.parameterNames')) {
                return [];
            }

            const { program, model } = open(document);

            return inlayHints(program, model, range.start.line, range.end.line, state.api).map((hint) => {
                const item = new vscode.InlayHint(new vscode.Position(hint.line, hint.character), hint.label, vscode.InlayHintKind.Parameter);

                item.paddingRight = true;
                return item;
            });
        },
    };
}

function registerProviders(state) {
    const { languages } = vscode;

    return [
        languages.registerCompletionItemProvider(SELECTOR, completionProvider(state), ' ', ',', '"'),
        languages.registerHoverProvider(SELECTOR, hoverProvider(state)),
        languages.registerSignatureHelpProvider(SELECTOR, signatureProvider(state), ' ', ','),
        languages.registerDefinitionProvider(SELECTOR, definitionProvider),
        languages.registerReferenceProvider(SELECTOR, referenceProvider),
        languages.registerRenameProvider(SELECTOR, renameProvider),
        languages.registerDocumentSymbolProvider(SELECTOR, symbolProvider),
        languages.registerFoldingRangeProvider(SELECTOR, foldingProvider),
        languages.registerColorProvider(SELECTOR, colorProvider),
        languages.registerInlayHintsProvider(SELECTOR, inlayProvider(state)),
    ];
}

module.exports = { registerProviders };
