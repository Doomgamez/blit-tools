'use strict';

(function sidebar() {
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');

    function element(tag, className, text) {
        const node = document.createElement(tag);

        if (className) node.className = className;
        if (text !== undefined) node.textContent = text;
        return node;
    }

    function button(label, command, options) {
        const node = element('button', options && options.primary ? 'primary' : '', label);

        node.disabled = Boolean(options && options.disabled);
        node.addEventListener('click', () => vscode.postMessage({ type: 'command', command }));
        return node;
    }

    function statusLine(state, text, title, action) {
        const line = element('div', 'status');
        const body = element('span', 'text', text);

        body.title = title || text;
        line.append(element('span', `dot ${state}`), body);

        if (action) {
            line.append(button(action.label, action.command));
        }

        return line;
    }

    function folderLine(label, folder, error, command) {
        const name = folder ? folder.split(/[\\/]/).filter(Boolean).pop() : 'not set';

        return statusLine(error ? 'bad' : '', `${label}: ${name}`, error || folder, { label: 'Change', command });
    }

    function welcome(status) {
        root.append(
            element('p', 'muted', 'Blit Engine projects build and run from here, with your own scripts and assets.'),
            button('New Project', 'blitscript.newProject', { primary: true, disabled: Boolean(status.busy) }),
            button('Open Project Folder', 'vscode.openFolder'),
        );
    }

    function projectView(status) {
        const working = Boolean(status.busy);
        const blocked = working || Boolean(status.engineError);

        root.append(
            element('h2', '', status.project.name),
            folderLine('Engine', status.project.engine, status.engineError, 'blitscript.chooseEngine'),
            folderLine('Tools', status.project.tools, status.toolsError, 'blitscript.chooseTools'),
            statusLine(status.engineBuilt ? '' : 'waiting', status.engineBuilt ? 'Engine built' : 'Engine not built yet, the first build takes a few minutes'),
        );

        root.append(element('h2', '', 'Game'));

        if (status.running) {
            root.append(button('Stop', 'blitscript.stopProject', { primary: true }));
        } else {
            root.append(button('Build and Run', 'blitscript.runProject', { primary: true, disabled: blocked }));
        }

        root.append(button('Build', 'blitscript.buildProject', { disabled: blocked || status.running }));

        root.append(
            element('h2', '', 'Tools'),
            button('Rebuild Engine', 'blitscript.buildEngine', { disabled: blocked || status.running }),
            button('Open Build in BLITFS Viewer', 'blitscript.openArchive', { disabled: !status.packed || Boolean(status.toolsError) }),
            button('Open API Cheatsheet', 'blitscript.openCheatsheet'),
        );

        if (status.busy) {
            root.append(element('p', 'busy', `${status.busy}...`));
        }
    }

    window.addEventListener('message', (event) => {
        if (event.data.type !== 'status') {
            return;
        }

        root.textContent = '';

        if (event.data.status.project) {
            projectView(event.data.status);
        } else {
            welcome(event.data.status);
        }
    });

    vscode.postMessage({ type: 'ready' });
}());
