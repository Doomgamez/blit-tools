'use strict';

const crypto = require('crypto');
const vscode = require('vscode');

// the shared shell of the webviews: only this extension's css and one script may run
function page(webview, extensionUri, script, body) {
    const nonce = crypto.randomBytes(16).toString('hex');
    const media = (name) => webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, 'media', name));

    return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource}; script-src 'nonce-${nonce}';">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="${media('ui.css')}">
</head>
<body>
${body}
<script nonce="${nonce}" src="${media(script)}"></script>
</body>
</html>`;
}

module.exports = { page };
