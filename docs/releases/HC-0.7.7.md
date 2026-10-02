## HC-0.7.7 — the Navigator palette, polygon buttons, and a pinball

HyperCard's **Navigator palette** is here, drawn point for point from
HyperCard itself, and every one of its eleven buttons works.

And HC gets its **first extensions** beyond HyperCard, made for games:
**polygon buttons with colours**, and **`the keysDown`**. They were built
for a pinball, and the pinball found three bugs in HC on the way, all fixed
here — one of them was already in 0.7.6.

### At a glance

| | 0.7.6 | 0.7.7 |
| -- | -- | -- |
| `palette navigator` | "can't do that" | **the Navigator palette**, as in HyperCard |
| `doMenu "Home"`, `"Message"`, `"Recent"`, `"Next window"` | did nothing, silently | **served** |
| `the visible of window "X"`, `close window "X"` | a syntax error | **understood**; a window HC does not have says so |
| polygon buttons, button colours | — | **new**, see "Extensions" |
| `the keysDown` | — | **new**, see "Extensions" |
| a stack function called many thousands of times in one loop | "buffer arena saturated" | **fixed** |
| `set the hilite of button (item k of names) to true` | "object not found" | **fixed** |
| `the hcVersion`, "About HC" | `0.7.6` | `0.7.7` |

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

`Pendu.stack` still sits next to the app in the DMG, and now **`Flipper.stack`**
too — see "A pinball".

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. The Navigator palette

```
palette navigator
```

opens HyperCard's Navigator: a small windoid with a close box and eleven
buttons. Its picture was taken **point for point** from a screenshot of
HyperCard 2 running under Basilisk II — two tones, 98 × 78 points, nothing
redrawn.

Each button runs its line of `the commands of window "Navigator"`, exactly as
measured in HyperCard: Back, Home, Help, Recent, First, Prev, Next, Last,
Find…, Message, Next window. A button inverts while pressed and acts only if
released inside. Drag the palette by its title bar; its close box sends
`closePalette`, and opening it sends `openPalette`, with the palette's name and
window id, as HyperCard does.

Scripts can drive it:

```
set the hilitedButton of window "Navigator" to 2
put the loc of window "Navigator"
hide window "Navigator"
close window "Navigator"
if there is a window "Navigator" then …
```

Served properties: `name`, `id`, `commands` (read and set), `buttonCount`,
`hilitedButton` (read and set), `visible`, `loc`, `topLeft`, `rect`.

Two buttons differ from HyperCard, knowingly:

- **Recent** opens HC's menu of recent cards, by name. HyperCard shows a box
  of card miniatures; reproducing it was judged too much work for what it
  adds. The menu leads to the same cards.
- **Help** does nothing: HC has no help stack.

### 2. `doMenu`: four more items

`doMenu "Home"`, `"Message"` (shows or hides the message box), `"Recent"` and
`"Next window"` (brings the next open stack to the front) were silently
ignored. Old stacks write them in their scripts, palette or not; they now work.

### 3. Named windows

`window "Name"` is understood everywhere a script can use it. The only named
window HC has is the Navigator; any other is reported: *"No such window"*.

This changes one real stack for the better. Apple's **Stack Templates** asks,
on every background it opens, `get visible of window "StackTemplatePal"` — its
own palette, made by an external command (XCMD) whose 68000 code cannot run
here. The line used to fail as a syntax error; it now runs, and fails only at
run time, because the window does not exist — which is the truth.

### 4. Extensions — for games

HC stays a HyperCard clone. These are its only two announced extensions, and
they follow one rule: **new words only where HyperCard had none**. No existing
word changes its meaning, and no HyperCard stack behaves differently.
The vocabulary is LiveCode's, so nothing is invented.

#### Polygon buttons, and button colours

```
set the style of button "wing" to polygon
set the points of button "wing" to "100,250" & return & "260,230" & return & "250,262"
set the backColor of button "wing" to "red"        -- the inside
set the foreColor of button "wing" to "black"      -- the outline and the title
set the hiliteColor of button "wing" to "yellow"   -- the inside, when hilited
put within(button "wing", the mouseLoc)
put intersect(button "ball", button "wing")
```

- **`the points`**: one `x,y` vertex per line, in card coordinates. Moving the
  button moves its shape; resizing it stretches it.
- A polygon button is **clicked only inside its shape** (Browse tool). With
  the Button tool its whole rectangle can be grabbed, so a thin shape can
  still be moved.
- **Colours work on every button.** They speak the language of `textColor`: a
  name (English or French), `#RRGGBB`, or `r,g,b`. Reading one gives `r,g,b`,
  or empty when none is set. Setting it to empty restores HyperCard's look.
- **`within(button, point)`** and **`intersect(button, button)`** see the real
  shape: a polygon, an ellipse for an oval button, a rectangle otherwise.

**Compatibility, measured.** 591 buttons imported from nine HyperCard stacks:
none carries a colour or a vertex. Three HC stacks re-saved by the kernel just
before the extensions and by this one: identical, byte for byte. `the backColor` without "of" is still the
paint colour.

#### `the keysDown`

The keys held down right now, one item per key — a character by its code
(`w` is 119, as `charToNum("w")`), arrows by their LiveCode codes (65361 to
65364). While a script loops, HyperCard delivers no `keyDown` until it ends;
`the keysDown` lets a game read the keyboard as it runs:

```
function held c
  return ("," & the keysDown & ",") contains ("," & c & ",")
end held

if held(charToNum("w")) then …
```

LiveCode writes `is among the items of` with it; that is not HyperTalk and HC
does not add it. The commas around `contains` keep `11` from being found in
`119`.

### 5. A pinball

**`Flipper.stack`**, in the DMG, is a small pinball written in plain
HyperTalk with the two extensions: two flippers on **W** and **N**, two
triangular bumpers, three mushrooms, three balls. The physics — gravity,
bounces reflected on each surface's normal, flippers that hit harder near
their tip — is all in the card script, commented, for anyone to change.

### 6. What the pinball found in HC

- **A leak in the buffer arena, already in 0.7.6.** Every call of a stack
  function left a buffer behind until the end of the whole handler. A loop
  calling a function some 16,000 to 20,000 times in one handler stopped with
  *"buffer arena saturated"* — about two minutes of a game at 30 frames per
  second. Fixed; any stack that loops long on its own functions benefits.
- **`set … of button (expression)`.** An `of` *inside* the parentheses —
  `button (item k of names)` — was taken for the reference's own `of`, and the
  object was not found. Reading worked; setting did not. Fixed, and the same
  for `to`.
- **Slowness and memory during long games** (in the app, from the new
  `keysDown`): fixed. Played in HC by its author afterwards: *"it's fluid"*.

### 7. What is not measured is written down as not measured

- The Navigator's `hilitedButton`, `loc` and `rect` have not been compared
  with HyperCard. HC answers 0, and for `loc` the top-left corner of the
  palette's content, under its title bar. The same convention places the
  palette for `palette name, point`.
- Whether a Navigator button inverts while pressed in HyperCard: not
  measured; HC does it as every Mac button does.
- The window id is macOS's window number.
- The extensions follow LiveCode's documentation; HC has not been compared
  with LiveCode itself.

---

### Test status

- C kernel: **279 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New ones include:
  - `polygone`: shapes, colours, `within`, `intersect`, copy and paste, save
    and reload, damaged files;
  - `fenetres` and `menunav`: named windows, and the Navigator's eleven
    `doMenu`;
  - `keysdown`, `parenref`, `arenefonction`: the three things the pinball
    found.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families. **Zero** static-analyser warnings.

### Known, and not fixed here

- `the long id of button 1` gives only the number; HyperCard gives the full
  descriptor.
- Fields take no `backColor` or `foreColor` yet; check boxes, radio buttons
  and popups ignore the button colours.
- A stack's own palettes (a `PLTE` resource, or an XCMD like
  `StackTemplatePal`) are not supported.
- Still standing from 0.7.6:
  - colour icons, pictures and sounds from a resource fork;
  - a fault inside a user function stops the function but not its caller;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#100](https://github.com/cleobuline/hc/pull/100): the Navigator palette and
  named windows; polygon buttons, button colours and `the keysDown`; the three
  fixes the pinball found; version 0.7.7 in all three places, and this note

**Full changelog:** [HC-0.7.6...HC-0.7.7](https://github.com/cleobuline/hc/compare/HC-0.7.6...HC-0.7.7)
