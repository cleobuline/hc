## HC-0.6.9.4 — the numberFormat stops corrupting arithmetic

One subject, and it had been wrong since the beginning: **the `numberFormat`
was rounding calculations, not just their display.** Under a narrow format
like `0.0`, every scientific calculation in HC was silently wrong.

**If you plot anything, replace 0.6.9.3.** A period plotter that sets `set the
numberFormat to 0.0` inside its loop drew a rosette in twelve-pixel stairsteps
where HyperCard draws a smooth curve — and the drawing was only the visible
part.

### At a glance

Measured in HyperCard under Basilisk II, with the format set to `0.0`:

| | 0.6.9.3 | HyperCard, and now 0.6.9.4 |
| -- | -- | -- |
| `(0.34*1 = 0.34)` | `false` | `true` |
| `(1/3*3 = 1)` | `false` | `true` |
| `(1/3 = 0.3)` | `true` | `false` |
| `(sqrt(2)*1 = 1.4)` | `true` | `false` |
| `put sqrt(2) into x`, `(x = 1.4)` | `true` | `false` |
| `… then (10*x = 14)` | `true` | `false` |
| `put 1000*sin(pi/144)` | `0.0` | `21.8` |
| `put word 2 of msg` | error — box was write-only | the word |

---

### 1. How the model was found: compare, do not display

Six weeks of careful reasoning had built a model of HyperCard that was wrong
end to end — *"operators format their result"*. It survived because every
measurement used `put`, and **`put` shows the display and the calculation
mixed together.** There is no way to separate them that way.

The unlock was to stop displaying and start comparing. `true` is not a number,
so no format can touch it, and the arithmetic becomes visible on its own:

```hypertalk
set the numberFormat to "0.0"
put (1/3*3 = 1)        -- HyperCard: true.  HC 0.6.9.3: false
```

Four booleans settled in one evening what six weeks of deduction had got
backwards. Two of them demolished values that had been *written into this
project's own test harnesses* as if they had come from HyperCard.

**The rule, measured:** arithmetic keeps full precision. Storing into a
variable keeps full precision. The `numberFormat` applies **only when a number
becomes text for output** — display, concatenation. A quoted text literal is
never formatted.

### 2. What was actually broken

`HctValeur` carried only text. A calculation's result entered it *already
formatted*, and nothing downstream could recover the precision — the rest of
the calculation re-read that rounded text.

The cost in the plotter: `cos(t)` under format `0.0` could only ever be `0.0`,
`0.1`, `0.2` — **twenty-one distinct values for a whole circle**. Ten
consecutive points of the plotting chain returned the same number:

```
HyperCard  416.0 416.0 414.0 413.0 410.0 407.0 403.0 398.0 392.0 386.0
0.6.9.3    416.0 416.0 416.0 416.0 416.0 416.0 416.0 416.0 416.0 416.0
0.6.9.4    416.0 416.0 414.0 413.0 410.0 407.0 403.0 398.0 392.0 386.0
```

Worse than the drawing: **any** narrow-format scientific calculation was
wrong, with no sign of it. `1000*sin(pi/144)` returned `0.0`, because
`sin(pi/144)` had been flattened to `0.0` before the multiplication. A
thousand times zero is zero — that is not a rounding error, it is a lost
value.

### 3. The fix, and the six frontiers it took

`HctValeur.txt` is **exactly** what it was before — formatted — so no existing
reader of it can change behaviour. The un-rounded double travels beside it in
`.brut`, and only these paths look at it:

| path | where |
| -- | -- |
| function return values | `hct_val_fonction` |
| operator results | `hct_val_calcul` |
| **numeric literals in the script** | `feuille()`, `hct_eval.c` |
| function arguments | `nombre_de()`, `hct_eval.c` |
| comparison | `vals_egales` / `vals_compare` |
| accumulation operands | `nombre_de_val()`, `hct_exec.c` |
| storing into a variable | `ecrit_var_nombre`, both hosts |

**Each frontier nullified the previous fix.** Correcting function returns
without correcting operators achieved nothing; operators without storage,
nothing; and as long as `0.34` written in a script was merely *text*, a
comparison fell back to comparing rounded text and four booleans stayed wrong
while everything else was already right.

A precision fix that stops at one frontier is undone by the next. That is the
transferable finding of this release.

### 4. NaN ordering is untouched

`hct_compare` treats a NaN as **text**, because that is what a period stack
expects of its bounds test — established in 0.6.9.3, and a real stack proved
it. `vals_compare` therefore requires that *both* operands carry a number and
that neither is a NaN. Outside that, the old path exactly.

Reading the raw double on only one side would also have reintroduced the
asymmetry being hunted: `x = 1.4` would change meaning with the order of its
operands.

### 5. The message box can be read

`msg`, `message`, `the message box` were a **write-only** container. `put word
3 of msg` found nothing.

There is now a persistent buffer in the kernel (`hc_message_lu` /
`hc_message_ecrit`), a `lit_message` host callback, and `HCview.m` keeps the
buffer current as you type.

One consequence is worth knowing, and it is HyperCard's behaviour too: **from
the message box, no line can read anything but itself**, because the line you
typed *is* the box's content when the script reads it.

```
put word 1 of msg  ->  put
put word 2 of msg  ->  word
put word 3 of msg  ->  3
```

Each answer is word N of your own command. To read the box for real you need
two lines, so a script — and the `into` matters, or the `put` overwrites the
box before you can use what you read:

```hypertalk
put "one two three"
put word 2 of msg into got   -- "two"
```

`the visible of msg` is still *"object not found"*: the window belongs to the
host, only its contents belong to the kernel.

### 6. A non-finite coordinate draws nothing, and says nothing

**Measured:** in point mode the plotter clicks *before* its bounds test, so
with a `NAN(004)` in hand — and HyperCard opens no alert. It draws nothing and
moves on.

0.6.9.3 raised `HC_ERR`, which reached the dialog. On a lemniscate, a hundred
and forty-two times. Deduplicating identical lines made that bearable; it did
not make it faithful.

The refusal to draw was right; the **level of the report** was wrong. It is now
`HC_INFO`: the dialog stays silent as in HyperCard, and whoever watches the
monitor still sees the discarded points. Measured silence applies to the user,
not to someone hunting a defect.

### 7. A runaway loop is now visible

The executor caps every loop at ten million iterations, but all **three** exits
were plain `break`s: the loop stopped and the handler **carried on** as though
it had finished normally. A wrong result, silently, after minutes of freeze.
`v3_respire`'s own comment had promised the opposite since day one.

Caught along the way, by AddressSanitizer: `hct_ctx_faute` **keeps the
pointer**, it does not copy. Every caller happened to pass a literal, which
made the contract true by accident. The first one to compose its message
dynamically fell in. The contract is now written down.

### 8. Two foreign images in the iconset

`icon_16x16.png` and `icon_32x32.png` did not contain the bird — two
multicoloured squares, measured at 0 % neutral pixels where the other eight are
at 100 %. Invisible, since the shipped `.icns` has no such slots, but a
regeneration by `iconutil` would have let them in.

### 9. A chantier that cancelled itself

HC has two ways of signalling an error: `emit(HC_ERR)` + `return 1`, which does
**not** stop the handler (83 sites), and `hct_ctx_faute`, which does (13
sites). This asymmetry was written up as a defect to fix, on the strength of
one sentence — *"in HyperCard an error STOPS the handler"* — that had been
reasoned, never measured.

Measured, in three lines:

```hypertalk
put "1"
set the zorglub of this card to 1
put msg & "2"
```

HyperCard's box ends up containing `12`. **It opens the dialog and carries
on.** The asymmetry runs the other way from what had been claimed, HC's
majority behaviour is the faithful one, and a fifty-site refactor was cancelled
before a line of it was written.

### 10. Two false references removed from the tests

Both written by this project, both contradicted by measurement, and both
recorded rather than quietly corrected:

- **`1/3*3 -> 0.9`, annotated "HyperCard"** in `numfmt.c`. That was HC's own
  value, read one evening with the two windows side by side.
- **"under HyperCard it shows 1.188 and not 1.194"** in `emballee.c` and
  `sane.c`. The `1.188` came from the original bug report *about HC*.

Both have since been re-measured in Basilisk II: HyperCard gives `1.0` and
`1.194`. A third reading, `10*x -> 14.0`, was the last contradiction in the
file — it implied that storing freezes the formatting, which the booleans deny.
It was followed *against* the other two only after saying so in the harness;
re-measured, HyperCard gives `14.1`.

Three independent routes now agree that storing does not freeze: the six
booleans, the ten plotted points, and that reading itself.

A false reference carved into a test suite is worse than no reference. It looks
like a measurement, and it becomes the evidence for the next fix. The harnesses
keep the story of both mistakes, because they were made the same evening with
two windows side by side, and that is the only guard against a third.

---

### Test status

- C kernel: **234 checks, all green**, including under AddressSanitizer.
- Zero compiler warnings at `-Wall -Wextra`.
- New harnesses: `boitemsg.c` (twelve sections, including the self-overwrite
  trap and the contract with `messageBoxEntered:`) and `arret.c` (an inventory
  of the three genres of error signalling).
- `numfmt.c` now carries the measured model, the six booleans, the ten-point
  chain, and the A–E battery — each line with HyperCard's value beside ours.
- The Cocoa layer still has **no** automated tests — CI only proves that it
  compiles. `HCview.m` changed in this release, so the message box wants a
  human at an Xcode build.

### Known, and not fixed here

- `delete <any word>` always succeeds.
- `show card at <not a coordinate>` is accepted.
- The message box has no properties.
- `the multiple`, `the multiSpace`, and the `cicon` family remain open.
- `CFBundleVersion` is still hard-coded to `1`. It is the *build* number and it
  wants a rule, not a value — still true since 0.6.9.3.

### The method, since it is again the actual finding

0.6.9.3 recorded that a ten-second measurement beats a sound deduction. This
release adds the corollary: **while you display, you measure the display as
much as the calculation, and you cannot separate them.** Comparing instead of
displaying isolates the arithmetic — and it overturned, in four booleans, a
model that had been reasoned about for weeks and written into the tests as
fact.

### Changes in this release

- [#63](https://github.com/cleobuline/hc/pull/63) — runaway loops become
  visible; the `numberFormat` does not apply to accumulation commands; two
  foreign icons removed; the function-return discrepancy recorded (`5f62174`,
  `7accae4`, `8c75df1`, `1d5ff1c`, `40f1418`)
- [#64](https://github.com/cleobuline/hc/pull/64) — a non-finite coordinate
  draws nothing and raises no dialog (`5976de0`)
- [#65](https://github.com/cleobuline/hc/pull/65) — the message box can be
  read; the `numberFormat` rounds output only (`db84760`, `a8ecdb6`, `ccfa2a9`,
  `4e0fbc0`, `403be1f`, `454d6b0`, `ac6d6bf`)
- [#66](https://github.com/cleobuline/hc/pull/66) — the full word series of the
  message box, pinned (`b413f1a`)
- [#67](https://github.com/cleobuline/hc/pull/67) — the two pending
  measurements confirmed (`e080e50`)

**Full changelog:** [HC-0.6.9.3...HC-0.6.9.4](https://github.com/cleobuline/hc/compare/HC-0.6.9.3...HC-0.6.9.4)
