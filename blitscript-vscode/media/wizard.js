'use strict';

(function wizard() {
    const vscode = acquireVsCodeApi();
    const form = document.getElementById('form');
    const create = document.getElementById('create');
    const failure = document.getElementById('failure');
    const fields = ['name', 'location', 'engine', 'tools'];
    const touched = new Set();
    let template = 'threeD';
    let timer = null;

    function values() {
        const out = { template };

        for (const field of fields) {
            out[field] = document.getElementById(field).value;
        }

        return out;
    }

    function validate() {
        vscode.postMessage({ type: 'validate', values: values() });
    }

    function showTemplates(list) {
        const holder = document.getElementById('templates');

        holder.textContent = '';

        for (const item of list) {
            const label = document.createElement('label');
            const input = document.createElement('input');
            const title = document.createElement('strong');
            const text = document.createElement('span');

            label.className = `template${item.id === template ? ' selected' : ''}`;
            input.type = 'radio';
            input.name = 'template';
            input.value = item.id;
            input.checked = item.id === template;
            title.textContent = item.title;
            text.className = 'muted';
            text.textContent = item.description;

            input.addEventListener('change', () => {
                template = item.id;
                holder.querySelectorAll('.template').forEach((card) => card.classList.toggle('selected', card === label));
            });

            label.append(input, title, text);
            holder.append(label);
        }
    }

    // an empty form shouldn't open with red everywhere, a field only complains once it's been used
    function showValidation(result) {
        for (const field of fields) {
            const input = document.getElementById(field);
            const error = result.errors[field];
            const shown = touched.has(field) || input.value !== '' ? error : null;

            document.getElementById(`${field}Error`).textContent = shown || '';
            input.classList.toggle('invalid', Boolean(shown));
            input.setAttribute('aria-invalid', shown ? 'true' : 'false');
        }

        create.disabled = !result.ok;
    }

    for (const field of fields) {
        const input = document.getElementById(field);

        input.addEventListener('input', () => {
            touched.add(field);
            clearTimeout(timer);
            timer = setTimeout(validate, 120);
        });

        // blur fires whenever the window or the webview loses focus, so it can't count as using a field
        input.addEventListener('blur', validate);
    }

    document.querySelectorAll('[data-browse]').forEach((button) => {
        button.addEventListener('click', () => vscode.postMessage({ type: 'browse', field: button.dataset.browse }));
    });

    form.addEventListener('submit', (event) => {
        event.preventDefault();

        if (!create.disabled) {
            create.disabled = true;
            failure.textContent = '';
            vscode.postMessage({ type: 'create', values: values() });
        }
    });

    document.getElementById('cancel').addEventListener('click', () => vscode.postMessage({ type: 'cancel' }));

    window.addEventListener('message', (event) => {
        const message = event.data;

        if (message.type === 'init') {
            showTemplates(message.templates);

            for (const field of fields) {
                document.getElementById(field).value = message.values[field] || '';
            }

            validate();
        } else if (message.type === 'field') {
            document.getElementById(message.field).value = message.value;
            touched.add(message.field);
            validate();
        } else if (message.type === 'validation') {
            showValidation(message.result);
        } else if (message.type === 'failed') {
            failure.textContent = message.message;
            validate();
        }
    });

    vscode.postMessage({ type: 'ready' });
    document.getElementById('name').focus();
}());
