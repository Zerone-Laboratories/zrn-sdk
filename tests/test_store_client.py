#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""`zrn store` against a local signed index: good, tampered, older and revoked."""
import hashlib, json, os, subprocess, sys, tempfile
from pathlib import Path
SDK = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(SDK / "tools"))
from zrnlib import sign, zexec  # noqa: E402

ZRN = [sys.executable, "-I", str(SDK / "tools" / "zrn")]
fails = 0
def check(ok, what):
    global fails
    print(("ok   " if ok else "FAIL ") + what)
    fails += not ok

t = Path(tempfile.mkdtemp(dir=os.environ.get("TMPDIR")))
home, idx = t / "home", t / "idx"
home.mkdir(); idx.mkdir()
key = sign.SecretKey.generate()
pub = t / "store.pub"
bad_key = sign.SecretKey.generate()
pub.write_text(key.public.dumps() if hasattr(key.public, "dumps") else "")
env = dict(os.environ, HOME=str(home), ZRN_STORE_INDEX=str(idx), ZRN_STORE_KEY=str(pub), ZRN_TRUSTED_KEYS=str(pub),
           XDG_CONFIG_HOME=str(home / ".c"), XDG_CACHE_HOME=str(home / ".k"), XDG_DATA_HOME=str(home / ".d"),
           XDG_STATE_HOME=str(home / ".s"))
env.pop("XDG_RUNTIME_DIR", None)
osr = t / "os-release"
osr.write_text('ID=ztos\nVERSION_ID="0.2.1-BETA-1.1"\n')
env["ZRN_OS_RELEASE"] = str(osr)
def z(*a):
    p = subprocess.run(ZRN + [*a, "--json"], capture_output=True, text=True, env=env)
    return p.returncode, json.loads(p.stdout or "{}")

# a package: the Python example, any CPU
pk = subprocess.run(ZRN + ["pack", "--no-sign", "--json", "-C", str(SDK / "examples/hello-py"), "--out", str(t)],
                    capture_output=True, text=True, env=env)
info = json.loads(pk.stdout)
pkg = Path(info["path"])
sha = info["sha256"]
with zexec.open_zexec(str(pkg)) as zf:
    m = zexec.read_manifest(zf)
Path(str(pkg) + ".minisig").write_text(sign.sign_file(key, str(pkg), zexec.trusted_comment(m, sha), "t"))

def publish(generated, revoked=(), signer=key, sha_in_index=None):
    rel = {"version": m["version"], "arch": m["arch"], "sha256": sha_in_index or sha, "size": pkg.stat().st_size,
           "url": str(pkg), "sig_url": str(pkg) + ".minisig", "permissions": []}
    index = {"format": 1, "generated": generated, "apps": [{"id": m["id"], "name": m["name"], "summary": "s", "author": "a",
             "licence": "MIT", "source": "https://github.com/x/y", "releases": [rel]}], "revoked": list(revoked)}
    p = idx / "index.json"
    p.write_text(json.dumps(index))
    h = hashlib.sha256(p.read_bytes()).hexdigest()
    (idx / "index.json.minisig").write_text(sign.sign_file(signer, str(p), f"zrn-index format=1 generated={generated} sha256={h}", "i"))

publish("2026-01-02T00:00:00Z", signer=bad_key)
rc, o = z("store", "refresh")
check(rc != 0 and o["error"]["code"] == "bad-index", "an index signed by another key is refused")
publish("2026-01-02T00:00:00Z", sha_in_index="0" * 64)
check(z("store", "refresh")[0] == 0, "good index taken")
rc, o = z("store", "install", m["id"])
check(rc != 0 and o["error"]["code"] == "bad-package", "a package differing from the index is refused")
publish("2026-01-03T00:00:00Z")
check(z("store", "refresh")[0] == 0, "newer index taken")
rc, o = z("store", "list")
check(rc == 0 and o["apps"][0]["id"] == m["id"] and not o["apps"][0]["installed"], "listed, not installed")
rc, o = z("store", "install", m["id"])
check(rc == 0, f"installed: {o.get('error')}")
check(z("store", "list")[1]["apps"][0]["installed"] == m["version"], "listed as installed")
app0 = home / ".d/zrn/apps" / m["id"] / f"{m['id']}.zexec"
osr.write_text('ID=ztos\nVERSION_ID="0.2.0-BETA-1"\n')
p = subprocess.run([sys.executable, "-I", str(SDK / "tools/zrn-exec"), str(app0)], capture_output=True, text=True, env=env)
check(p.returncode == 126 and "0.2.1" in p.stderr, "zrn-exec refuses it on an older ztOS and says why")
osr.write_text('ID=ztos\nVERSION_ID="0.2.1-BETA-1.1"\n')
publish("2026-01-01T00:00:00Z")
rc, o = z("store", "refresh")
check(rc != 0 and o["error"]["code"] == "old-index", "an older index is refused")
(idx / "index.json").write_text((idx / "index.json").read_text() + " ")
check(z("store", "refresh")[0] != 0, "an index changed after signing is refused")
publish("2026-01-04T00:00:00Z", revoked=[{"id": m["id"], "version": m["version"], "arch": m["arch"], "sha256": sha, "reason": "test reason"}])
rc, o = z("store", "refresh")
check(rc == 0 and m["id"] in o["disabled"], "a revoked release is disabled here")
app = home / ".d/zrn/apps" / m["id"] / f"{m['id']}.zexec"
p = subprocess.run([sys.executable, "-I", str(SDK / "tools/zrn-exec"), str(app)], capture_output=True, text=True, env=env)
check(p.returncode != 0 and "test reason" in p.stderr, "zrn-exec refuses it and says why")
rc, o = z("store", "install", m["id"])
check(rc != 0, "a revoked release cannot be installed again")
# removal: a throwaway app only
aid = m["id"]
os.environ.update(env)
for base in (home / ".d" / aid, home / ".s" / aid, home / ".c" / aid, home / ".k" / aid):
    base.mkdir(parents=True, exist_ok=True); (base / "x").write_text("x")
osr.write_text('ID=ztos\nVERSION_ID="0.2.1-BETA-1.1"\n')
keep = t / "keep"; keep.mkdir(); (home / ".d" / "other").mkdir(); (home / ".d" / "other" / "y").write_text("y")
rc, o = z("uninstall", aid)
check(rc == 0, f"uninstall {o.get('error')}")
gone = all(not (home / d / aid).exists() for d in (".d", ".s", ".c", ".k"))
check(gone, "its data is deleted from all four places")
check((home / ".d" / "other" / "y").exists(), "another app's data is left alone")
check(not (home / ".d/zrn/apps" / aid).exists(), "its package is gone")
sys.exit(1 if fails else 0)
