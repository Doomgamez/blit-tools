"""Decide how to preview a file's bytes. No FLTK here, so it can be tested on its own."""

TEXT_LIMIT = 1 << 20        # show at most 1 MiB of text
HEX_LIMIT = 64 << 10        # and at most 64 KiB as hex; bigger files get a note at the end

IMAGE_MAGIC = {
    b"\x89PNG\r\n\x1a\n": "png",
    b"\xff\xd8\xff": "jpeg",
    b"GIF87a": "gif",
    b"GIF89a": "gif",
    b"BM": "bmp",
}


def image_kind(data):
    for magic, kind in IMAGE_MAGIC.items():
        if data.startswith(magic):
            return kind

    return None


def as_text(data):
    """Decoded text when the bytes look like text, else None."""
    if b"\x00" in data[:4096]:
        return None

    head = data[:TEXT_LIMIT]

    try:
        text = head.decode("utf-8")
    except UnicodeDecodeError as e:
        if e.start < len(head) - 4:         # a real error, not a character cut in half by the limit
            return None

        text = data[:e.start].decode("utf-8")

    sample = text[:4096]
    printable = sum(c.isprintable() or c in "\r\n\t" for c in sample)

    if sample and printable / len(sample) < 0.95:
        return None

    if len(data) > TEXT_LIMIT:
        text += f"\n\n... ({len(data) - TEXT_LIMIT:,} more bytes not shown)"

    return text


def hexdump(data):
    lines = []

    for off in range(0, min(len(data), HEX_LIMIT), 16):
        chunk = data[off:off + 16]

        hexes = " ".join(f"{b:02x}" for b in chunk)
        ascii_ = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)

        lines.append(f"{off:08x}  {hexes:<47}  |{ascii_}|")

    if len(data) > HEX_LIMIT:
        lines.append(f"... ({len(data) - HEX_LIMIT:,} more bytes not shown)")

    return "\n".join(lines) or "(empty file)"


def human_size(n):
    if n < 1024:
        return f"{n} B"

    for unit in ("KB", "MB"):
        n /= 1024

        if n < 1024 or unit == "MB":
            return f"{n:.1f} {unit}"
