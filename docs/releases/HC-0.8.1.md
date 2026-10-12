## HC-0.8.1 — Coloured fields, and a source audit

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

**Update from 0.8.** Fields get colours, like buttons: a background, a
border, and an opacity, so a text field can be half transparent over a
painted card. The rest is the outcome of an audit of the sources: properties
HC kept but never looked at, numbers typed by hand that were read too
loosely, a print job that a stopped script left open, and three answers
measured in HyperCard 2.4.1 that HC gave wrong.

### At a glance

| | 0.8 | 0.8.1 |
| -- | -- | -- |
| `set the backColor of field 1 to "255,255,200,128"` | "unknown property" | a **half-transparent background**; `foreColor` colours the border |
| Field Info | — | **Back** and **Fore**, each with a checkbox and a colour well |
| `doMenu "Bold"`, `doMenu "Group"` on a selection | "unknown menu item" | **applies the style**, as in HyperCard 2.4.1 |
| `lock recent`, then visiting cards | they still entered the Recent menu | **kept out of Recent**; `go back` still follows them |
| `set the itemDelimiter to "/"` in the message box | still `/` on the next line | **back to a comma at idle**, as the reference says; `dragSpeed` back to 0 |
| `the textSize` of a new field | `0` | **`12`** (measured in HyperCard 2.4.1) |
| `the textSize`, `the textFont` of a new button | `0` and empty | **`12` and `Chicago`** (measured in HyperCard 2.4.1) |
| A field drawn with the Field tool | system font, `the textFont` empty | **Geneva 12** (measured in HyperCard 2.4.1) |
| "Other…" in the Style menu, given `12abc` | took 12 | **refused**, with a beep |
| A script stopped between `open printing` and `close printing` | every later `print` went into a job that never closed | **the job is dropped** |
| `the hcVersion`, "About HC" | `0.8` | `0.8.1` |

---

### 1. Coloured fields

HC is a HyperCard clone, and its few extensions are where HyperCard had
nothing to say. Buttons already have colours, with an opacity. Fields now
have the same, with the same words:

```
set the backColor of card field "Note" to "255,255,200,128"
set the foreColor of card field "Note" to navy
put the backColor of card field "Note"      -- 255,255,200,128
```

- **`backColor`** is the background of the field, **`foreColor`** its
  border: the frame, the shadow of a `shadow` field, the scroll bar and its
  arrows on a `scrolling` one.
- The **fourth number is the opacity**, from 0 to 255, exactly as for a
  button. Below 255, the card shows through the field.
- Colour names work too (`navy`, `gold`, `red`…), and `empty` removes the
  colour.
- **Nothing for the text.** `set the textColor of word 2 of field 1` already
  colours text, by chunk.
- No `hiliteColor`: a field is not highlighted the way a button is.
- A `transparent` field paints nothing, coloured or not, like a transparent
  button.
- **Field Info** has the row that Button Info has: **Back** and **Fore**,
  each with a checkbox and a colour well. The well opens macOS's colour
  picker, whose **Opacity** slider gives the fourth number. **An unticked box
  sets nothing**, so opening the Info of an old stack's field and clicking OK
  adds nothing to it.

**Old stacks are not touched.** A field with no colour set is drawn white and
black, as before, to the pixel. No existing kernel check changed its output
when fields got colours.

While trying it, a half-transparent `shadow` field came out entirely in its
border colour: the shadow was painted under the whole field, and showed
through. Only the two strips that stick out are painted now. A `shadow`
button and a popup button with a half-transparent `backColor` had the same
drawing, and are fixed the same way.

### 2. Measured in HyperCard 2.4.1, now the same in HC

- **`doMenu "Bold"`** and the other items of the Style menu. In HyperCard,
  `select word 2 of card field 1`, then `doMenu "Bold"` and `doMenu "Group"`,
  makes word 2 bold and grouped. HC answered "unknown menu item". It now
  applies the item: through the menu itself when a field is being edited,
  and otherwise to the text selected by the script. Your stack can still
  catch the item in a `doMenu` handler first.
- **The size of a new field reads 12.** HC answered 0, while drawing it at
  12; a word set to 12 in such a field also read as `mixed` next to its
  neighbours.
- **A new button reads `12` and `Chicago`.** HC answered 0 and empty. HC has
  no Chicago font, and draws such a button in the system font, as it did
  before; the answer is HyperCard's, so that scripts testing it behave the
  same.
- **A field drawn with the Field tool is born in Geneva 12**, as in
  HyperCard. It came out in the system font, and `the textFont` answered
  empty. Geneva ships with macOS, so the field is drawn as it answers. Fields
  from an opened stack, a paste or a script keep what they have.

### 3. Found by the audit of the sources

- **`lock recent`** now keeps the cards you visit out of the Recent menu and
  `the recent cards`. The property was kept and returned, and nothing read
  it. As the "lockRecent" card of HyperTalk Reference says, it "does not
  affect the trail of cards you can go back to": `go back` and `go forth`
  still follow every card.
- **`itemDelimiter` and `dragSpeed` return to their defaults at idle**: a
  comma, and 0. HyperTalk Reference says so of both. A delimiter set in the
  message box used to stay for the next line, and every script after it.
- **Sizes typed by hand are read strictly**: "Other…" in the Style menu, the
  size and line height of the style dialog, and the Text size boxes of Field
  Info and Button Info. `12abc` was read as 12 and `12.5` as 12; a refused
  entry now leaves the value as it was, with a beep. These boxes also wrote
  any size into the object, 99999 included; they now stop where
  `set the textSize` stops.
- **The print job.** A second `open printing` used to start the job again,
  and the cards already given to it were lost; it now keeps them. A script
  stopped by an error or by Cmd-. between `open printing` and
  `close printing` used to leave the job open for good: every `print` of
  every later script went into it, and nothing came out. At idle, such a job
  is now dropped, not printed: Cmd-. means stop. The reference says nothing
  of either case, and HyperCard under Basilisk II has no printer driver to
  measure them; these are choices for robustness.

---

### Test status

- C kernel: **307 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. Four are new, for the
  audit, `doMenu` with the Style menu, the sizes of new objects, and field
  colours. Drawing and Field Info live in the Cocoa interface, which these
  checks do not reach: coloured fields were tried in the application.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: the canary check caught its three planted faults; the
  campaign of 80,000 damaged stacks is still running as this note is written.

### Not measured yet

- Coloured fields: typing in a half-transparent field (the editor paints no
  background of its own, so the field's should stay visible underneath); the
  Back and Fore wells of Field Info; a half-transparent `shadow` button and
  popup button after the fix.
- In HyperCard: whether `lockRecent` returns to false at idle (the reference
  says so of `lockScreen` and `lockErrorDialogs`, not of it; HC leaves it
  set); what `doMenu "Bold"` does on a selection that is already bold (HC
  removes it, as the menu does), on a mixed selection (HC sets it
  everywhere), and with no selection (HC does nothing). `doMenu "Other…"` is
  not served.
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
- `play stop` does not stop a sound: HC looks for a sound named "stop" and
  beeps.
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

- [#123](https://github.com/cleobuline/hc/pull/123): `lock recent`,
  `itemDelimiter` and `dragSpeed` at idle, sizes typed by hand
- [#124](https://github.com/cleobuline/hc/pull/124): `doMenu` and the Style
  menu
- [#125](https://github.com/cleobuline/hc/pull/125): the size of a new field
  and of a new button
- [#126](https://github.com/cleobuline/hc/pull/126): the print job, a new
  field in Geneva 12
- [#127](https://github.com/cleobuline/hc/pull/127): coloured fields,
  version 0.8.1 and this note

**Full changelog:** [HC-0.8...HC-0.8.1](https://github.com/cleobuline/hc/compare/HC-0.8...HC-0.8.1)
