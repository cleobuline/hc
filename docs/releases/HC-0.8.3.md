## HC-0.8.3 — `flash`, and a notarised DMG

### Installing

**This release is signed and notarised.** HC is signed with the Developer ID
of the Association Labynet, which distributes it, and Apple's notary service
has checked it. Open the DMG, drag `HC.app` into Applications, and open it:
macOS opens it like any other application. The `xattr` command that earlier
releases asked for is no longer needed.

The binary is universal (`x86_64 arm64`), and the deployment target is
macOS 10.13. The latest DMG is always at
<https://github.com/cleobuline/hc/releases/latest/download/HC.dmg>.

---

**Update from 0.8.2.** One more HyperTalk command, `flash`, and the first
HC that macOS opens without a warning.

### At a glance

| | 0.8.2 | 0.8.3 |
| -- | -- | -- |
| `flash 3` | "unknown command", and the script stopped | **the card flashes three times** |
| Opening the downloaded HC.app | "HC is damaged", until `xattr` cleared the quarantine | **opens**: signed and notarised |
| `the hcVersion`, "About HC" | `0.8.2` | `0.8.3` |

---

### 1. `flash`

```
flash 3
```

The card flashes: black turns to white and back, as many times as asked.
`flash` alone flashes once. It is HyperTalk's visual signal, the cousin of
`beep`, for a wrong answer in a quiz or a hit in a game. HC answered
"unknown command".

- Cmd-. stops a long `flash`, and the card is always turned back the right
  way before the script goes on.
- `flash` works on top of everything, a locked screen included.

### 2. Signed and notarised

Until now, macOS said that HC "is damaged and can't be opened": it was
neither signed with a Developer ID nor notarised, and Gatekeeper refuses what
it cannot trace. From this release on:

- **HC is signed** with the Developer ID of the Association Labynet, with
  the Hardened Runtime;
- **Apple's notary service has checked it**: an automated scan for known
  malicious code and for a valid signature. It is not an App Store review,
  and Apple does not distribute HC: it stays on GitHub and on labynet.fr;
- **the ticket is stapled** to the application and to the DMG, so HC opens
  even on a Mac that is offline.

### 3. For those who build HC themselves

`docs/livraison.md` describes every step: signing, notarising, stapling,
building the DMG, checking it as a user would. Two scripts come with it:

- `outils/notarise.py` notarises a file through Apple's notary web API, the
  one `notarytool` calls. It needs nothing but Python, `openssl` and `curl`,
  so it works on a Mac that cannot run Xcode 13 — HC's own releases are
  made on macOS Catalina with Xcode 12.4, where `notarytool` does not exist
  and Apple no longer accepts the older `altool`.
- `outils/livrer.sh` does the whole release from a folder that holds HC.app
  and the stacks: it signs and notarises the application, builds the DMG,
  signs and notarises it, and checks both with `spctl`. Run again after an
  interruption, it does not sign or send again what is already done.

---

### Test status

- C kernel: **310 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. One is new, for `flash`. The
  flashing itself lives in the Cocoa interface, which these checks do not
  reach: it was tried in the application.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: FUZZ_RESULT.

### Not measured yet

- `flash`, in HyperCard: its speed (HC holds each state for 4 ticks, so
  `flash 3` takes 20 ticks); what `flash 0`, a negative or decimal number,
  or `flash "x"` does (HC: nothing, nothing, the whole part, an error); and
  `flash` under `lock screen` (HC flashes anyway). That `flash` alone flashes
  once was given by the author, not yet seen in HyperCard.
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
  `create stack`, `the long version`, `the selectedLoc`,
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
  - `CFBundleVersion` is still hard-coded to `1`.

### Changes in this release

- PR_083: `flash`, the signing and notarisation tools, version 0.8.3 and
  this note

**Full changelog:** [HC-0.8.2...HC-0.8.3](https://github.com/cleobuline/hc/compare/HC-0.8.2...HC-0.8.3)
