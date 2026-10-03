## HC-0.7.9 — a fix that matters, a truer `the id`, and a pinball you build yourself

**Update from 0.7.8.** 0.7.7 and 0.7.8 had a bug that broke scripts which
loop while the mouse is held down. It is fixed here, along with a few
HyperCard fidelity fixes measured against the real thing. The DMG also ships
a new pinball that you can rebuild with copy and paste.

### At a glance

| | 0.7.8 | 0.7.9 |
| -- | -- | -- |
| `repeat while the mouse is down`, dragging the mouse | **broke**: the loop stopped following the mouse | **fixed** |
| `the id of this card` | `2865` | **`card id 2865`**, as in HyperCard |
| `the long id of this card` | `2865` | **`card id 2865 of stack "…"`** |
| `put the id of this card into x` … `go x` | "can't do that" | **goes back to that card** |
| `the long id of this stack` | `1` | **refused**, as in HyperCard (a stack has no id) |
| button colours on check boxes, radio buttons, popups | ignored | **applied** |
| "About HC" | the bare system panel | **what HC is, and a link to labynet.net** |
| `Flipper.stack` in the DMG | one script runs the whole table | **every piece carries its own script** |
| `the hcVersion`, "About HC" | `0.7.8` | `0.7.9` |

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

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13,
and **0.7.8 was measured on macOS 10.13 on a Core 2 Duo MacBook**: polygons,
colours and the pinball all draw correctly. It is slow on that machine, but
everything works.

---

### 1. The fix: dragging the mouse during a script

Many old stacks track the mouse in a script loop, for menus, sliders and
drag-and-drop:

```
repeat while the mouse is down
  get clickLine(me)
  select line it of me
end repeat
```

In 0.7.7 and 0.7.8 this stopped following the mouse: the selection stayed
where the click began. The cause was in the code behind `the keysDown`. While
a script runs, HC keeps checking incoming events for Cmd-period and
`the mouseClick`. Since 0.7.7, that check also asked every event for its key
code, including mouse drags. macOS answers that question with an exception
when the event is a mouse event, and the exception broke the loop.

HC now reads a key code only from keyboard events. A second place could fail
the same way, on a dead key (an accent being typed). It was found by reading
the code, not seen happening, and it is protected too.

This was found and confirmed in HC itself by its author, with the Xcode
console showing the exception. If one of your stacks "stopped tracking the
mouse" after 0.7.7, this is why.

### 2. `the id`, measured in HyperCard

Every form of `the id` was measured in HyperCard 2 under Basilisk II, and HC
now gives the same answers:

| | HyperCard | HC 0.7.9 |
| -- | -- | -- |
| `the id of me` (a button), any adjective | `3` | the number |
| `the id of this card` | `card id 2865` | `card id 2865` |
| `the short id of this card` | `2865` | `2865` |
| `the long id of this card` | `card id 2865 of stack "…"` | the same, with the macOS path |
| `the id of this bkgnd`, any adjective | `2703` | the number |
| `the long id of this stack` | refused | refused |

So the common idiom works again:

```
put the id of this card into back
…
go back
```

`card id` itself still expects a number: write `card id (the short id of …)`,
as in HyperCard.

**A correction to earlier notes.** The 0.7.7 and 0.7.8 notes said that
HyperCard gives a full descriptor for `the long id of button 1`. That was
never measured, and it is wrong: HyperCard gives the number, as HC always
did. The real difference was on cards, and it is fixed above.

### 3. Button colours on every button style

`backColor`, `foreColor` and `hiliteColor`, with their opacity, now apply to
check boxes, radio buttons and popups too:

- **check box, radio button**: `backColor` fills the box, `foreColor` draws
  the frame and the title, and `hiliteColor` colours the **mark** (the cross
  or the dot), because the mark is what lights up on them;
- **popup**: `backColor` fills the box; `foreColor` draws the shadow, the
  frame, the arrow and the text. A popup never lights up, so `hiliteColor`
  has nothing to colour.

With no colour set, they look exactly as before.

### 4. "About HC"

The About panel now says what HC is, in English and French, and links to
[labynet.net](https://labynet.net).

### 5. A pinball you build yourself

**`Flipper.stack`**, in the DMG, has been rewritten in the style of a 1990
HyperCard pinball by HC's author.

- **The ball knows nothing about the table.** At each small step it uses
  `click at` just ahead of itself. HC finds the object under the click,
  exactly as for a mouse click, and sends it `mouseUp`. A polygon only
  answers inside its own shape. The ball is a *disabled* button, so its own
  clicks pass through it.
- **Every piece carries its own behaviour**, in three lines:

  ```
  on mouseUp
    bord 1, 3, 10      -- a bumper: bounce, kick, points
  end mouseUp
  ```

  A mushroom says `rond 4, 100`; a grey guide says `bord 0.5, 0, 0`.
- **A row of drop targets** across the top: hit one and it drops. When all
  six are down you get a 5,000-point bonus, and they stand up again.
- **The flippers aim.** The shot follows where the ball sits on the flipper.
  Near the pivot it goes up your own side, near the tip it crosses the
  table, and it is stronger near the tip.

**It is a construction set.** With the Button tool, copy a mushroom, a bumper
or a target, paste it anywhere, shape it with the polygon editor, and play.
The engine in the stack script does not change.

Measured in the HC engine with automatic players over 24 games: no ball
through a flipper, no stuck ball, no script error, about 1 ms of computing
per frame. Played in HC by its author, who scored **42,490**. Can you beat it?

---

### Test status

- C kernel: **281 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `idcarte`, which
  replays the HyperCard measurements of `the id` line by line.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families. **Zero** static-analyser warnings.

### Known, and not fixed here

- Fields take no `backColor` or `foreColor`.
- A fault inside a user function stops the function but not its caller.
- A script error inside a handler reached by `send` does not stop the script
  that sent it.
- The path in `the long id` and `the long name` of a stack is a macOS path
  (`/Volumes/…`), not the colon path of the old Mac OS.
- A stack's own palettes (a `PLTE` resource, or an XCMD) are not supported.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#102](https://github.com/cleobuline/hc/pull/102): the drag fix, `the id`
  of cards and stacks, colours on check boxes, radio buttons and popups
- [#103](https://github.com/cleobuline/hc/pull/103): the pinball video in the
  READMEs, and the new "About HC"
- [#104](https://github.com/cleobuline/hc/pull/104): version 0.7.9 in all three
  places, and this note

**Full changelog:** [HC-0.7.8...HC-0.7.9](https://github.com/cleobuline/hc/compare/HC-0.7.8...HC-0.7.9)
