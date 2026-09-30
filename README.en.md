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

## Building and testing

```sh
xcodebuild -project HC.xcodeproj -target HC -configuration Release build   # the app, on a Mac
make test                                                                  # the kernel, anywhere
```

The kernel and interpreter are C99 and are also tested on Linux: more than
260 harnesses, checked against HyperCard running under Basilisk II.

## License

[MIT](LICENSE): free to use, modify and redistribute, provided the authors
are credited — **Patricia Benedetto** and **Claude** (Anthropic).
