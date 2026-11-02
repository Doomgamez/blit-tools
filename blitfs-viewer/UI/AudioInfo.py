"""Reads what an audio file says about itself. No FLTK here, so it can be tested on its own."""
import struct

MP3_BITRATES = (0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320)
MP3_RATES = (44100, 48000, 32000)


def audio_kind(data):
    if data[:4] == b"RIFF" and data[8:12] == b"WAVE":
        return "wav"

    if data[:4] == b"OggS":
        return "opus" if b"OpusHead" in data[:256] else "ogg"

    if data[:4] == b"fLaC":
        return "flac"

    if data[:3] == b"ID3" or _mp3_header(data) is not None:
        return "mp3"

    return None


def _wav(data):
    pos = 12
    info = {}

    while pos + 8 <= len(data):
        tag = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]

        if tag == b"fmt " and pos + 24 <= len(data):
            _, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", data, pos + 8)
            info.update(channels=channels, rate=rate, bits=bits)
        elif tag == b"data" and "rate" in info and info["bits"]:
            per_second = info["rate"] * info["channels"] * info["bits"] // 8

            if per_second:
                info["seconds"] = min(size, len(data) - pos - 8) / per_second

        pos += 8 + size + (size & 1)        # chunks are padded to an even size

    return info


def _ogg(data, kind):
    if kind == "opus":
        at = data.find(b"OpusHead")
        info = {"channels": data[at + 9], "rate": 48000} if at + 10 <= len(data) else {}
    else:
        at = data.find(b"\x01vorbis")
        info = {}

        if at >= 0 and at + 16 <= len(data):
            info = {"channels": data[at + 11], "rate": struct.unpack_from("<I", data, at + 12)[0]}

    last = data.rfind(b"OggS")

    # the last page says how many samples came before its end
    if info.get("rate") and last >= 0 and last + 14 <= len(data):
        info["seconds"] = struct.unpack_from("<q", data, last + 6)[0] / info["rate"]

    return info


def _flac(data):
    if len(data) < 42:
        return {}

    packed = int.from_bytes(data[18:26], "big")
    rate = packed >> 44
    info = {"rate": rate, "channels": ((packed >> 41) & 7) + 1, "bits": ((packed >> 36) & 31) + 1}
    samples = packed & ((1 << 36) - 1)

    if rate and samples:
        info["seconds"] = samples / rate

    return info


def _mp3_header(data, pos=0):
    if pos + 4 > len(data):
        return None

    a, b, c, d = data[pos:pos + 4]

    # MPEG-1 layer III, the one a game's files are in practice
    if a != 0xFF or (b & 0xFE) != 0xFA:
        return None

    bitrate = (c >> 4) & 15
    rate = (c >> 2) & 3

    if bitrate in (0, 15) or rate == 3:
        return None

    return {"bitrate": MP3_BITRATES[bitrate], "rate": MP3_RATES[rate], "channels": 1 if d >> 6 == 3 else 2}


def _mp3(data):
    pos = 0

    if data[:3] == b"ID3" and len(data) >= 10:
        pos = 10 + ((data[6] << 21) | (data[7] << 14) | (data[8] << 7) | data[9])

    info = None

    for start in range(pos, min(pos + 4096, len(data))):
        info = _mp3_header(data, start)

        if info:
            pos = start
            break

    if not info:
        return {}

    # exact for a constant bitrate, an estimate for a variable one
    info["seconds"] = (len(data) - pos) * 8 / (info["bitrate"] * 1000)
    return info


def read_info(data, kind):
    try:
        if kind == "wav":
            return _wav(data)

        if kind in ("ogg", "opus"):
            return _ogg(data, kind)

        return _flac(data) if kind == "flac" else _mp3(data)
    except (struct.error, IndexError):
        return {}


def duration_text(seconds):
    minutes, rest = divmod(seconds, 60)
    return f"{int(minutes)}:{rest:05.2f}"


def describe(data, kind):
    """The lines shown for a file, whatever could be read from it."""
    info = read_info(data, kind)
    lines = [f"{kind.upper()} audio"]

    if not info:
        lines.append("Its header could not be read, the file may be damaged.")
        return lines

    if "seconds" in info:
        lines.append(f"Length: {duration_text(info['seconds'])}")

    if "rate" in info:
        lines.append(f"Sample rate: {info['rate']:,} Hz")

    if "channels" in info:
        lines.append({1: "Mono", 2: "Stereo"}.get(info["channels"], f"{info['channels']} channels"))

    if "bits" in info:
        lines.append(f"{info['bits']}-bit")

    if "bitrate" in info:
        lines.append(f"{info['bitrate']} kbit/s")

    return lines
