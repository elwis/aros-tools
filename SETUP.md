# AROS Development Setup

This is my personal guide, might contain errors or hallucinations..

How to get a working AROS x86_64 (ABIv11) development environment on 
Linux, from zero to compiling and running your own Zune apps.

Tested on Pop!_OS / Ubuntu. Should work the same on Fedora etc, swap 
`apt` for `dnf`/`pacman` and package names as needed.

---

## Important: use the right source tree

There are two AROS repos on GitHub. Use the right one or you'll waste 
a day chasing ABI mismatches.

- ✅ `https://github.com/deadwood2/AROS` — stable ABIv11, what 
  application developers should build against
- ❌ `https://github.com/aros-development-team/AROS` — bleeding edge 
  nightly, can have broken builds and will produce binaries that crash 
  on stable AROS distributions (e.g. AROS One) with 
  "Illegal address access" in Exec_B3_OpenResource

If you're targeting a specific AROS distro (e.g. AROS One), check what 
ABI version it ships and try to match it. AROS One 1.3 currently ships 
ABIv11 20250418-1.

---

## 1. Install build dependencies

```bash
sudo apt install gcc g++ make git flex bison gawk python3 python3-mako \
                 libx11-dev libpng-dev genisoimage cmake curl nasm \
                 autoconf automake libxext-dev liblzo2-dev libxxf86vm-dev \
                 libsdl1.2-dev byacc yasm xorriso mtools
```

`cmake` is required even though older guides may not mention it — recent 
AROS builds depend on it for some bundled libraries (cunit, etc).

---

## 2. Clone and build the toolchain

```bash
mkdir -p ~/arosbuilds
cd ~/arosbuilds
git clone https://github.com/deadwood2/AROS.git AROS
cp ./AROS/scripts/rebuild.sh .
./rebuild.sh
```

Choose option `1) toolchain-core-x86_64`. This builds the cross-compiler 
(`x86_64-aros-gcc` and friends). Takes 20–40 minutes depending on your 
machine.

---

## 3. Build AROS (hosted)

```bash
./rebuild.sh
```

Choose option `2) core-linux-x86_64 (DEBUG)`. This builds a complete 
hosted AROS system that runs as a Linux process. Takes 1–2 hours.

---

## 4. Add the toolchain to your PATH

```bash
echo 'export PATH="$HOME/arosbuilds/toolchain-core-x86_64:$PATH"' >> ~/.bashrc
source ~/.bashrc
x86_64-aros-gcc --version
```

---

## 5. Run hosted AROS

```bash
cd ~/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS
./boot/linux/AROSBootstrap
```

This is a real AROS desktop running as a Linux process. Good for fast 
iteration but it seems it has **no network-stack**

---

## 6. Compile your own app

Always build with `--sysroot` pointing at the Development directory 
that was just built, otherwise you'll link against the wrong headers/ABI:

```makefile
CC       = x86_64-aros-gcc
SYSROOT  = $(HOME)/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/Development
INCLUDES = -I$(SYSROOT)/include
LIBS     = -lmui
TARGET   = myapp

$(TARGET): myapp.c
	$(CC) --sysroot=$(SYSROOT) $(INCLUDES) -o $(TARGET) myapp.c $(LIBS)

install:
	cp $(TARGET) $(HOME)/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/C/

clean:
	rm -f $(TARGET)
```

Run it in hosted AROS by copying to `C:` and typing its name in the 
Shell.

**Avoid POSIX headers** (`sys/socket.h`, `netdb.h` etc) for anything that 
also needs to run on AROS One or other distros — they pull in 
`libposixc` startup code that can crash with an ABI mismatch. Use native 
AROS/Amiga headers instead (`proto/bsdsocket.h` etc).

---

## 7. Testing with networking — AROS One in VirtualBox

Hosted AROS has no network what i can understand. For anything that needs sockets (FTP 
clients, etc), test in a real AROS distro inside a VM instead.

[AROS One](https://www.aros-exec.org) is the starting 
distro I chose — preconfigured, good software selection.

### VirtualBox network configuration that actually works

- Adapter type: **Intel PRO/1000 MT Desktop (82540EM)**
- Attached to: **NAT**
- Inside AROS One's Network Preferences: select **e100.device**, leave 
  everything else on automatic/DHCP
- Restart the VM — networking should come up automatically

PCnet-FAST III (the "obvious" choice many guides suggest) does **not** 
work reliably and produces "Message too long" errors regardless of 
configuration.

### Getting files in/out of the VM

Easiest approach: a local FTP server on the host.

```bash
pip install pyftpdlib
python3 -m pyftpdlib -p 2121 -w -d /path/to/share
```

Connect from inside the VM to the host's LAN IP (not 127.0.0.1 — find 
it with `ip addr` on the host), port 2121, anonymous login.

For one-way host → VM transfers without networking, you can also build 
an ISO and mount it as a virtual optical drive:

```bash
genisoimage -R -o /tmp/transfer.iso /path/to/files
```

(`-R` enables Rock Ridge extensions — without it, filenames without an 
extension get a trailing dot appended, which breaks executables.)

---

## Common pitfalls

- **Wrong ABI** — always build against deadwood2/AROS with `--sysroot`, 
  not the nightly tree
- **`cmake` missing** — AROS build fails partway through with cunit 
  errors
- **POSIX headers** — avoid them for anything cross-distro; they cause 
  startup crashes via `libposixc`
- **AI coding assistants and AROS/Zune APIs** — general-purpose coding 
  agents will confidently hallucinate AmigaOS/Zune API calls that don't 
  exist. Point them at the actual AROS source tree 
  (`~/arosbuilds/AROS/`) and real Zune example code before trusting 
  generated code
- **Filenames with no extension on ISO transfers** — use `genisoimage -R`
- **`.info` files** — every AmigaOS-style icon is a separate `.info` 
  file sitting next to the real file/program; don't confuse one for 
  the other when poking around in Shell
