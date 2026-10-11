## HC-0.8 — Apple's help stacks, end to end

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

**Update from 0.7.9.3.** This release is fidelity work only, with no new
extension. Apple's three help stacks — HyperCard Help, HyperTalk Reference
and Help Extras — now work end to end when you open them in HC: the
Reference menu, the glossary, the underlined words. Then HyperTalk
Reference was read card by card against HC, and the commands it describes
that stopped a script in HC are now supported. And HC now has HyperCard's
**Style menu**, which is how you make links of your own.

### At a glance

| | 0.7.9.3 | 0.8 |
| -- | -- | -- |
| `play s`, where `s` holds a sound name | looks for a sound named "s" | **plays the sound named in `s`**, as HyperCard 2.4.1 does |
| Clicking an underlined word in Help Extras | nothing happens | **opens its glossary entry** |
| The Reference menu of Apple's help stacks | "menu not found" dialogs; the chosen stack stays behind | **works**, and brings the chosen stack to the front |
| `put menuItem 4 of menu "X"`, `put menu "X"` | "object not found" | the item's text; the menu's items, each followed by a return (**measured** in HyperCard 2.4.1) |
| `mark cards where …`, `open printing`, `go forth`, `lock recent`, `set the printMargins …` and others | stopped the script | **supported** |
| Grouping words to make a link in a field | only by script | **the Style menu**, as in HyperCard 2.4.1 |
| `show groups` on a card already drawn | the grey underline did not appear | **appears at once** |
| `the hcVersion`, "About HC" | `0.7.9.3` | `0.8` |

---

### 1. Apple's help stacks

- **`pass` reaches a stack put in use during the handler.** HyperTalk
  Reference does `start using stack "HyperCard Help"`, then `pass openStack`,
  and it is HyperCard Help that creates the Reference menu. HC fixed the
  path of a message when it was sent, so that `pass` reached nobody, and
  `set the checkMark of menuItem 4 of menu gHMnu` answered "menu not found".
  The stacks in use are now read again at each `pass`, and no stack is
  visited twice.
- **A chunk held in a variable reads and writes like the chunk.** After
  `put the clickChunk into theChunk`, `the textStyle of theChunk` returned
  the text "textStyle of theChunk", so clicking an underlined word in Help
  Extras went nowhere. It now works for reading and for `set`.
- **A `go` to another stack leaves the stack messages to the window that
  loses the front.** HC gives each stack its own window, and that window
  already sends `suspendStack` and `resumeStack`. The kernel also sent
  `closeStack` and `openStack`, so HyperTalk Reference left twice; the
  second time, its `setCheckMark` found the menu already gone. Apple's
  reference, on the "closeStack" card: with more than one stack open,
  HyperCard sends `suspendStack`, not `closeStack`. A second word of the
  glossary no longer reopens the entry of the first one, either.
- **`this card of stack "X"`** is now the card that X shows: the last one
  visited, or its first card. It used to mean the current card of the
  current stack, so the Reference menu's `go this card of stack "Help
  Extras" in a new window` stayed where it was.
- **HyperCard Help gets its own window** when you go to it. Put in use by
  HyperTalk Reference, it was loaded without one, and there was nothing to
  bring to the front.
- **`put menuItem 4 of menu "X"`** returns the item's text, and **`put menu
  "X"`** its items, each followed by a return. Both answered "object not
  found". Measured in HyperCard 2.4.1.
- **`play s` plays the sound whose name is in `s`.** HC looked for a sound
  named "s". Measured in HyperCard 2.4.1.

### 2. What HyperTalk Reference describes, now supported

- **`mark cards where …`** and **`mark cards by finding …`**, and the same
  with `unmark`. The `where` condition is evaluated on each card, like the
  key of `sort … by`, and `by finding` searches as `find` does. `where`
  used to stop the script with "object not found", and `by finding` marked
  the current card, whatever it searched. `unmark first marked card` works
  too.
- **`enable button 1`** and **`disable button 1`**, a short form of
  `set the enabled`, as the "disable" card describes. They worked for menus
  only.
- **`delete menuItem "New" from menu "File"`.** Writing it found a second
  fault: after deleting an item, the check mark and the command key of the
  items below it stayed one row too low. They now move with their item.
- **`open printing [with dialog]` … `close printing`.** Every `print` in
  between adds its cards to one print job, and `close printing` hands them
  over together: one dialog, one document. **`print 5 cards`** prints five
  cards from the current one, and stops at the last card.
- **`lock recent`**, **`unlock recent`**, **`lock error dialogs`** and
  **`unlock error dialogs`**. The properties existed; the commands did not.
- **`go forth`** after `go back`, and **`go recent card`**. A new move
  forgets what could be done again, like a browser's Back button.
- **Sixteen global settings** that stopped a script on `set`, or on
  reading: `userModify` (also `of this stack`), `printMargins`,
  `printTextAlign`, `printTextFont`, `printTextHeight`, `printTextSize`,
  `printTextStyle`, `scriptTextFont`, `scriptTextSize`, `traceDelay`,
  `dialingVolume`, `longWindowTitles`, `debugger`, `scriptEditor`,
  `messageWatcher` and `variableWatcher`. HC keeps them and returns them,
  with the defaults that HyperTalk Reference gives, and refuses a value out
  of range. **HC does not use them**: setting `printTextFont` does not change
  how HC prints. `the environment` returns `development`, `the suspended`
  what the system says, and `the stackSpace` one megabyte.

### 3. The Style menu

- **HyperCard's Style menu**, as HyperCard 2.4.1 shows it: Plain, Bold,
  Italic, Underline, Outline, Shadow, Condense, Extend, Group; 9, 10, 12,
  14, 18, 24; Other…. Each style is written in its own style. It applies to
  the text you select in a field you are editing, and is greyed out
  elsewhere. A style is turned on for the whole selection, or off if all of
  it already has it; Plain removes them all. The check marks show what the
  whole selection has, and Cmd-Z undoes.
- **Group is how you make a link.** Select some words, choose Group, and
  give the field (locked) a script that follows the click, as Apple's help
  stacks do:

  ```
  on mouseUp
    if "group" is in the textStyle of the clickChunk then
      go card (the clickText)
    end if
  end mouseUp
  ```

  Grouped text shows its grey underline only after `show groups`; Apple's
  help stacks turn it on themselves when they open. Put `show groups` in
  your stack's `openStack` handler to see it in yours.
- **`show groups` redraws the fields already shown.** HC kept the drawing
  of recently shown fields, and that memory did not take `show groups` into
  account: the grey underline did not appear on the current card, and
  "Hide Active Text" in the help stacks left it on cards already seen.
- **Courier and Monaco fields no longer come out condensed after editing.**
  HC corrects the spacing of these old fonts slightly, and that correction
  was read back as Condense, or Extend for some sizes, when a field was
  edited. Found by reading the code, not seen on screen.

---

### Test status

- C kernel: **303 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. Thirteen are new, for the
  fixes in sections 1 and 2. The Style menu lives in the Cocoa interface,
  which these checks do not reach: it was tried in the application.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.

### Not measured yet

- In HyperCard: a `pass` that reaches a stack put in use during the
  handler. HC's behaviour is deduced from Apple's own help scripts, which
  need it to create their menu.
- What HyperCard does with a stack in use when you close its window. HC
  closes it, and it leaves the stacks in use.
- When the dialog of `open printing with dialog` appears, and whether
  Cancel sets `the result`; `print 5 cards` near the last card; a clipped
  `print card from … to …` inside a print job (HC prints it at once, on its
  own).
- Whether `go forth` also forgets its steps after a detour.
- The Style menu outside a field being edited (an object selected with the
  Button or Field tool, the paint Text tool): HC greys it out. A selection
  with mixed styles: HC turns the style on. `doMenu "Bold"` from a script is
  not served.
- The default of `dialingVolume` (HC says 7), and the answer of
  `the stackSpace`.
- Still from 0.7.9.3: `go card "Nonexistent"`; the exact format of
  `the params` and of `the destination`; writing into or deleting
  `char 1 to 0`; whether Shift-Cmd-Option also outlines the buttons, and the
  order of the Handlers and Functions menus; whether HyperCard tells Cmd-B
  from Cmd-Shift-B in `commandKeyDown "B"`.

### Known, and not fixed here

- **"Print Topic"** in the Reference menu prints every page with the same
  picture: the help script locks the screen before printing, and HC prints
  the frozen screen. Print page 1 only in the print dialog. On the four
  contents cards of HyperTalk Reference, "Print Topic" stops with an error:
  their "Pages" field is empty.
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
- `play stop` does not stop a sound: HC looks for a sound named "stop" and
  beeps.
- Seen once while testing 0.7.9.3, and not explained yet: a game that seemed
  frozen when started again. It has not come back. If you see it, please say
  how the previous game had ended.
- HC's own dialogs and messages are in French.
- The other keyboard messages written as commands (`arrowKey "left"`,
  `tabKey`, `returnKey`…) reach a handler, but have no default action yet.
- A stack's own palettes (a `PLTE` resource, or an XCMD such as the one in
  Stack Templates) are not supported.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#118](https://github.com/cleobuline/hc/pull/118): Apple's help stacks
  end to end, `play` with a variable, menus read as values
- [#119](https://github.com/cleobuline/hc/pull/119): what HyperTalk
  Reference describes and HC could not run
- [#120](https://github.com/cleobuline/hc/pull/120): version 0.8 and this
  note
- [#121](https://github.com/cleobuline/hc/pull/121): the Style menu,
  `show groups` on fields already drawn, Courier and Monaco after editing

**Full changelog:** [HC-0.7.9.3...HC-0.8](https://github.com/cleobuline/hc/compare/HC-0.7.9.3...HC-0.8)
