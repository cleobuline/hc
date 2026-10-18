# HC

*[Version française](README.md)*

**HC is HyperCard for today's Mac.**

- **HyperTalk-compatible.** HC opens original HyperCard stacks (Apple's
  binary format, 1987–1998) and runs their scripts **as they are**, with no
  conversion or rewriting. On a corpus of real stacks, two of them Apple's,
  99.7 % of scripts are accepted. Known differences are listed in the
  [release notes](docs/releases/).
- **A superset of HyperCard, in colour.** Everything HyperCard does, plus
  colour: paint, icons, and objects (`backColor`, `foreColor`, `textColor`).
- **XCMDs and XFCNs, sandboxed.** As in HyperCard, a stack extends itself
  with commands and functions written in C; HC compiles them to
  WebAssembly, and they run without being able to touch the Mac. From the
  compiler to a first "Hello world": [`xcmd/MANUAL.md`](xcmd/MANUAL.md).

Native macOS app, universal binary, macOS 10.13 and later.

https://github.com/user-attachments/assets/3f2150d7-665b-4b79-8978-b6f2b3017958

*The demo pinball, written in plain HyperTalk with polygon buttons and
`the keysDown`. It ships in the DMG. From 0.7.9 on, every obstacle carries
its own script, so you add one by copying it.*

## Installing

Download the DMG from the latest [release](https://github.com/cleobuline/hc/releases)
and copy `HC.app` to `/Applications`. The app is  notarised; 

 
## Building

On a Mac, with Xcode:

```sh
xcodebuild -project HC.xcodeproj -target HC -configuration Release build
```

## Testing on Linux

The kernel and interpreter are C99: more than 260 harnesses, checked against
HyperCard running under Basilisk II, run without a Mac. On Ubuntu or Debian:

```sh
sudo apt install build-essential zlib1g-dev git
git clone https://github.com/cleobuline/hc.git
cd hc
make test          # the regression suite
make test-asan     # the same under AddressSanitizer, UBSan and LeakSanitizer
```

`zlib1g-dev` is required: `build-essential` does not provide `zlib.h`.
Details in [`tests/LISEZMOI.md`](tests/LISEZMOI.md) (in French).

## Provenance and inspiration

**How HC was made.** HC is written by Patricia Benedetto with Claude
(Anthropic) as co-author, since July 2026, as the [LICENSE](LICENSE) says.
Apple never released HyperCard's source code, so there was nothing to copy:
HC is a clean-room reimplementation, built from what HyperCard visibly does.
Its behaviour was measured detail by detail in HyperCard 2.4.1 running in the
Basilisk II emulator. The measurements, and what is still unmeasured, are
written down in [`docs/mesures/`](docs/mesures/) (in French), and since the
first weeks the commit messages name the cause, the symptom and the fix.

**Apple's stack format** was read from two public, independent descriptions:
`StackFormat.md` from HyperCardPreview (Pierre Lorenzi) and `stakread.py`
from mystextract (Andrew Plotkin), checked against each other and against
real stacks. Neither project carries a license, so no code was taken from
them: offsets are facts about a format, and the code is HC's own. The picture
compression, “Wrath of Bill Atkinson”, was reverse-engineered and named by
Rebecca Bettencourt.

**Third-party code.** One library only: WAMR, the WebAssembly Micro Runtime
of the Bytecode Alliance, under Apache 2.0 with LLVM exceptions. It lives in
[`HC/tiers/wamr`](HC/tiers/wamr) with its license and attributions, which
also ship inside the app.

**Not included.** No Apple code, no Apple headers, no Apple stacks.
HyperCard and HyperTalk are trademarks of Apple Inc.; HC is not affiliated
with or endorsed by Apple.

**Inspiration.** HyperCard, by Bill Atkinson (1987), and HyperTalk, by Dan
Winkler. Where HC goes beyond HyperCard (colour, polygon buttons,
`the keysDown`, field colours), the words are those of LiveCode and
SuperCard, so that nothing is invented and no HyperCard stack changes
behaviour. The XCMD and XFCN work was judged on MAC•CAM Workshop,
Patricia's own stack from 1990, a cellular automata lab modelled on the
CAM-6 of Tommaso Toffoli and Norman Margolus (*Cellular Automata Machines*,
MIT Press, 1987). It runs in HC without a single script line changed.

## License

[MIT](LICENSE): free to use, modify and redistribute, provided the authors
are credited — **Patricia Benedetto** and **Claude** (Anthropic).
