## HC-0.8.2 — `play` plays its notes

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
macOS 10.13. The latest DMG is always at
<https://github.com/cleobuline/hc/releases/latest/download/HC.dmg>.

---

**Update from 0.8.1.** `play` now plays the notes that follow the sound
name, as HyperCard did: a tempo, pitches, durations, rests. Apple's own
stacks play tunes this way, and HC played the sound once, without its
notes.
`play stop` now stops a sound, and `the sound` names the one that is
playing.

### At a glance

| | 0.8.1 | 0.8.2 |
| -- | -- | -- |
| `play "boing" tempo 120 "c4q e g c5h"` | one boing | **a tune**: four notes at their pitch and length |
| `play harpsichord tempo 300 a#2q c3w` (notes written as bare words) | the sound once | **the notes** |
| `play stop` | looked for a sound named "stop", and beeped | **stops the sound** (measured in HyperCard 2.4.1) |
| `play "stop"`, in quotes | the same | plays a sound named "stop", **as in HyperCard** |
| `the sound` while a sound plays | always `done` | **its name**, then `done` |
| `wait until the sound is "done"` | did not wait | **waits for the end** of the sound or the tune |
| `the hcVersion`, "About HC" | `0.8.1` | `0.8.2` |

---

### 1. Tunes

```
on mouseUp
  put "c4q c4 c4 d4 e4h d4 c4q e4 d4 d4 c4w" into couplet
  put "d4q d4 d4 d4 a3h a3 d4q c4 b3 a3 g3w" into milieu
  put couplet && couplet && milieu && couplet into air
  play "piano" tempo 140 air
end mouseUp
```

That is *Au clair de la lune*, 44 notes, played in HC on a piano sound.

- **A note** is a name (`a` to `g`, or `r` for a rest), an optional sharp
  `#` or flat `b`, an optional octave (`c4` is middle C), an optional
  length — `w` whole, `h` half, `q` quarter, `e` eighth, `s` sixteenth,
  `t` thirty-second, `x` sixty-fourth — and optional `.` (dotted, ×1.5) or
  `3` (triplet, ×2/3). A number alone is a MIDI note: `60` is middle C.
- **The notes** can be a string, a variable of any length, or bare words
  on the line, as in Apple's introductory stack (*Découvrir HyperCard*, in
  its French edition).
- **Measured in HyperCard 2.4.1**, by timing seven tunes with
  `wait until the sound is "done"`: without `tempo`, the tempo is 120
  quarter notes a minute; a note keeps the length of the one before it
  unless it gives its own; a rest takes the current length; `the sound`
  lasts until the last note. HC's timings match HyperCard's to within the
  2 to 4 ticks HyperCard takes to start.
- **How it sounds.** HyperCard played each note by replaying its sound
  faster or slower: an octave up is twice as fast. HC does the same. **The
  sound is taken to be a middle C**: a sound recorded on another note plays
  every tune transposed by as much.
- `play stop` stops a tune, and `the sound` names it until its last note.

**Instruments.** HC ships no harpsichord: HyperCard's belongs to Apple, and
`play "harpsichord"` still beeps unless you have one. Any sound HC can find
plays notes: its own (`boing`, `cling`, `coq`…), macOS's system sounds
(`Glass`, `Ping`, `Tink`…), or a `.wav` or `.aiff` file you put in
`~/Library/Sounds`. Name it `harpsichord.wav`, and Apple's stacks play their
tunes with it.

### 2. `play stop` and `the sound`

- **`play stop`** stops every sound that is playing, and the ones still
  waiting their turn. HC looked for a sound named "stop", and beeped.
  Measured in HyperCard 2.4.1: the bare word stops the sound, and
  `play "stop"`, in quotes, does not — HC now does the same.
- **`the sound`** returns the name of the sound that is playing, and `done`
  once it has finished or been stopped. It always returned `done`, so
  `wait until the sound is "done"` did not wait at all. It now waits: 141
  ticks for a sound of 136.

---

### Test status

- C kernel: **309 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. Two are new: one for
  `play stop`, one for the notes — HyperCard's seven timings, the pitches,
  and the tune itself, rendered from a synthetic sample and checked by
  counting its zero crossings. The sound output lives in the Cocoa
  interface, which these checks do not reach: it was tried in the
  application.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: the canary check and the campaign of 80,000 damaged
  stacks are still running as this note is written.

### Not measured yet

- Notes, in HyperCard: the octave of a note written without one (HC: 4),
  and whether it carries over to the next note (HC: yes); MIDI numbers (HC
  accepts them); a note longer than its sound (HC falls silent when the
  sound ends); a word that is not a whole note, such as `flute` (HC skips
  it); `.` or `3` without a length letter; two tunes started one after the
  other (HC plays them together).
- What `the sound` returns just after `play stop` (HC: `done`).
- In HyperCard: whether `lockRecent` returns to false at idle (HC leaves it
  set); what `doMenu "Bold"` does on a selection that is already bold, on a
  mixed selection, and with no selection. `doMenu "Other…"` is not served.
- The Style menu outside a field being edited (an object selected with the
  Button or Field tool, the paint Text tool): HC greys it out.
- Still from 0.8: a `pass` that reaches a stack put in use during the
  handler; what HyperCard does with a stack in use when you close its
  window; when the dialog of `open printing with dialog` appears, and
  whether Cancel sets `the result`; `print 5 cards` near the last card;
  whether `go forth` forgets its steps after a detour; the default of
  `dialingVolume` and the answer of `the stackSpace`.
- Still from 0.7.9.3: `go card "Nonexistent"`; the exact format of
  `the params` and of `the destination`; writing into or deleting
  `char 1 to 0`; whether Shift-Cmd-Option also outlines the buttons, and the
  order of the Handlers and Functions menus; whether HyperCard tells Cmd-B
  from Cmd-Shift-B in `commandKeyDown "B"`.

### Known, and not fixed here

- A sound longer than 30 seconds is read from its file, and plays once
  without its notes. A tune longer than two minutes is cut there.
- **"Print Topic"** in the Reference menu prints every page with the same
  picture: the help script locks the screen before printing, and HC prints
  the frozen screen. Print page 1 only in the print dialog. On the four
  contents cards of HyperTalk Reference, "Print Topic" stops with an error:
  their "Pages" field is empty.
- A button's size is still drawn by its style (11 to 16 points), whatever
  `the textSize` answers when none is set.
- Apple's help stacks cut paths at `:`, as on a classic Mac, and HC uses `/`
  paths. Leaving HyperTalk Reference for another stack therefore removes the
  Reference menu without leaving HyperCard Help in use; closing HyperTalk
  Reference from behind, or quitting HC while it is behind, can still show a
  "menu not found" dialog. Two HyperTalk Reference demonstrations print the
  wrong disk name.
- Not supported yet: `ask password`, `open report printing`,
  `the abbreviated time`, `the param of 1`, `show all cards`,
  `create stack`, `flash`, `the long version`, `the selectedLoc`,
  `select button "x"` (selecting an object), `edit script of …`, and the
  Message Watcher.
- Seen once while testing 0.7.9.3, and not explained yet: a game that seemed
  frozen when started again. It has not come back. If you see it, please say
  how the previous game had ended.
- HC's own dialogs and messages are in French.
- The other keyboard messages written as commands (`arrowKey "left"`,
  `tabKey`, `returnKey`…) reach a handler, but have no default action yet.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - a stack's own palettes (`PLTE`);
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#129](https://github.com/cleobuline/hc/pull/129): `play` plays its
  notes, `play stop`, `the sound`
- [#130](https://github.com/cleobuline/hc/pull/130): version 0.8.2 and this
  note

**Full changelog:** [HC-0.8.1...HC-0.8.2](https://github.com/cleobuline/hc/compare/HC-0.8.1...HC-0.8.2)
