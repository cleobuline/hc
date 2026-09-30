## HC-0.7.3 — Cmd-period stops a script

One subject: **you can now stop a running script.** Press **Cmd-.** and HC
stops the script, along with every handler that called it.

Until now nothing could stop a script. A `repeat forever` ran until the
ten-million-iteration cap, or until you quit the app — losing whatever had not
been saved.

The project also gets a README, in French and English, and a licence: MIT.

### At a glance

| | 0.7.2 | 0.7.3 |
| -- | -- | -- |
| stop a running script | **not possible** | **Cmd-.** |
| `repeat forever`, then Cmd-. | runs to the 10-million cap | stops at once |
| the handler that *called* the loop | — | stops too |
| `put total() into field "Sum"`, `total` stopped mid-way | — | the field is **left intact** |
| `wait 20 seconds`, then Cmd-. | waits 20 seconds | returns at once |
| error dialog on Cmd-. | — | none |
| README / licence | none | French and English / **MIT** |
| `the hcVersion`, "About HC" | `0.7.2` | `0.7.3` |

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

**The binary is universal** — `x86_64 arm64`. Deployment target is macOS 10.13.

---

### 1. Cmd-period

*"We need a Cmd-. — it's essential."*

**What it does.** Cmd-. stops the running handler **and every handler that
called it**, the whole chain, without an error dialog. A real error that
happened *before* the stop is still reported. Loops and `wait` stop at once.
Typed in the message box, `repeat forever` stops too.

**A stopped function leaves no damage.** A HyperTalk function stopped mid-way
returns empty. Without care, `put total() into field "Sum"` would have
finished its line with that empty value and **wiped the field**. The line that
called the function does not run, and the field stays as it was.

**Why there was none.** There were two missing pieces:

- **The kernel** had no way to stop a script at all.
- **The key never arrived.** During a script, HC keeps the window alive, but
  nothing takes keystrokes out of the queue: they wait for the script to end.
  Cmd-. waited for the end of the very script it was meant to stop.

**Now:**

- **Kernel:** each handler in the chain checks for the request before its next
  line, and every loop and `wait` checks too. The request clears once nothing
  is running. Pressed when no script runs, Cmd-. does nothing, and it cannot
  stop the *next* script.
- **App:** while a script runs, HC looks for Cmd-. in the event queue up to
  sixty times a second. The other keystrokes are put back, in order, and
  handled after the script as before. Cmd-. is recognised by its character,
  so it works on an AZERTY keyboard, where the period needs Shift.

Tried in HC by its author: *"it works very well."*

### 2. A README and a licence

The repository had neither. `README.md` (French) and `README.en.md` (English)
now say in a few lines what HC is: HyperCard's scripts run as they are, 99.7 %
of a corpus of real stacks, and HC adds colour — paint, icons, and
`backColor` / `foreColor` / `textColor` on objects.

The licence is **MIT**: free to use, modify and redistribute, provided the
authors are credited — **Patricia Benedetto** and **Claude** (Anthropic).

### 3. What is not measured is written down as not measured

- **What HyperCard does on Cmd-.** is not measured: a dialog, a beep, or
  nothing? Do the callers stop too? HC assumes everything stops, silently. To
  play **in HyperCard**:
  ```
  on mouseUp
    put "before" into msg
    repeat forever
    end repeat
    put "after" into msg
  end mouseUp
  ```
- The key check in the app is compiled on macOS and was tried by hand; it has
  no automated test.
- A script that neither loops nor waits cannot be stopped: it finishes on its
  own.

---

### Test status

- C kernel: **261 checks, all green**, including under AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer. One new one: `cmdpoint`,
  fourteen cases — loops with and without a body, callers, a stopped function,
  `wait`, the message box, no dialog, back to rest.
- **Zero warnings**, gcc at three optimisation levels *and* clang with Xcode's
  families.
- The Cocoa layer still has **no** automated tests — CI only proves it
  compiles.

### Known, and not fixed here

- The stack property **`cantAbort`**, which forbids Cmd-. in HyperCard, does
  not exist.
- Keystrokes typed during a script are put back at the head of the queue, so
  they now come before a click that preceded them.
- After a fault raised **by a command** (`hide`, `set`, a message nobody
  handles), the handler **carries on** and the dialog comes at the end.
  HyperCard probably stops on the spot — to play in HyperCard.
- A fault inside a user function stops the function but not its caller.
- `visual effect` followed by `go`, **without** `lock screen`, still plays
  after the script rather than inside `go`.
- Still standing from 0.7: named windows, patterns, resource-fork icons,
  HyperCard 1.x, private-access stacks, the proportional period fonts.
  `CFBundleVersion` is still hard-coded to `1`. Not notarised; see
  "Installing".

### Changes in this release

- [#83](https://github.com/cleobuline/hc/pull/83) — Cmd-. stops the running
  script and every handler that called it (`6bc0fcc`)
- [#84](https://github.com/cleobuline/hc/pull/84) — README in French and
  English, MIT licence (`c90c474`, `18f3b09`)
- this release — version 0.7.3 in all three places, and this note

**Full changelog:** [HC-0.7.2...HC-0.7.3](https://github.com/cleobuline/hc/compare/HC-0.7.2...HC-0.7.3)
