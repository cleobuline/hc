## HC-0.8.4 — XCMDs and XFCNs, in WebAssembly

### Installing

**This release is signed and notarised.** HC is signed with the Developer ID
of the Association Labynet, which distributes it, and Apple's notary service
has checked it. Open the DMG, drag `HC.app` into Applications, and open it.
The first time, macOS asks the question it asks of anything downloaded from
the internet — "HC is an app downloaded from the Internet. Are you sure you
want to open it?" — and offers **Open**. It asks once.

The binary is universal (`x86_64 arm64`), and the deployment target is
macOS 10.13. The latest DMG is always at
<https://github.com/cleobuline/hc/releases/latest/download/HC.dmg>.

---

**Update from 0.8.3.** HyperCard could be extended with externals: XCMDs
(commands) and XFCNs (functions), small programs in C or Pascal kept inside
a stack. HC now has them too, compiled to **WebAssembly** and run **sandboxed**. The proof:
MAC•CAM Workshop, a cellular-automata stack written in 1990 around four
externals, runs in HC **without a single line of its scripts changed**.

### At a glance

| | 0.8.3 | 0.8.4 |
| -- | -- | -- |
| A stack that calls an XCMD or an XFCN | an error: nothing answers | **the external runs**, once its module is added to the stack |
| Objects ▸ Modules… | — | **lists the stack's modules**, Add…, Remove |
| A `.wasm` file dropped on a stack window | — | **added to that stack** |
| Cmd-. during a long external | — | **stops it**, as it stops a script |
| Help ▸ Show Balloons | — | **System 7 help balloons**, for the stacks that show them |
| `drag` with the select tool, from inside the selection | a new selection | **moves it**, or copies it `with optionKey` |
| `type numToChar(8)` | nothing on a paint selection; a raw character 8 in a field | **erases** the selection, or the character before |
| `the hcVersion`, "About HC" | `0.8.3` | `0.8.4` |

---

### 1. XCMDs and XFCNs, sandboxed

```
on mouseUp
  hello                                -- an XCMD
  put double(21) into fld "Result"     -- an XFCN
end mouseUp
```

An external is compiled to a `.wasm` file — a **module** — which you add to
the stack: **Objects ▸ Modules…**, then **Add…**, or drop the file on the
stack's window. The module is stored *in* the stack, and saved with it:
whoever opens the stack elsewhere has its externals, with nothing to
install. Scripts call them like any HyperTalk word.

A module runs **enclosed**. It sees only its own memory and the callbacks
HC hands it; it cannot open a file, reach the network, or touch anything
else on the Mac. No native code is ever loaded next to a stack. A stack
someone shares with you can do no more with its modules than with its
scripts.

- The callbacks carry HyperCard's names: `SendCardMessage`, `EvalExpr`,
  `GetGlobal` and `SetGlobal`, `GetFieldByName` / `ByNum` / `ByID` and
  their `Set…` twins. Parameters and results are plain C strings: no more
  Handles, no more Pascal strings.
- **Cmd-.** stops a module that runs too long, as it stops a script, and
  `cantAbort` is respected. A module that polls `Button()` — a loop that
  stops on a click — gets one second to finish by itself.
- **Windows**, on the model of HyperCard 2's XWindows: an external opens
  one with `NewXWindow`, draws into it, and scripts address it by its title
  — `send "step" to window "CAM"`, `set the scale of window "CAM" to 2`,
  `the phaseA of window "CAM"`, `close window "CAM"`. HC serves `rect`,
  `loc`, `visible`, `name`, `width` and `height` itself.
- **Help balloons**: `HMGetBalloons` and `HMShowBalloon`, System 7's Help
  Manager calls. HC's **Help** menu gains **Show Balloons**, and the
  balloons stay on or off from one session to the next.
- **`DragGrayRgn`**: a grey outline of rectangles follows the mouse while
  the button is held, as in the Toolbox.

### 2. Writing one: the kit, and two manuals

The `xcmd/` folder of the repository is the kit: `hc_xcmd.h`, modelled on
Apple's `HyperXCmd.h`, a small C library, examples, and a `Makefile`.

```c
#include "hc_xcmd.h"

HC_XCMD(hello)
{
    SendCardMessage("put \"Hello world !\"");
}
```

**`xcmd/MANUAL.md`** (and `xcmd/MANUEL.md`, in French) takes a beginner
from an empty Mac to that "Hello world" in a stack: installing the
compiler, writing the file, compiling it, adding it, saving the stack, and
what each error message means. Every command in it was run as written.
Both manuals are also in the DMG, next to `HC.app`.

The compiler is the `clang` of the emsdk project, which knows the `wasm32`
target; Apple's own `clang` does not, as far as we know. It runs on macOS
10.15 Catalina (measured on 10.15.8). Neither `emcc` nor JavaScript is
needed.

### 3. MAC•CAM Workshop, from 1990, unchanged

The judge of this work was a real stack: MAC•CAM Workshop, written in 1990,
a workshop for the CAM cellular automata machine. Its four
externals have been rewritten as modules, from what their 68000 code does:

- **FORTH**, the XFCN that compiles a rule written in Forth into the
  machine's lookup tables. Checked against the stack itself: 71 of its
  cards keep both a Forth program and the tables the original compiled
  from it — 175 tables. 163 are rebuilt identical, character for
  character; the twelve others are older than the original FORTH in its
  last state (they follow an earlier bit layout);
- **MACCAM**, the XFCN that opens the CAM window and runs the machine:
  Conway's Life, Brian's Brain and the stack's other rules, step by step or
  continuously, with the 30 properties the stack sets and reads on its
  window;
- **Ballons**, the XCMD that shows a help balloon over the workshop's
  buttons;
- **FullDrag**, Frédéric Rinaldi's XFCN (1990–1992), which drags a drawing
  from square to square, and onto the CAM window.

Played in HC on the original stack: the rules compile, the CAM window runs,
the balloons appear, drawings are dragged and copied, the eraser clears its
square. Not one line of the stack's scripts was touched.

**MAC•CAM FORTH**, the demonstration of the language written in 1990 by
the same author, is in the DMG too: 29 cards, each with a program and its
explanation, down to words written in 68000 machine code. The FORTH module
was rebuilt on the original's own 68000 code, read instruction by
instruction:

- **recursion**: a word knows itself from its `:` on (indirect recursion,
  as in the original, does not exist);
- **16-bit** signed numbers; `NOT` complements bit by bit; `DO … LOOP`
  advances its index before the body;
- the return stack is laid out as the original's — a return address takes
  four bytes — so that `R> R> DROP DROP` leaves a word, and the words the
  stack writes in machine code with `,` (`EXIT`, `LEAVE`, `PICK`, the loop
  index) run;
- `ALTERNATE` is an internal variable at address -4: the "-4" left on the
  stack of the Margolus cards;
- an error is **sent** to the card, as `COMPILE_ERR "Unknow'd word ", 13`,
  for the handlers both stacks carry: a dialog names the word, and it is
  selected in the field; `FORTH()` returns the stack, top first.

The results the original left in the demonstration stack, which HC could
not reach before, now come out the same — Fermat's 225 numbers, character
for character, 11^1024, which is -20479 in 16 bits, `LEAVE`, the loops.

`ballons.wasm` and `fulldrag.wasm` are useful beyond MAC•CAM: any stack can
add them. They are prebuilt in the repository, in `tests/donnees/modules/`,
next to `forth.wasm` and `maccam.wasm`; their sources are in
`xcmd/exemples/` and `xcmd/maccam/`.

### 4. Scripted painting

Found while running MAC•CAM, whose scripts paint by replaying gestures:

- **`drag` with the select tool, starting inside the selection**, now lifts
  the selection and moves it — or copies it, `with optionKey` — as the
  mouse does. It used to make a new selection instead, so nothing moved.
  Choosing another tool drops the floating selection.
- **`type numToChar(8)`** — the Delete key, typed by a script — now erases
  the paint selection, as the key does. In a field, the message box, or
  with the paint Text tool, it erases the character before the insertion
  point; it used to insert a raw character 8.

### 5. For those who build HC themselves

- The licence of the WebAssembly runtime HC embeds — WAMR, from the
  Bytecode Alliance, under the Apache License 2.0 with LLVM exception —
  now travels inside the application, in `HC.app/Contents/Resources/wamr/`,
  with its attributions.
- `outils/livrer.sh`, run again after an interruption, clears the Finder's
  attributes from an application that is already signed too. Without it,
  the second run stopped on "resource fork, Finder information, or similar
  detritus not allowed".

---

### Test status

- C kernel: **313 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. Three are new: the modules
  (every callback, windows, balloons, `DragGrayRgn`, Cmd-., the refusals),
  the FORTH compiler against the 175 tables of MAC•CAM Workshop and the
  results kept in MAC•CAM FORTH, and the CAM machine. The windows, balloons
  and outlines themselves live in the Cocoa interface, which these checks
  do not reach: they were tried in the application, on MAC•CAM Workshop.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults. The fuzzer now starts from 1,089 stacks saved by the
  checks, against 58 when it was written.

### Not measured yet

- The original FORTH was read, not run: what it does is taken from its
  68000 code and checked against the results its stacks kept, not seen in
  HyperCard.
- In HyperCard, an external of a stack came before the scripts of the
  stacks in use and before HyperCard's own commands — an XCMD named `beep`
  replaced `beep`. In HC, HyperTalk's commands and functions come first,
  then every script, then the modules. This is read, not measured.
- The look of the balloons (HC draws them in Geneva 11) and of the
  `DragGrayRgn` outline (a one-point grey dotted line; the Mac drew a grey
  pattern) is HC's choice, not compared with System 7.
- The speed of the CAM machine on a Mac: one step of Life on a 256 × 256
  grid takes 9.3 ms on the Linux machine that runs the checks.
- Still from 0.8.3: `flash` in HyperCard (its speed, `flash 0`, `flash`
  under `lock screen`); the octave of a note written without one, and the
  other note questions; what `the sound` returns just after `play stop`;
  whether `lockRecent` returns to false at idle; `doMenu "Bold"` on a
  selection that is already bold.
- Still from 0.8: a `pass` that reaches a stack put in use during the
  handler; what HyperCard does with a stack in use when you close its
  window; `open printing with dialog`; `print 5 cards` near the last card;
  whether `go forth` forgets its steps after a detour; the default of
  `dialingVolume` and the answer of `the stackSpace`.

### Known, and not fixed here

- The modules' C library is small: memory comes in power-of-two sizes,
  `snprintf` knows only `%d %u %x %c %s %%`, there is nothing for
  floating-point numbers and nothing for files. `SendHCMessage` and the
  conversion callbacks (`StrToNum`, `PasToZero`…), which C strings make
  unnecessary, are not there.
- A module window receives neither clicks nor keystrokes: HC moves and
  closes it itself, and the module only sees its messages and properties.
  `GetNewXWindow`, `SetXWIdleTime` and `GetXWindowValue` are not there.
- The FORTH, where HC parts from the original on purpose: two comments in a
  row (the original read the second `(` as a word and stopped; HC skips
  both, so that two cards of MAC•CAM Workshop still compile); a division by
  zero (the original stopped the Mac; HC gives 0); an endless recursion
  (stopped by `STACK_ERR`); machine code beyond the few instructions the
  demonstration stack uses (refused with `Can't understand`). A `BEEP`
  sounds at most ten times per call.
- FullDrag's optional `<test>`, `<if true>` and `<if false>` parameters are
  not supported yet: they return an error rather than being ignored.
- Externals written for the classic Mac — 68000 or PowerPC code in a
  stack's resource fork — do not run: HC never loads native code. Each has
  to be rewritten as a module, as MAC•CAM's four were.
- `type numToChar(8)` does not delete an object selected with the Button or
  Field tool, as the Delete key does.
- Still standing from earlier versions:
  - `flash` under `lock screen` should not flash in HC (read from the code,
    not yet seen);
  - a sound longer than 30 seconds plays once without its notes;
  - **"Print Topic"** in the Reference menu prints every page with the same
    picture;
  - a button's size is drawn by its style, whatever `the textSize` answers
    when none is set;
  - Apple's help stacks cut paths at `:`, and HC uses `/` paths;
  - not supported yet: `ask password`, `open report printing`,
    `the abbreviated time`, `the param of 1`, `show all cards`,
    `create stack`, `the long version`, `the selectedLoc`,
    `select button "x"`, `edit script of …`, and the Message Watcher;
  - HC's own dialogs and messages are in French;
  - colour icons, pictures and sounds from a resource fork; a stack's own
    palettes (`PLTE`); patterns, HyperCard 1.x stacks, private-access
    stacks; the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`.

### Changes in this release

- [#133](https://github.com/cleobuline/hc/pull/133): the WebAssembly
  runtime, the kit, modules stored in stacks, Objects ▸ Modules…, and the
  `livrer.sh` resume fix
- [#134](https://github.com/cleobuline/hc/pull/134): Cmd-. stops a module
- [#135](https://github.com/cleobuline/hc/pull/135): `EvalExpr`,
  `GetGlobal`, `SetGlobal` and the field callbacks
- [#136](https://github.com/cleobuline/hc/pull/136): module windows, FORTH
  and MACCAM
- [#137](https://github.com/cleobuline/hc/pull/137): help balloons and the
  Ballons XCMD
- [#138](https://github.com/cleobuline/hc/pull/138): FullDrag and
  `DragGrayRgn`
- [#139](https://github.com/cleobuline/hc/pull/139): a scripted `drag`
  moves or copies the selection
- [#140](https://github.com/cleobuline/hc/pull/140): `type numToChar(8)`
  erases
- [#141](https://github.com/cleobuline/hc/pull/141): the step-by-step manual
- [#142](https://github.com/cleobuline/hc/pull/142): the English manual,
  the WAMR licence inside HC.app, version 0.8.4 and this note
- this release: the FORTH rebuilt on the original's code, for MAC•CAM FORTH

**Full changelog:** [HC-0.8.3...HC-0.8.4](https://github.com/cleobuline/hc/compare/HC-0.8.3...HC-0.8.4)
