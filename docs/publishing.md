# Publishing an app to the ztOS store

From an app that runs on your phone to an app in the store. The store is its own repository,
[ztOS-apprepo](https://github.com/Zerone-Laboratories/ztOS-apprepo); this page says what it does, with the files that do it.

**Before you start, know these three things.**

- **You never build, sign or upload the package.** The store builds your app from your public source at the commit you
  name, signs it with the store's key and lists it. Nobody merges a pull request to add an app.
- **You need no signing key.** `tools/zrn setup-key` puts your **ssh** key on a phone so `tools/zrn run` needs no
  password. It has nothing to do with the store.
- **Not yet:** the store cannot build SDK v2 apps until zrn-sdk's v2 work is committed and pinned in the store's
  `sdk.lock` (`commit_v2` is all zeros today). Until then a v2 build stops with
  `sdk.lock: no SDK v2 pinned yet; SDK v2 apps cannot build` (`ci/build-in-arch.sh`).

## 1. Make the app work on the phone

```sh
tools/zrn sysroot                              # one time: the aarch64 sysroot
tools/zrn -p - setup-key 192.168.7.2           # one time per phone: your ssh key (~/.ssh/id_ed25519.pub, made if missing)
tools/zrn run 192.168.7.2 path/to/your-app     # phone-release build, upload, run
```

`tools/zrn run` builds with the `phone-release` preset (`ZRN_PRESET`), which links zrnui and zrnapp statically, so one
file goes up. It expects a `CMakePresets.json` in the app folder (copy one from `examples/helloworld`) and a program named
like the folder. See [Building for a phone](../zrnui/docs/devices.md).

## 2. Let the store build it

The store does **not** use the presets. It runs, on an arm64 machine inside Arch Linux ARM with only the packages of the
ztOS image (`ci/ztos-packages.txt`):

```sh
cmake -S <app> -B <app>/build/store -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DZRN_SDK_DIR=<zrn-sdk at sdk.lock commit_v2> \
      -DZRNUI_BUILD_SHARED=OFF -DZRNAPP_BUILD_SHARED=OFF -DZRNUI_BUILD_TESTS=OFF -DZRNAPP_BUILD_TESTS=OFF
cmake --build <app>/build/store
```

(`build_v2` in `tools/store.py`.) So your `CMakeLists.txt` must:

1. Take the SDK from `ZRN_SDK_DIR`. The examples use a fixed `../../zrnapp`; an app for the store does this instead:

   ```cmake
   set(ZRN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zrn-sdk" CACHE PATH "zrn-sdk checkout")
   add_subdirectory(${ZRN_SDK_DIR}/zrnapp ${CMAKE_BINARY_DIR}/zrnapp)   # also builds zrnui
   add_executable(notes main.c)
   target_link_libraries(notes PRIVATE zrnapp::zrnapp zrnui::zrnui)
   ```

2. Put the program at the top of the build folder (the default), named as `[build] target` says.
3. Link nothing the image lacks. After the build, `ci/check-linkage.py` runs `ldd` on the program and fails on any
   `not found` library, and on any `libzrnui` / `libzrnapp` (they must be linked in).
4. Download nothing during the build. Everything must be in the commit.

## 3. Write `zrnapp.toml`

Put it in the app folder (the repository root, or the `subdir` you submit). The store reads it at the commit you submit.

```toml
[app]
id = "io.github.alice.notes"     # never changes; see below
name = "Notes"
version = "1.0.0"                # semantic; higher than every version already listed
summary = "Short notes"
author = "Alice"
licence = "MIT"                  # an SPDX id
icon = "icon.svg"                # packed as icon.svg
categories = ["Utility"]
permissions = ["network"]        # everything the app uses, nothing else
min_ztos = "0.2.1"               # optional; this is the default
data_dirs = ["notes"]            # optional; removed on uninstall with <id>

[build]
sdk = 2                          # SDK v2: CMake, zrnui and zrnapp linked statically
target = "notes"                 # the program CMake builds; default: the id
lang = "c"                       # optional, "c" or "cpp", shown to moderators
data = ["data/*.png"]            # optional; packed under share/
```

**The id.** Reverse DNS you control, at least three parts, lower case (`io.github.alice.notes`). `zrn-` ids are only for
repositories of Zerone Laboratories (`ID_RE` in `client/zrnlib/zexec.py`, `check_ledger` in `tools/store.py`).
The phone's app list matches your window by `StartupWMClass=<id>`, so give the window the same id:

```c
const ZWindowOptions window = { .title = "Notes", .app_id = "io.github.alice.notes" };
```

**Permissions** the client knows (`PERMISSIONS` in `client/zrnlib/zexec.py`): `network`, `audio`, `camera`, `location`,
`notifications`, `bluetooth`, `phone`, `contacts`, `messages`, `background`, `files.music`, `files.pictures`,
`files.documents`, `files.downloads`. Moderators ask about each one.

**Login services** (a sync engine, a notifier) are `[[service]]` tables; they need `background`. The fields are in the
store's README ("Login services"). Never write systemd units yourself: the client writes and removes them.

## 4. Raise the version

Every submission needs a version that is not in the store yet. The submission relay refuses one that is
(`services/app-submit/src/worker.js`: "already in the store's ledger; raise the version"). The build stops if the
`zrnapp.toml` at the commit says another id or version than the approval.

## 5. Check the package before you submit (optional)

You can run the store's own build on your computer with a ledger entry of your own. It needs a checkout of
ztOS-apprepo and a commit of your app (a local clone named with `--source-override` stands in for GitHub).

```sh
mkdir -p /tmp/ledger
cat > /tmp/ledger/io.github.alice.notes.toml <<'EOF'
id = "io.github.alice.notes"
source = "https://github.com/alice/notes"
subdir = ""
[[release]]
version = "1.0.0"
commit = "<the 40-hex SHA>"
status = "approved"
approved_by = "discord:0"
approved_at = "2026-01-01T00:00:00Z"
EOF
STORE_LEDGER=/tmp/ledger ZRN_SDK2=/path/to/zrn-sdk \
  python3 tools/store.py build --id io.github.alice.notes --version 1.0.0 --out out \
  --source-override "https://github.com/alice/notes=/path/to/your/clone"
python3 client/zrn info out/io.github.alice.notes-1.0.0-*.zexec      # the manifest and the files
python3 client/zrn verify out/io.github.alice.notes-1.0.0-*.zexec    # the structure checks; a signature error is expected
```

This is what the store's trial run does (`publish.yml`, `workflow_dispatch` with `trial_*`). The package is built for
the CPU you run it on, so on a laptop it is an `x86_64` package; the store builds `aarch64`. It is **not signed**, and the
phone's `zrn install` refuses an unsigned package. Test on the phone with `tools/zrn run`, not by installing this file.
SDK v2 has no signing command of its own.

A format 2 package (`.zexec`: a shebang line and a zip, `zexec.json` first) holds `bin/<id>`, your icon, your `data`
files and zrnui's fonts and theme (`share/zrnui/`).

## 6. Submit

1. Commit and push. The repository must be **public on GitHub**.
2. On the ztOS Mobile Discord:

   ```
   /submit-app repo:https://github.com/alice/notes commit:<40-hex SHA> [subdir:app]
   ```

   The relay checks that the repository is public, the commit exists, and `zrnapp.toml` at that commit has the id
   and version, then opens a post in **#ztos-app-submissions**.
   The relay also takes an entry on `POST /submit` (the store page's form). SDK v1's `zrn submit` is **not** in SDK v2.
3. A moderator reads the source at that commit and presses **Accept** or **Reject** (with a reason).

## 7. What happens after Accept

All in ztOS-apprepo; you do nothing.

1. The bot appends an approved `[[release]]` (version, commit, who approved, when) to `ledger/<id>.toml` and starts
   `.github/workflows/publish.yml`.
2. `plan`: approved releases with nothing published yet (`store.py plan`).
3. `build`, on arm64, with no secrets: fetch your commit and check `HEAD` is exactly it, fetch zrn-sdk at
   `sdk.lock` `commit_v2`, build and pack (`store.py build`), check what it links (`ci/check-linkage.py`).
4. `publish`, the only job with the store key: check each package against the ledger, sign it (`.zexec.minisig`,
   trusted comment `zexec id= version= arch= sha256=`), upload both to the GitHub release `<id>-<version>`, and write
   a new signed `index.json` on the `store-index` branch.

A failed build means that release is not in the store; the run fails. `sdk.lock` changes only by a reviewed change to
ztOS-apprepo; `validate.yml` checks the ledger and the tools on every change.

## 8. Updates

An update is a new submission: raise `version`, commit, push, `/submit-app` with the new commit. Moderators see a
compare link from your last approved commit. The store never follows a branch: pushing or force-pushing changes
nothing that is already listed.

## 9. What people see on the phone

- The store lists your app from the signed index, with name, summary, icon, permissions and version
  (`zrn store refresh`, `zrn store list`). An installed app with a higher listed version is marked as an update.
- Install or update (`zrn store install <id>`): the package and its signature are checked against the store key, the
  index's sha256 and your id, version and arch. The package goes to `~/.local/share/zrn/apps/<id>/<id>.zexec`, the app
  list gets `<id>.desktop`, and your services are written and started. **An update never deletes the app's data.**
- Uninstall asks first, then removes the package, the entry, the icon, the services and the data (`<id>` and
  `data_dirs`), unless `--keep-data`.
- A moderator can revoke a release. Phones that have it disable it and show the reason. Fix it and submit a new version.

The store's [CONTRIBUTING.md](https://github.com/Zerone-Laboratories/ztOS-apprepo/blob/main/CONTRIBUTING.md) lists what
gets an app rejected.
