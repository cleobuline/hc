## HC-0.8.6 — Four outside reviews, their fixes, and MAC•CAM colours

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

**Update from 0.8.5 — recommended.** Four outside reviewers read HC's code
looking for faults: Jules, Grok, ChatGPT and a second Claude. Every finding
was replayed before anything was changed; the ones that reproduced are
fixed here. The most serious one let a module brought by a stack write
outside its sandbox: **if you open stacks from people you do not know,
update.** MAC•CAM Workshop also gains `colors`, a colour for every
combination of its planes.

### At a glance

| | 0.8.5 | 0.8.6 |
| -- | -- | -- |
| A module asks for the card's picture with an absurd rectangle | **HC writes outside the module's memory** | refused |
| `closeCard` deletes the card a `go`, `find` or `pop` is heading to | HC goes to a freed card | **stays**, and `the result` says so |
| A drawing cannot be prepared for saving | the old drawing is saved, and the save **announced as done** | **nothing is written**, and an alert says why |
| Paste a grey or 1-bit picture from another application | read past its lines | **converted** to the drawing's format |
| A button of an original stack hanging off the left edge | imported **with no width** | imported whole |
| `"1/1/2000 12:99999999999" is a date` | integer overflow | `false` |
| A very large `.wasm` dropped on the window | read whole before being refused | **refused before reading** |
| The CAM window: a colour per combination of planes | — | **`colors`** |
| `the hcVersion`, "About HC" | `0.8.5` | `0.8.6` |

---

### 1. A module could write outside its sandbox

Modules — the XCMDs and XFCNs of a stack, rebuilt in WebAssembly — run
sealed: they see their own memory and nothing else. `GetCardPicture` lets a
module ask for the picture of the card in a rectangle, and HC checked that
the rectangle fitted the module's buffer by multiplying its width by its
height. At the extremes of 32-bit numbers, that product overflowed, the
check passed, and HC drew the picture far past the buffer: a module written
on purpose could make HC write into its own memory. No stack published with
HC does this, and a module built with the kit's own `GetCardPicture`
cannot.

The sides are now checked before they are multiplied, as `DrawXWindow`
already did. Two calls built to trigger the fault are part of the test
suite; without the fix, the test process is killed.

### 2. Going to a card that `closeCard` deletes

`go`, `find` and `pop` send `closeCard` to the card you leave before they
settle on the card you go to. A `closeCard` handler that deleted that very
card freed it, and HC went on to read it and send it `openCard`. HC now
stays where it is, and `the result` is `No such card` (or `Not found` for
`find`), as for a card that does not exist. What HyperCard does in this case
has not been measured.

The same family as the next one: an object held by HC while a handler can
delete it. A handler that deletes its own card on `closeCard`, taking its
background with it, is handled too.

### 3. Saving, and a drawing that could not be kept

Before saving, HC turns each card's drawing into the form it writes to
disk. If that failed — for lack of memory — the drawing was skipped, the
stack was written with the old drawing, and HC said the save had worked.
The session's drawing then lived only in memory. Now nothing is written, the
original file stays as it was, and an alert explains. `save stack … as …`
returns the failure to the script.

And the saved file now reaches the disk before it takes the original's
name, so that a power cut at the wrong moment cannot leave a stack with its
name but without its contents.

### 4. Smaller fixes

- **Paste.** A picture pasted from another application is converted to the
  drawing's format first. A grey or 1-bit picture was read past the end of
  its lines.
- **`idle`** is no longer sent to a card that a `mouseLeave` or
  `mouseWithin` handler has just deleted.
- **Original stacks.** A button or field whose rectangle went past the left
  or top edge of the card was imported with no width or height: invisible,
  and deaf to clicks. QuickDraw rectangles are signed, and are now read so.
- **Dates.** A time with absurdly long minutes or seconds overflowed an
  integer while being read. It is now simply not a date.
- **Modules.** A `.wasm` file over the 8 MB limit is refused before it is
  read, not after. `DrawXWindow`'s scale is kept between 1 and 64.
- **Very large cards.** The pixel arithmetic of the drawing tools no longer
  overflows on drawings over about 46,000 points across.

### 5. MAC•CAM Workshop: `colors`

The original MAC•CAM coloured its planes with colour tables. HC does not
read those tables; the CAM window now takes a colour for every combination
of its four planes:

```
set the colors of window "CAM" to "1000 255,0,0" & return & "0100 0,128,255"
set the colors of window "CAM" to "1xxx 255,255,255"   -- plane 0 on: white
set the colors of window "CAM" to empty                 -- the original palette
put the colors of window "CAM"     -- all sixteen, 0000 to 1111, one per line
```

Each line is a pattern — one sign per plane, `x` for "on or off" — and a
colour `r,g,b` from 0 to 255. Lines apply in order, the last one wins, and a
combination no line names keeps its colour. `planes` comes first: a hidden
plane counts as off. Only the drawing changes; the machine knows nothing of
colours.

### 6. Around HC

- **The reviews.** Jules (two passes), Grok (two passes), ChatGPT (two
  passes) and a second Claude each read a part of HC. Their reports are in
  `docs/audits/`, except Grok's, kept outside; what was replayed, what
  reproduced, what did not and why, is in `docs/mesures/audits_octobre.txt`.
  Several findings did not reproduce, and are not fixes: a review is a
  question asked of the code.
- The README credits FullDrag to Frédéric Rinaldi, and the FORTH and MACCAM
  externals of MAC•CAM to their author.

---

### Test status

- C kernel: **314 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: a module that asks for
  an impossible picture, a module that deletes the current card (from a
  button, from the card, from the message box), navigation towards a card
  that `closeCard` deletes, and absurd times.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.

### Not measured yet

- What HyperCard does when `closeCard` deletes the card a `go` is heading
  to.
- The fixes on the Cocoa side — `idle`, paste, saving, a dropped module —
  build on macOS in continuous integration, but were not run here: the
  checks run on Linux. A power cut during a save cannot be replayed.
- The colours of a pasted picture that was not RGB: it now goes through
  AppKit's colour management. An ordinary RGB picture is not touched.
- The rest of 0.8.5's list stands.

### Known, and not fixed here

- The kernel's messages are still in French, among them some module
  errors; the refusal of an oversized `.wasm` is in English.
- Everything listed in 0.8.5 under "Known, and not fixed here".

### Changes in this release

- [#154](https://github.com/cleobuline/hc/pull/154): who wrote the
  externals of MAC•CAM, in the README
- [#155](https://github.com/cleobuline/hc/pull/155): `colors`
- [#157](https://github.com/cleobuline/hc/pull/157): the first reviews —
  `GetCardPicture`, `idle`, dates, original rectangles
- [#159](https://github.com/cleobuline/hc/pull/159): the second reviews —
  navigation, paste, saving, dropped modules
- this release: version 0.8.6 and this note

**Full changelog:** [HC-0.8.5...HC-0.8.6](https://github.com/cleobuline/hc/compare/HC-0.8.5...HC-0.8.6)
