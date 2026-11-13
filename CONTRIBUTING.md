# Building and learning together

We are developing Plum-OS in small, hands-on steps. The first-boot code is a
baseline to understand and verify before adding more OS facilities.

For each change:

1. Explain the concrete goal and the machine behavior involved.
2. Read the relevant existing code and predict what QEMU should do.
3. Choose a small part for Brad to implement or experiment with before providing
   the complete patch. Keep the exercise optional and adjust to his preference.
4. Run the focused check together and explain what it proves and what it does not.
5. Review the diff, record the result and agree on the next step.

Keep each session to one main concept. Explain new assembly, CPU registers,
memory layouts and native ABI details when they become relevant. Prefer small
questions and experiments over long lectures or large completed subsystems.
Do not race ahead into the next OS milestone without choosing it together.

Compiler bugs and missing reusable language/FFI capabilities belong in Plum
issues, linked from this repository. Hardware policies and OS subsystems belong
here. The compiler pin changes deliberately, with the OS tests passing against
the proposed revision before it is adopted.
