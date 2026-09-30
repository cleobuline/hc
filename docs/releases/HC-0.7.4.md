## HC-0.7.4 — the torture stack agrees with HyperCard, line for line

One result sums this release up. HC's **torture stack** is 108 checks, each
carrying an answer measured in real HyperCard running under Basilisk II. It
now gives the same report on both sides:

| | passed | failed | still to measure |
| -- | -- | -- | -- |
| in HyperCard (Basilisk II) | 108 | 0 | 0 |
| in HC | 108 | 0 | 0 |

It took three fixes to get there, and all three come down to **which layer
HyperCard looks at first**. This release also fixes a crash that an outside
audit found, keeps a stack intact when memory runs out, and adds clang's
static analyser to the CI.

### At a glance

| | 0.7.3 | 0.7.4 |
| -- | -- | -- |
| `field 1`, `field "Name"` with no layer | the **card** field | the **background** field, as in HyperCard |
| `the number of fields` | card + background, added up | the **background** only |
| `the number of buttons` | card + background, added up | the **card** only |
| `find`, same word on both layers of a card | card first | **background first** |
| `select the foundChunk`, then `the foundChunk` | still filled | **empty**, as in HyperCard |
| `go first background` with no card open | **crash** | refused cleanly |
| static analysis (`clang --analyze`) | not run | **zero warnings**, enforced in CI |
| out of memory while writing a field, a menu, a sort | stack could be damaged | **left intact**, error shown |
| `lock screen` when macOS forces a redraw | showed the card being painted | **stays frozen** |
| `the mouseClick` after a click during a script | always `false` | **`true`** |
| torture stack, HC vs HyperCard | 2 lines disagreed, 2 still open | **all 108 agree, none open** |
| `the hcVersion`, "About HC" | `0.7.3` | `0.7.4` |

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

### 1. A field with no layer is a background field

`field "Total"` with no `card` or `bkgnd` in front of it is the most common
phrase in stacks from 1987. HyperCard reads it as the **background** field.
HC read it as the **card** field. A stack with a field of the same number or
name on both layers therefore read and wrote the wrong text in HC, with no
error at all.

Measured in HyperCard with a full bench, played twice:

- **a field with no layer is on the background**: reading it, writing it,
  hiding it and `the name of field 1` all go to the background;
- **a button with no layer is on the card**, which HC already did;
- `the number of fields` counts the **background** fields only, and
  `the number of buttons` the **card** buttons only. HC added both layers
  together, so it answered 7 and 3 where HyperCard answered 3 and 2;
- a name that is **not on the background** raises an error in HyperCard.

HC now follows the same rule, with one deliberate improvement. Where
HyperCard would stop with an error, HC **falls back on the card field** of
that name. The answer is the same wherever HyperCard succeeds, and stacks
written in HC, where `field "X"` often means a card field, keep working. A
layer you write out (`card field`, `bg field`) never falls back.

**If you wrote stacks in HC:** a loop over `the number of fields` that meant
the card fields now counts the background ones. Write
`the number of card fields`, as HyperCard requires.

### 2. `find` looks at the background first

When the same word is in a card field and a background field of the same
card, HyperCard finds the **background** one first, then the card one. HC did
it the other way round. This was also the cause of the only two lines of the
torture stack that had disagreed with HyperCard since September 27: the
search took a different route from there on.

It is the same priority as section 1. The two differences had a single cause.

### 3. `select` clears the found text

In HyperCard, right after `find "x"` then `select the foundChunk`,
`the foundChunk` is **empty**, and so are `the foundText`, `the foundField`
and `the foundLine`. Any `select` of text in a field does the same. HC kept
them until the next search, and its source code said so without ever having
measured it. `select empty` clears nothing, in HyperCard or in HC.

HC now empties what these four functions return. The search position itself
is kept, so the next `find` carries on from where it was: what a `select`
does to it has not been measured.

The two last open questions of the torture stack were settled on the way.
After a `find` that fails, the next one starts again from the current card,
and `the foundChunk` names the background field. HC already did both.

### 4. What two outside audits found

A first outside audit, made with clang's static analyser, reported four points.
Each was checked before anything was changed:

- **a real crash**: `go first background` typed with no card open — before
  the first stack is opened, or after the current one is closed — read
  through a null pointer. It is reproduced under AddressSanitizer, fixed, and
  held by a test;
- one point **cannot happen**, and the code now states its assumption once;
- one is **right**: the function that exits on an allocation failure is now
  marked as never returning;
- one is a **false alarm**, and the fix the audit recommended **would have
  broken** the reading of older stack files and of paint. Measured: two tests
  fail with it. The invariant is written down instead.

The analyser now reports **zero warnings** on the kernel. `make analyse`
requires that, and it runs as its own CI job. It was checked that it does
fail on the old code.

A second audit then pointed at the moments when **memory runs out**. Several
writes threw the old value away before holding the new one. A test harness
now makes the n-th allocation fail, one after the other, and counts the runs
that leave the stack damaged. On 0.7.3 it found damage in six places:

- the text of a card field, and of a background field on one card;
- turning a field's text into shared text, which lost its styles;
- `sort cards`, which could change the order without a word;
- `open file`, which lost the file and still said it had succeeded;
- the name of a menu item, which was written **empty**, with no error.

All six now leave the stack **as it was** and say
"mémoire insuffisante". The last one came from deeper down: an expression
that ran out of memory evaluated to empty. That is fixed at the source, for
every command. It is rare on a Mac with gigabytes free. When it does happen,
though, a stack that stays intact is worth more than a stack that loses a
field without a word.

### 5. The screen, the mouse and the keyboard during a script

The same second audit read the Cocoa side too.

- **`lock screen` now really freezes the screen.** The frozen picture was
  taken, but never drawn: HC only held back the redraws it asked for
  itself. A redraw forced by macOS — a dialog closing over the card, a
  window uncovered — showed the card as the script was painting it. The
  frozen picture is now what gets drawn until `unlock screen`.
- **Each window keeps its own lock and its own visual effect.** They were
  shared by every open stack, so another window could play an effect that
  was meant for the first one.
- **A click and a keystroke given during a script stay in order.** Measured
  in HC: click a button while a script runs, then type a key, and the key
  was served first. They now come out in the order they were given.
- **`the mouseClick` answers `true` for a click given during the script.**
  The same measurement showed that a click waits for the end of the script
  before it reaches the card. `the mouseClick` was only told about clicks
  that reached the card, so a real click never made it answer `true`, and
  `if the mouseClick then exit repeat` never left the loop.

Not measured: what HyperCard does with that click. HC still delivers it
once the script has finished; HyperCard may drop it instead. That is the
next bench, to play in HyperCard.

### 6. What is not measured is written down as not measured

- Applied for consistency, **not measured**: a field designated by **id**
  (`field id 1`) or by an **ordinal** (`first field`) follows the same layer
  rule as `field 1`.
- A **click** on text in a field does not clear the found text. Only a
  script `select` does. What a click does in HyperCard is not measured.
- `select before …` and `select after …` are treated like any other `select`,
  without a measurement.
- Deduced from the torture stack, **not measured**: in HyperCard, writing into
  another field seems to lose the current text selection. HC keeps it.
- The Cocoa layer still has no automated tests. CI only proves it compiles.

---

### Test status

- C kernel: **265 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. Four new ones:
  `sanscarte`, `couchefond`, `findcouches`, `findselect`.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families. **Zero** static-analyser warnings.
- The torture stack: **108 / 0 / 0** in HyperCard and in HC.

### Known, and not fixed here

- After a fault raised **by a command** (`hide`, `set`, a message nobody
  handles), the handler **carries on** and the dialog comes at the end.
  HyperCard probably stops on the spot — to play in HyperCard.
- A fault inside a user function stops the function but not its caller.
- `visual effect` followed by `go`, **without** `lock screen`, still plays
  after the script rather than inside `go`.
- Still standing from 0.7: named windows, patterns, resource-fork icons,
  HyperCard 1.x, private-access stacks, the proportional period fonts.
  `CFBundleVersion` is still hard-coded to `1`. Not notarised; see
  "Installing".

### Changes in this release

- [#87](https://github.com/cleobuline/hc/pull/87) — the outside audit: the
  no-card crash, `make analyse` and its CI job; and an inventory of what an
  App Store sandbox would break, kept for later (`5bb18a6`, `8c852ed`)
- [#88](https://github.com/cleobuline/hc/pull/88) — a field with no layer is a
  background field; the field and button counts follow (`c78af21`)
- [#89](https://github.com/cleobuline/hc/pull/89) — `find` looks at the
  background first (`e60ac10`)
- [#90](https://github.com/cleobuline/hc/pull/90) — `select` clears the found
  text; the torture stack's last questions settled (`1e42c32`, `23ed3b3`)
- [#91](https://github.com/cleobuline/hc/pull/91) — the torture stack agrees
  with HyperCard, 108 / 0 / 0 (`8638959`)
- this release — out-of-memory writes leave the stack intact; `lock screen`
  really frozen, per window; clicks and keys in order, `the mouseClick`
  during a script; version 0.7.4 in all three places, and this note

**Full changelog:** [HC-0.7.3...HC-0.7.4](https://github.com/cleobuline/hc/compare/HC-0.7.3...HC-0.7.4)
