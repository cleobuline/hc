## HC-0.7.6 — the old expression engine is gone, and stack icons come back

The old expression engine that HC still fell back on is now **removed**:
1,800 lines out, and the test suite gives the same answers without them.
Real stacks found four more gaps along the way, and all four are fixed.

And HC can now open a **MacBinary file** (`.bin`), which keeps a classic
Mac file whole: the stack **and its icons**.

### At a glance

| | 0.7.5 | 0.7.6 |
| -- | -- | -- |
| the old expression engine | still there, could be switched off | **removed** |
| one click during `if the mouseClick then add 1 to n` | counted on every pass of the loop (235 for one click) | **counted once**, as in HyperCard |
| `the top of card window` | "a number is expected here" | **served**, measured from the screen |
| `f(1,,3)`, `myHandler 1,,3` | "expression expected" | **an empty parameter**, as `send` already did |
| `PopUpMenu(list,,top,left)` | unknown function | **imitated**: a real popup menu |
| opening a `.bin` | not recognised | **the stack and its icons** |
| `the hcVersion`, "About HC" | `0.7.5` | `0.7.6` |

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

`Pendu.stack` still sits next to the app in the DMG.

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. Stack icons, through MacBinary

A classic Mac file has two parts: the **data**, which holds the stack, and
the **resource fork**, which holds its icons, pictures and externals. Almost
every way of copying a file off a classic Mac keeps only the first part. That
is why imported stacks showed empty boxes on their icon buttons.

A **MacBinary** archive (`.bin`) keeps both parts in one file. Make it
**inside the emulator** — StuffIt and most utilities of the time offer it —
then open it in HC with **File → Open…**, the same command as for any other
stack. HC recognises the three kinds on its own:

- an HC stack;
- a HyperCard stack (data part only);
- a `.bin` holding a HyperCard stack.

The `ICON` resources become the stack's icons, under their original numbers
and names, so buttons find them again. Tried on Apple's "Stack Templates":
all 53 icon buttons show their picture — 6 icons from the stack itself, 47
from HyperCard's own set, which HC already ships.

If the `.bin` does not hold a stack, HC says so and names the file type it
found. If the resource fork is damaged, the stack still opens, and the import
dialog says why some buttons are empty.

Not read yet: colour icons (`cicn`), pictures (`PICT`), sounds. A `.bin`
double-clicked in the Finder or dropped on the Dock icon is **not tried**;
**File → Open…** is the way.

### 2. The old expression engine is removed

In 0.7.5 the old engine could be switched off, and the suite lost nothing
without it. The engine itself is now gone: 28 functions, plus 8 that were
already dead, leftovers of an even older line executor. Nothing called them
any more. They called one another in a closed loop, which is why the compiler
never flagged them as unused.

Several real stacks were played in HC after the removal: nothing to report.

If a stack somewhere relied on a turn of phrase that only the old engine
understood, it will now say so with a named error — "object not found",
"unknown function" — instead of quietly answering.

### 3. `the mouseClick` counts each click once

```
repeat until the ticks - t > 300
  if the mouseClick then add 1 to n
end repeat
```

One click during this loop gave **235**: the same click was seen again on
every pass. It now gives 1, and four clicks give 4.

**Measured in HyperCard** under Basilisk II, with the same script: the same
answers, and a click held down also counts once. The reference says that
`the mouseClick` waits for the button to be released; HyperCard does not, and
neither does HC.

### 4. The card window knows where it is

```
put (bottom of target + top of card window + 1) into tp
```

This line, from a 1992 stack, raised "a number is expected here", for two
reasons:

- after `card`, a rank can be computed — `card i + 1` is the next card — so
  `top of card window + 1` looked for the card ranked "window + 1". `card
  window` now stops at the word `window`;
- of the card window, only `width`, `height`, `rect` and `loc` were served,
  and `rect` always read `0,0,width,height`.

`left`, `top`, `right`, `bottom`, `topLeft`, `bottomRight` and `rect` are now
measured from the top-left corner of the screen with the menu bar, as the
HyperCard reference describes. That is what lets a script turn a point on
the card into a point on the screen.

### 5. Minkowski Stack 1: empty parameters, and `PopUpMenu`

```
put PopUpMenu(list,,tp,lp) into it
```

- **An empty parameter.** The place between two commas is an empty string,
  in a function call as in a message. `send` already worked this way.
- **`PopUpMenu`** is an external function (XFCN) by Andrew Gilmartin, whose
  68000 code lived in the stack's resource fork and cannot run here. HC
  imitates it: a real popup menu, placed where the script asks, returning
  the number of the chosen item. A stack that defines its own `PopUpMenu`
  function keeps precedence.

**Palettes** (`palette "Name"`, the `PLTE` resource) are **not supported**,
by decision: HC could read them but never edit them. A stack that opens one
gets an error at that line, and the rest of it runs.

### 6. What is not measured is written down as not measured

- The card window values themselves have not been compared with HyperCard.
  `the loc of card window` is the top-left corner of the window according to
  the reference, and the centre of the card in HC: **not measured**,
  unchanged.
- A trailing comma — `f(1,)`, `myHandler 1,` — counts as one more empty
  parameter, as with `send`. **Not measured** in HyperCard.
- What the original `PopUpMenu` returned when nothing was chosen (0 here),
  and what it did with the Menu Manager's other special characters: **not
  known**.

---

### Test status

- C kernel: **274 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New ones include:
  - `macbinaire`: a MacBinary file and a resource fork built by the test,
    damaged files refused, 7,000 damaged variants read without a fault;
  - `popupmenu` and `argvide`: the Minkowski line, and empty parameters by
    every route;
  - `fenetrecarte`: the card window, with and without a placed window.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families. **Zero** static-analyser warnings.

### Known, and not fixed here

- Palettes (`PLTE`), colour icons, pictures and sounds from a resource fork.
- A fault inside a user function stops the function but not its caller, which
  carries on with an empty value.
- Still standing from 0.7:
  - named windows, patterns;
  - HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#98](https://github.com/cleobuline/hc/pull/98): the old expression engine
  removed; `the mouseClick`; the card window; empty parameters and
  `PopUpMenu`; icons through MacBinary
- this release: version 0.7.6 in all three places, and this note

**Full changelog:** [HC-0.7.5...HC-0.7.6](https://github.com/cleobuline/hc/compare/HC-0.7.5...HC-0.7.6)
