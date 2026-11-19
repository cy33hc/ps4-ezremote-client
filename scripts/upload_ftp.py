#!/usr/bin/env python3
"""Upload the web assets and daemon payloads to the PS4 over FTP.

Usage:
  upload_ftp.py <ftp-url> <remote-data-dir> <source-dir> [<source-dir> ...] [--dry-run]

The FTP URL supplies the host, port and optional credentials; each source
directory is uploaded to <remote-data-dir>/<source-basename>.
"""

import argparse
import ftplib
import os
import posixpath
import sys
from urllib.parse import unquote, urlparse


def build_jobs(remote_base, sources):
    jobs = []
    for src in sources:
        src = os.path.normpath(src)
        if not os.path.isdir(src):
            continue
        remote_root = posixpath.join(remote_base, os.path.basename(src))
        for root, _dirs, files in os.walk(src):
            rel = os.path.relpath(root, src)
            remote_dir = remote_root if rel == "." else posixpath.join(
                remote_root, rel.replace(os.sep, "/"))
            for name in sorted(files):
                jobs.append((os.path.join(root, name),
                             posixpath.join(remote_dir, name)))
    return jobs


def ensure_dir(ftp, remote_dir):
    current = ""
    for part in remote_dir.split("/"):
        if not part:
            continue
        current += "/" + part
        try:
            ftp.mkd(current)
        except ftplib.error_perm as exc:
            # 550 normally means the directory already exists.
            if not str(exc).startswith("550"):
                raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("url")
    parser.add_argument("remote_dir")
    parser.add_argument("sources", nargs="+")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    jobs = build_jobs(args.remote_dir.rstrip("/"), args.sources)
    for local, remote in jobs:
        print("{} -> {}".format(local, remote))

    if args.dry_run or not jobs:
        return 0

    parsed = urlparse(args.url)
    if parsed.scheme != "ftp" or not parsed.hostname:
        print("error: expected an ftp:// URL, got {}".format(args.url),
              file=sys.stderr)
        return 1

    user = unquote(parsed.username) if parsed.username else "anonymous"
    password = unquote(parsed.password) if parsed.password else "anonymous@"

    ftp = ftplib.FTP()
    ftp.encoding = "utf-8"
    try:
        ftp.connect(parsed.hostname, parsed.port or 21, timeout=30)
        ftp.login(user, password)
        ftp.set_pasv(True)

        current_dir = None
        for local, remote in jobs:
            remote_dir = posixpath.dirname(remote)
            if remote_dir != current_dir:
                ensure_dir(ftp, remote_dir)
                current_dir = remote_dir
            with open(local, "rb") as handle:
                ftp.storbinary("STOR " + remote, handle, blocksize=64 * 1024)
            print("uploaded {}".format(remote))

        ftp.quit()
    except ftplib.all_errors as exc:
        print("error: FTP upload failed: {}".format(exc), file=sys.stderr)
        try:
            ftp.close()
        except Exception:
            pass
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
