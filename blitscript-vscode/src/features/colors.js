'use strict';

const CHANNEL = /^\d+$/;

// the literal COLOR r g b a values in a document, for the color swatches and the picker
function colors(document) {
    const out = [];

    document.lines.forEach((line, index) => {
        const { tokens } = line;

        for (let i = 0; i + 4 < tokens.length; i++) {
            const channels = tokens.slice(i + 1, i + 5);

            if (tokens[i].quoted || tokens[i].text.toUpperCase() !== 'COLOR' ||
                !channels.every((token) => !token.quoted && CHANNEL.test(token.text) && Number(token.text) <= 255)) {
                continue;
            }

            const [r, g, b, a] = channels.map((token) => Number(token.text));
            out.push({ line: index, start: tokens[i].start, end: channels[3].end, r, g, b, a });
        }
    });

    return out;
}

function colorText(r, g, b, a) {
    const byte = (value) => Math.max(0, Math.min(255, Math.round(value * 255)));
    return `COLOR ${byte(r)} ${byte(g)} ${byte(b)} ${byte(a)}`;
}

module.exports = { colors, colorText };
