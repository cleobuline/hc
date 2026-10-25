## HC-0.8.8 — Colour icons at full speed, and background messages where HyperCard sends them

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

**Update from 0.8.7 — recommended if your stacks use colour icons.** A card
covered in colour icons was redrawn so slowly that a game built on them could
not be played. It now can. And a `go` from one background to another sends
`closeBackground` and `openBackground` to the cards, as measured in HyperCard
2.4.1.

### At a glance

| | 0.8.7 | 0.8.8 |
| -- | -- | -- |
| A colour icon, each time the card is drawn | painted point by point: 1,024 fills | drawn in one go |
| A game with 240 colour-icon buttons, on a MacBook | not playable | playable |
| `on closeBackground`, `on openBackground` in a **card** script, during `go` | never called | called |
| `the target` in those handlers | the background | the card, as in HyperCard |
| `the hcVersion`, "About HC" | `0.8.7` | `0.8.8` |

---

### 1. Colour icons, drawn in one go

HC painted a colour icon point by point: for each of its 1,024 points, a new
colour and a fill. HC also redraws the whole card when anything visible
changes during a script, and once more at each `lock screen`, to keep a
picture of the card for a possible visual effect. A card with 240 colour
icons thus cost close to half a million fills per redraw.

**Boulder-Prout**, a homemade Boulder Dash written in HyperTalk, found it:
each of its 240 cells is a button carrying a colour icon. With 0.8.7 it was
not playable on a MacBook; with 0.8.8, on the same MacBook, it is.

Each colour icon is now turned into a picture once, kept, and drawn in a
single operation. Its colours and the positions of its points are computed
as before, and an enlarged icon keeps its square points. Black-and-white
icons, and a colour icon in a highlighted button, are drawn as before.

What has not been measured is how the time split between the icons
themselves, the redraw of the whole card, and the picture taken at each
`lock screen`. Those two still cost what a redraw of the card costs — much
less, now.

Boulder-Prout is on <https://hc.labynet.fr>, with HC's other demo games.

### 2. `closeBackground` and `openBackground` go to the card

Measured in HyperCard 2.4.1, with a log kept in the stack script: a `go`
from a card of one background to a card of another sends `closeBackground`
to the card you leave, and `openBackground` to the card you arrive at — as
New Background does, which 0.8.7 already imitated. HC sent them to the
backgrounds themselves: an `on openBackground` handler in a card script was
never called, and `the target` said `bkgnd` where HyperCard says `card`.
`go`, `find` and `pop` now send them to the cards.

If a `closeCard` handler deletes the card you leave, `closeBackground` goes
to the background instead. What HyperCard does then has not been measured.

---

### Test status

- C kernel: **316 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: the targets of
  `closeBackground` and `openBackground` during a `go` from one background
  to another, there and back.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.
- The colour icon change is on the Cocoa side, which the automatic checks
  build but do not run: it was tried in HC, by playing Boulder-Prout.

### Not measured yet

- How a redraw of a card with many colour icons splits its time: the icons,
  the whole-card redraw, the picture taken at `lock screen`.
- Where `closeBackground` goes in HyperCard when `closeCard` has deleted
  the card being left.
- Going to another stack that is already open: HyperCard sends
  `suspendStack` and `resumeStack`; their order and their targets have not
  been measured.
- The rest of 0.8.7's list stands: a handler that deletes the card a `go`
  or a New Card is heading to; `the short name` of a background, button or
  field without a name; `go bkgnd ""`.

### Known, and not fixed here

- Everything listed in 0.8.6 under "Known, and not fixed here".

### Changes in this release

- [#165](https://github.com/cleobuline/hc/pull/165): `closeBackground` and
  `openBackground` go to the card, as in HyperCard
- [#166](https://github.com/cleobuline/hc/pull/166): colour icons drawn in
  one go

**Full changelog:** [HC-0.8.7...HC-0.8.8](https://github.com/cleobuline/hc/compare/HC-0.8.7...HC-0.8.8)
