# Building Ear Olympics

These steps were worked out on an Apple-silicon Mac. The JieLi toolchain only runs on Linux x86-64, so the device
build runs inside Docker; the simulator is native.

## One-time setup (macOS)

1. **Command line tools and Git:** `xcode-select --install` (skip if `git --version` already works).
2. **Docker Desktop** (Apple silicon version): https://www.docker.com/products/docker-desktop/. Open it once and
   leave it running. If Terminal says `docker: command not found`, add its tools to your PATH:
   ```
   echo 'export PATH="/Applications/Docker.app/Contents/Resources/bin:$PATH"' >> ~/.zprofile
   export PATH="/Applications/Docker.app/Contents/Resources/bin:$PATH"
   ```
3. **Homebrew** (https://brew.sh), then the font shaper the build needs: `brew install fribidi`.
4. **Clone with the submodule:**
   ```
   git clone --recursive https://github.com/asilagy/ear-olympics
   cd ear-olympics
   ```
   (already cloned without it? `git submodule update --init --recursive`. fm1-base has a submodule of its own,
   felucca-bsp, so `--recursive` matters: without it the build stops at `base/bsp/loader/crt0_ldr.S: no such file`.
   Maintainers adding fm1-base to a fresh repo: `git submodule add https://github.com/charlesvestal/fm1-base base`
   and then `git submodule update --init --recursive`.)
5. **The pinned toolchain and Python environment** (several minutes; downloads and checks the JieLi toolchain and
   SDK files into `base/.toolchain`). If it stops with "needs uv", fetch uv first:
   ```
   base/tools/setup_toolchain.sh --only uv
   base/tools/setup_toolchain.sh
   ```
6. **The installer's MIDI libraries:** `base/.toolchain/uv/uv sync --frozen --group device` run inside `base/`
   (that is: `cd base && .toolchain/uv/uv sync --frozen --group device && cd ..`).

## Build and test

```
./build.sh --host        # the simulator, native: app/build/host/earolympics_host
tests/run_tests.sh       # the music unit tests, then app/tests/*.txt in the simulator, with screenshots in app/build/host/
./build.sh               # the firmware: app/build/earolympics.fwsc (Docker must be running)
```

Pasting commands into Terminal sometimes adds stray characters (`[200~`, `›`). If a command fails with "bad
pattern" or a strange character, quit Terminal (Cmd+Q), reopen it and type the command by hand.

## Install on an FM-1

Charge the FM-1, use a USB data cable straight into the computer, and close other MIDI apps and browser tabs
that use MIDI. Then, from `base/`:

```
cd base
.venv/bin/python tools/fm1_install.py --info                       # read-only: finds the FM-1?
.venv/bin/python tools/fm1_install.py ../app/build/earolympics.fwsc
```

If the FM-1 is not found while it runs M-VAVE's own firmware, switch it off, hold **OCT-** while switching it on
(it then waits for an update), and install again.

## Back to the original firmware

Open https://thefirmwarehub.com/ in Chrome or Edge and use **Back to official V15**. If an FM-1 will not start,
see the Firmware Hub's Rescue page.
