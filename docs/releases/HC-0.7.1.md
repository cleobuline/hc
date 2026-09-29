## HC-0.7.1 — imported stacks draw the way Apple drew them

Two drawing defects, both found by looking at the same card side by side —
HC on the left, HyperCard under Basilisk II on the right, Apple's "Stack
Templates", the *Year Calendar*. Both were fixed by asking Apple's own
geometry what the right number was, rather than by nudging a constant until
the picture looked better.

**If you opened a HyperCard stack in 0.7, replace it.** The text was read
correctly; it was drawn in boxes too small for it, and lines fell out
silently.

### At a glance

| | 0.7 | 0.7.1 |
| -- | -- | -- |
| the weekday header of each month (`M T W T F S S`) | **invisible** | shown |
| a month's sixth week | dropped | shown |
| January in the year calendar | ran to 19 | runs to **31** |
| vertical text margin of a field | 8 px total | **2 px** (measured) |
| Courier / Monaco advance | 0.6 em, 8 % too wide | **integer**, as a bitmap font |
| `the hcVersion` | see below | `0.7.1` |

**A note on the 0.7 tag.** It points at a commit made *before* the version was
raised, so an app built from that tag answers `0.6.9.4` to `the hcVersion` and
shows it in "About HC". Check yours; if it says `0.6.9.4`, that is why, and
0.7.1 settles it.

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

### 1. The measurement that moved the search from one end of the program to the other

Before touching any code, the **real** reader and the **real** importer were
run on the file, and what the file holds was compared with what the model
shows:

```
file  : 44 contents for the "Year Calendar" background
model : 44 contents, including all TWELVE "M  T  W  T  F  S  S"
```

**The days were loaded.** So the defect was not in reading the stack — it was
in drawing it. Without that comparison the fix would have gone hunting in the
importer, which had done nothing wrong.

### 2. A field's vertical margin was eating whole lines

```c
CGFloat m = o->wide_margins ? 8 : 4;
return NSInsetRect(r, m, m);
```

`NSInsetRect` removes `m` from the **top and the bottom** as much as from the
sides: eight pixels of height. The twelve `Weekdays` fields are **104×12** —
exactly one line of 12. After the margin, **four pixels** were left for a line
that needs twelve, and the line was dropped entirely. Same cause, quieter, for
each month's sixth week: 72 px tall, six lines of 12.

**How much, then? The question went to Apple's 349 fields**, not to a
document. Apple sized fields in whole lines, so the right margin is the one
for which `(height − margin)` lands exactly on the line height most often:

| total vertical margin | normal margins (261) | wideMargins (88) |
| -- | -- | -- |
| 2 px | **28.4 %** ← peak | **47.7 %** ← peak |
| 8 px *(what we did)* | 0.4 % | 4.5 % |

Two pixels in total — one per side — and the peak of **both** independent
populations. 8 px was not an approximation; it was the **floor** of the table,
the worst value available. And `wideMargins` adds nothing vertically: both
columns peak in the same place, which is what licensed separating the two axes
instead of tuning by eye.

### 3. The month box was too small: our Courier is 8 % too wide

With the headers back, the months still lost their last weeks. **It was not
the margin**, and Apple's geometry says so:

| field | width | characters | px per character available |
| -- | -- | -- | -- |
| `4 Month` | 108 | 21 | **5.14** ← the tightest |
| `Weekdays` | 104 | 19 | 5.47 |
| `Calendar 1` | 115 | 20 | 5.75 |

macOS's Courier 9 advances 0.6 em, or **5.40 px**. Twenty-one characters need
113.4 px in a field that is 108 wide: **it does not fit at any margin, not
even zero.** Shaving the horizontal margin would have squeezed the `Calendar`
fields in (115 px) and left the `Month` fields broken (108 px) — half the
symptom cured and the cause hidden underneath, to be paid for again at the
next template.

**The cause is the font.** The Macintosh's Courier was a **bitmap** font, and
a bitmap font advances by a whole number of pixels: 5 at 9 points. Apple sized
these fields to the pixel for that advance; we draw with an outline font that
advances 5.40. Eight per cent, which twenty-one characters turn into 5.4
pixels of overflow. The advance is now pulled back to the integer by kerning,
for **Courier and Monaco only** — the fixed-pitch fonts of the period, where
the error accumulates into columns and where the correction is a single
number.

**And a latent defect found on the way:** `style_attrs` **overwrote**
`NSKernAttributeName` for *condense* and *extend*. Laying the advance
correction on top would have made one of the two disappear without a word. The
three now add up.

### 4. What is not measured is written down as not measured

- The **horizontal** margin does not move, and that is no longer a hole — it
  was examined, and it is not the cause. See `docs/mesures/marges.txt`.
- The **proportional** period fonts — Geneva, Chicago, New York, Palatino —
  are untouched. The same discrepancy must exist, but correcting it needs a
  per-character advance table that nobody here has measured.
- Only one case is measured for the fixed-pitch rule: Courier 9, from the
  geometry of Apple's fields. Extending it to other sizes and to Monaco rests
  on the principle that a bitmap font advances by an integer, not on a reading.

**And a check that did not settle anything, written down precisely because it
did not.** To confirm the integer-advance rule across the corpus' 44
fixed-pitch fields: 34 fit with macOS's advance, 35 with the integer one, and
the worst gaps were −1200 px. The instrument was counting fields of **prose**,
which are perfectly entitled to wrap. It measured something other than what it
announced, and it proves nothing. What carries the conclusion is the one
layout that *cannot* wrap: the calendar's grid.

**The instrument lied once more**, which is becoming this project's signature
lesson: the probe tested `if (!hc_origine_lit(...))` when that function
returns **zero** on success. Its first reading announced a refusal on a
perfectly successful parse.

---

### Test status

- C kernel: **250 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families.
- `math.h` is declared rather than relied on through a transitive Cocoa
  include: one line uses `floor`, and the only compiler that would have
  settled it does not run in the container.
- The Cocoa layer still has **no** automated tests — CI only proves it
  compiles. `HCtext.m` changed here, so the check that decided was a human
  one: **January runs to 31**, and all twelve months show their six weeks.

### Known, and not fixed here

Everything listed under 0.7 still stands — named windows, patterns,
resource-fork icons, HyperCard 1.x, private-access stacks, a fault inside a
user function not stopping its caller, and the four benches still to play in
HyperCard. `CFBundleVersion` is still hard-coded to `1`. Not notarised; see
"Installing".

### Changes in this release

- [#77](https://github.com/cleobuline/hc/pull/77) — version 0.7 in all three
  places, the English release note, the stale re-record list in
  `docs/livraison.md` corrected, and the decision that the import stays
  one-way written down with its reasons (`352c5f5`, `48b1dbe`, `6af7e89`)
- [#78](https://github.com/cleobuline/hc/pull/78) — the two drawing defects
  above, measured on Apple's geometry (`d27e9a4`, `4c09031`)

**Full changelog:** [HC-0.7...HC-0.7.1](https://github.com/cleobuline/hc/compare/HC-0.7...HC-0.7.1)
