## HC-0.7.9.1 — Apple's own help stacks run, unmodified

**Update from 0.7.9.** This is a fidelity release. HC now runs Apple's three
HyperCard help stacks, **HyperCard Help**, **HyperTalk Reference** and
**Help Extras**, without changing a single line of their scripts.

Those stacks use almost every corner of HyperTalk. Every place where HC
stopped on them was a gap in HC, and each one is now closed. They were
converted from their original files by HC's author and played in HC. They
are Apple's, so they are not included in the DMG. Convert your own copies
with File > Open.

### At a glance

| | 0.7.9 | 0.7.9.1 |
| -- | -- | -- |
| HyperCard Help, HyperTalk Reference, Help Extras | stopped on the first page | **run, scripts unmodified** |
| `push recent card`, `pop card into x` | "can't do that" | **work** |
| `the cantModify of this stack` | unknown property | **works**, and a locked stack refuses field writes |
| `the cantAbort of this stack` | unknown property | **works**: Cmd-period no longer stops the script |
| `show menuBar`, `hide menuBar` | "can't do that" | **work** |
| `show groups`, `hide groups` | "can't do that" | **work**: grouped text gets its grey underline |
| `the cmdChar of menuItem …` | unknown property | **works** |
| `select line 0 of field …` | an error | **clears that list's selection** |
| `the stacks`, `the stacksInUse` | names | **paths**, as Apple's scripts expect |
| `bkgnd field "x" of card id N` | read the current card | **reads and writes card N** |
| `the multiple`, `the multiSpace`, Draw Multiple | unknown | **work**, with the Options menu item |
| `the visible of tool window`, `the loc of …` | "object not found" | **work** for the tool, pattern and message windows |
| `show msg`, `hide message box` | "object not found" | **work** |
| `choose tool 3` | a syntax error | **works** |
| `the clickChunk` after `wait until the mouseClick` | empty | **the click you just made** |
| `the clickText` | the whole line | **the word, or the whole grouped phrase** |
| `go card x of stack "y"`, stack not open yet | "object not found" | **opens the stack and goes** |
| `go card theSection & theTopic` | "object not found" | **goes to the card named by the whole text** |
| `the hcVersion`, "About HC" | `0.7.9` | `0.7.9.1` |

---

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
macOS 10.13. HC 0.7.9 has been reported working on an Apple M5 running a
macOS beta.

---

### 1. Navigation: where you came from, and where you are going

- `push recent card` pushes the card you were on before this one.
- `pop card into x`, `before x` and `after x` put that card's long id into
  `x` **without going there**. This was measured in HyperCard. Apple's help
  uses it to remember where you came from.
- `go card "x" of stack "Help Extras"` opens the stack if it is not open
  yet, the same way `go stack "Help Extras"` already did. The stack file is
  looked for next to the current stack, next to the app, and in Documents.
- `go card theSection & theTopic` goes to the card named by the **whole**
  text. Apple writes it without parentheses. In any other expression,
  `&` still joins text after a card reference, so
  `the name of card 1 && the name of card 2` is unchanged.
- A stack that cannot be found now says so: "No such stack" in
  `the result`. The script continues.

### 2. Stack properties: cantModify and cantAbort

- **`the cantModify of this stack`** can be read and set, and it is saved
  with the stack. Measured in HyperCard: while it is true, a script that
  writes into a field is **silently refused** and the script goes on.
- **`the cantAbort of this stack`** can be read and set. While it is true,
  Cmd-period does not stop a running script. This is how Apple's
  demonstrations protect themselves while they play.

### 3. Menus and display

- **`show menuBar` / `hide menuBar`** show or hide the menu bar.
- **`show groups` / `hide groups`** draw a thick grey underline under text
  in the `group` style. Apple's help uses it to show its active words.
- **`the cmdChar of menuItem …`** reads and sets a menu item's keyboard
  shortcut, which then appears in the menu.
- A menu or menu item can be named by a **variable**:
  `menuItem art of menu gMenu`.

### 4. Lists and fields

- **`select line 0 of field "x"`** and **`select line empty of field "x"`**
  clear the selected line of **that** list. Other lists keep their selection.
- **`bkgnd field "Title" of card id N`** now reads and writes the field **on
  card N**. HC used to read the field of the current card instead. Apple's
  "Find Topic" lists its results this way, and they were coming out as empty
  lines.
- **`the stacks`** and **`the stacksInUse`** give paths, with the active
  stack first. Apple's help compares them with `the long name` to decide
  whether it is in use. With names, it never put itself in use, and
  `showSection` and `goTopic` were "not found".

### 5. Clicks in text

Apple's reference demonstrates `the clickChunk` and `the clickText` like
this:

```
wait until the mouseClick
get the clickChunk
select it
```

- **A click made while a script runs** now records where it happened.
  `the clickLoc`, `the clickLine`, `the clickChunk` and `the clickText`
  report that click. Before, they still reported the previous one, here the
  click on the button that started the script.
- **`the clickChunk`** is the word you clicked, or the **whole run of text in
  the `group` style** around it, as Apple's reference says. A click on one
  word of a grouped phrase gives the whole phrase.
- **`the clickText`** is the text of that same chunk. HC used to give the
  whole line, which is not what HyperCard does.
- **`the clickChunk` counts characters, not bytes.** Each accented or special
  character before the click (such as the `¬` of Apple's scripts) used to
  shift the chunk one character to the right.

### 6. Paint: Draw Multiple

`the multiple`, `the multiSpace` (1 to 100, default 1) and a **Draw
Multiple** item in the Options menu. With it on, the line, rectangle,
rounded rectangle, oval and regular polygon tools leave a copy every
`multiSpace` pixels as you drag. `drag from … to …` does the same in a
script.

Also: `choose tool 3` selects a tool by its number, counted across the tool
palette from left to right and top to bottom, as in HyperCard (1 to 18).

### 7. HyperCard's own windows

`tool window`, `pattern window` and `message window` can be named in a
script, directly or through a variable. You can read and set their
`visible` and their `loc`, and `show`, `hide` or `close` them.
`window "Tools"` and `window "Patterns"` name the same windows.
`show msg` and `hide message box`, found in many old `openStack` handlers,
work again. The message box is still a container for `put`.

`set the loc of window "Navigator" to 10,20` used to set only the `20`. It
now sets the whole point.

### 8. Sturdier against damaged stacks

Before submitting HC anywhere, its stack reader was fuzzed: hundreds of
thousands of damaged files, in HC's format and in Apple's, under
AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer. No
reports. The fuzzer now lives in the repository (`make fuzz`), together
with a canary check that proves it can still detect a crash, a leak and a
hang before it reports a clean run.

A review of the macOS side then found two problems:

- a text style range stored past the end of a field's text made the app
  **read past the end of a buffer**. This is fixed;
- a picture layer whose header announces a huge size is now refused if the
  memory cannot be allocated. Before, it would have written to address zero.

### 9. "About HC"

The link now goes to [labynet.fr](https://labynet.fr). In 0.7.9 it went to
labynet.net.

---

### Test status

- C kernel: **284 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `verrou` (cantModify,
  cantAbort), `aidemenus` (the shape of Apple's help handlers) and
  `runscript` (the "Run the Script" demonstration).
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings.
- Stack fuzzing: no reports.

### Not measured in HyperCard yet

- The exact text of `the stacks` lines. HC's format follows what Apple's
  scripts require.
- `the loc` of the tool, pattern and message windows. HC gives the top-left
  corner of their content on screen, and a `loc` read back can always be set
  again.
- The exact spacing of Draw Multiple copies.
- `go card "Nonexistent"`: HC stops the script, and HyperCard may continue
  with "No such card".

### Known, and not fixed here

- With `drag`, a filled shape is filled with no border. With the mouse, it
  is filled and bordered.
- Fields take no `backColor` or `foreColor`.
- A fault inside a user function stops the function but not its caller.
- A script error inside a handler reached by `send` does not stop the script
  that sent it.
- A stack's own palettes (a `PLTE` resource, or an XCMD such as the one in
  Stack Templates) are not supported.
- Still standing from earlier versions:
  - colour icons, pictures and sounds from a resource fork;
  - patterns, HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#105](https://github.com/cleobuline/hc/pull/105): the "About HC" link to
  labynet.fr
- [#106](https://github.com/cleobuline/hc/pull/106): the stack fuzzer, in
  the repository
- [#107](https://github.com/cleobuline/hc/pull/107): Apple's help stacks, the
  fidelity fixes above, the two macOS-side fixes, version 0.7.9.1 and this
  note
- [#108](https://github.com/cleobuline/hc/pull/108): clicks in text:
  `the clickChunk` and `the clickText`

**Full changelog:** [HC-0.7.9...HC-0.7.9.1](https://github.com/cleobuline/hc/compare/HC-0.7.9...HC-0.7.9.1)
