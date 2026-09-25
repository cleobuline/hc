## HC-0.6.9.3 — SANE arithmetic

One subject only: what HyperCard returns when a calculation has no ordinary
answer. Eight values were read in HyperCard's own message box, running under
Basilisk II. None was deduced — and that matters, because **two deductions
that looked sound turned out to be wrong.**

**If you are running 0.6.9.2, replace it.** Its arithmetic is wrong in four
ways, and wrong silently: a script gets a plausible-looking value instead of
an error, or an error instead of a value.

### At a glance

| | 0.6.9.2 | HyperCard, and now 0.6.9.3 |
| -- | -- | -- |
| `ln(-1)` | `NAN` | `NAN(036)` |
| `sqrt(-1)` | `NAN` | `-NAN(001)` |
| `0*(1/0)` | `NAN` | `NAN(008)` |
| `7 div 0` | `INF` | error — *"can't div by zero"* |
| `0 div 0` | `NAN(004)` | error — *"can't div by zero"* |
| `5 mod 0` | `NAN` | error — *"can't mod by 0"* |
| `"NAN(037)" + 0` | `NAN(031)` | `NAN(037)` |

---

### 1. The codes of invalid domains

```
put ln(-1)     ->  NAN(036)
put sqrt(-1)   -> -NAN(001)      with the minus sign
put 0*(1/0)    ->  NAN(008)
```

C supplies none of these. On this machine `log(-1)`, `sqrt(-1)` and `0*INF`
all produce a NaN with a **zero payload**, so both the code and the sign have
to be set explicitly.

Guessing would have been wrong. `022` was the plausible value for the
logarithm; it is `036`.

A NaN's sign bit carries no arithmetic meaning, but SANE displays it and a
stack can compare it: `sqrt(-1)` carries a minus, `ln(-1)` does not.

`ln(0)` remains `-INF`. That is a limit, not an invalid domain, and C already
gives it.

### 2. Only `/` returns a value

```
put 1/0        ->  INF
divide n by 0  ->  INF
put 7 div 0    ->  dialog, "can't div by zero"
put 0 div 0    ->  dialog, "can't div by zero"
put 5 mod 0    ->  dialog, "can't mod by 0"
```

0.6.9.2 established that `/` by zero returns a value rather than raising an
error — measured on a real stack, and correct. It then **extended that rule to
`div` and `mod`**, with the noted uncertainty being about their SANE *code*,
not about the *nature* of the answer. The doubt was aimed at the number
instead of the shape, and it was wrong in both cases.

The consistency is legible once seen: `div` and `mod` are **integer**
operations, and the SANE arithmetic that produces `INF` and the NaNs is
floating-point. It does not apply to them, and HyperCard checks before
dividing.

The dividing line is therefore not between expression and command — `divide`
does follow `/` — but between floating-point and integer.

### 3. A silent corruption of NaN codes

`strtod` reads a NaN's payload with `strtoull` in base 0, so a leading zero
makes it **octal**. Four of the six codes HyperCard writes were being corrupted
on any round-trip through a string:

```
NAN(008) -> 0     NAN(009) -> 0
NAN(036) -> 30    NAN(037) -> 31
```

`NAN(037)` is hard-coded in HypoGraph 0.91 — `if y≠"NAN(037)"` — and because
the code travels in the payload, the corruption would have survived every
subsequent calculation with nothing to report it. An equality test against a
code would simply have answered false, and nothing would have said why.

Payloads are now read in decimal, as they are written. `hct_vers_nombre`
handles `INF` and `NAN` itself and hands only ordinary numbers to `strtod`.

### 4. The application now reports its own version

`MARKETING_VERSION` had been left at `0.6.5`, so 0.6.8, 0.6.9 and 0.6.9.1 all
announced "0.6.5" in the Finder and in the About box. Fixed in 0.6.9.2, but
after the DMG was built — so that build still says 0.6.5. This one says
0.6.9.3.

`CFBundleVersion` is still hard-coded to `1`. That is the *build* number, and
it needs a rule rather than a value.

---

### Test status

- C kernel: **230 checks, all green**, including under AddressSanitizer.
- Zero compiler warnings at `-Wall -Wextra`.
- `tests/harnais/sane.c` now has seventeen sections, **and each one states
  where its truth comes from** — measured under HyperCard, or reasoned from
  the code. A harness that shows a result without saying which is a harness
  that freezes a guess.
- The Cocoa layer still has **no** automated tests — CI only proves that it
  compiles.

Nothing in this family is left unmeasured. The `0/0` case of the `divide`
command was not read separately and does not need to be: it goes through the
same line of code as the operator, so the two cannot diverge. That is an
argument from structure, not an assumption — a distinction that cost enough
today to be worth writing down.

### The method, since it is the actual finding

Every fix in this release came from typing one line into HyperCard's message
box and reading the answer. Each took about ten seconds. Together they
corrected what several hours of careful reasoning had got wrong twice.

When reconstructing a system you did not write, a ten-second measurement beats
a sound deduction. Period stacks and a working emulator remain the most useful
tools this project has, and neither of them is in the repository.

### Changes in this release

- [#59](https://github.com/cleobuline/hc/pull/59) — SANE codes for invalid
  domains and their sign; `div` and `mod` by zero raise errors; NaN payloads
  read in decimal (`9a1af6d`, `781b2fb`, `3951d74`, `57b8ed0`)

**Full changelog:** [HC-0.6.9.2...HC-0.6.9.3](https://github.com/cleobuline/hc/compare/HC-0.6.9.2...HC-0.6.9.3)
