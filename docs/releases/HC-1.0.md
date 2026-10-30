## HC-1.0 — Sandboxed, with a working folder, and ready for the Mac App Store

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

**Update from 0.8.9 — read this first.** HC now runs in the **App Sandbox**,
which the Mac App Store requires. The DMG and the App Store build are the same
application: one HC, tested once, behaving the same everywhere.

In the sandbox, HC only reads and writes what you give it: a stack you open
from a panel or double-click in the Finder, a file you save. To let it find
stacks by name — `go to stack "Games"`, `start using stack "Tools"`, and your
Home stack at launch — **choose a working folder once**:

> **File ▸ Choose Working Folder…**

HC remembers it from one launch to the next. Put your stacks and your Home
stack in it. Your stacks themselves are not touched. HC's own settings live in
the sandbox now, so they start afresh once.

### At a glance

| | 0.8.9 | 1.0 |
| -- | -- | -- |
| App Sandbox | no | **yes** |
| Where HC looks for stacks by name | next to the open stack, next to HC, Documents | the same, **plus the working folder** — the only place it may read in the sandbox |
| HC launched on its own | a new "Untitled" stack | **the Home stack**, as in HyperCard |
| Demo stacks | download them from the website | **File ▸ Load Demos…** |
| `go to stack the short name of me` | did nothing, silently | goes to that stack |
| Saving "Untitled" as "home stack" | the window still said "Untitled" | the stack takes the file's name |
| Option-drag in FatBits | nothing | scrolls the magnified view, as in HyperCard |
| FatBits pixels and their grid | one screen point off, vertically | aligned |
| Edit ▸ Icon… with two stacks open | only the built-in icons | the stack's icons too |
| `the hcVersion`, "About HC" | `0.8.9` | `1.0` |

---

### 1. The App Sandbox, and the working folder

Before anything was changed, the sandbox was switched on for a test, and three
things broke: Home no longer opened at launch, `start using stack "outils"`
answered that the stack could not be found, and saving a stack was refused.
All three had one cause: the sandbox lets HC read and write only the files it
is given, and nothing around them.

- **File ▸ Choose Working Folder…** picks a folder once. HC keeps the right to
  use it through a security-scoped bookmark, and reopens it at every launch,
  before it looks for Home. It is HC's simpler version of the "Search Paths"
  of HyperCard's Home stack: one folder instead of a list, and not a single
  new HyperTalk word.
- **Saving works everywhere.** HC still writes a stack to a temporary file
  first and only then replaces the old one, so a failed save never damages the
  original. Outside the working folder, where the sandbox refuses that
  temporary file, HC now uses the method Apple provides for sandboxed apps:
  a temporary folder macOS allows, then a single replacing step.
- **Open and Save panels** start in the working folder, or in your real
  Documents folder — never in the hidden one the sandbox gives each app.
- **`open file`** on a name HC cannot find already asks you where the file
  is. In the sandbox, answering that question is also what grants access.

HC asks for no other right: no network, no microphone, no camera.

### 2. Home at launch, as in HyperCard

Measured inside HyperCard 2.4.1, under Basilisk II: launched on its own,
HyperCard opens Home; launched by double-clicking another stack, it opens only
that stack. HC now does the same. Home is looked for the way
`go to stack "Home"` looks for it — in the working folder first. When there is
no Home, HC opens "Untitled" as before, without a dialog.

The first time you choose a working folder, if HC is still showing an empty
"Untitled", Home opens right away.

### 3. Load Demos…

**File ▸ Load Demos…** opens the demo stacks' download page in your web
browser. The browser downloads `demo.dmg`, the Finder opens it, and you drag
the stacks into your working folder. HC itself downloads nothing: it needs no
network access, and the demos can be updated on the website without a new
version of HC.

### 4. Fixes

- **`go to stack the short name of me`** — and `go to card the short name of
  me` — did nothing at all: no stack, no error, an empty `the result`. HC read
  `stack` alone, meaning "this stack", and left the property aside. A property
  now names the stack. With parentheses, it always worked.
- **Saving a stack under a new name** now gives the stack that name, in its
  window and in its file. "Untitled" saved as "home stack" used to stay
  "Untitled", even after reopening.
- **FatBits**: Option-drag scrolls the magnified view, as measured in
  HyperCard; ⌘-drag still does. The magnified drawing was one screen point too
  high on its grid; HC now enlarges the drawing itself instead of leaving it
  to macOS, and the pixels sit exactly in their squares.
- **The Icons panel**, opened from Edit ▸ Icon… while a second stack was open,
  showed only the built-in icons: the menu spoke to the first window, not the
  one in front. Card Info…, Bkgnd Info… and Stack Info… had the same flaw —
  read in the code, not seen on screen — and are fixed with it.

---

### Test status

- C kernel: **321 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `go to stack` with a
  property, its twin for cards, and the forms that already worked.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.
- The sandbox was tested inside HC itself, with the sandbox checked by
  `codesign`: no working folder, choosing one, Home at the next launch,
  `start using`, and saving inside and outside the working folder.

### Not measured yet

- Whether HyperCard accepts `go to stack the short name of me` without
  parentheses. HC accepts it either way.
- Whether Home, without a window, stays in the message path in HyperCard
  when another stack was opened, and whether Home receives `startUp`.
- What HyperCard does when Home cannot be found.
- Everything listed in 0.8.8 under "Not measured yet".

### Known, and not fixed here

- A stack renamed in the Finder keeps, once opened, the name written inside
  its file.
- Some error messages from the HyperTalk interpreter are still in French —
  among them the one for a stack that cannot be found.
- `find x in card field "f"`, where `x` is a variable, answers "unknown
  property" (see 0.8.9).
- Everything listed in 0.8.6 under "Known, and not fixed here".

### Changes in this release

- [#172](https://github.com/cleobuline/hc/pull/172): the Icons panel lost the
  stack's icons when opened from the menu
- [#173](https://github.com/cleobuline/hc/pull/173): FatBits — Option-drag,
  and the drawing back on its grid
- [#174](https://github.com/cleobuline/hc/pull/174): `go to stack the short
  name of me`, and a saved stack takes its file's name
- [#175](https://github.com/cleobuline/hc/pull/175): HC launched on its own
  opens Home
- [#176](https://github.com/cleobuline/hc/pull/176): the App Sandbox, the
  working folder, and Load Demos…
- [#177](https://github.com/cleobuline/hc/pull/177): version 1.0

**Full changelog:** [HC-0.8.9...HC-1.0](https://github.com/cleobuline/hc/compare/HC-0.8.9...HC-1.0)
