## HC-0.8.7 — A correction to 0.8.6, and New Card as HyperCard does it

### Installing

**This release is signed and notarised.** HC is signed with the Developer ID
of the Association Labynet, which distributes it, and Apple's notary service
has checked it. Open the DMG, drag `HC.app` into Applications, and open it.
The first time, macOS asks the question it asks of anything downloaded from
the internet — "HC is an app downloaded from the Internet. Are you sure you
want to open it?" — and offers **Open**. It asks once.

The binary is universal (`x86_64 arm64`), and the deployment target is
macOS 10.13. The latest DMG is always at
<https://github.com/cleobuline/hc/releases/latest/download/HC.dmg>.

---

**Update from 0.8.6 — recommended.** The day after 0.8.6, a deep review of
its code found that two of its own fixes were incomplete. **The 0.8.6 note
says that HC "stays where it is" when a handler deletes the card a `go`,
`find` or `pop` is heading to. That was not true in the most common case —
a `mouseUp` handler — and it is now.** The review found other faults, fixed
here, and raised two questions that were settled by measuring HyperCard
2.4.1 itself: what a card without a name is called, and what **New Card**
and **New Background** do.

### At a glance

| | 0.8.6 | 0.8.7 |
| -- | -- | -- |
| A button's `mouseUp` goes to a card that `closeCard` deletes | HC **loses its current card** | stays, and `the result` says so |
| `openBackground` or `openCard` deletes the card being opened | HC reads a freed card | stays, and `the result` says so |
| `go` to a card that does not exist, in a script | the script **stops** | `the result` is `No such card`, and the script goes on |
| `the name` of a card made with New Card | `card ""` | `card id 9789` |
| `the short name` of a card without a name | empty | `card id 9789` |
| New Card, New Background | the card goes **last**; few or no messages | the card goes **after the current one**, with HyperCard's messages |
| Copy, Cut or Duplicate Card when a drawing cannot be prepared | the **old** drawing copied; Cut **loses** it | refused, with a beep |
| Paste a CMYK picture, or one with its alpha first | read as RGBA | **converted** |
| Paste a picture over 20,000 points across | pastes an **older** picture | refused, with a beep |
| An accented name in an alert, the script editor's title, the checker | `mÃ©moire` | `mémoire` |
| `the hcVersion`, "About HC" | `0.8.6` | `0.8.7` |

---

### 1. Going to a card that a handler deletes — the fix completed

0.8.6 guarded `go`, `find` and `pop` against a `closeCard` handler that
deletes the card they head to. Two cases were left open:

- **In a handler.** While a handler runs, a deleted card is detached from
  its stack at once but freed only when the handler ends. 0.8.6 watched for
  the card being *freed*, so inside a button's `mouseUp` — the usual place
  for a `go` — it did not see the deletion, settled on the detached card, and
  lost its current card: no script reached the stack any more.
- **The opening messages.** An `openBackground` or `openCard` handler that
  deleted the arriving card was not guarded at all.

`go`, `find` and `pop` now share one sequence of messages, and after **each**
message HC checks that the card it heads to still belongs to its stack. If
it does not, HC stays where it is, and `the result` is `No such card` (or
`Not found` for `find`). What HyperCard does in these cases has not been
measured.

And when `find` gives up this way, `foundText`, `foundChunk` and the box
around the found text are cleared, as for any failed `find`.

### 2. Measured in HyperCard: cards without a name, and a `go` that fails

These were measured in HyperCard 2.4.1, by a button that writes its answers
into a field.

- **A card without a name is called by its ID**, in `the name` and in
  `the short name` alike: `card id 9789`. Cards made with New Card have no
  name, and HC called them `card ""`. That is what the **Recent** menu
  showed for each of them, and `go to line 2 of the recent cards` led to the
  wrong card. A card whose name is set to empty is a card without a name.
- **`go card ""` finds no card**, even in a stack whose cards have no name.
- **A `go` to a card that does not exist does not stop the script.** `the
  result` is `No such card` — no full stop —, HyperCard stays on the card it
  was on, and the next line runs. HC stopped the script with an error. A
  test script written for HyperCard, in HC's test corpus, checks exactly
  this, and never got that far: it now runs through.

### 3. New Card and New Background, as in HyperCard

Measured in HyperCard 2.4.1 with a log kept in the stack script:

- **The new card goes right after the current card.** From card 2, the new
  card is card 3. HC put it at the end of the stack.
- **New Card** sends `closeCard` to the card you leave, then `newCard` and
  `openCard` to the new card. HC sent nothing.
- **New Background** sends `closeCard` and `closeBackground` to the card you
  leave, then `newBackground`, `newCard`, `openBackground` and `openCard` to
  the new card. **All six go to a card**: an `on newBackground` handler in a
  card script is now called. HC sent `newBackground` to the background
  itself, then `newCard`, and nothing else.

A handler that deletes the new card, or the card you leave, no longer makes
HC read a deleted card. What HyperCard does then has not been measured.

### 4. Drawings, copied and cut

When a card's drawing cannot be prepared for the stack — for lack of
memory —, 0.8.6 refused to save. Copy Card, Cut Card and Duplicate Card did
not ask: they took the old drawing, and Cut Card then discarded the only
copy of the new one. They now refuse, with a beep, and nothing is lost.
Opening a stack no longer reuses the window of an untitled stack whose
drawing is in that state.

### 5. Paste

- A picture from another application in CMYK, or with its transparency
  stored first or unpremultiplied, has the same size per point as HC's own
  drawings, and 0.8.6 took it as is. It is now converted.
- A picture HC cannot take — over 20,000 points across, or unreadable — is
  refused with a beep. 0.8.6 pasted the last picture copied in HC instead,
  without a word.

### 6. Accented text

Stack names and some of HC's messages are in UTF-8, and six places of the
interface read them as MacRoman: an accented name proposed by **Save As**,
the alerts for a stack that cannot be read, the title of the script editor,
and the lines of the script checker. They now read UTF-8. The script checker
also cut its excerpt in the middle of a character.

### 7. MAC•CAM Workshop

Three computations of the MAC•CAM modules relied on behaviour that C leaves
undefined: a return address with its high word negative, `PICK` with a
negative index, and a `planes` value of more than thirty-one digits. They
now compute the same result without it. `forth.wasm` is unchanged byte for
byte; `maccam.wasm` is rebuilt. A stack that carries the old `maccam.wasm`
keeps it until the new one is dropped on its window; only an absurd
`planes` value reaches the old fault.

### 8. Two sounds

Two sounds join the ones HC carries: `piano`, and `shtak`, the sound of the
targets of HC's pinball table. `play "piano"` and `play "shtak"` find them
by name, like `boing`.

---

### Test status

- C kernel: **316 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: navigation from a
  button, with an opening message that deletes the card, and `find` from a
  background field; cards without a name and a `go` that fails; New Card
  and New Background, their place, their messages and their targets; the
  two FORTH programs; a `planes` of forty digits; an accented excerpt in the
  script checker.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: **no reports on 80,000 damaged stacks** (60,000 in HC's
  format, 20,000 in Apple's), after the canary check caught its three
  planted faults.

### Not measured yet

- What HyperCard does when a handler deletes the card a `go`, or a New
  Card, is heading to.
- Where `closeBackground` and `openBackground` go during a plain `go` from
  one background to another: New Background sends them to a card, and HC
  still sends them to the background when it navigates.
- `the short name` of a background, button or field without a name: HC
  still answers empty.
- `go bkgnd ""` moved HyperCard to another card in a stack whose
  backgrounds have no names; HC does not imitate this.
- The fixes on the Cocoa side — drawings, paste, accented text, the two
  menus — build on macOS in continuous integration, but were not run here:
  the checks run on Linux.
- The rest of 0.8.6's list stands.

### Known, and not fixed here

- Everything listed in 0.8.6 under "Known, and not fixed here".

### Changes in this release

- [#161](https://github.com/cleobuline/hc/pull/161): the review of 0.8.6,
  what HyperCard measured, and version 0.8.7
- [#162](https://github.com/cleobuline/hc/pull/162): the `piano` and
  `shtak` sounds

**Full changelog:** [HC-0.8.6...HC-0.8.7](https://github.com/cleobuline/hc/compare/HC-0.8.6...HC-0.8.7)
