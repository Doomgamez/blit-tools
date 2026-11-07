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
    level: {
        title: 'Level editor level',
        description: 'A room made in the Blit level editor: lights, a trigger, a door and plated walls, run by classes.',
        script: (engine) => fs.readFileSync(path.join(engine, 'blitscript', 'examples', 'scripted.bs'), 'utf8')
            .replace('INCLUDE "../level.bs"', 'INCLUDE "level.bs"'),
        // [from in the engine, to in the project]: level textures go in assets/textures, the level names
        // the model by its path from the project's root
        assets: [
            ['levels/testmap.blvl', 'levels/testmap.blvl'],
            ['assets/models/house.glb', 'assets/models/house.glb'],
            ...['checker', 'panel', 'panel_normal', 'panel_rough', 'clip', 'nodraw', 'skip', 'trigger']
                .map((name) => [`assets/textures/${name}.png`, `assets/textures/${name}.png`]),
        ],
        needs: ['blitscript/level.bs', 'levels/testmap.blvl'],
    },
    level2d: {
        title: '2D level editor level',
        description: 'A platformer on a 2D level from the Blit level editor: platforms that can be turned, a goal and a sprite.',
        script: (engine) => fs.readFileSync(path.join(engine, 'blitscript', 'examples', 'platformer.bs'), 'utf8')
            .replace('INCLUDE "../level2d.bs"', 'INCLUDE "level2d.bs"'),
        assets: [
            ['levels/level2d.blvl', 'levels/level2d.blvl'],
            ['assets/textures/checker.png', 'assets/textures/checker.png'],
        ],
        needs: ['blitscript/level2d.bs', 'blitscript/examples/platformer.bs', 'levels/level2d.blvl'],
    },
};

const GITIGNORE = `build/
.blit/
${LOCAL_FILE}
scripts/engine.bs
scripts/level.bs
scripts/level2d.bs
scripts/levelsprite.bs
scripts/leveltext.bs
scripts/levelnodes2d.bs
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

    // a template that needs newer engine files says so before anything is written
    const missing = (template.needs || []).find((file) => !fs.existsSync(path.join(values.engine, file)));

    if (missing) {
        throw new Error(`The ${template.title} template needs ${missing} from a newer engine`);
    }

    const root = path.join(values.location, values.name);
    const script = template.script(values.engine);
    const levelLibrary = path.join(values.engine, 'blitscript', 'level.bs');

    fs.mkdirSync(path.join(root, 'scripts'), { recursive: true });
    fs.mkdirSync(path.join(root, 'assets'), { recursive: true });

    fs.writeFileSync(path.join(root, PROJECT_FILE), `${JSON.stringify({ name: values.name, main: 'scripts/main.bs' }, null, 4)}\n`);
    writeLocal(root, { engine: values.engine, tools: values.tools });
    fs.writeFileSync(path.join(root, '.gitignore'), GITIGNORE);
    fs.writeFileSync(path.join(root, 'scripts', 'main.bs'), script);
    fs.copyFileSync(path.join(values.engine, 'blitscript', 'engine.bs'), path.join(root, 'scripts', 'engine.bs'));

    // every build refreshes them too, copying them now lets the editor find them before the first build
    for (const library of [levelLibrary, ...['leveltext.bs', 'level2d.bs', 'levelsprite.bs', 'levelnodes2d.bs'].map((name) => path.join(values.engine, 'blitscript', name))]) {
        if (fs.existsSync(library)) {
            fs.copyFileSync(library, path.join(root, 'scripts', path.basename(library)));
        }
    }

    // a plain path lands in assets/ by its name, a [from, to] pair where it says
    for (const asset of template.assets) {
        const [from, to] = Array.isArray(asset) ? asset : [asset, path.join('assets', path.basename(asset))];
        const target = path.join(root, to);

        fs.mkdirSync(path.dirname(target), { recursive: true });
        fs.copyFileSync(path.join(values.engine, from), target);
    }

    return root;
}

function templateList() {
    return Object.entries(TEMPLATES).map(([id, template]) => ({ id, title: template.title, description: template.description }));
}

module.exports = { createProject, templateList };
