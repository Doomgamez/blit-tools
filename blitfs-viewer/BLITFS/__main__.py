"""Command line BLITFS tool.

    python -m BLITFS list    game.bfs
    python -m BLITFS extract game.bfs out_dir
    python -m BLITFS pack    game.bfs folder_to_pack
"""
import argparse
import os
import sys

import BLITFS


def cmd_list(args):
    fs = BLITFS.Open(args.archive)

    for f in fs:
        print(f"{f.size:>10}  {f.name}")

    print(f"{fs.size} file(s)")


def cmd_extract(args):
    written, skipped = BLITFS.Extract(BLITFS.Open(args.archive), args.out)

    for path in written:
        print(os.path.relpath(path, args.out))

    for name in skipped:
        print(f"skipping unsafe name {name!r}", file=sys.stderr)


def cmd_pack(args):
    files = []

    for folder, _, names in os.walk(args.folder):
        for n in sorted(names):
            full = os.path.join(folder, n)
            rel = os.path.relpath(full, args.folder).replace(os.sep, "/")

            with open(full, "rb") as fh:
                files.append(BLITFS.FSInput(rel, fh.read()))

    BLITFS.Save(args.archive, BLITFS.FS(files))
    print(f"packed {len(files)} file(s) into {args.archive}")


def main():
    ap = argparse.ArgumentParser(prog="python -m BLITFS")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("list")
    p.add_argument("archive")
    p.set_defaults(run=cmd_list)

    p = sub.add_parser("extract")
    p.add_argument("archive")
    p.add_argument("out")
    p.set_defaults(run=cmd_extract)

    p = sub.add_parser("pack")
    p.add_argument("archive")
    p.add_argument("folder")
    p.set_defaults(run=cmd_pack)

    args = ap.parse_args()

    try:
        args.run(args)
    except BLITFS.BlitFSError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
