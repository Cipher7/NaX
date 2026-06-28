# NoNameAx (NaX)

Position-independent C2 beacon for the [Adaptix Framework](https://github.com/Adaptix-Framework/Adaptix) with module stomping, malleable C2 profiles, BOF execution, and a Stardust-pattern UDRL loader.

![Language](https://img.shields.io/badge/beacon-C-blue)
![Language](https://img.shields.io/badge/server-Go-00ADD8)
![Platform](https://img.shields.io/badge/target-Windows%20x64-0078D6)
![Framework](https://img.shields.io/badge/framework-Adaptix-purple)

## Features

- **PIC shellcode beacon** -- single `.text` section, no CRT, no imports, all APIs resolved via PEB walk
- **Module stomping** -- beacon runs from image-backed DLL memory (IMG), not private allocations (PRV)
- **Clean stack walks** -- `.pdata`/`.xdata` stomped into DLL with correct RVAs, LDR entry patched with full PEB flags (ImageDll, LoadNotificationsSent, ProcessStaticImport, EntryProcessed)
- **Control Flow Guard (CFG) aware** -- queries CFG status at runtime, whitelists beacon entry (loader), BOF go() and sleep_mask() in the CFG bitmap via SetProcessValidCallTargets; safe in CFG-enabled processes
- **Malleable C2 profiles** -- configurable encoding pipeline (base64/hex/raw, XOR mask, body templates, header/cookie/parameter placement), runtime profile switching
- **DLL load notification unhooking** -- removes all `LdrRegisterDllNotification` callbacks at startup, preventing EDR DLL-load telemetry; always enabled, also available at runtime via `dll-notify list/remove`
- **BeaconGate + Sleepmask** -- extensible API call proxy routes Sleep/WaitForSingleObject/WaitForMultipleObjects through a sleepmask BOF (module-stomped, image-backed); ships with a WFSO PoC that replaces Sleep with `NtWaitForSingleObject` on a dummy event — bring your own sleep obfuscation technique
- **BOF execution** -- in-process COFF loader with module stomping (image-backed BOF .text, IFT .pdata injection)
- **Stardust UDRL loader** -- self-relocating PIC loader with TLS egghunter globals, thread pool execution
- **TCP tunneling** -- SOCKS4/5 proxy, local port forwarding and reverse port forwarding through beacon, works over both HTTP and SMB transports
- **Token manipulation** -- steal, impersonate, create from credentials (4 logon types), privilege listing, console header updates
- **26 built-in commands** -- file I/O, process management, token ops, screenshots, downloads, uploads, SMB pivoting
- **AES-128-CBC encryption** -- all frames encrypted end-to-end via BCrypt
- **Proxy-aware transport** -- WinHTTP with automatic proxy detection (PAC/WPAD)

## Architecture

```
+------------+ +----------------+ +--------------+ +-------+ +-------+
|   UDRL     | |  NaxHeader v2  | |  PIC Beacon  | | .pdata| | .xdata|
|  Loader    | |  (160 bytes)   | |  (.text)     | |       | |       |
| Stardust   | |  magic, sizes  | |  NaxMain()   | | RUNTM | | UNWIND|
| PEB walk   | |  flags, DLL    | |  + commands  | | _FUNC | | _INFO |
| Mod stomp  | |                | |  + transport | |       | |       |
+------------+ +----------------+ +--------------+ +-------+ +-------+
                                        |
                                   HTTP(S) / SMB
                                        |
+---------------------------------------------------------------+
|                  Adaptix Framework Server                      |
|  +------------------------+  +-----------------------------+  |
|  | listener_nonameax_http |  |     agent_nonameax          |  |
|  | Profile-driven HTTP    |->|  BuildPayload, CreateCommand|  |
|  | AES crypto, transforms |  |  ProcessData, AxScript UI   |  |
|  +------------------------+  +-----------------------------+  |
+---------------------------------------------------------------+
```

## Quick Start

### Prerequisites

```bash
sudo apt install gcc-mingw-w64-x86-64 nasm binutils golang python3
```

### Build

```bash
make                                    # Release build
make debug                              # Debug build (NaxDbg output)
make MODULE_STOMP=1 STOMP_PDATA=1       # Module stomping + unwind data
make MODULE_STOMP=1 STOMP_DLL=mshtml.dll  # Custom sacrificial DLL

# Go server plugins
cd src_server/agent_nonameax && make
cd src_server/listener_nonameax_http && make
```

### Deploy

1. Copy `.so` plugins to `Server/extenders/`
2. Register in `Server/profile.yaml`
3. Start Adaptix, create a listener, generate a payload

## Commands

| Command | Description |
|---------|-------------|
| `whoami` | Current identity (domain\user) |
| `sleep <s> [jitter%]` | Set callback interval |
| `sleepmask-set` | Send sleepmask BOF (auto-fires on connect) |
| `ls`, `cd`, `pwd`, `mkdir`, `rmdir` | Directory navigation |
| `cat`, `rm`, `download`, `upload` | File operations |
| `ps list`, `ps kill`, `ps run` | Process management |
| `token getuid/steal/use/list/rm/revert/make/privs` | Token manipulation and impersonation |
| `screenshot` | GDI desktop capture |
| `bof [-a] <file.o>` | Execute BOF (sync or async) |
| `bof-stomp sync/async/show` | Reconfigure BOF module stomping |
| `profile <json>` | Runtime C2 profile switch |
| `link`, `unlink` | SMB pivot management |
| `socks start/stop` | SOCKS4/5 proxy (via Adaptix tunnel system) |
| `lportfwd` / `rportfwd` | TCP tunnel forwarding (via Adaptix UI) |
| `terminate thread/process` | Exit beacon |

## Documentation

Full documentation is in the [wiki/](wiki/) folder:

### Core
- **[Beacon Architecture](wiki/Beacon-Architecture.md)** -- NAX_INSTANCE, bootstrap, heartbeat loop, PIC constraints
- **[Wire Protocol](wiki/Wire-Protocol.md)** -- Frame format, message types, command IDs, encryption
- **[Malleable C2 Profiles](wiki/Malleable-C2-Profiles.md)** -- OutputConfig encoding pipeline, profile JSON, runtime switching
- **[Module Stomping](wiki/Module-Stomping.md)** -- Loader-phase beacon stomping, DLL selection, LDR patching

### BOF System
- **[BOF Execution](wiki/BOF-Execution.md)** -- COFF loader, sync/async dispatch, Beacon API reference
- **[BOF Module Stomping](wiki/BOF-Module-Stomping.md)** -- Image-backed BOF .text, IFT .pdata injection, slot pool

### Evasion
- **[BeaconGate & Sleepmask](wiki/BeaconGate-Sleepmask.md)** -- API call proxy, WFSO PoC sleepmask, extensible gate architecture

### Post-Exploitation
- **[Token Commands](wiki/Token-Commands.md)** -- Token theft, impersonation, credential logon, privilege listing

### Networking
- **[Tunneling](wiki/Tunneling.md)** -- Local/reverse port forwarding, wire protocol, OPSEC, flow control

### Extending NaX
- **[Adding Commands](wiki/Adding-Commands.md)** -- End-to-end walkthrough from Wire.h to AxScript
- **[Stardust Loader Guide](wiki/Stardust-Loader-Guide.md)** -- UDRL architecture, how to write your own loader

### Operations
- **[Build and Deploy](wiki/Build-and-Deploy.md)** -- Prerequisites, build commands, Adaptix setup
- **[Operator Reference](wiki/Operator-Reference.md)** -- All commands with syntax and examples

## Project Structure

```
Makefile                # Top-level: loader + beacon -> nax.x64.bin
src_beacon/             # PIC shellcode beacon (C, MinGW cross-compile)
  include/              #   Instance.h, Wire.h, Config.h, Macros.h, Bof.h
  src/Core/             #   Bootstrap, Ldr (PEB walk), Config, Crypto, Packer
  src/Transport/        #   Http.c (WinHTTP), HttpCodec.c, Smb.c
  src/Commands/         #   Dispatch + command handlers + Tunnel.c
  src/Bof/              #   COFF loader, Beacon API, module stomping
src_loader/             # Stardust UDRL loader
  src/                  #   PreMain, Main, Ldr, Stomp, Pe, Exec, Entry.asm
src_sleepmask/          # Sleepmask BOF (COFF .o, loaded by beacon at runtime)
  src/                  #   main.c (WFSO PoC — extend with your own technique)
  include/              #   Gate.h, Imports.h
src_server/             # Go plugins for Adaptix Framework
  agent_nonameax/       #   Agent extender (build, commands, results, AxScript)
  listener_nonameax_http/  # HTTP listener (profile transforms, crypto)
profiles/               # Malleable C2 profile JSON files
scripts/                # Build scripts (pack_nax.py)
```

## Acknowledgments

- [Adaptix Framework](https://github.com/Adaptix-Framework/Adaptix) -- C2 framework and operator console
- [Stardust](https://github.com/Cracked5pider/Stardust) -- PIC loader template by Paul Ungur
- [ZeroPoint Security](https://training.zeropointsecurity.co.uk/) -- UDRL and Sleepmask course
- [Kharon](https://github.com/Adaptix-Framework/Kharon) -- Reference Adaptix agent
