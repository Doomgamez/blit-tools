'use strict';

// what the compiler knows by itself, the docs show up in hovers and completion

const JUMPS = {
    JMP: 'Always jumps to the label.',
    JZ: 'Jumps if the last CMP was equal, or the last arithmetic result was 0.',
    JE: 'Jumps if a == b after `CMP a b`.',
    JNE: 'Jumps if a != b after `CMP a b`.',
    JS: 'Jumps if the last result was negative.',
    JNS: 'Jumps if the last result was not negative.',
    JG: 'Jumps if a > b after `CMP a b`.',
    JGE: 'Jumps if a >= b after `CMP a b`.',
    JL: 'Jumps if a < b after `CMP a b`.',
    JLE: 'Jumps if a <= b after `CMP a b`.',
    JA: 'Jumps if a > b, comparing as unsigned.',
    JAE: 'Jumps if a >= b, comparing as unsigned.',
    JB: 'Jumps if a < b, comparing as unsigned.',
    JBE: 'Jumps if a <= b, comparing as unsigned.',
};

const ARITHMETIC = {
    ADD: '`ADD dst value`: dst = dst + value. Sets the flags from the result.',
    SUB: '`SUB dst value`: dst = dst - value. Sets the flags, so `SUB n 1` then `JNE loop` loops until n is 0.',
    MUL: '`MUL dst value`: dst = dst * value. Sets the flags from the result.',
    DIV: '`DIV dst value`: integers divide like C, dividing by 0 stops the script.',
    MOD: '`MOD dst value`: remainder. Sets the flags from the result.',
};

const STATEMENTS = {
    SET: '`SET name value...`: creates or updates a variable, up to 32 values. Inside a function a new name becomes a local.',
    LOCAL: '`LOCAL name value...`: like SET, but always stays in the function, even if a global has the same name.',
    MOV: '`MOV register value`: stores one value in EAX, EBX, ECX or EDX.',
    READ: '`READ dst src index`: dst = src[index], index 0..255.',
    CALL: '`CALL fn args -> result`: calls an engine function or a script function. `-> result` is optional.',
    CMP: '`CMP a b`: compares two values, the jump after it decides.',
    RET: 'Returns from a function, END does too.',
    END: 'Stops the script, or closes a function.',
    FOR: '`FOR variable start end [step]`: counts from start up to end, not including it, until ENDFOR. A negative step counts down.',
    FOREACH: '`FOREACH variable values`: runs once for every value of an array, a list or the values written here, until ENDFOR.',
    ENDFOR: 'Closes a FOR or FOREACH.',
    BREAK: 'Leaves the innermost loop.',
    CONTINUE: 'Goes on with the next turn of the innermost loop.',
    IF: '`IF condition`: runs the lines up to ELSEIF, ELSE or ENDIF when the condition holds. `a < b`, `a == "text"`, `x != null`, `AND`, `OR`, `NOT`, parentheses.',
    ELSEIF: '`ELSEIF condition`: when no branch above ran and this condition holds. `ELSE IF` works too.',
    ELSE: 'When no branch above ran.',
    ENDIF: 'Closes an IF.',
    WHILE: '`WHILE condition`: runs the lines up to ENDWHILE again and again while the condition holds. BREAK and CONTINUE work in it.',
    ENDWHILE: 'Closes a WHILE.',
    CLASS: '`CLASS Name [EXTENDS Parent]`: a class, with FIELD and METHOD lines, closed by ENDCLASS.',
    ENDCLASS: 'Closes a CLASS.',
    FIELD: '`FIELD name [value]`: a value every object of the class has, one value of any kind, null if none is given.',
    METHOD: '`METHOD name params...`: a function of a class, closed by END. `this` is the object. A method called Init runs on NEW.',
    NEW: '`NEW Class [values] -> variable`: makes an object, the values go to its Init method.',
    NEWOF: '`NEWOF className [values] -> variable`: like NEW, but the class is a value, such as a string from a level\'s "script" property. The name is case sensitive, an unknown one stops the script.',
    RETURN: '`RETURN [value]`: leaves a function or method, `CALL fn -> result` gets the value.',
    ENDC: 'Closes a C block.',
    LBL: '`LBL name`: a jump target.',
    MAIN: '`MAIN LBL name`: where the script starts, exactly one per program.',
    FN: '`FN name params...`: a function, closed by END. Parameters are locals.',
    CONST: '`CONST NAME value`: replaced at compile time, costs nothing at runtime.',
    ONCE: 'At the top of a file: an INCLUDE of this file after the first does nothing, like `#pragma once`.',
    INCLUDE: '`INCLUDE "file.bs"`: pastes another file in here, the path is relative to this file.',
};

const KEYWORDS = Object.assign({}, STATEMENTS, ARITHMETIC, JUMPS);

// how many values follow each constructor
const CONSTRUCTORS = {
    VEC2: 2, VEC3: 3, VEC4: 4,
    IVEC2: 2, IVEC3: 3, IVEC4: 4,
    UVEC2: 2, UVEC3: 3, UVEC4: 4,
    QUAT: 4, RECT: 4, COLOR: 4, MAT4: 16,
};

const REGISTERS = ['EAX', 'EBX', 'ECX', 'EDX'];
const VALUE_WORDS = ['true', 'false', 'null'];

const NUMBER = /^[+-]?(0x[0-9a-f]+|\d+\.?\d*(e[+-]?\d+)?|\.\d+(e[+-]?\d+)?)(u64|i64|u|f|d)?$/i;

const isKeyword = (text) => Object.prototype.hasOwnProperty.call(KEYWORDS, text.toUpperCase());
const isJump = (text) => Object.prototype.hasOwnProperty.call(JUMPS, text.toUpperCase());
const isArithmetic = (text) => Object.prototype.hasOwnProperty.call(ARITHMETIC, text.toUpperCase());
const isRegister = (text) => REGISTERS.includes(text.toUpperCase());
const isValueWord = (text) => VALUE_WORDS.includes(text.toLowerCase());
const isNumber = (text) => NUMBER.test(text);
const constructorArity = (text) => CONSTRUCTORS[text.toUpperCase()];

module.exports = {
    KEYWORDS, JUMPS, ARITHMETIC, STATEMENTS, CONSTRUCTORS, REGISTERS, VALUE_WORDS,
    isKeyword, isJump, isArithmetic, isRegister, isValueWord, isNumber, constructorArity,
};
