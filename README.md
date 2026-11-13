# Plum-OS

An experimental operating system written in [Plum](https://github.com/bradcypert/plum).
The initial kernel boots one x86-64 CPU with Limine, prints `PLUM_KERNEL_OK\n`
from Plum, and halts. Native C/assembly currently handles machine setup and the
small hardware/runtime boundary. This is a working first-boot baseline, not a
complete operating system.

## Repository boundary

Plum owns its compiler, freestanding target, entry generation, core runtime and
hook ABI. This repository consumes those capabilities and owns the bootloader,
image layout, hardware helpers, allocator, drivers and eventual OS facilities.
Nothing here is automatically embedded or selected by the Plum compiler.

The initial files were extracted from the work in
[Plum PR #524](https://github.com/bradcypert/plum/pull/524), based on commits
`fb544fa` and `1a270a1`. Their development continues here rather than growing an
OS inside the language repository. Limine's separately supplied license is
preserved in the downloaded dependency and generated ISO.

OS work is tracked in [GitHub issues](https://github.com/bradcypert/plum-os/issues).
For a guided first session, see [First boot](docs/first-boot.md). Our development
approach is described in [CONTRIBUTING.md](CONTRIBUTING.md).

## Build and run

Run these commands from the repository root on Linux x86-64. Required tools:
Python 3, Clang with x86-64 support and its integrated assembler, LLD (`ld.lld`),
binutils (`nm`, `readelf`), `curl` for the first download, `xorriso`, and
`qemu-system-x86_64` with PC BIOS firmware (normally packaged with QEMU).
For Debian/Ubuntu, install `clang lld binutils curl xorriso qemu-system-x86`.

The compatible compiler revision is recorded in `plum-toolchain.json`.
Build that compiler separately (first-time setup):

```sh
git clone https://github.com/bradcypert/plum.git .toolchain/plum
git -C .toolchain/plum checkout 0efc86228d04ae58a152f5c4628984f21ab9a18e
(cd .toolchain/plum && ./bootstrap/from-seed -o /tmp/plum-os-seed && /tmp/plum-os-seed build bootstrap/self_host -o /tmp/plum-os-compiler)
python3 build.py --plum /tmp/plum-os-compiler
python3 smoke.py
python3 tests/kernel-hooks-check.py /tmp/plum-os-compiler
```

The build writes `kernel.elf`, `kernel.iso`, native objects and `toolchain.json`
under `build/`. Add `--release` to `build.py` for an optimized image. Both scripts
accept `--output /absolute/output/directory`; use the same directory for build
and smoke. `--plum` is required: the OS does not build or bundle its compiler.
CI builds the pinned compiler from source and runs the same support/boot checks.

To run interactively instead of the bounded smoke check:

```sh
qemu-system-x86_64 -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M \
  -cdrom build/kernel.iso -boot d \
  -display none -serial stdio -monitor none -no-reboot -no-shutdown
```

The expected serial bytes are exactly `PLUM_KERNEL_OK\n`. Normal halt keeps
QEMU running; stop the interactive run with Ctrl-C. This boots an ISO with
BIOS, rather than relying on QEMU's `-kernel` ELF handling. UEFI is not included.

The smoke check has a 20-second deadline (override with `--timeout SECONDS`). It
uses QMP over pipes to require both the exact serial bytes and `HLT=1` with RIP
inside `plum_kernel_halt`, then terminates QEMU. A timeout or a marker without
the expected halt fails. It saves `serial.log` and `halt-registers.txt` and adds
the actual QEMU version to `toolchain.json`. Broader OS panic/allocation fixtures and GDB checks are tracked in this
repository; compiler-only code-generation audits remain in Plum.

## Pinned boot inputs

The build fetches only the immutable commit archive below, verifies SHA-256
before extraction/compilation, and verifies the matching header separately.
It never follows a moving branch. The archive is cached in the output directory;
a bad cached checksum fails rather than silently fetching a replacement.
For offline use, pass `--limine-archive /path/to/archive.tar.gz`; checksum
verification still applies. Limine's license accompanies the extracted files
and is copied into the ISO at `/boot/limine/LICENSE`.

| Input | Pin |
| --- | --- |
| Limine binary distribution | `v8.7.0-binary`, commit `aad3edd370955449717a334f0289dee10e2c5f01` |
| Source URL | <https://codeload.github.com/Limine-Bootloader/Limine/tar.gz/aad3edd370955449717a334f0289dee10e2c5f01> |
| Archive SHA-256 | `9c23ca261778cbe015041c02220ce26f45e05f38fe3e0c9913be71961f9023e5` |
| Protocol header | `limine.h` from that same archive; base revision 3, header API revision 0 |
| Header SHA-256 | `f8b35287579dbe50e9d84cca7b558e39b2b64e499f9b37ef5ebe969c05104c33` |

Boot configuration, protocol and bootloader inputs are pinned. ISO timestamps
and local toolchain/compiler versions are not normalized for byte-identical
images. `toolchain.json` records the versions actually used; CI prints it too.
Locally tested: Clang/LLD 23.1.4, xorriso 1.5.8.pl02, QEMU 11.1.3, debug and
release. These are tested versions, not minimum version claims.

## Native boundary and machine setup

| File | Responsibility |
| --- | --- |
| `boot/start.S` | Disable maskable interrupts, clear DF, select a private 64 KiB aligned BSS stack, initialize x87/SSE and MXCSR, call the C adapter with System V alignment |
| `boot/adapter.c` | Negotiate Limine base revision 3; require HHDM, a nonempty memory map and four-level paging; retain borrowed boot data; initialize hooks before the generated entry |
| `boot/kernel.ld` | Higher-half ELF64 ET_EXEC at `0xffffffff80000000`, `_start` entry, page-aligned RX/R/RW regions, writable retained request records, mapped BSS and debug sections |
| `boot/hardware.c` | C ABI serial write, interrupt disable/status and non-returning halt helpers |
| `main.plum` | Check interrupts remain disabled, emit the fixed serial marker, halt |

The adapter retains the HHDM offset returned by Limine and the borrowed memory
map. It does not guess address offsets, reclaim boot memory, change CR3/GDT,
start secondary CPUs, or enable interrupts. Limine zero-fills BSS. Failures use
`plum_kernel_panic` before heap initialization, sharing the reference UART
initializer so later initialization cannot clear pending adapter output.

The helper signatures match the current emitter:

| Plum | C |
| --- | --- |
| `CStr` | borrowed `const char *` data pointer |
| `Int` | `int64_t` |
| extern `Bool` | `int` (LLVM `i32`), **not** `_Bool` |
| extern `Unit` | `void` |

`plum_x86_serial_write` takes an explicit nonnegative byte length; callers must
supply a valid borrowed span for the entire call. The hello marker is 15 ASCII
bytes including LF; its CStr terminator is not transmitted. No `strlen` is used.
Only disable/status interrupt helpers are exposed: enabling interrupts requires
a kernel-owned IDT and exception handling, which this demo does not supply.

The [early kernel support](support/README.md) uses a
1 MiB bump arena with no-op frees. This is a bounded boot demo, not a sustained
allocator or a general OS runtime. See the
[compiler target/entry contract](https://github.com/bradcypert/plum/blob/0efc86228d04ae58a152f5c4628984f21ab9a18e/docs/freestanding-x86_64.md).

Protocol/configuration references for the selected release:
[Limine protocol](https://github.com/Limine-Bootloader/Limine/blob/v8.7.0/PROTOCOL.md),
[Limine configuration](https://github.com/Limine-Bootloader/Limine/blob/v8.7.0/CONFIG.md).
