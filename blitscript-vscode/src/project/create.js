'use strict';

const fs = require('fs');
const path = require('path');
const { PROJECT_FILE, LOCAL_FILE, validate, writeLocal } = require('./layout');

const EMPTY_SCRIPT = `INCLUDE "engine.bs"

MAIN LBL main

LBL frame
    CALL ClearBackground COLOR 24 26 32 255
    CALL DrawText "Hello from BlitScript" 40 40 32 RAYWHITE

    CALL quitOnEscape

    CALL WaitFrame
    JMP frame

END

FN quitOnEscape
    CALL IsKeyPressed KEY_ESCAPE -> escape
    CMP escape true
    JNE stay
        CALL Quit
    LBL stay
END
`;

// the two bigger templates are the engine's own scripts, so they always match its API
const TEMPLATES = {
    empty: {
        title: 'Empty',
        description: 'A window, some text and a frame loop.',
        script: () => EMPTY_SCRIPT,
        assets: [],
    },
    twoD: {
        title: '2D',
        description: 'Shapes, sprites, text and a 2D camera.',
        script: (engine) => fs.readFileSync(path.join(engine, 'blitscript', 'examples', '2d.bs'), 'utf8')
            .replace('INCLUDE "../engine.bs"', 'INCLUDE "engine.bs"'),
        assets: ['assets/textures/checker.png'],
    },
    threeD: {
        title: '3D walking',
        description: 'A first-person walk around a glTF scene with a door.',
        script: (engine) => fs.readFileSync(path.join(engine, 'blitscript', 'main.bs'), 'utf8'),
        assets: ['assets/textures/checker.png', 'assets/models/house.glb'],
    },
};

const GITIGNORE = `build/
.blit/
${LOCAL_FILE}
scripts/engine.bs
`;

function createProject(values) {
    const { errors, ok } = validate(values);

    if (!ok) {
        throw new Error(Object.values(errors).find(Boolean));
    }

    const template = TEMPLATES[values.template];

    if (!template) {
        throw new Error('Choose a template');
    }

    const root = path.join(values.location, values.name);
    const script = template.script(values.engine);

    fs.mkdirSync(path.join(root, 'scripts'), { recursive: true });
    fs.mkdirSync(path.join(root, 'assets'), { recursive: true });

    fs.writeFileSync(path.join(root, PROJECT_FILE), `${JSON.stringify({ name: values.name, main: 'scripts/main.bs' }, null, 4)}\n`);
    writeLocal(root, { engine: values.engine, tools: values.tools });
    fs.writeFileSync(path.join(root, '.gitignore'), GITIGNORE);
    fs.writeFileSync(path.join(root, 'scripts', 'main.bs'), script);
    fs.copyFileSync(path.join(values.engine, 'blitscript', 'engine.bs'), path.join(root, 'scripts', 'engine.bs'));

    for (const asset of template.assets) {
        fs.copyFileSync(path.join(values.engine, asset), path.join(root, 'assets', path.basename(asset)));
    }

    return root;
}

function templateList() {
    return Object.entries(TEMPLATES).map(([id, template]) => ({ id, title: template.title, description: template.description }));
}

module.exports = { createProject, templateList };
