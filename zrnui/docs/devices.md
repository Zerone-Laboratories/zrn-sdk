# Building for a phone and running on it

Two ways to develop, both without an IDE of your own: run the app as an ordinary Linux program on the PC, or build it for
the phone (Arch Linux ARM, aarch64) and run it there. The second one is wrapped in one script, `tools/zrn`.
It is `cmake` + `podman` + `rsync` + `ssh` and nothing else: no daemon on the phone, no SDK to install.

## 1. On the PC, like any Linux app

```sh
cd examples/helloworld
cmake --preset host-debug && cmake --build --preset host-debug
./build/host-debug/helloworld               # inside a Wayland session
```

Use this for most of the work. Use the phone to check touch, the UI scale, the GPU renderer and speed.

## 2. On the phone

### One time on the PC

Needs `podman`, `qemu-user-static` with binfmt for aarch64, `clang`, `lld`, `cmake`, `ninja`, `rsync`, `ssh`.

```sh
tools/zrn sysroot      # builds an Arch Linux ARM container (`tools/Dockerfile`) with the dependencies and exports it to ~/.cache/zrn/sysroot
```

It takes some minutes (the container runs under qemu) and is repeated only when the dependencies change.
The sysroot is what the cross build links against, so the libraries are exactly those of the phone.

### One time per phone: the login

The phone is given on the command line; nothing about it is stored in the repository or by the script.

| option | meaning |
|---|---|
| `-u <name>` | the user on the phone, `alarm` (the default account of Arch Linux ARM) when not given; `user@host` in the address wins |
| `-p <password>` | the password; `-p -` asks for it without echo. Without it only a key is used |
| `-i <file>` | a private key, when it is not one of the default `~/.ssh/id_*` or in the agent |

The host is an address (USB-LAN `192.168.7.2`, Wi-Fi, `phone.local`) or an alias from your own `~/.ssh/config`.

Use the password once, to put your key on the phone, and work with the key afterwards:

```sh
tools/zrn -p - setup-key 192.168.7.2     # asks for the password of alarm, makes ~/.ssh/id_ed25519 if you have none, copies it
```

The phone needs a running Wayland session of that user and `rsync`; nothing from zrnSDK has to be installed.

A password on the command line is visible in `ps` and stays in the shell history, so prefer `-p -` or the
`ZRN_PASSWORD` variable, and only for `setup-key`. The password is handed to ssh through `SSH_ASKPASS`, never in its
arguments, and is not written to a file. A fresh Arch Linux ARM has the password `alarm` for the user `alarm`: change it on a real device.

### Every time

```sh
tools/zrn run 192.168.7.2 examples/helloworld          # with the key; add -u and -p to log in another way
```

This builds for aarch64, uploads the binary (only the changed bytes), and starts it over ssh with the output in your
terminal. `Ctrl-C` stops the app on the phone. The steps are also available separately:

| command | what it does |
|---|---|
| `tools/zrn build <dir>` | cross build in `<dir>/build/phone-release/` |
| `tools/zrn setup-key <host>` | copy your ssh key to the phone (needs `-p`) |
| `tools/zrn push <host> <dir>` | upload to `~/zrn-dev/<name>/` on the phone, plus the fonts and the theme to `~/zrn-dev/share/` |
| `tools/zrn run <host> <dir> [args]` | build, push, run; arguments after the directory go to the app |
| `tools/zrn watch <host> <dir>` | run again when a source changes (needs `inotify-tools`) |

Options go before the command. `ZRN_HOST=192.168.7.2` makes `<host>` optional; `ZRN_USER`, `ZRN_PASSWORD` and `ZRN_KEY`
are the variables of the options; `ZRN_PRESET=phone-debug` builds a debug binary.

### What happens

- The app is linked **statically** with zrnui and zrnapp (the `phone-*` presets turn the shared libraries off), so one
  file is uploaded and the phone needs only its system libraries (wayland, freetype, mesa).
- The fonts and `theme.ini` go to `~/zrn-dev/share` and are given to the app with `ZRNUI_FONT_DIR` and `ZRNUI_THEME`
  (see [Theme and configuration](theme-and-resources.md)).
- `WAYLAND_DISPLAY` is the first `wayland-*` socket in `/run/user/<uid>`; set `ZRN_WAYLAND_DISPLAY` when the
  shell has several. `XDG_RUNTIME_DIR` defaults to `/run/user/<uid>`.

## IDEs

Every project has a `CMakePresets.json` (`host-debug`, `host-release`, `phone-debug`, `phone-release`). CLion, VS Code,
Qt Creator and Visual Studio read it, so the presets are the same in all of them and `phone-*` cross builds from the IDE too
(after `tools/zrn sysroot`). Uploading and running stay in `tools/zrn`, which any IDE can call as an external tool.

## Troubleshooting

| symptom | cause |
|---|---|
| `no aarch64 sysroot at ...` | run `tools/zrn sysroot` first |
| `exec format error` while the sysroot is built | binfmt for aarch64 is not registered (`qemu-user-static-binfmt`) |
| `Permission denied` on the phone when starting | the home of the phone user is mounted `noexec`: set `ZRN_REMOTE_DIR` to a path elsewhere |
| `no Wayland socket in /run/user/...` | the session is not running or belongs to another user; log in on the phone or set `ZRN_WAYLAND_DISPLAY` |
| `Permission denied (publickey)` | no key on the phone yet: `tools/zrn -p - setup-key <host>` |
| `REMOTE HOST IDENTIFICATION HAS CHANGED` | the phone was reflashed and has a new host key: `ssh-keygen -R <host>`; the script accepts a new host but never a changed key |
| the text shows boxes | the fonts were not found: check `~/zrn-dev/share/fonts` on the phone |

Back to the [documentation index](index.md).
