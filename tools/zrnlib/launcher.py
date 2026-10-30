# SPDX-License-Identifier: GPL-2.0-or-later
"""zrn-exec: run a .zexec.

The signature is checked (against the keys this user and the system trust)
before anything in the package runs, and again whenever the file changes;
the package is unpacked once into ~/.cache/zrn/apps/<id>/<sha256> and run
from there. What it prints goes to ~/.local/state/zrn/<id>.log when there is
no terminal.
"""
import hashlib
import json
import os
import sys
from pathlib import Path

from . import zexec
from .cli import SDK, cache_home, host_arch, state_home, tell_shell, verify_package
from .errors import ZrnError


def _fail(m, title, text, code):
    print(f"zrn-exec: {text}", file=sys.stderr)
    name = (m or {}).get("name") or "App"
    tell_shell(f"notify {name}|{title}|{text.replace('|', '/')[:300]}")
    return code


def _verified(path, st):
    """The sha256 recorded for this exact file (inode, size, mtime) if it was verified before."""
    key = hashlib.sha256(f"{path}:{st.st_dev}:{st.st_ino}:{st.st_size}:{st.st_mtime_ns}".encode()).hexdigest()
    rec = cache_home() / "verified" / key
    return rec, (rec.read_text().strip() if rec.exists() else None)


def launch(file, argv):
    path = os.path.abspath(file)
    try:
        st = os.stat(path)
    except OSError as e:
        print(f"zrn-exec: {path}: {e.strerror}", file=sys.stderr)
        return 127
    rec, sha = _verified(path, st)
    m = None
    if sha is None:
        try:
            m, problems, signer, sig_error, sha = verify_package(path)
        except ZrnError as e:
            return _fail(None, "Cannot start", f"{path}: {e.message}", 126)
        if problems:
            return _fail(m, "Broken app", "; ".join(problems), 126)
        if sig_error:
            return _fail(m, "Not trusted", f"{sig_error}. It was not started.", 126)
        rec.parent.mkdir(parents=True, exist_ok=True)
        rec.write_text(sha + "\n")
    with zexec.open_zexec(path) as z:
        m = m or zexec.read_manifest(z)
    if m["arch"] not in ("any", host_arch()):
        return _fail(m, "Cannot start", f"built for {m['arch']}, this phone is {host_arch()}", 126)
    root = cache_home() / "apps" / m["id"] / sha[:32]
    if not (root / zexec.MANIFEST).exists():
        root.parent.mkdir(parents=True, exist_ok=True)
        zexec.extract(path, str(root))
    env = dict(os.environ)
    libs = [str(root / "lib")] if (root / "lib").is_dir() else []
    tree = SDK / "libzrnui" / "build"
    if not libs and not Path("/usr/lib/libzrnui.so.0").exists() and (tree / "libzrnui.so.0").exists():
        libs.append(str(tree))     # the SDK's own build, on a development machine
    if libs:
        env["LD_LIBRARY_PATH"] = ":".join(libs + ([env["LD_LIBRARY_PATH"]] if env.get("LD_LIBRARY_PATH") else []))
        env["ZRNUI_LIB"] = str(Path(libs[0]) / "libzrnui.so.0")
    if (root / "share" / "zrnui" / "fonts").is_dir():
        env["ZUI_DATA_DIR"] = str(root / "share" / "zrnui")
    env["ZRN_APP_ID"] = m["id"]
    env["ZRN_APP_DIR"] = str(root)
    entry = str(root / m["entry"])
    if m["runtime"] == "python3":
        pyp = [str(root / "app")]
        if (root / "python").is_dir():
            pyp.append(str(root / "python"))
        elif (SDK / "python" / "zrnui").is_dir():
            pyp.append(str(SDK / "python"))
        env["PYTHONPATH"] = ":".join(pyp + ([env["PYTHONPATH"]] if env.get("PYTHONPATH") else []))
        cmd = [sys.executable, "-s", entry, *argv]
    else:
        cmd = [entry, *argv]
    if not sys.stderr.isatty() and not os.environ.get("ZRN_EXEC_NO_LOG"):
        logdir = state_home()
        logdir.mkdir(parents=True, exist_ok=True)
        log = logdir / f"{m['id']}.log"
        if log.exists():
            os.replace(log, str(log) + ".1")
        fd = os.open(log, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
        os.dup2(fd, 1)
        os.dup2(fd, 2)
        os.close(fd)
    os.chdir(root)
    os.execve(cmd[0], cmd, env)
