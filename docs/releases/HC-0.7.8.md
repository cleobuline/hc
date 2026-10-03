## HC-0.7.8 — shaping polygon buttons with the mouse, and see-through colours

0.7.7 brought polygon buttons and button colours, made for games. They could
only be shaped by script. In 0.7.8 you shape a polygon **with the mouse**, button
colours can be **semi-transparent**, and the **Button Info** dialog has a colour
chooser.

This completes the two extensions announced in 0.7.7. **No new extension
follows.** 0.7.8 is meant to last: what comes next is faithfulness to HyperCard
and solidity, not new features.

### At a glance

| | 0.7.7 | 0.7.8 |
| -- | -- | -- |
| a button set to `polygon` that has no vertices | stayed a rectangle | **becomes a triangle**, drawn inside its rectangle |
| moving a vertex, adding one, removing one | `set the points` only | **with the mouse** too, using the Button tool |
| Delete key on a polygon whose handles are shown | deleted the whole button | **removes the selected vertex**, never below three; **never the button** |
| `"r,g,b,a"` for a button colour | the fourth number was dropped | **opacity**, 0 to 255, as for painting |
| button colours in Button Info | — | **Back, Fore, Hilite**, each with a checkbox and a colour well |
| `the hcVersion`, "About HC" | `0.7.7` | `0.7.8` |

---

### Installing: macOS will say the app is damaged. It is not.

**This release is not notarised.** macOS quarantines anything downloaded from
the internet, and Gatekeeper refuses an app it cannot trace to a notarised
Developer ID — with a message that says the wrong thing:

> *"HC is damaged and can't be opened. You should move it to the Trash."*

The app is not damaged. It is unnotarised, which is a different problem.
Copy `HC.app` to `/Applications`, then clear the quarantine flag once:

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

It opens normally after that. An app you build yourself from this repository
is never quarantined.

`Pendu.stack` and `Flipper.stack` still sit next to the app in the DMG.

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. Shaping a polygon with the mouse

You create one the HyperCard way. There is no new tool in the palette:

1. **New Button** in the Objects menu;
2. in **Button Info**, choose the style **polygon**.

The button becomes a **triangle** drawn inside its rectangle.
`set the style of button 1 to polygon` does the same.

With the **Button tool**, the vertices of the selected polygon show as small
round handles:

- **drag a handle** to move that vertex;
- **click on a side** to add a vertex there, then drag it;
- **drag anywhere else** in the button's rectangle to move the whole
  button, as before;
- **Delete** removes the selected vertex (it is drawn filled).

A polygon never goes below a triangle. With three vertices left, or no vertex
selected, Delete beeps and does nothing else. While a polygon's handles are
shown, **Delete never deletes the button**. To remove the whole button,
**Cut** it (⌘X), a deliberate gesture that Paste can undo.

The editor and `set the points` go through the same code. So
`the points` reads back exactly what the mouse made, and a stack can still
shape its polygons by script.

A polygon is now **filled with the even-odd rule**, the same rule that its
click, `within()` and `intersect()` already used. Nothing changes for a
simple shape. A shape whose sides cross shows a hole exactly where a click
does not count.

### 2. Semi-transparent colours

A button colour can carry a fourth number, its **opacity**, from 0 to 255 —
the same `r,g,b,a` that painting already understood:

```
set the backColor of button "glass" to "255,0,0,128"
put the backColor of button "glass"      -- 255,0,0,128
```

Over a patterned card, a half-transparent button lets the pattern show
through. An opaque colour reads back as before, with three numbers.

### 3. Colours in Button Info

The **Button Info** dialog has a new top row: **Back**, **Fore** and
**Hilite**, each with a checkbox and a colour well. Click a well to open
macOS's colour picker. Its **Opacity** slider gives the fourth number.

- Choosing a colour ticks its box.
- **An unticked box sets nothing**, and the button keeps HyperCard's look.
  So opening the Info of an old stack's button and clicking OK adds nothing
  to it.

### 4. Old stacks are not touched — measured

The rule of the extensions still holds: **nothing set, nothing changed**. It
was measured again at the end of this work:

1. **Import.** 591 buttons from nine HyperCard stacks: none gains a colour,
   an opacity or a vertex.
2. **Re-saving.** Four HC stacks (including the pinball), read and written
   again by the 0.7.7 kernel and by this one: **identical, byte for byte**.
3. **A 0.7.8 stack opened by 0.7.7.** We tested a polygon edited with the
   mouse, a background at opacity 100 and a hilite at 50. 0.7.7 opens the
   stack without a fault and keeps the style, the vertices and the three
   colours, drawn opaque. Opacity is written on its own line in the file
   (`backalpha 128`), so an older HC ignores that line instead of losing
   the colour.

### 5. What is not measured is written down as not measured

- Shaping with the mouse and the see-through drawing were tried in HC itself,
  by hand, by its author: an eight-vertex shape made with the mouse, and a
  red triangle at opacity 128 over a brick pattern. They have no automatic
  test. The kernel functions underneath do (`editeurpoly`).
- The extensions still follow LiveCode's documentation; HC has not been
  compared with LiveCode itself.

---

### Test status

- C kernel: **280 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New or extended:
  - `editeurpoly`: the triangle a polygon is born as, moving, inserting and
    removing vertices, never below three;
  - `polygone`: opacity, how it is saved, and damaged opacity lines in a file
    (ignored);
  - `clonepart`: copying a button copies its opacities.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families. **Zero** static-analyser warnings.

### Known, and not fixed here

- `the long id of button 1` gives only the number; HyperCard gives the full
  descriptor.
- Fields take no `backColor` or `foreColor`; check boxes, radio buttons and
  popups ignore the button colours.
- A stack's own palettes (a `PLTE` resource, or an XCMD) are not supported.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - a fault inside a user function stops the function but not its caller;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#101](https://github.com/cleobuline/hc/pull/101): the polygon editor, button
  colour opacity, colours in Button Info, even-odd filling; version 0.7.8 in
  all three places, and this note

**Full changelog:** [HC-0.7.7...HC-0.7.8](https://github.com/cleobuline/hc/compare/HC-0.7.7...HC-0.7.8)
