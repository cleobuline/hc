## HC-0.7.5 — the old expression engine can be switched off, and nothing is lost

HC runs HyperTalk on its own engine, but until now it still fell back on an
older expression engine whenever it got stuck. This release switches that old
engine off on demand, finds out **exactly** what it was still answering, and
moves all of it into the new engine. The whole test suite now gives the same
answers with the old engine switched off.

To check that against the real thing, a new **torture stack for twisted
expressions** was played in HyperCard under Basilisk II. It found four places
where HC and HyperCard disagreed, and all four are fixed.

And the DMG now comes with a game: **Le Pendu**, a hangman stack.

### At a glance

| | 0.7.4 | 0.7.5 |
| -- | -- | -- |
| `the number of items of "a,b,"` | 3 | **2**, as in HyperCard |
| `round(2.5)`, `round(-2.5)` | 3, -3 | **2, -2**: round half to even, as in HyperCard |
| `length()`, `sqrt(4, 9)` | 0, 2 | **an error**, as in HyperCard |
| `sqrt("abc")`, `exp2("x")` | 0, 1 | **an error**, like `"abc" + 1` |
| `the number of cards of bg 2`, `of buttons of card 3`, `of bgs of this stack` | right only through the old engine | **answered by the new engine** |
| `the number of cards of bg 9`, with no such background | the count of the whole stack | **an error**, as in HyperCard |
| `the charToNum of "a"`, `the random of 6` | old engine | **new engine**, same code as `charToNum("a")` |
| `the zorglub of 3` | the text `zorglub of 3` | **an error** naming `zorglub` |
| `put "X" into line 4 of v`, where `v` ends with a return | 5 lines, line 4 empty | **4 lines** |
| what the DMG contains | `HC.app` | `HC.app` **and `Pendu.stack`** |
| `the hcVersion`, "About HC" | `0.7.4` | `0.7.5` |

---

### Installing: macOS will say the app is damaged. It is not.

**This release is not notarised.** macOS quarantines anything downloaded from
the internet, and Gatekeeper refuses an app it cannot trace to a notarised
Developer ID — with a message that says the wrong thing:

> *"HC is damaged and can't be opened. You should move it to the Trash."*

The app is not damaged. It is unnotarised, which is a different problem, and
the wording sends people to the Trash for no reason.

Copy `HC.app` to `/Applications`, then clear the quarantine flag once:

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

It opens normally after that, and the flag does not come back. An app you
build yourself from this repository is never quarantined — it is only the
*download* that triggers this.

`Pendu.stack` sits next to the app in the DMG. Copy it wherever you like and
open it from HC.

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. What the old engine was still doing

The old engine could only be removed once we knew what it still answered.
Counting how often it was called did not say that; switching it off did. HC
now has a switch for this, `HC_SANS_V1=1`, read once at launch, and the whole
test suite can run with the old engine off:

    HC_SANS_V1=1 ./tests/lance.sh --complet

The first run lost answers in four families, three of which the suite did not
even exercise. Small benches written for the neighbouring forms found the
rest:

- **counts with a target**: `the number of cards of bg 1` returned 1 instead
  of 2, with no error, and every other count with a target failed;
- **the "the" form** of `charToNum`, `numToChar`, `random`, and of a stack's
  own functions (`the myFunction of "ok"`);
- **a built-in function with an argument missing or too many**: HC said
  "unknown function: length", which is wrong, since the function exists;
- **an empty argument** to a maths function: `sqrt("")`.

All of it is now answered by the new engine. `f(x)` and `the f of x` go
through the same code, so they can no longer disagree. Without the old
engine, the suite now loses nothing. A new check plays every one of these
cases twice, with and without the old engine, and flags any difference.

The old engine also had faults of its own, which this work turned up:
- `the number of cards of bg 9` returned the whole stack's count;
- `sqrt("abc")` returned 0;
- `the zorglub of 3` returned its own text;
- `the charToNum of ("a")` raised a parse error and then returned 0;
- it sometimes sent a stray message (`width`, `hilite`) while looking for a
  function of that name.

Removing the old engine for good is the next step.

### 2. A torture stack for twisted expressions, played in HyperCard

The new torture stack keeps its tests in an **editable field**, one expression
per line:

    2 ^ 3 ^ 2 ==> 512
    the number of items of "a,b," ==> 2
    sqrt(4, 9) ==> ERREUR

`?` marks a case that is not measured yet, and the value is shown without a
verdict. Anyone can add a line without touching a script. The stack has 127
lines over five sections: operators, chunks, functions in both forms, a
stack's own functions, and objects and counts.

It was played in HyperCard under Basilisk II, and that settled sixteen open
questions:

| | HyperCard | HC 0.7.4 |
| -- | -- | -- |
| `-7 div 2`, `-7 mod 3` | -3, -1 | same |
| `2 ^ 3 ^ 2`, `-2 ^ 2` | 512, 4 | same |
| `the number of items of "a,b,"` | **2** | 3 |
| `round(2.5)`, `round(-2.5)` | **2, -2** | 3, -3 |
| `length()` | **"Can't understand arguments of "length"."** | 0 |
| `sqrt(4, 9)` | **"Can't understand arguments of "sqrt"."** | 2 |
| `sqrt("abc")` | "Expected number here." | 0 |
| `the number of cards of bg 9` | "No such bkgnd." | the stack's count |

The four that disagreed are fixed. Fixing the item count turned up a related
fault: writing past the end of a list or a text that ends with its separator
added one separator too many. `put "X" into line 4` of two lines ending with a
return gave five lines instead of four. This one predates the release, and it
is fixed for items and lines alike.

**HyperCard stops the whole script on an error.** The first version of the
stack ran its tests in one loop, and HyperCard stopped at the first expected
error: no more tests, no summary. The error came from inside a `do`, and it
took the calling loop down with it. The stack now plays one test per `idle`,
so it survives on both sides.

### 3. Le Pendu

A hangman game, in French, to try HC on something other than tests:

- a word is drawn at random when the stack opens;
- click the letters, or type them on the keyboard;
- seven mistakes and the little man is hanged.

The gallows is drawn in text. It uses the sounds that already ship inside
HC.app: `boing` on a mistake, `cocorico` when you win, `canard` when you
lose.

The whole game lives in the stack script. To add words, edit the `lesMots`
function in UPPER CASE, without accents. The stack is built from versioned
sources (`tests/donnees/pendu_pile.txt`), and a test plays a won game, a lost
game, a repeated letter and a lower-case key before every release.

### 4. What is not measured is written down as not measured

- A wrong number of arguments is measured for `length()` and `sqrt(4, 9)`.
  The other built-in functions follow the same rule, **not measured** one by
  one.
- `the max of "3,9,2"` and a stack function in the "the" form
  (`the myFunction of "ok"`) are **refused by HyperCard**. HC still answers
  them, for now.
- HyperCard's stop-everything on an error is measured for an error inside a
  `do`. For an error in a command or in a function, it is **not measured**.
  The bench is in `docs/mesures/erreur_abandon.txt`.
- Whether HyperCard resets `the itemDelimiter` to a comma when a script ends
  is **not measured**. HC keeps it.
- The object section of the new torture stack has not yet been played in
  HyperCard on the intended layout.

---

### Test status

- C kernel: **270 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New ones include:
  - `sansv1`: every case, with and without the old engine;
  - `torture3`: the twisted-expression stack, played both ways;
  - `separateurfinal`: the trailing separator, read and written;
  - `pendu`: the hangman, played before it ships.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families. **Zero** static-analyser warnings.
- The twisted-expression stack, in HC: **125 passed, 0 failed, 2 still to
  measure**. In HyperCard, every expression section agrees.

### Known, and not fixed here

- A fault inside a user function stops the function but not its caller, which
  carries on with an empty value. HyperCard stops everything, at least for
  `do`.
- After a fault raised by a command, the handler carries on and the dialog
  comes at the end.
- `visual effect` followed by `go`, without `lock screen`, still plays after
  the script rather than inside `go`.
- Still standing from 0.7:
  - named windows, patterns, resource-fork icons;
  - HyperCard 1.x stacks, private-access stacks;
  - the proportional period fonts;
  - `CFBundleVersion` is still hard-coded to `1`;
  - not notarised: see "Installing".

### Changes in this release

- [#93](https://github.com/cleobuline/hc/pull/93): what the old engine was
  still doing, moved into the new one; the twisted-expression torture stack;
  items, `round` and argument counts aligned with HyperCard
- [#96](https://github.com/cleobuline/hc/pull/96): Le Pendu, in the DMG
- this release: version 0.7.5 in all three places, and this note

**Full changelog:** [HC-0.7.4...HC-0.7.5](https://github.com/cleobuline/hc/compare/HC-0.7.4...HC-0.7.5)
