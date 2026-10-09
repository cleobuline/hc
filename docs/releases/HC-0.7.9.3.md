## HC-0.7.9.3 — Two arcade games, and sound that keeps up

### Installing: macOS will say the app is damaged. It is not.

**This release is not notarised.** macOS quarantines anything downloaded from
the internet. Gatekeeper refuses an app it cannot trace to a notarised
Developer ID, and its message says the wrong thing:

> *"HC is damaged and can't be opened. You should move it to the Trash."*

The app is not damaged. It is not notarised, which is a different problem.
Copy `HC.app` to `/Applications`, then clear the quarantine flag once:

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

It opens normally after that. Do not double-click the app inside the DMG:
copy it out first. An app you build yourself from this repository is never
quarantined.

The binary is universal (`x86_64 arm64`), and the deployment target is
macOS 10.13.

---

**Update from 0.7.9.2.** Two new demonstration stacks, **Space Invaders** and
a **brick breaker**, written in plain HyperTalk with polygon buttons and
`the keysDown`. Playing them found three things in HC worth fixing: `play`
held the script up, so the ball stopped for a moment at every sound; Esc
stopped none of the games; and a polygon button with many points was drawn
cut short.

### At a glance

| | 0.7.9.2 | 0.7.9.3 |
| -- | -- | -- |
| `play "cling"` in a game loop | the script waits while the sound is prepared: the ball stops for a moment | **returns at once**, and the sound starts with the hit |
| Esc in the Tetris demonstration | nothing; only Cmd-. stopped the game | **stops the game**, as its card says |
| A polygon button with more than 256 points | drawn with its first 256 | **drawn whole**, as it is clicked |
| Space Invaders, brick breaker | — | **two new demonstration stacks** |
| `the hcVersion`, "About HC" | `0.7.9.2` | `0.7.9.3` |

---

### 1. Two games in HyperTalk

- **Space Invaders.** Forty invaders as polygon buttons, drawn after Tomohiro
  Nishikado's originals. The formation moves as one block, one
  `set the loc` per invader, and speeds up as it thins out. Four bunkers
  crumble under fire, and a saucer passes now and then. Arrows, or Q and D to
  move and Z or space to fire.
- **Brick breaker.** Sixty bricks in rainbow rows. The paddle and the ball
  are polygon buttons; the ball keeps its position as decimal numbers and is
  rounded only for display. In the middle of the paddle the ball goes
  straight back up; near the edge it leaves at up to sixty degrees. Each
  brick makes it a little faster. Arrows, or Q and D to move and Z or space
  to launch the ball.
- In both, Esc stops the game. Each game lives entirely in its stack script:
  open it to read it, or to change the rules. Both are in the DMG, and there
  is a video of each on [the HC page](https://hc.labynet.fr).

### 2. Sound

- **`play` no longer holds the script up.** Each `play` used to build a new
  sound player on the thread that runs the script. On an older MacBook, a
  frame or two were lost at every sound: the brick breaker's ball stopped
  for a moment on the paddle and on every brick. Commenting out the two
  `play` lines made the stop disappear, and that pointed to the sound.
- HC now keeps an audio output open while sounds are being played, with each
  sound loaded into memory once. `play` hands the sound over and returns at
  once, and the sound starts with the hit. All of the audio work happens on
  a thread of its own.
- Sounds still overlap, as before: up to eight at a time for sounds of the
  same format; beyond that, the oldest one is cut. A sound longer than thirty
  seconds is read from its file instead of memory.
- After twenty seconds without any sound, the audio output is released, and
  the next `play` opens it again. Only that first sound may start a little
  late; a game that makes a sound every few seconds never meets this.

### 3. Fixes

- **Esc in the demonstration games.** The Tetris loop waited for key code 27,
  the character code of Esc, while `the keysDown` gives the code of the key,
  65307, as LiveCode does. Esc stopped nothing; only Cmd-. did. The Tetris in
  this release's DMG is fixed, and the two new games use the right code. Each
  game's test now runs the real game loop and holds Esc.
- **Polygon buttons with more than 256 points.** Two limits had the same
  name: 256 points for the paint tools' shapes, and 1000 for polygon buttons.
  Where both were visible, the smaller one won, so a polygon button was drawn
  with at most 256 points, while the click, `within()` and `intersect()` used
  all of them. Found from the "macro redefined" warning that Xcode showed on
  every build; the warning is gone.

---

### Test status

- C kernel: **290 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `invaders` and
  `briques`, each with a robot that plays thousands of frames and checks at
  every frame that the screen, the state and the score agree; and in the
  three games' tests, Esc held in the real game loop.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa build
  on macOS: **no warnings** from HC's code.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.

### Not measured yet

- What HyperCard does with `play` while the same sound is still playing.
  HC lets the two overlap.
- Plugging in headphones during a game. According to Apple's documentation
  the audio engine stops and starts again with the next sound; this was not
  tried.
- Whether an open audio output keeps a Mac from going to sleep. It is
  released after twenty seconds of silence in case it does.
- A real polygon button with more than 256 points, drawn on screen.
- Still from 0.7.9.2: `go card "Nonexistent"`; the exact format of
  `the params` and of `the destination`; `the long version`; writing into or
  deleting `char 1 to 0`; whether Shift-Cmd-Option also outlines the buttons,
  and the order of the Handlers and Functions menus; whether HyperCard tells
  Cmd-B from Cmd-Shift-B in `commandKeyDown "B"`.

### Known, and not fixed here

- `play stop` does not stop a sound: HC looks for a sound named "stop" and
  beeps.
- Seen once while testing, and not explained yet: a game that seemed frozen
  when started again. It has not come back since the sound changes. If you
  see it, please say how the previous game had ended.
- HC's own dialogs and messages are in French.
- The other keyboard messages written as commands (`arrowKey "left"`,
  `tabKey`, `returnKey`…) reach a handler, but have no default action yet.
- `flash`, `the selectedLoc`, `select button "x"` (selecting an object),
  `edit script of …`, and the Message Watcher are not supported.
- Apple's help stacks cut paths at `:`, as on a classic Mac. HC uses `/`
  paths, so two HyperTalk Reference demonstrations print the wrong disk name,
  and HyperCard Help removes its menu when you leave its window.
- A stack's own palettes (a `PLTE` resource, or an XCMD such as the one in
  Stack Templates) are not supported.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#117](https://github.com/cleobuline/hc/pull/117): Space Invaders and
  the brick breaker, Esc in the games, `play` that keeps up, polygon buttons
  drawn whole, version 0.7.9.3 and this note

**Full changelog:** [HC-0.7.9.2...HC-0.7.9.3](https://github.com/cleobuline/hc/compare/HC-0.7.9.2...HC-0.7.9.3)
