# dgz's blitscript

Create, build and run Blit Engine games, with full language support for BlitScript (`.bs`). It has no dependencies.

> The Blit Engine isn't public yet. Editing `.bs` files works for everyone, with highlighting, completion and linting.
> Creating, building and running projects needs a copy of the engine.

## Projects

Click the **Blit Engine** icon in the activity bar, then **New Project**. The wizard asks for a name, a location, a
template and the two folders every project needs:

- **homeinvasionsdl**, the engine your game is built with
- **blit-tools**, for inspecting models and opening archives

Both are checked before you can create the project. A project looks like this:

```
MyGame/
    blit.project.json   the name and entry script, commit this
    blit.local.json     where the engine and tools are on this computer, git-ignored
    scripts/main.bs     your game
    assets/             your models, textures, shaders and audio
    .blit/engine/       the engine, built here so its repository stays clean
    build/              the game, ready to zip and ship
```

`build/` holds everything the game needs: `HomeInvasion.exe`, `Engine.blfs` and on Windows every DLL it uses, so it runs
on a computer without MSYS2.

The sidebar shows the project's state and runs **Build and Run**, **Stop**, **Build** and **Rebuild Engine**. The first
build compiles the engine, which takes a few minutes, later ones only take seconds. A file in `assets/` replaces an
engine file with the same name, so a project can use its own `basic.frag`. `scripts/engine.bs` is refreshed on every
build.

On Windows the engine builds with MSYS2's MinGW and SDL2. It's found in `C:\msys64\mingw64\bin`, set
`blitscript.toolchainPath` if yours is elsewhere. CMake has to be on your PATH.

**Tools:** right-click a `.glb` for **Inspect glTF Model**, which runs `gltf-inspector`, and use **Open Build in BLITFS
Viewer** to look inside the archive. The viewer needs `pyfltk`. Both use the `python` on your PATH, not the toolchain's,
and `blitscript.pythonPath` sets another one.

**Engine API:** the sidebar lists every engine function by category, clicking one writes its `CALL` into the open
script. The search button finds one by name or description.

## Language features

- **Highlighting** for instructions, labels, functions, constants, registers, `%d`-style placeholders and number suffixes.
- **Completion** that follows the statement: instructions, engine and script functions after `CALL`, labels after a jump,
  files in `INCLUDE`, values in arguments. Engine functions fill in their parameters.
- **Hovers** with signatures and docs, what a constant holds, and where a variable was first set.
- **Signature help** while typing `CALL` arguments, a `VEC3 1 2 3` counts as one argument.
- **Go to definition, references and rename** for labels, functions, constants and variables, through `INCLUDE`s.
  Labels and functions ignore case and variables don't, like the compiler. A parameter stays inside its function.
  Engine functions and the generated `engine.bs` can't be renamed.
- **Macros**: type one at the start of a line and press Enter to write it out, Tab moves between its values. Each one
  picks a free label, so you can use them as often as you like:
  - `if`, `ifne`, `iflt`, `ifle`, `ifgt`, `ifge`: run a block if a is ==, !=, <, <=, > or >= b
  - `ifelse`: one block if a == b, another otherwise
  - `for`, `foreach`, `while`, `repeat`: loops. The first three can run 0 times, `repeat` always runs at least once
  - `ifkey`, `ifpressed`: run a block if a key is held down, or went down this frame
  - `class`: a class with a field and an `Init`
- **Outline, folding and snippets**: try `main`, `fn`, `mode3d` and `scene`.
- **Color swatches and a picker** on `COLOR 24 26 32 255`.
- **Parameter names in front of arguments**, turn on `blitscript.inlayHints.parameterNames`.

## Finding mistakes

While you type it checks for what would otherwise only show up at runtime or in the build:

- unknown instructions, labels and functions, wrong argument counts and types
- variables that are never set, and `-NAME`, since BlitScript has no unary minus
- `RET` outside a function, nested `FN`, a second `MAIN`, duplicates, missing `INCLUDE` files, open strings
- loops without `ENDFOR`, `BREAK` outside a loop, a `FOR` step of 0
- classes: missing `ENDCLASS`, unknown parents, repeated fields, `this.` a field that doesn't exist, methods nothing has,
  and the wrong number of values for `NEW`. Fields of other objects are only known when the script runs
- in a project, assets that aren't in `assets/`, folders in asset names, and audio formats the engine can't play
- unreachable code, faded out, the lines the compiler's optimizer removes

When you save it runs `blitscriptc` and puts its errors on their lines. The status bar shows the code size, click it to
compile again. The output goes to a temp file, so a build is never touched.

## Setup

In a project everything comes from its engine folder, so new engine functions work right away after `make api_docs`.

You can also open the engine's repository itself. It then looks for `blitscriptc` in `build/` or `blitscriptc/`, or set
`blitscript.compilerPath`, and uses the repository's `docs/cheatsheet.txt` and `blitscript/engine.bs`. Outside both it
uses its own copy of the API.

Functions and variables the cheatsheet doesn't know about go in `blitscript.lint.customNatives` and
`blitscript.lint.exposedVariables`.

## Commands

- **BlitScript: Compile This File**, also the play button on a `.bs` tab
- **BlitScript: Open the API Cheatsheet**
- **BlitScript: Reload the Engine API**

## Development

`src/core` is the language, with no editor in it: the tokenizer follows `Tokenize` in `blitscriptc.c`. `src/features`
builds on it, each file is one feature. `src/project` creates and builds projects, `src/ui` and `media` are the sidebar
and wizard. `src/providers.js` and `src/extension.js` connect them to VS Code.

```
node tools/gen-api.js <engine repo>             refresh data/api.json
node test/core.test.js <engine repo>            the language and features, with no editor
node test/project.test.js <engine> <tools>      creating projects and planning builds, with no editor
node test/mutation.js <engine repo>             breaks the code on purpose, the tests have to notice
node test/grammar.test.js <folder>              the grammar, needs vscode-textmate and vscode-oniguruma installed there
```

`test/host/run.ps1 <engine> <tools> <folder with blitscriptc.exe> [Code.exe]` runs everything inside a real VS Code, up
to building and running a project. `test/host/download.js` fetches a private VS Code for it, which also works while the
installed one is updating. Set `SKIP_PROJECT_SUITE` to leave out the engine build.
