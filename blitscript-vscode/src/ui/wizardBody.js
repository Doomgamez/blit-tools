'use strict';

function folderField(id, label, help, placeholder) {
    return `
        <div class="field">
            <label for="${id}">${label}</label>
            <div class="help">${help}</div>
            <div class="row">
                <input id="${id}" type="text" spellcheck="false" placeholder="${placeholder}" aria-describedby="${id}Error">
                <button type="button" data-browse="${id}" aria-label="Browse for ${label}">Browse</button>
            </div>
            <div id="${id}Error" class="error" role="alert"></div>
        </div>`;
}

const WIZARD_BODY = `
<main class="wizard">
    <h1>New Blit Engine Project</h1>
    <p class="muted">Your game gets its own folder. The engine and tools stay where they are, and all projects share them.</p>

    <form id="form" novalidate>
        <div class="field">
            <label for="name">Project name</label>
            <input id="name" type="text" spellcheck="false" placeholder="My Game" aria-describedby="nameError">
            <div id="nameError" class="error" role="alert"></div>
        </div>
        ${folderField('location', 'Location', 'The project folder is created inside this one.', 'Choose a folder')}

        <div class="field">
            <span class="legend" id="templateLabel">Template</span>
            <div id="templates" class="templates" role="radiogroup" aria-labelledby="templateLabel"></div>
        </div>

        <h2>Required folders</h2>
        ${folderField('engine', 'homeinvasionsdl', 'The engine repository. Your game is built with it.', 'Path to homeinvasionsdl')}
        ${folderField('tools', 'blit-tools', 'The tools repository, for inspecting models and opening archives.', 'Path to blit-tools')}

        <div class="actions">
            <button id="create" type="submit" class="primary" disabled>Create Project</button>
            <button id="cancel" type="button">Cancel</button>
        </div>
        <div id="failure" class="error" role="alert"></div>
    </form>
</main>`;

module.exports = { WIZARD_BODY };
