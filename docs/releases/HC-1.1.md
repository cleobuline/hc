## HC-1.1 — The debugger

### Installing

This release is signed and notarised, like 1.0. Open the DMG, drag `HC.app`
into Applications, and open it. It is also on the Mac App Store; both are the
same application. The latest DMG is always at
<https://github.com/cleobuline/hc/releases/latest/download/HC.dmg>.

---

**HC now has a script debugger**, built on HyperCard's own: checkpoints,
Step, Step Into, Trace, Trace Into, Go and Abort, a Variable Watcher whose
values you can change during a pause, and a Message Watcher. Its words, its
keys and the default trace delay were read inside HyperCard 2.4.1, running
under Basilisk II. HyperCard keeps them in a Debugger menu; HC puts them on
buttons in a small panel, because clicking Step fifty times in a menu is no
fun.

### At a glance

| | 1.0 | 1.1 |
| -- | -- | -- |
| Debugger | none | checkpoints, Step, Step Into, Trace, Trace Into, Go, Abort, Pause |
| Variable Watcher | none | locals and globals at each stop, **editable** |
| Message Watcher | none | **Tools ▸ Message Watcher**, or `show window "Message Watcher"` |
| Script error dialog | OK, Script | **Cancel, Script, Debug**, as in HyperCard |
| `zorglub` with no handler | a French message | `Can't understand "zorglub".` |
| `traceDelay` | remembered, never used | **30** by default, used by Trace |
| `click twice at` | — | closes a polygon from a script |
| A filled shape painted by script | no frame | framed, as with the mouse |
| `charToNum(return)` | `10` | `13` |
| `charToNum(linefeed)` | `13` after the fix above | `10` |
| `send "t2" to me`, no handler | silence | `Can't understand "t2".` |
| `send "put 5 into x" to me` | nothing | runs the line |
| Script editor, OK after visiting another stack | did nothing | saves |
| HC menu | — | **Leave a Comment…** |

---

### 1. The debugger

**Checkpoints.** Click in the line-number margin of a script window: a red
dot appears, and the script stops *before* that line runs. Click it again to
remove it. `debug checkpoint` in a script stops on its own line. Checkpoints
last for the session and die with their object.

**The stop.** The script window comes forward, and the line about to run is
shown with a green arrow in the margin and a green background. Next to it,
the **Debugger** panel shows where you are — object, handler, line — and the
variables. Its buttons carry HyperCard's words and keys:

| | | |
| -- | -- | -- |
| Step | ⌘S | the next line of the same handler |
| Step Into | ⌘I | the next line, even inside a handler it calls |
| Trace | | runs by itself, line by line, in the same handler |
| Trace Into | ⌘T | runs by itself, going into called handlers |
| Go | ⌘G | runs freely until the next checkpoint |
| Abort | ⌘A | stops the script |
| Pause | | during a trace: stops on the next line |

**Trace** waits `traceDelay` ticks between lines — **30 by default**, half a
second, the value HyperCard shows in its *Trace Delay…* dialog. The panel
has a **Trace Delay** box to change it; `set the traceDelay to …` works too.
During a trace the arrow moves on its own and the variables follow. **Pause**
stops it on the next line, from where Step, Go or Trace carry on; Abort and
⌘. stop the script. A checkpoint met along the way also stops the trace.

**The Variable Watcher.** The panel lists the handler's local variables, then
the globals (marked `global`), name and value. Below the list, the full value
of the selected variable — and you can **edit it**. Every keystroke is
written to the variable, and Step or Go carry on with the new value:

```
on mouseUp
  put 1 into x            -- checkpoint here; at the stop, set x to 7
  put x * 10 into y
  put "x=" & x & " y=" & y   -- x=7 y=70
end mouseUp
```

Only variables that exist can be changed: the watcher corrects a value, it
does not create one.

**The Message Watcher** — *Tools ▸ Message Watcher* — lists each message as
it is sent: `mouseUp`, and indented below it the messages its handler sends
in turn. **Hide unused messages** hides the ones no handler took (a `pass`
all the way counts as unused); **Hide idle** hides `idle`. Closed, it costs
nothing; open, it redraws at most ten times a second, so it does not slow
your scripts down.

**After an error.** The script error dialog now has HyperCard's three
buttons: **Cancel**, **Script** — the script, the faulty line framed in red —
and **Debug**, which opens the same script *and* the Debugger panel with the
variables **as they were at the moment of the error**. Step, Step Into, Trace
and Go are greyed out, as in HyperCard: the script has stopped, you can look
but not resume. HyperCard keeps the script suspended while its dialog is up;
HC freezes the variables instead, which shows the same thing more safely.

### 2. `Can't understand`

A command or message that nobody handles now says what HyperCard says, to
the character, final full stop included:

```
zorglub          ->  Can't understand "zorglub".
send "t2" to me  ->  Can't understand "t2".
```

It used to be a French sentence. When the command has arguments, HC names
the message alone (`convert "x"` gives `Can't understand "convert".`).

### 3. `send`

- `send "t2" to me`, with no `t2` handler anywhere, is an error, as in
  HyperCard. It used to do nothing, silently.
- `send "put 5 into x" to me` runs the line, which sees the sender's
  variables: `put x` then shows `5`. During that line, `me` is still the
  sender — measured in HyperCard, where `send "put the name of me" to cd 1`
  from a button shows the button's name.

### 4. `return` is 13, `linefeed` is 10

```
put charToNum(return)          ->  13
put numToChar(13) is return    ->  true
put charToNum(linefeed)        ->  10
put linefeed is return         ->  false
put the number of lines of ("a" & linefeed & "b")   ->  1
```

All five measured in HyperCard. HC stores line ends as byte 10, as it always
has; the two codes are crossed in `numToChar` and `charToNum` so that scripts
see HyperCard's values. Stacks are not affected.

### 5. Painting by script

- **`click twice at <point>`** — a double click from a script, the LiveCode
  word. With the polygon tool, `click at` places a vertex and `click twice
  at` places the last one and finishes the shape, filled if `filled` is set.
  HyperCard cannot close a polygon from a script at all (measured); this is
  an **announced HC extension**, so that polygons can be painted by script —
  facets, stained glass, spheres. HyperCard stacks are not affected: it
  refuses the form.
- **A filled shape painted by script** gets its frame, as it does with the
  mouse. A filled rectangle drawn with `drag` had no border.

### 6. Fixes

- **Script editor:** open a script, go to another stack, come back — OK did
  nothing. Each script window now keeps its own object, and OK saves
  whatever stack is in front. If the object was deleted meanwhile, OK says
  so and keeps the window open, so the text can still be copied.

### 7. Leave a comment

**HC ▸ Leave a Comment…**, and a link in *About HC*, open
<https://hc.labynet.fr/commentaires.php>: tell us what you think, what you
made, what you miss.

---

### Test status

- C kernel: **336 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. New: `debogueur` (every
  debugger gesture, the frozen variables after an error, editing a variable,
  Trace, Pause), `messagewatcher`, `clicdeux`.
- **Zero warnings**: gcc at three optimisation levels, *and* clang with
  Xcode's warning families. **Zero** static-analyser warnings. The Cocoa
  application builds on macOS in continuous integration.
- Stack fuzzing: FUZZ_RESULT
- The debugger, the Message Watcher, the editable variables, Trace, Pause,
  the Debug button and Leave a Comment were each tried inside HC itself
  before being merged.

### Not measured yet

- The order of the three buttons in HyperCard's script error dialog, and
  which one is the default. In HC, Return means Cancel.
- What HyperCard names in `Can't understand` when the command has arguments.
- Whether HyperCard's Message Watcher shows function calls (HC does), how it
  indents nested messages, and whether it lists built-in commands such as
  `put` (HC does not).
- How HyperCard's Variable Watcher commits an edit, and how it shows globals.
- The unit of `traceDelay` in HyperCard (HC uses ticks), and whether its
  trace climbs back into the calling handler when the traced one ends (HC
  does, as Step does).
- Which line `debug checkpoint` stops on in HyperCard (HC: its own).
- Everything listed in 1.0 under "Not measured yet".

### Known, and not fixed here

- Many error messages from the HyperTalk interpreter are still in French.
  They will be translated one by one, each with the text HyperCard shows.
- Closing a script window with its close box discards unsaved changes
  without asking.
- ⌘A is Abort in the Debugger panel, as in HyperCard: it does not select
  all in the value box.
- Everything listed in 1.0 under "Known, and not fixed here".

### Changes in this release

- [#196](https://github.com/cleobuline/hc/pull/196),
  [#197](https://github.com/cleobuline/hc/pull/197): `return` is 13,
  `linefeed` is 10
- [#198](https://github.com/cleobuline/hc/pull/198),
  [#199](https://github.com/cleobuline/hc/pull/199): `send` — an unhandled
  message is an error, a HyperTalk line runs
- [#200](https://github.com/cleobuline/hc/pull/200): script editor, OK after
  visiting another stack
- [#212](https://github.com/cleobuline/hc/pull/212): `click twice at`
- [#213](https://github.com/cleobuline/hc/pull/213): the frame of a filled
  shape painted by script
- [#214](https://github.com/cleobuline/hc/pull/214): the debugger,
  the Debug button, `Can't understand`
- [#215](https://github.com/cleobuline/hc/pull/215): the Message Watcher,
  editable variables, Trace and `traceDelay`
- [#216](https://github.com/cleobuline/hc/pull/216): Pause during a trace
- [#217](https://github.com/cleobuline/hc/pull/217): Leave a Comment…
- version 1.1

**Full changelog:** [HC-1.0...HC-1.1](https://github.com/cleobuline/hc/compare/HC-1.0...HC-1.1)
