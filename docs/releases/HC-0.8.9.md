## HC-0.8.9 — `replace` and `does not contain`, from LiveCode

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

**Update from 0.8.8 — for those who write scripts.** HC now accepts two
phrases that LiveCode and SuperCard have and HyperCard does not:
`does not contain`, and `replace … with … in`. Nothing else changes, and
every HyperCard stack behaves as before.

### At a glance

| | 0.8.8 | 0.8.9 |
| -- | -- | -- |
| `if x does not contain "z" then …` | refused | accepted |
| `replace "a" with "b" in x` | refused | accepted |
| A stack's own `on replace` handler, called as `replace x, "a", "b"` | called | still called |
| `the hcVersion`, "About HC" | `0.8.8` | `0.8.9` |

---

### 1. Two phrases HyperCard does not have

Writing a game in HyperTalk, both came naturally, and HC refused both. Before
anything else, they were played **inside HyperCard 2.4.1**, under Basilisk II,
which refuses them too:

| | HyperCard 2.4.1 | HC 0.8.8 | HC 0.8.9 |
| -- | -- | -- | -- |
| `if x does not contain "z" …` | *Can't understand this keyword* | refused | works |
| `replace "a" with "b" in x` | *Can't understand arguments of replace* | refused | works |

They are accepted here as tolerances, like `chars 1 to 5 of X` before them:
HC accepts a **superset** of what the original accepts, never the reverse.
The other side of that rule still holds: a stack that uses them will not run
in HyperCard, so a bench meant to be played on both sides does not use them.

**`does not contain`** is the exact opposite of `contains`, and like it,
ignores case: `"ABC" does not contain "b"` is false.

**`replace old with new in container`** replaces every occurrence, from left
to right and without overlap, ignoring case as `contains` does:

    put "banana" into x
    replace "an" with "AN" in x        -- bANANa
    replace "a" with empty in x        -- removes every "a"

It writes wherever `put … into` writes — a variable, a field, or a chunk of
either (`replace "e" with "E" in item 2 of card field "f"`). A variable never
set counts as empty. An empty search string replaces nothing. When the new
text contains the old one, each occurrence is replaced once, and the command
ends. `the result` is emptied. A target that cannot be written, such as
`the name of this card`, is refused before anything is evaluated.

**No HyperCard stack changes behaviour.** HyperCard has no `replace`, so a
stack could have defined its own `on replace` handler and called it with
commas. That call still reaches the stack's handler. `replace` and `does`
remain usable as variable names.

On the way, a trap: in HyperTalk, `in` also means `of`, so
`replace "a" with new in s` read `new in s` as "the property new of s". A
quoted string worked, a variable did not. The parser now leaves that `in` to
the command, and `the name of card 1` still means what it says.

### 2. The arcade games join the test suite

Boulder-Prout, Gobe-Prout and Astro-Prout — a homemade Boulder Dash, Pac-Man
and Asteroids, written in HyperTalk — are now built by the test suite and
played by robots at every run, frame by frame, with what is on the card
checked against the game's own state.

Two more joined them on <https://hc.labynet.fr>: **Pousse-Prout**, a
homemade Sokoban whose thirteen levels were all solved by a solver before
going into the stack, and **Lingot-Prout**, a homemade Lode Runner on the
original's 28 × 16 board, with guards, holes that fill back in, and a hidden
ladder.

---

### Test status

- C kernel: **320 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `does not contain` and
  `replace`, and the stacks' own `replace` handlers and variable names they
  must not disturb.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: see below.

### Not measured yet

- Everything listed in 0.8.8 under "Not measured yet" stands.

### Known, and not fixed here

- `find x in card field "f"`, where `x` is a **variable**, answers "unknown
  property" in HC: the same trap as `replace` above, `x in card field "f"`
  read as a property. A quoted string works: `find "two" in card field "f"`.
  What HyperCard does with a variable there has not been measured.
- Everything listed in 0.8.6 under "Known, and not fixed here".

### Changes in this release

- [#168](https://github.com/cleobuline/hc/pull/168): Boulder-Prout joins the
  test suite
- [#169](https://github.com/cleobuline/hc/pull/169): Gobe-Prout and
  Astro-Prout join the test suite
- [#170](https://github.com/cleobuline/hc/pull/170): `does not contain` and
  `replace … with … in`

**Full changelog:** [HC-0.8.8...HC-0.8.9](https://github.com/cleobuline/hc/compare/HC-0.8.8...HC-0.8.9)
