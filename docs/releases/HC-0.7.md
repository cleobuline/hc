## HC-0.7 — HC opens real HyperCard stacks

One subject, and it is the one the whole project was for: **HC now reads
Apple's own binary stack format, and runs the stacks.** Not a converter run
once by hand — File ▸ Open recognises the format from the four letters of the
file's first block, reads the stack, and plays it.

Four stacks from 1990–1995 were read end to end, and **296 scripts, 4 073
lines** went through the interpreter. What they refused was fixed: twenty-one
defects, most of them found by scripts nobody wrote to please us.

**If you have HyperCard stacks, this is the release that matters.** Everything
before it could only open stacks HC had written itself.

### At a glance

| | 0.6.9.5 | 0.7 |
| -- | -- | -- |
| open an Apple `.stack` | not possible | File ▸ Open, format recognised |
| the corpus' scripts accepted | — | **99.7 %** (296 scripts, 4 073 lines) |
| card order | — | verified by **both** of HyperCard's checksums |
| field text, part properties, part ids | — | read |
| paint (WOBA compression) | — | read |
| styled runs (`STBL`) | — | read |
| `the target` across a nested call | reset to `me` | survives |
| `set showPict of this card to false` | unknown property | works |
| `select the foundChunk` | *"doesn't know how"* | works |
| `find "x" in ch` (computed designator) | **"Not found"** | searches |
| a lit transparent button | blackens its area | inverts it |
| warning gate | gcc, `-Wall -Wextra` | **gcc ×3 levels + clang, Xcode's families** |

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

It opens normally after that, and the flag does not come back. An app you build
yourself from this repository is never quarantined — it is only the *download*
that triggers this.

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. Reading the original format

An extractor first, then a full importer of Apple's binary format (1987–1998):
the block chain, the card order, the MacRoman table, part properties, field
text, part ids, paint, and styled text runs.

Two things are worth naming because they are the parts that could have been
faked and were not:

**The card order is verified, not trusted.** HyperCard stores two checksums
over the card list, and both are recomputed here. When they do not agree with
the order read from the file, the order is reported as *not read* rather than
guessed — a stack whose cards come out in file order, silently, would be worse
than a refusal.

**Opening a stack cannot overwrite it.** The imported stack is installed
*without a path*, so "Save" has nowhere to write and asks. The original
`.stack` binary is never the target of a save, and cannot become one by
accident.

### 2. The corpus

| stack | blocks | bkgnds | cards | anomalies | scripts | lines | accepted |
| -- | -- | -- | -- | -- | -- | -- | -- |
| 3D Parametric Equations | 45 | 3 | 11 | 0 | 66 | 804 | 100 % |
| Découvrir HyperCard (Apple) | 32 | 1 | 11 | 0 | 28 | 164 | 100 % |
| TEST3 (our torture stack) | 16 | 1 | 5 | 0 | 4 | 941 | 100 % |
| Stack Templates (Apple) | 65 | 17 | 17 | 1 | 198 | 2 164 | 99.5 % |
| **total** | | | | **1** | **296** | **4 073** | **99.7 %** |

"Stack Templates" is the first *production* stack of the corpus, and it is the
one that went from **94.4 % to 99.5 %** — eleven refused handlers, four causes.
Eleven was never a count of four defects: three shapes fell on one cause, and
they had to be pulled out of the stack one at a time to find out.

**The one remaining anomaly is the only one we can explain.** The `MAST` block
of "Stack Templates" declares its size as `0x40000400` where it is 1 024 — the
high byte is corrupt, which the format's own documentation warns about. The
byte is masked off, and the mask is believed for one reason only: the chain of
65 blocks then lands *exactly* on the end of the file. A repair that displaced
anything would have no reason to land there. Measured before being believed: of
158 blocks across the four stacks, three stacks have not one non-zero high
byte, and the fourth has exactly one.

Five witnesses hold that repair from both ends, and **two of them are worth
nothing apart**: "four extra bytes without repair → read, chain does not reach
the end of the file" and "the same four with repair → refused, the repair does
not land right". Taken alone the first says we tolerate junk and the second
says we reject a file; it is their *difference* that shows the guard. One
measurement would not have reached it.

### 3. The twenty-one defects the stacks found

**In the interpreter** (ten)

| | before |
| -- | -- |
| `¬` continuation followed by a comment | the continued line was lost |
| `else` after a command with an optional argument | `else` swallowed as the argument |
| `print card from x,y to x,y` | rectangle ignored, then about to lie |
| `last menuItem of menu "T"` | the ordinal did not count as a designator |
| `bg field "Year" + 1` | the quoted name swallowed the operator |
| `select char 1 to 5 of line 3 of target` | a chunk *inside* a chunk did not resolve |
| `the target` inside a nested handler or function call | reset to `me` |
| `set showPict of this card to false` | unknown property |
| `bkgnd` on the writing side | *"object not found: this bkgnd"* |
| `the version` | answered ours, closing every stack with a version gate |

That last one is worth a line of its own. Apple's own teaching stack,
"Découvrir HyperCard", carries `if the version < 2.2 then` in its background
script, and HC answered `0.6.9.4` — so the stack politely refused to run. A
version gate was standard practice in 1993; every archived stack carrying one
would have stayed shut, with no recourse. `the version` now answers **2.4.1**,
HyperCard's. Our own number has its own name, `the hcVersion`, and the witness
checks that the two are *not* the same thing.

**In the application and the import** (eight)

| | before |
| -- | -- |
| paste in the icon editor | went into the card — the target search crossed the panel |
| duplicating a colour icon | read freed memory |
| `the clickChunk`, read before any other click property | empty — layout had not happened yet |
| a lit **transparent** button | blackened its area instead of inverting it |
| the default line height | rounded, where HyperCard **truncates** (measured on 140 of Apple's parts) |
| field text on import | did not arrive |
| a radio button on import | drawn as a rectangle |
| part ids on import | collided — a part's namespace is its **layer**, not the stack |

**In the diagnostics** (three)

| | before |
| -- | -- |
| `select word 0 of …` | *"doesn't know how"* — the truth was "there is no word 0", and the selection was cleared while success was announced |
| Apple's uncommented copyright banner ahead of a handler | verdict ERROR, where HyperCard compiles a handler at call time and never parses the banner |
| the import dialog's "1 anomaly" | announced a loss that had not happened |

The last one is a distinction the code now keeps: an **anomaly** is something
that wants checking, a **loss** is something that could not be read. Thirteen
of the twenty-four anomaly sites are losses; the other eleven are suspicions.
Zero lost with one anomaly is a perfectly legitimate state, and it is the one
"Stack Templates" is in.

And one number in that dialog was mine, not the stacks': **sixteen of seventeen
anomalies came from my own table**, which reserved the `shadow` style to
buttons. It is not reserved to buttons — measured on 574 parts.

### 4. The torture stack, played on both sides

Nothing in the archives was demanding enough, so a torture stack is written
here, built by the code, and used twice: a harness runs it under the suite, and
**the same program** writes it to disk to be opened and clicked in the app.
Building it twice would have given two stacks that diverge at the first change.

It was then played **inside HyperCard**, on a stack assembled by hand under
Basilisk II, and compared line by line with ours. That is the first time both
columns existed at once, and it produced something better than fixes:

| | HyperCard | HC |
| -- | -- | -- |
| `chars 1 to 5 of X` | *Can't understand* | works |
| `the number of chars of X` | *Can't understand* | works |
| `the number of card fields of this card` | *Can't understand* | works |
| `hide ch` / `find in ch` / `set … of ch` | works | works, since this release |

All three tolerances go the same way: HC accepts a **superset** of what the
original accepts, never the reverse. For a preservation project that is the
right side to be on — every HyperCard stack runs here. The rule that follows is
written down too: *tolerance in the language, strict fidelity in the benches*,
because a bench written in the generous dialect cannot be played on the
original.

The worst of the four defects that stack found was `find "bet" in ch`, where
`ch` is a computed designator. It answered **"Not found"** — the same answer as
if the word were not there. A search restricted to a field it cannot resolve
searched nothing and gave a false, plausible answer without a word of warning.

### 5. Three divergences measured, and deliberately not fixed

- **`field 1` with no layer designates the BACKGROUND** in HyperCard, the card
  in HC. It is the commonest turn of phrase in stacks from 1987; a stack
  carrying both layers reads the other text, silently. The bench is written;
  the fix touches the resolution of every field and waits for its measurement.
- **Field order in a search** — HyperCard skips the occurrence on the starting
  card, then comes back to it. Confirmed twice. Same family as the above: layer
  priority. One cause may close both.
- **The selection does not survive a write** in HyperCard; here it does.

### 6. What remains refused, on purpose

```
get visible of window "StackTemplatePal"
```

A **named window**. This is not a syntax hole: it is an object type the
interpreter's tree does not have, and the cost is stated rather than hidden —
the whole script is refused, so the 300-line stack script of "Stack Templates"
is lost for one reference. HC has no external palette, no Tools window, no
Patterns window; answering anything at all to `the visible of window "X"` would
be deciding alone what HyperCard would have answered. The refusal is written
into a harness so it cannot pass for an oversight.

### 7. The warning gate now asks Xcode's questions

`make avertissements` compiles with **both compilers**: gcc at `-O0 -O1 -O2`,
plus the families Xcode adds and `-Wall -Wextra` ignores — `-Wcomma`,
`-Wconditional-uninitialized`, `-Wnewline-eof`.

This exists because sixteen warnings were visible in Xcode on code this target
had just declared clean. Fifteen "possible misuse of comma operator" and one
"may be uninitialized". A gate that does not ask the same questions as the
machine the app is built on is not a gate, it is a surprise — and it always
arrives from the wrong side.

---

### Test status

- C kernel: **250 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families.
- New harnesses: `origine.c` (the binary format, with a `STBL` fixture and the
  five witnesses of the repaired block size), `pilereelle.c` (what real Apple
  stacks do, including the refusals we own), `torture2.c` and `torturecasse.c`
  (the torture stack, and a deliberately crooked build of it, because a guard
  that never bites is not a guard), `selectcalcule.c`, `interligne.c`,
  `idcouche.c`, `showpict.c`.
- Two tools exist because **a truncated paste looks exactly like a code
  defect**: `tests/denude.sh` (1 162 bench lines down to 543, to retype into
  Basilisk) and `tests/isole.sh` (one section in its own button, with markers).
- The Cocoa layer still has **no** automated tests — CI only proves that it
  compiles. Five `.m` files changed here — `AppDelegate.m`, `HCview.m`,
  `HCprint.m`, `HCdialogs.m`, `Hciconedit.m` — so the import dialog, the icon
  editor and printing all want a human at an Xcode build.

### Known, and not fixed here

- **Named windows** (`window "X"`) — see section 6.
- **Patterns**, **resource-fork icons**, **HyperCard 1.x**, and
  **private-access stacks** are not read.
- A fault inside a **user function** stops the function but not its caller.
- Four benches still to play **in HyperCard**: transparent-button hilite,
  `set the fixedLineHeight of … to true`, the page geometry of
  `print card from 100,100 to 300,200`, and `the target` across a nested call —
  the last being the one thing in this release inferred from three sites rather
  than measured.
- What a paint tool does on a layer whose paint is hidden is not measured, and
  is written down as not measured.
- `CFBundleVersion` is still hard-coded to `1`. It is the *build* number and it
  wants a rule, not a value — still true since 0.6.9.3.
- Not notarised. See "Installing".

### The method, since it is again the actual finding

0.6.9.3 recorded that a ten-second measurement beats a sound deduction. 0.6.9.4
added that you cannot separate the calculation from its display. This release
adds the one that cost the most: **eight times, the instrument measured
something other than what it announced.**

A probe printed `style & 0xFF` and turned −1 into `0xFF`, nearly overturning a
bit order that was correct. A probe searched a part id across *all* backgrounds
— the very cross-layer trap documented that same morning. A bench sent its
message to the button instead of the field, so `the target` was a button and a
second defect was nearly declared. A witness counted four markers where it
announced two, because an earlier section had left a layer hidden.

What worked, every time, was to **instrument the real code** — a copy of it with
counters — instead of re-deriving its reasoning alongside it. A reading that
*confirms* what the code already does deserves more suspicion than one that
contradicts it: the second makes you work, the first makes you conclude.

### Note on 0.6.9.5

0.6.9.5 shipped a DMG with a one-line description and no written note. Its
contents, so that nothing goes unrecorded:
[#69](https://github.com/cleobuline/hc/pull/69) /
[#70](https://github.com/cleobuline/hc/pull/70) — changing stack resets the
`numberFormat` to `0.######`, it does not empty it;
[#71](https://github.com/cleobuline/hc/pull/71) — colour icons: format,
drawing, editing, palette, paste;
[#72](https://github.com/cleobuline/hc/pull/72) — `find`: eleven measurements
and four defects, one of which made it useless;
[#73](https://github.com/cleobuline/hc/pull/73) — `find`, `print` and the
keyboard on the interpreter's tree; designators measured; and a menu that
refused to die.

### Changes in this release

- [#74](https://github.com/cleobuline/hc/pull/74) — a torture stack, and four
  defects it found, three of them by asking whether two turns of phrase that
  pose the same question give the same answer (`7f1eb65`, `95a6adc`, `535bf50`,
  `837c6cb`, `be6036e`)
- [#75](https://github.com/cleobuline/hc/pull/75) — the torture stack played on
  both sides: six divergences measured, three tolerances kept on purpose, and
  five times the instrument was at fault rather than the code (`5fe04da`,
  `bef2ff0`, `3375675`, `117fd60`, `2a67dc4`, `81507b4`, `563e5ad`, `e35822d`,
  `112ad63`, `9925d53`)
- [#76](https://github.com/cleobuline/hc/pull/76) — reading original HyperCard
  stacks, and twenty-one defects found by running them (`550e190`, `2a36481`,
  `5328be3`, `ad23314`, `767c7a5`, `43bea7b`, `16f77ef`, `54d1b47`, `b83bd44`,
  `73fdc82`, `4969272`, `944ee92`, `101d59a`, `ab19d33`, `714fe0f`, `510140f`,
  `67a7815`, `221b44c`, `830db18`, `24bb8f4`, `47b3602`, `533f922`, `af2bd98`,
  `5e1e718`, `5cb3fbd`, `f60e1cf`, `e6df34c`)

**Full changelog:** [HC-0.6.9.5...HC-0.7](https://github.com/cleobuline/hc/compare/HC-0.6.9.5...HC-0.7)
