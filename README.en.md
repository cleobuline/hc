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

Native macOS app, universal binary, macOS 10.13 and later.

## Installing

Download the DMG from the latest [release](https://github.com/cleobuline/hc/releases)
and copy `HC.app` to `/Applications`. The app is not notarised; once:

```sh
xattr -dr com.apple.quarantine /Applications/HC.app
```

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

## License

[MIT](LICENSE): free to use, modify and redistribute, provided the authors
are credited — **Patricia Benedetto** and **Claude** (Anthropic).
