## HC-0.7.2 — errors you can find, and an audit that found real bugs

Two things in this release, and they meet in the middle.

**When a script fails, HC now shows you exactly where.** The error dialog
names the right object and the right line — in every one of twenty measured
cases, where five used to point somewhere else — and "Script" opens the editor
with the faulty line **framed in red**, the way HyperCard framed it, with line
numbers in the margin.

**And two audits went through the whole project** — one of our own, with a
fuzzer and a static analyser, and one brought from outside. Between them they
found a line that executed half of itself before reporting an error, a freeze,
four crashes, and a handful of operations that damaged what they were working
on when they failed. All fixed, and each one held by a test that was checked
to fail without its fix.

Apple's **Stack Templates** calendar works too: "Show The Year…" redraws all
twelve months.

### At a glance

| | 0.7.1 | 0.7.2 |
| -- | -- | -- |
| a syntax error on a line | reported, **after half the line had run** (`delete card "D` deleted the card) | reported, **nothing runs** |
| "Script" in the error dialog | opens the editor with the line *selected* — the first key typed **replaced the whole line** | opens it with the line **framed**, cursor at its start, nothing selected |
| line numbers in the script editor | none | in the margin, the faulty one in red |
| the right object and line, over 20 measured error cases | 15 | **20** |
| a handler without its `end` | did **nothing**, silently | an error, pointing at the handler's first line |
| the dialog | `propriété inconnue (v3, ligne 3 de button "B".mouseUp)` | the fault as the title; the object, the line **and the line of code** below |
| Stack Templates, "Show The Year…" | only the title changed | the twelve months redraw |
| its `Next` / `Previous` arrows, held down | runaway years, nested runs | one year per repeat |
| `put value("the name of" & return & "me")` | **froze the app** | returns |
| `charToNum()`, `offset()` with no argument | **crashed** | empty |
| `global g` then `put g + 1` | could print `-5.31401e+303` | `1` |
| `the hcVersion`, "About HC" | `0.7.1` | `0.7.2` |

---

### Installing: macOS will say the app is damaged. It is not.

**This release is not notarised.** macOS quarantines anything downloaded from
the internet, and Gatekeeper refuses an app it cannot trace to a notarised
Developer ID — with a message that says the wrong thing:

> *"HC is damaged and can't be opened. You should move it to the Trash."*

The app is not damaged. It is unnotarised, which is a different problem, and
the wording sends people to the Trash for no reason.

Copy `HC.app` to `/Applications`, then clear the quarantine flag once:

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

It opens normally after that, and the flag does not come back. An app you
build yourself from this repository is never quarantined — it is only the
*download* that triggers this.

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. Finding the line that fails

The request was plain: *the user must see exactly which part of the script is
at fault; complex scripts get cryptic, and everything must be done to make
debugging easy.*

**First, the kernel had to point at the right place — and it often didn't.**
The dialog received an object and a line, but from two different sources: the
object from the *first* error line that happened to be collected, the line
from the first *runtime* error. Measured over sixteen cases, then twenty
(`ouerreur`), five were wrong:

- A button fails on line 3. The *card's* script, read on the way, has a typo in
  a **different** handler that never runs. "Script" opened **the card**, at
  line 3, under the title of the card's typo — an innocent script, accused.
- A line typed into the message box opened the card's script.
- `zorglub 3`, `hide button "Absent"`, `set the zorglub of me to 3`,
  `delete field "Absent"`: line 0 — the editor opened at the top.
- `on mouseUp` without its `end`: line 0, and the handler did **nothing at
  all**, skipped without a word.

The cause was one thing: diagnostics from **parsing** a script — produced when
a script is first read, not when it fails — went into the dialog and took its
first line. They now go to the log only. The object and the line are set
**together**, by whatever actually stopped the script, and nothing is lost:
running a faulty line raises its own error, at its own line.

**Then the editor had to show it.** It used to *select* the line. A selection
vanishes at the first click — and the first key you typed **replaced the whole
line**, so fixing an error meant deleting the line that carried it. The line is
now **framed**, the way HyperCard framed it: a red rectangle under the text,
which stays while you read the rest of the script and goes away at your first
edit (the line numbers no longer mean the same thing after that). Lines are
numbered in the margin; the faulty one is red. The **"Vérifier"** (check)
button frames the first fault it finds, too.

The dialog puts the fault in the title, then the object, the line number and
**the line of code itself** (HC's messages are in French):

```
propriété inconnue
button "B", ligne 3 :

    put the zorglub of me
```

*"Line 42"* makes you count. The line itself you recognise.

**The script editor changes are compiled on macOS but have not been run
there.** The bench is in `docs/mesures/ligne_fautive.txt`.

### 2. A line that ran half of itself

Found by the fuzzer, behind what looked like a false alarm. The parser kept
the part of a line it understood and put the error *next to* it, so the
executor ran the first half, then complained about the rest:

| line | what happened |
| -- | -- |
| `delete card "D` | the current card was **deleted**, then the error |
| `put 1 into g zz` | `g` was set, then the error |
| `go next card "zz` | moved to the next card, **no error at all** |
| `repeat with i = 1 to 10 step 3` | ten iterations of step 1, **no error at all** |

Reporting an error *and* acting is the worst of both: you read that the line
was not understood, and the stack has already changed. A faulty line now does
nothing, and the handler stops on it.

**And a test that had been measuring two lines out of seven since it was
written.** It wrote `\"Une\"` — a C escape — into HyperTalk, which has none.
Line 5 had always failed, the five after it had never run, and the reference
output had recorded the failure. It only showed once the first half of the
faulty line stopped printing.

### 3. The Stack Templates calendar

*"`convert todaysDate to dateItems` runs but gives no result."* The `convert`
worked. The trouble was two lines further down, in Apple's "Show The Year…"
button:

```
ask "Show what year?" with item 1 of todaysDate
if ((it is empty) or (it is not a date)) then exit mouseUp
else updateCalendar it, "barn door open"
```

The answer is a **year on its own**, `2027`. HC only took a bare number for a
date — a count of seconds since 1904 — above 100 000, so `2027` was *not a
date* and the button left through `exit mouseUp` without a word. For Apple's
button ever to have worked, HyperCard must take any integer as a date; HC now
does too. *(Deduced from Apple's script, not measured in HyperCard.)*

**Then only the title changed.** `updateCalendar` sets the title, then loops
over the twelve months starting with `if the mouseClick then exit repeat` — a
way out for whoever clicks during the calculation. HC counted **the click that
had launched the button**, so the loop left on its first turn. HyperCard has
already consumed that click by the time the script runs; now HC has too. A
click only counts for `the mouseClick` if it arrives *while* a script is
running. Reproduced on Apple's real stack, then fixed: January 2027 starts on a
Friday, February on a Monday.

**And the arrows ran away when held.** The calendar's `Next` and `Previous`
buttons act on `mouseDown`, and repeat through `mouseStillDown` while the
button stays down. HC's repeat timer kept firing *while a script was running*
— the event loop turns during long scripts to keep the window responsive — so
each tick started a new `updateCalendar` **inside** the one in progress, which
added another year. Replayed in the kernel with three ticks during the script:
one press, 1995 became **1999**. HyperCard never delivers a message in the
middle of a handler; HC now sends `mouseStillDown` only between runs.

### 4. What the audits found

**Our own audit** — clang's static analyser, and a fuzzer run under
AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer on HC's three
doors: **240 000 scripts**, **20 000 damaged `.stack` files**, and **8 000
damaged HyperCard stacks** derived from six real ones.

- `put value("the name of" & return & "me")` **froze the app for good**: a
  word scanner skipped spaces and tabs but stopped on any whitespace, read a
  zero-length word on a line break, and never moved again.
- `charToNum()` and `offset()` with no argument **crashed** (`strlen(NULL)`).
- A `.stack` file with two `stack` headers **leaked the whole first stack**; it
  is now refused, with a reason.
- `set the dragSpeed to 1e31850` and `go to marked card 1e30` converted
  infinity to an `int` before checking its range — undefined behaviour.
- **Three probable crashes in the Cocoa layer**, found by reading: a single
  byte of invalid UTF-8 — in a field's font name, its text, a window title, a
  menu — raised an exception inside AppKit (a `.stack` edited in a Latin-1
  text editor is enough); the seven **Info panels** outlived the object they
  showed, so closing the stack and clicking OK wrote into freed memory; and
  **Cmd-S with no window open** saved a stack that had already been freed.

**The outside audit** brought fifteen points with their code quotations. Each
was read before being touched, and measured where it could be:

- **A declared global read uninitialised memory.** Its numeric cache was never
  set, and came straight out of `realloc`. With the allocator filling fresh
  blocks with a pattern: `global g1, …, g12` then `put g12 + 1` gave
  **−5.31401e+303**. None of our sanitizers could see it — it is
  MemorySanitizer's territory — so the test dirties the heap itself.
- **Operations that damaged their object when they failed.** Measured by
  making the *n*-th allocation fail, for every *n*:

  | operation | damaged, before |
  | -- | -- |
  | pasting an image on a black-and-white icon | 2 — **its drawing was erased**, then "failed" was reported |
  | pasting on a colour icon | 1 |
  | copying a colour icon | 1 |
  | importing a stack with drawings | **18 stacks returned without their drawing** |

  All zero now. On the way, the same test found two more: `put … after v`
  did **nothing**, silently, when memory ran short, and reading a variable's
  name under shortage gave the empty string, so `put v into r` put `""` in
  `r`.
- **An icon pixel could name a colour its palette did not have** — and the
  editor's own brush reached that state. The file reader had the same hole,
  though a comment in the writer promised the reader checked.
- **The colour picker painted the icon on screen, not the one that opened
  it.** Open colour 8 of icon A, select icon B, move the slider: B changed.
- `int`/`size_t` arithmetic in concatenation and `put before/after`, closed on
  the same contract as the chunk module. *(Not measured: it takes
  gigabyte-sized strings.)*

**The instrument got it wrong twice before measuring right**, and the notes
say so: an exit code that collided with the kernel's own announced stop, and a
sweep that ended at the first success. Both showed up because the result
contradicted the reading of the code.

### 5. What is not measured is written down as not measured

- The script-editor frame, the line numbers and the dialog are **compiled**
  on macOS, **not run**. Benches in `docs/mesures/ligne_fautive.txt`.
- The two calendar fixes in the app layer — the launching click and the
  repeat timer — are compiled, not run; the kernel side is replayed on
  Apple's real stack.
- That a bare integer is a date in HyperCard is **deduced** from Apple's
  calendar script. To play **in HyperCard**: `put "2026" is a date`.
- The three Cocoa crashes of section 4 were found by **reading**, not seen.
- What HyperCard does with a faulty line — refuse the whole handler when
  compiling it, or stop on reaching it — is not measured. Both exclude running
  half of it, which is all the fix assumes.

---

### Test status

- C kernel: **259 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. Nine new ones: `lignefautive`,
  `valeurgel`, `sansargument`, `deuxpiles`, `horsplage`, `globalevierge`,
  `penurie_atomique`, `iconeindex`, `ouerreur`.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families.
- The Cocoa layer still has **no** automated tests — CI only proves it
  compiles. That is the main blind spot left, and the audits say so.

### Known, and not fixed here

- **A script cannot be interrupted.** There is no Cmd-period: a `repeat
  forever` only stops at the ten-million-iteration cap, or by quitting.
- After a fault raised **by a command** (`hide`, `set`, a message nobody
  handles), the handler **carries on** and the dialog comes at the end.
  HyperCard probably stops on the spot — to play in HyperCard.
- A fault inside a user function stops the function but not its caller.
- A click or a mouse release **during** a running script is still handled on
  the spot, inside the script, instead of after it the way HyperCard queues
  it. The repeat timer no longer does this; the click itself still does.
- Still standing from 0.7: named windows, patterns, resource-fork icons,
  HyperCard 1.x, private-access stacks, the proportional period fonts.
  `CFBundleVersion` is still hard-coded to `1`. Not notarised; see
  "Installing".

### Changes in this release

- [#80](https://github.com/cleobuline/hc/pull/80) — the first audit: the
  half-executed faulty line, the freeze, the crashes, the two-header file, the
  undefined conversions, and the three Cocoa crashes (`e732ba0`, `e66a90f`,
  `71dcd0d`, `b20c5cf`, `31da0a5`)
- [#81](https://github.com/cleobuline/hc/pull/81) — the outside audit, the
  right object and line for every error, the framed line and line numbers in
  the editor, and the Stack Templates calendar (`0edf2ab`, `098d41d`,
  `9c89e9f`, `cbf5592`, `fb0e7c6`, `61d5ac2`, `d11c4ca`, `455ace8`)
- this release — version 0.7.2 in all three places, and this note

**Full changelog:** [HC-0.7.1...HC-0.7.2](https://github.com/cleobuline/hc/compare/HC-0.7.1...HC-0.7.2)
