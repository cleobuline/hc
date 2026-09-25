## HC-0.6.9.2 — compatibility

Ten fixes, all found the same way: by translating one real HyperCard stack —
**HypoGraph 0.91 (1995, Dominic Yu)**, a function plotter — and running the
original side by side under Basilisk II.

The result worth reporting is not the count. It is that **the stack's scripts
now run in HC unchanged, character for character**, including the parts that
depend on Macintosh floating-point behaviour from 1995.

Two of these ten also close a gap that had been open since the beginning:
`the lockErrorDialogs` and the `errorDialog` message. HC's script verifier had
listed `errorDialog` as a legitimate handler name since the day it was
written — so a stack containing one produced no warning and looked like it
worked — but nothing in the kernel ever sent it.

### At a glance

| Symptom | Shows up when |
| -- | -- |
| An entire stack script fails to compile: *"end doesn't match the name"* | a handler or function is named `sec`, `loc`, `rect`, `msg`, `cd`, `fld`… (17 names) |
| An entire stack script fails to compile: *"unexpected character"* | a handler, parameter or variable name contains an accented letter |
| *"division by zero"* dialog | a curve crosses a removable singularity, or any script divides by zero |
| A vertical bar drawn across the artwork | after the above, at the coordinates responsible |
| *"unknown property or function: centered"* | a stack reads or sets `the centered` |
| *"unknown property or function: cmdKey"* | a stack uses the short form of `the commandKey` |
| *"unknown property: lockErrorDialogs"* | a stack wants to handle its own errors |
| No way to set a field's alignment or line height with the mouse | always |

---

### 1. A synonym used as a handler name killed the whole script

**Reported as:** `function sec x … end sec` refused with *"end doesn't match
the name"* — although the name is repeated correctly.

HC's lexer expands the Appendix F synonyms everywhere, without regard to
position, so `end sec` reaches the parser as `seconds`. The parser compared
that token against the **raw** source text of the opening name, `sec`. One
word read in two forms, one comparison, a false refusal.

**Seventeen names were unusable** — `sec`, `secs`, `abbr`, `abbrev`, `bg`,
`bkgnd`, `btn`, `cd`, `char`, `fld`, `grey`, `hilite`, `loc`, `mid`, `msg`,
`poly`, `prev`, `rect`, `reg`, `tick` — and **one was enough to bring down an
entire script**. HypoGraph defines `function sec x` for the secant, so its ten
other handlers — `openStack`, `errorDialog`, `arrowKey`, `csc`, `cot` — ceased
to exist as well.

A genuinely mismatched `end` is still refused. That guard is in the test
harness, because "fix" by no longer comparing anything would have passed every
other test.

### 2. An accented letter in a name

**Reported as:** `on ayudar cómo` produced two *"unexpected character"* errors
per line, and the whole script was refused over one accent.

HyperCard named things in Mac Roman and period stacks use it — HypoGraph
passes its Spanish help text through that parameter.

Only **letters** are accepted, and only two-byte ones: the Latin-1 supplement
minus × and ÷, plus Œ œ Ÿ. Accepting any byte ≥ 0x80 would have been immediate
and wrong: `≠` would have glued itself to the preceding word, and
`if y≠"NAN(037)"` — a line from the same stack — would have become a single
identifier `y≠`.

### 3. Dividing by zero returns a value, not an error

**This is SANE**, the Macintosh's floating-point environment, which HyperCard
uses: `1/0` is `INF`, `0/0` is `NAN(004)`. HyperCard has no "division by zero"
error at all. Measured on the original stack: where the curve crosses its
singularity, the plotter displays `-.500,NAN(004)` and carries on.

The defect was double. Not only does the error not exist in HyperCard, but the
stack had **written the code that handles these values** —

```
if y contains "NAN" or y is "INF" then
  if y≠"NAN(037)" then put "ERR: x=" & x & ",y=" & y into fld errr
  if y is "INF" then put "y=±∞" into item 2 of fld errr
  else if y is "NAN(004)" then put "y=0/0 (indeterminate)" into item 2 of fld errr
```

— code that could never run, because the dialog fired first. Three SANE value
names hard-coded in a 1995 stack are the best available evidence of what
HyperCard actually returned.

The equation is not a contrived case:

```
y = (x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5
```

`(x+1/2)` appears in both numerator and denominator. The curve is smooth
everywhere, including at −1/2 where it has a perfectly well-defined limit;
only the *computation* goes through 0/0. With `theScale` at 64 the mouse
advances in steps of 1/64 and hits −0.5 exactly.

The NaN code travels in the NaN's payload and **survives subsequent
arithmetic**, which is what lets `…/(x+1/2)/5` still produce `NAN(004)` after
its division by five.

**The SANE codes are measured, not deduced.** Three of them were read in
HyperCard's own message box, running under Basilisk II:

```
put ln(-1)     ->  NAN(036)
put sqrt(-1)   -> -NAN(001)      (with the minus sign)
put 0*(1/0)    ->  NAN(008)
```

C gives none of these — on this machine all three produce a NaN with a zero
payload — so the code and the sign are set explicitly. Guessing would have
been wrong: 022 was the plausible value for the logarithm, and it is 036.

**`mod` by zero is an error, not a value** — and that too was measured rather
than deduced. `put 5 mod 0` under HyperCard opens a dialog: *"can't mod by
0"*. The assumption had been that it returned a NaN like division does, merely
with an unknown code. It does not. Two operations that look alike are not
treated alike, which is the whole reason for measuring.

`div` by zero follows the `/` rule here — `INF` and `NAN(004)` — because it is
a division. That is an **assumption, recorded as one** in the harness: nobody
has yet typed `put 7 div 0` into HyperCard.

**A silent corruption found on the way.** `strtod` reads a NaN's payload with
base 0, so a leading zero makes it **octal**. Four of the six codes HyperCard
writes were being corrupted on any string round-trip:

```
NAN(008) -> 0     NAN(009) -> 0
NAN(036) -> 30    NAN(037) -> 31
```

`NAN(037)` is hard-coded in HypoGraph — `if y≠"NAN(037)"` — and because the
code travels in the payload, the corruption would have survived every
subsequent calculation with nothing to report it. Payloads are now read in
decimal, as they are written.

### 4. The vertical bar across the plot — two defects, one of them introduced by this release

With the error gone, a vertical bar appeared across the curve at the offending
coordinates. Two independent causes:

- **A NaN was being compared as a number.** The first version of fix 3 made
  all four ordering operators return false whenever a NaN was involved — correct
  as IEEE arithmetic, wrong here, and the stack said so in its own code. Its
  NaN handling sits *inside* its bounds test, so `ny > itt` must answer **true**
  for a NaN; that is what text comparison gives. With ordering false, the guard
  never fired and the stack plotted to a non-finite point. The rule is now the
  same for ordering and equality: a NaN is compared as the string it is. It
  remains readable in arithmetic — a separate question, and one the `/5`
  requires.

- **A coordinate that is not one was drawn anyway.** `hc_coord` requires the
  whole field to be a number and returns the **default** — zero — otherwise. So
  `round(-(y-cy)*yScale + 171)` with `y` equal to `NAN(004)` produced coordinate
  zero, `drag` drew to the top edge of the card and back down to the next point:
  two near-vertical segments. What made this silent is that **zero is a
  perfectly valid coordinate** — nothing distinguished the edge of the card from
  a value that could not be read. `drag` and `click at` now refuse, naming the
  offending field.

### 5. Three missing properties

**`the centered`** — the centred drawing mode existed, but only through the
Option key, which a script cannot read. A menu item carries a mode that lasts;
a held key does not. `Draw Centered` joins the Options menu, and one definition
now serves both the preview and the commit, where two private copies had been
waiting to diverge.

**`the cmdKey`** — the short form of `the commandKey`. The kernel had declared
it since forever; the Cocoa host only knew the long name, so it returned NULL
and the property read as unknown. Found by crossing the kernel's two property
lists against every `strcasecmp` in `HCview.m` — the only one of forty-three
missing.

**`the lockErrorDialogs`** — set it true and a script error is sent to the
current card as `errorDialog <text>` instead of opening HC's error dialog. This
is what HypoGraph's own `on errorDialog them / answer them with "Cancel"` was
written for: its author plotted thousands of points and did not want
HyperCard's error dialog at each one.

Three things that could have gone wrong, and are handled: **recursion** (an
`errorDialog` handler that itself fails goes to the ordinary dialog, not to
itself); **automatic release** when returning to idle, as `lockScreen` already
does — HypoGraph sets the lock and never clears it, so without this the whole
rest of the session would have gone to `errorDialog`; and **nobody handling the
message** — the dialog is not restored, because the stack asked for silence, but
a line in the message watcher says the lock is set and no handler took the
error.

### 6. The field Text Style panel: Align and Line height

HyperCard's field text dialog offers **Align: Left / Center / Right** and
**Line height**. HC's offered neither — although the kernel has known
`textAlign` and `textHeight` all along, `hc_file` saves and reloads them, and
the renderer honours them. Only the way to set them with the mouse was missing.

**`textHeight` of zero means automatic** — four thirds of the point size,
rounded as HyperCard rounds it, so a field whose text you enlarge follows on its
own. Showing that computed value in the box would have been a silent trap:
opening the panel and clicking OK would have **frozen** the line height. The box
therefore stays empty until a value is set, with the computed one shown as a
placeholder — and clearing it restores automatic, so the setting is not
one-way.

Both controls appear **only for a field**: a button's title is always centred
and fits on one line. HyperCard shows them in both dialogs, but they are inert
controls there, and that is not a reason to copy them. The button panel keeps
its previous geometry exactly.

Line height is only *visible* when **Fixed Line Height** is ticked in the field
dialog — HyperCard's rule, and where HC applies it. That checkbox is not
duplicated in this panel: one setting in two places eventually diverges.

---

### Test status

- C kernel: **230 checks, all green**, including under AddressSanitizer.
- Zero compiler warnings at `-Wall -Wextra`.
- Five new harnesses: `synonymes.c`, `accents.c`, `sane.c`, `lockerreur.c`,
  `alignement.c`.
- The Cocoa layer still has **no** automated tests — CI only proves that it
  compiles. Every symptom above was found by a human with two screens.

Two things are **recorded in the harnesses rather than fixed**, so they are not
rediscovered later: `mod` by zero has no SANE code, and the kernel has **two ways
of reporting an error** — `put the zorglub` raises a fault and stops the
handler, `drag` merely emits and declares itself handled. Only one of the two
stops. HyperCard always stops.

### Where to hammer, if you have ten minutes

1. Name a handler or function `sec`, `loc`, `rect` or `msg`. It should work,
   and `wait 2 sec` should still be a duration.
2. Put an accent in a variable or parameter name. Then check that `≠`, `≤` and
   `≥` still work as operators next to a word.
3. `put 1/0`, `put 0/0`, `put 7 div 0`, `divide n by 0`.
4. Set a field's alignment and line height from the Text Style panel, save the
   stack, reopen it.
5. `set the lockErrorDialogs to true` with an `on errorDialog` handler, then
   without one.

Period stacks remain by far the most useful thing you can send — all ten fixes
above came from a single one.

### Changes in this release

- [#56](https://github.com/cleobuline/hc/pull/56) — synonyms as handler names,
  accented identifiers, SANE arithmetic, `the centered`, `the cmdKey`,
  `the lockErrorDialogs`, the vertical bar (`34bc9e4`, `656ee60`, `13e1ffe`,
  `a262645`)
- [#57](https://github.com/cleobuline/hc/pull/57) — the field Text Style panel
  gains Align and Line height (`713bc7d`, `7a963ed`)

**Also in this build:** the application finally reports its own version.
`MARKETING_VERSION` had been left at `0.6.5`, so 0.6.8, 0.6.9 and 0.6.9.1 all
announced "0.6.5" in the Finder and in the About box — the DMG filename was the
only thing telling the truth.

**Full changelog:** [HC-0.6.9.1...HC-0.6.9.2](https://github.com/cleobuline/hc/compare/HC-0.6.9.1...HC-0.6.9.2)
