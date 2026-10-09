## HC-0.7.9.2 — Apple's HyperTalk Reference, demo by demo

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

**Update from 0.7.9.1.** This is a fidelity release. Apple's **HyperTalk
Reference** stack shows every word of the language with a **Run the Script**
button. All 136 of those demonstrations were played, one by one, and every
place where HC refused something the stack itself documents is now fixed.
Where HC and HyperCard could differ, the answer was measured in HyperCard
2.4.1 before anything was changed.

The release also answers the first feedback from users: the script editor
gets HyperCard's shortcuts and its **Handlers** and **Functions** menus, and
the **Verify** button no longer flags ordinary handlers.

### At a glance

| | 0.7.9.1 | 0.7.9.2 |
| -- | -- | -- |
| Cmd-Option-click a button, Shift-Cmd-Option-click a field | nothing | **opens its script** |
| Holding Cmd-Option / Shift-Cmd-Option | nothing | **outlines the buttons / the fields** |
| Script window | text only | **Handlers and Functions menus** |
| Verify on `on subroutine param` | a remark, and the line framed | **"no error"** |
| `char 1 to 0 of "abc"` | `a` | **empty**, as in HyperCard |
| `set lockMessages to true` | unknown property | **works** |
| `the number of this bkgnd` | unknown property | **works** |
| `the name of the selectedField`, `of the foundField` | "object not found" | **work** |
| `doMenu "Background","Edit"`, `doMenu … without dialog` | "can't do that" | **work** |
| `show card picture`, `hide picture of bkgnd 1` | "object not found" | **work** |
| `commandKeyDown "V"` in a script | "can't do that" | **plays Cmd-V** |
| Cmd-I | nothing | **Icon…**, as in HyperCard |
| `the hcVersion`, "About HC" | `0.7.9.1` | `0.7.9.2` |

---

### 1. The script editor

- **Cmd-Option-click** a button opens its script, with any tool, and the
  button receives no click. **Shift-Cmd-Option-click** does the same for a
  field. While the keys are held, the buttons (or the fields) are outlined,
  so you can see what you are about to open. This is HyperCard's shortcut,
  checked in HyperCard 2.4.1.
- **Handlers** and **Functions**: two menus at the top of the script window
  list the script's `on` and `function` handlers, in script order. Choose one
  to jump to its line. The lists are rebuilt from the text you are editing,
  so a handler you have just typed is already there.
- **Verify** used to remark "is not a known system message" on every `on`
  handler with a name of its own, and to frame that line. It now speaks only
  when the name looks like a mistyped system message (`on mouseDwon`: "did
  you mean mouseDown?"), and not even then if the script calls that handler
  itself.

### 2. Text chunks

- **`char 1 to 0`** is empty, and so is `char 1 to -1`. HC returned the first
  character. Measured in HyperCard: the character range is empty, while
  `word 1 to 0 of "a b c"` is `a`, so words keep their reading. Apple's own
  help stacks count on this: their `textToLineNum` function returned 1
  instead of 0 when the text was not found.
- `select char 1 to 0 of field 1` puts the insertion point at the start of
  the field.

### 3. Commands and properties from the HyperTalk Reference

- **`set [the] lockMessages to true`** is the same lock as `lock messages`,
  and is released when the handler ends. Only the command worked before.
- **`the number of this bkgnd`**, `the number of bkgnd 2`: a background's
  rank, in the same order that `bkgnd 2` uses.
- **`the name of the selectedField`**, `the name of the foundField`, and any
  other property of them. `the textFont of the selectedField` used to return
  the text "textFont of the selectedField", with no error.
- **`doMenu "Background", "Edit"`** (an item, then its menu) and
  **`doMenu "…" without dialog`** are accepted.
- **`show card picture`**, `hide background picture`, `show picture of card
  3`, `hide pict of first cd`: the four forms on Apple's `show` card. They do
  what `set the showPict` does.
- **`commandKeyDown "V"`** written in a script sends the message, and when
  no handler takes it (or a handler passes it), plays the Cmd-key shortcut
  of the menu bar. That is what Apple's card says: "it acts exactly as if
  you had pressed Command at the same time as the specified character".
- **Cmd-I** opens **Icon…**. HC had no shortcut there; HyperCard does, and
  Apple's `commandKeyDown "I"` demonstration relies on it.
- `select it`, when `it` is empty (for example after a click outside the
  text, then `get the clickChunk`), now says that there is nothing to select.
  It used to answer "can't do that", which pointed at the syntax.

### 4. Since 0.7.9.1, already on the main branch

- **Copy, paste, duplicate under memory shortage** are all or nothing. A
  harness refuses each memory allocation of nine operations in turn: before,
  13 of 189 cases gave a truncated copy or stopped the program; now all 194
  refusals end cleanly, also under the sanitizers.
- **Paste** pastes what was copied **last** (an object, a card or a
  picture). A picture that was only moved no longer comes back on Paste. A
  copied button pastes whatever tool is chosen, and the Button tool is then
  selected, as measured in HyperCard.
- **Tetris**, a demonstration stack for polygon buttons and `the keysDown`,
  with the arrow keys or Z Q D X. It is on [the HC page](https://hc.labynet.fr).
- **Two sounds for the pinball demo**, `cling` and `tik`, bundled with the
  app: `play "cling"`.
- **"About HC"** has a link to support HC, and the repository has a Sponsor
  button.

---

### Test status

- C kernel: **288 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `refdemos` (the
  HyperTalk Reference demonstrations) and `listegest` (the Handlers and
  Functions menus); `clavier`, `lockmsg`, `rang` and `verifnoms` extended.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.

### Not measured in HyperCard yet

- `go card "Nonexistent"`: HC stops the script, and HyperCard may continue
  with "No such card". Apple's `result` demonstration is the test.
- The exact format of `the params`, and of `the destination`.
- `the long version`.
- Writing into or deleting `char 1 to 0` (HC leaves the text unchanged).
- Whether Shift-Cmd-Option also outlines the buttons; the order of the
  Handlers and Functions menus.
- Whether HyperCard tells Cmd-B from Cmd-Shift-B in `commandKeyDown "B"`
  (HC plays Cmd-B).

### Known, and not fixed here

- HC's own dialogs and messages are in French.
- The other keyboard messages written as commands (`arrowKey "left"`,
  `tabKey`, `returnKey`…) reach a handler, but have no default action yet:
  it waits for a measurement in HyperCard.
- `flash`, `the selectedLoc`, `select button "x"` (selecting an object),
  `edit script of …`, and the Message Watcher are not supported.
- Apple's help stacks cut paths at `:`, as on a classic Mac. HC uses `/`
  paths, so two Reference demonstrations print the wrong disk name, and
  HyperCard Help removes its menu when you leave its window (it comes back
  when you return).
- A stack's own palettes (a `PLTE` resource, or an XCMD such as the one in
  Stack Templates) are not supported.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#109](https://github.com/cleobuline/hc/pull/109): copy and paste, all or
  nothing under memory shortage, and a faithful Paste
- [#110](https://github.com/cleobuline/hc/pull/110): a link to support HC
- [#112](https://github.com/cleobuline/hc/pull/112): the Tetris demonstration
  stack
- [#114](https://github.com/cleobuline/hc/pull/114): the `cling` and `tik`
  sounds
- [#115](https://github.com/cleobuline/hc/pull/115): repository clean-up
- [#116](https://github.com/cleobuline/hc/pull/116): the HyperTalk
  Reference fixes, the script editor, version 0.7.9.2 and this note

**Full changelog:** [HC-0.7.9.1...HC-0.7.9.2](https://github.com/cleobuline/hc/compare/HC-0.7.9.1...HC-0.7.9.2)
