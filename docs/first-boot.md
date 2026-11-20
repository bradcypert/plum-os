# First session: follow one boot

The goal is to understand how a machine with no OS reaches `main.plum` and why
it keeps running after the program finishes.

## Follow the entry points

Read these small pieces in order:

1. `boot/limine.conf` chooses the kernel ELF from the ISO.
2. `boot/kernel.ld` names `_start` as the ELF entry and places code/data in memory.
3. `boot/start.S` disables interrupts, clears the direction flag, sets a private
   stack and initializes x87/SSE state before calling compiled C.
4. `boot/adapter.c` checks bootloader responses, initializes our runtime hooks and
   calls the compiler-generated `plum_kernel_start`.
5. The generated entry initializes Plum globals and invokes `main.plum`.
6. `main.plum` asks the adapter to send a boot snapshot back through the
   `boot_snapshot_matches` export while the Plum runtime is active.
7. `boot/hardware.c` forwards serial output and halt requests to `support/`.

The bootloader prepares the machine, the adapter satisfies the compiler's entry
contract, and Plum runs the kernel behavior. Those responsibilities live in
different layers so each can change without duplicating the others.

## Predict, then run

Before running the README commands, predict:

- Which function runs first: `_start` or Plum `main`?
- What would be missing if assembly called Plum before setting a stack?
- Why does the smoke test check a halted CPU as well as a printed message?

There is no need to learn every register yet. Start with RIP (the instruction
position) and RSP (the stack position). We will explain the remaining setup as
we read the assembly together.

Run the boot smoke and inspect `build/serial.log` and `build/halt-registers.txt`.
`RIP` should be inside `plum_kernel_halt` and the register dump should say
`HLT=1`. This verifies the first-boot path, not memory management or an OS scheduler.

## First hands-on change

When ready, change the greeting in `main.plum` and predict the result. Its serial
helper takes an explicit byte length: keep that length consistent with the ASCII
message. Rebuild and run the existing smoke test before updating its expectation.
It should fail because the expected output changed. Then update the expected
bytes in `smoke.py`, rebuild and verify the new result.

We will work through that change together before choosing a subsystem to add.
