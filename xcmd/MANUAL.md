# Your first XCMD for HC, step by step

*[Version française](MANUEL.md)*

From installing the compiler to a "Hello world" that shows up in your
stack. Allow half an hour the first time, most of it waiting for a
download.

Every command in this manual was run exactly as written before being
written down.

## What an XCMD is in HC

In HyperCard, an XCMD (a command) or an XFCN (a function) was a small
program written in C or Pascal, kept inside the stack, that scripts called
like a HyperTalk word.

HC does the same, with one difference: the program is compiled to
**WebAssembly**. You get a `.wasm` file, which you add to the stack and
which travels with it. It runs **sandboxed**: it cannot touch anything on
your Mac, it can only talk to HC. So you can safely open a stack someone
shares with you, modules included.

## What you need

- a Mac running macOS 10.15 (Catalina) or later;
- enough bandwidth to download a few hundred megabytes;
- the HC folder, the one that contains the `xcmd` folder. The DMG does
  not hold it — only the application, stacks and this manual —: the end of
  step 2 tells you how to get it.

In this manual, the HC folder is called `~/hc` (the `~` means "my home
folder"). If yours is somewhere else, replace `~/hc` with its path. Tip:
dragging a folder from the Finder into the Terminal types its path.

## Step 1 — Open the Terminal

Applications ▸ Utilities ▸ **Terminal**. A window opens, with a prompt
waiting for your commands.

For each command in this manual: copy it, paste it into the Terminal (⌘V),
then press Return. Wait for the prompt to come back before going on to the
next one.

## Step 2 — Apple's developer tools

They provide `git` and `make`. If Xcode is already installed, you most
likely have them. Type:

```
xcode-select --install
```

A window offers to install them: accept. If the Terminal answers that they
are already installed, that's perfect, go on.

If you don't have the HC folder yet, `git` brings it:

```
cd ~
git clone https://github.com/cleobuline/hc.git
```

It lands in your home folder, under the name `hc`: that is this manual's
`~/hc`.

## Step 3 — Install the WebAssembly compiler

Apple's `clang` compiler, the one that comes with Xcode, cannot produce
WebAssembly. So we install the one from the **emsdk** project, which can.
It even runs on Catalina: this was measured on macOS 10.15.8.

Four commands, one after the other:

```
cd ~
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
```

The last one downloads the compiler: allow several minutes. Then:

```
./emsdk activate latest
```

Check that the compiler is there, and that it knows WebAssembly:

```
~/emsdk/upstream/bin/clang --print-targets | grep wasm32
```

The Terminal should answer with a line like:

```
    wasm32      - WebAssembly 32-bit
```

This is done once and for all. From now on the compiler is
`~/emsdk/upstream/bin/clang`, and that is the path we will give it.

## Step 4 — Write "Hello world"

The program is six lines long. The safest way to create the file is to
paste this **whole** block into the Terminal:

```
cat > ~/hc/xcmd/exemples/hello.c <<'FIN'
#include "hc_xcmd.h"

HC_XCMD(hello)
{
    SendCardMessage("put \"Hello world !\"");
}
FIN
```

What these lines say:

- `#include "hc_xcmd.h"`: we use HC's kit;
- `HC_XCMD(hello)`: we declare a command named `hello`; that is the word
  scripts will use;
- `SendCardMessage(…)`: we have HC run a line of HyperTalk, here
  `put "Hello world !"`, which writes into the message box. It is the same
  callback the XCMDs of 1990 used.

(`FIN` is French for "end": it only marks where the file stops.)

Prefer an editor? TextEdit will do, on one condition: switch the document
to plain text (Format ▸ Make Plain Text), otherwise the compiler cannot
read the file. Save it as `hello.c`, in HC's `xcmd/exemples` folder.

## Step 5 — Compile it

```
cd ~/hc/xcmd
make CLANG=~/emsdk/upstream/bin/clang exemples/hello.wasm
```

The Terminal shows the command it runs, a line that starts with the
compiler's path and ends with `hc_xcmd.c`. If nothing else appears, it
worked: the module is born, next to its source.

```
exemples/hello.wasm
```

## Step 6 — Add the module to your stack

In HC, open your stack, then **Objects ▸ Modules…**. Click **Add…** and
choose `hello.wasm`, in HC's `xcmd/exemples` folder.

Quicker: drag `hello.wasm` from the Finder onto the stack's window.

The Modules panel then shows the module, `hello`, and what it brings — its
size depends on the compiler's version:

```
635 bytes, 1 external:

XCMD hello
```

## Step 7 — Try it

Open the message box (⌘M, or Tools ▸ Message), type `hello`, then
Return. It answers:

```
Hello world !
```

And from a script, like any other command, for instance in a button:

```
on mouseUp
  hello
end mouseUp
```

## Step 8 — Save the stack

The module is kept **inside the stack**, but the stack is only written to
disk when you save it: **File ▸ Save a Copy…** (⌘S). Despite its name, this
item saves the stack itself, wherever you choose. The module goes with it:
whoever opens the stack elsewhere has the module, with nothing to install.

## A little further: a parameter, and a function

An XCMD receives the parameters the script passes to it, and an XFCN
returns a value. Paste this block into the Terminal:

```
cat > ~/hc/xcmd/exemples/salut.c <<'FIN'
#include <stdio.h>
#include <stdlib.h>
#include "hc_xcmd.h"

HC_XCMD(salut)
{
    char ligne[200];
    const char *qui = paramPtr->paramCount > 0 ? paramPtr->params[0]
                                               : "tout le monde";
    snprintf(ligne, sizeof ligne, "put \"Salut, %s !\"", qui);
    SendCardMessage(ligne);
}

HC_XFCN(double)
{
    static char resultat[20];
    int n = paramPtr->paramCount > 0 ? atoi(paramPtr->params[0]) : 0;
    snprintf(resultat, sizeof resultat, "%d", 2 * n);
    paramPtr->returnValue = resultat;
}
FIN
```

(*Salut* is French for "hi", *tout le monde* for "everybody". The names
are yours to change: the command is called by the word inside
`HC_XCMD(…)`.)

Then compile:

```
cd ~/hc/xcmd
make CLANG=~/emsdk/upstream/bin/clang exemples/salut.wasm
```

Add `salut.wasm` to the stack as in step 6, and in the message box:

| you type | it answers |
| --- | --- |
| `salut "Patricia"` | `Salut, Patricia !` |
| `salut` | `Salut, tout le monde !` |
| `put double(21)` | `42` |

`paramPtr` is HyperCard's parameter block:

- `paramCount`: how many parameters;
- `params[0]`, `params[1]`…: the parameters themselves;
- `returnValue`: what a function returns.

They are ordinary C strings: no more Handles, no more Pascal strings.

One file can hold several commands and functions, like `salut` and
`double` here.

**After every change to the source**, compile again, then add the `.wasm`
again: it replaces the one with the same name.

## When it doesn't work

HC's own messages are in French; the table quotes them as they appear.

| what you see | what it means |
| --- | --- |
| `git` or `make` not found ("command not found"), or a window offering to install tools | the tools from step 2 are missing: accept, or do step 2 again |
| "no such file or directory" when typing `./emsdk …` | you are not in the `emsdk` folder: type `cd ~/emsdk`, then try again |
| a message about `wasm32` and a "target" | `make` used Apple's clang: `CLANG=~/emsdk/upstream/bin/clang` is missing |
| `No rule to make target` | the `.c` file is not in `xcmd/exemples`, or its name is different |
| `error:` followed by a line of your program | a mistake in the source: the compiler says which one, and on which line |
| `warning:` | a warning, not an error: the module is built anyway, but read it |
| HC answers `personne ne répond à « hello »` ("nobody answers `hello`") | the module is not in **this** stack: each stack has its own modules |
| HC refuses the module: `le module demande « … », que HC ne fournit pas` ("the module asks for `…`, which HC does not provide") | the program calls a function HC does not know; use only those of the kit |
| the reopened stack no longer has the module | it was not saved after adding it (step 8) |
| a module that runs forever | ⌘. stops it, as it stops a script |

## Going further

- `LISEZMOI.md`, in this same folder (in French): everything the kit
  offers, with HyperCard's names. Reading and writing fields and global
  variables, evaluating an expression, opening a window, showing a help
  balloon…
- `exemples/bonjour.c`: a command and a function with the same name.
- `exemples/ballons.c` and `exemples/fulldrag.c`: two externals of the
  time, remade. They are already built in `tests/donnees/modules/`: any
  stack can use them.
- `maccam/`: the externals of MAC•CAM Workshop, a stack from 1990 that runs
  in HC without a line of its scripts changed.
