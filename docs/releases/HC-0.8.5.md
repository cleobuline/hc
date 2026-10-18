## HC-0.8.5 — MAC•CAM FORTH, and Paste fixed

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

**Update from 0.8.4.** Pasting a picture after copying a card pasted the
card: fixed. The FORTH of MAC•CAM has been rebuilt on the original's own
68000 code, so the demonstration stack written with it in 1990, MAC•CAM
FORTH, runs too — recursion, 16-bit arithmetic, machine code and all. The
Objects menu gains New Button and New Field, new objects get HyperCard's
names, and HC's dialogs speak English. The 0.8.4 DMG was republished on
17 October with part of these changes, still numbered 0.8.4; this release
carries all of them under its own number.

### At a glance

| | 0.8.4 | 0.8.5 |
| -- | -- | -- |
| Copy Card, then copy a picture, then Paste | the card | **the picture**, the most recent copy |
| Objects menu | no New Button, no New Field | **New Button**, **New Field** — also for `doMenu` |
| A new button, field, card, background | "Bouton 3", "Champ 3", "carte 2", "Fond 1" | **"New Button"**, **"New Field"**; cards and backgrounds **unnamed** |
| HC's dialogs and alerts | in French | **in English** |
| FORTH: a word that calls itself | `COMPILE_ERR "Unknow'd word "` | **recursion** |
| FORTH: words written in 68000 machine code | not run | **run** |
| The CAM window: look at some planes only | — | **`planes`**: `"1000"`, `"0011"`… |
| `the hcVersion`, "About HC" | `0.8.4` | `0.8.5` |

---

### 1. Paste: a picture copied after a card

Copy Card, then copy a picture — in another application, or with the
button of MAC•CAM Workshop that puts the CAM window on the clipboard — then
Paste: HC pasted the card. It now pastes the picture, the most recent copy,
as it already did after copying a button. Copying text does not displace
the card.

### 2. MAC•CAM FORTH, and the FORTH rebuilt on the original's code

**MAC•CAM FORTH**, the demonstration of the language written in 1990 by the
author of MAC•CAM Workshop, is in the DMG: 29 cards, each with a program
and its explanation, down to words written in 68000 machine code. The FORTH
module was rebuilt on the original XFCN's 68000 code, read instruction by
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
And MAC•CAM Workshop gains with it: **163** of its 175 tables are rebuilt
identical, character for character, against 159 in 0.8.4 — the four of
"Dentrite•" come out right, now that `MAKE-TABLE` passes over a word it does
not know, as the original did. The twelve others are older than the original
FORTH in its last state: they follow an earlier bit layout.

The machine code is also **twice as fast**: Fermat takes 0.54 s instead of
1.07 s, measured on the Linux machine that runs the checks. An operand's
address is computed in one pass, and the three instructions that `PICK` and
the loop index repeat run in one go. No JIT: WebAssembly is still
interpreted, and nothing native is made at run time.

### 3. MAC•CAM Workshop: `planes`

The original MAC•CAM hid planes with colour tables, each one colouring a
single plane. HC does not read those tables; the CAM window has a property
instead:

```
set the planes of window "CAM" to "1000"    -- plane 0 alone
set the planes of window "CAM" to "0011"    -- planes 2 and 3
set the planes of window "CAM" to "1111"    -- all four, the default
```

Only the drawing changes: a hidden plane is drawn as if it were off, and
the machine still computes all four, so a reversible rule still runs back.
Fewer than four digits are padded on the left; a value HC does not
understand changes nothing.

MAC•CAM Workshop is in the DMG.

### 4. New Button, New Field

The **Objects** menu has **New Button** and **New Field**, before New
Background, and `doMenu "New Button"` works — the way HyperCard stacks
make their buttons, HyperTalk having no `create button`. The new object
appears in the middle
of the card, selected, with the Button or Field tool chosen; it goes on the
background when you are editing the background. It receives `newButton` or
`newField`, and so does an object drawn with the tool.

New objects get HyperCard's names: **"New Button"** and **"New Field"**. New
cards and backgrounds have **no name**, where HC used to call them
"carte 2" or "Fond 1".

### 5. In English

Everything HC's interface shows in dialogs and alerts is now in English:
deleting a card, a save that fails, an unreadable stack and the import
report, the new stack, the icon panel, the modules, pasting a card, the
spray can, a script's title and its check, the empty Recent menu.
"Sans titre" is "Untitled". The `ask` dialog says **Cancel**, as
HyperCard's does.

The About panel stays bilingual. The messages of the kernel — script errors,
modules, files — are next.

### 6. Fixed

- The list of a module's externals, in Objects ▸ Modules…, could compute
  an address past the end of its buffer when the names were long, and a
  name came out cut in the middle. The names are whole now, followed by "…"
  when some do not fit. A module comes from a stack you may not know, so
  this mattered.

### 7. Around HC

- The README has a **Provenance and inspiration** section: Apple never
  released HyperCard's source code, so HC was built from what HyperCard
  visibly does, measured in HyperCard 2.4.1 under Basilisk II; the stack
  format was read from HyperCardPreview and mystextract; WAMR is the only
  third-party code.
- **Reporting a bug**: two issue forms, one for a bug in HC and one for a
  difference with HyperCard, and a section in the README.
- A short film of MAC•CAM Workshop running in HC, in the README.

---

### Test status

- C kernel: **313 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. The FORTH checks now hold
  both stacks of 1990 — the tables of MAC•CAM Workshop, and the results
  MAC•CAM FORTH kept — with recursion and machine code; the module checks
  gained `planes`, and the list of externals with buffers from 1000 bytes
  down to 1.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: __.

### Not measured yet

- The original FORTH was read, not run: what it does is taken from its
  68000 code and checked against the results its stacks kept, not seen in
  HyperCard.
- In HyperCard, the names, place, size and style of a new button or field,
  the tool chosen, and whether `newButton` is sent when it comes from the
  menu. And `the name` of a card without a name: HC answers `card ""`.
- The speed of the FORTH and of the CAM machine on a Mac. Played in HC,
  Fermat runs and a 256 × 256 grid steps smoothly, but neither was timed.
- Still from 0.8.4: whether an external of a stack came before HyperCard's
  own commands (an XCMD named `beep`); the look of the balloons and of the
  `DragGrayRgn` outline.
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

- The FORTH, where HC parts from the original on purpose: two comments in a
  row (the original read the second `(` as a word and stopped; HC skips
  both, so that two cards of MAC•CAM Workshop still compile); a division by
  zero (the original stopped the Mac; HC gives 0); an endless recursion
  (stopped by `STACK_ERR`); machine code beyond the few instructions the
  demonstration stack uses (refused with `Can't understand`). A `BEEP`
  sounds at most ten times per call.
- The colour tables of the original MAC•CAM (`set the palette`) are not
  read; `planes` does what they were used for.
- From 0.8.4: the modules' C library is small (no floating point, no files,
  a short `snprintf`); a module window receives neither clicks nor
  keystrokes; FullDrag's `<test>`, `<if true>` and `<if false>` parameters
  return an error; externals written for the classic Mac — 68000 or PowerPC
  code in a resource fork — do not run, and have to be rewritten as
  modules; `type numToChar(8)` does not delete an object selected with the
  Button or Field tool.
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
  - the kernel's messages are still in French (see section 5);
  - colour icons, pictures and sounds from a resource fork; a stack's own
    palettes (`PLTE`); patterns, HyperCard 1.x stacks, private-access
    stacks; the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`.

### Changes in this release

- [#144](https://github.com/cleobuline/hc/pull/144): the FORTH rebuilt on
  the original's 68000 code
- [#145](https://github.com/cleobuline/hc/pull/145): its machine code twice
  as fast, and a register kept inside its stack
- [#146](https://github.com/cleobuline/hc/pull/146): dialogs in English,
  HyperCard's default names, New Button and New Field
- [#147](https://github.com/cleobuline/hc/pull/147): the list of externals
  kept inside its buffer
- [#148](https://github.com/cleobuline/hc/pull/148): `planes`
- [#150](https://github.com/cleobuline/hc/pull/150): a newer picture comes
  before a copied card
- [#151](https://github.com/cleobuline/hc/pull/151),
  [#152](https://github.com/cleobuline/hc/pull/152): provenance, and the
  issue forms
- this release: the film of MAC•CAM Workshop in the README, version 0.8.5
  and this note

**Full changelog:** [HC-0.8.4...HC-0.8.5](https://github.com/cleobuline/hc/compare/HC-0.8.4...HC-0.8.5)
