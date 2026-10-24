# Claude 68

**Talk to Claude from a Macintosh built before the web existed.**

*For the ones who see things differently.*

Claude 68 is a native System 7 app for 68k Macs. It streams Claude's replies word by word into a styled TextEdit window, over real TLS to `api.anthropic.com`. There's no proxy, no modern helper machine and no stunnel box hiding in a closet: the 16 MHz 68030 does the cryptography itself.

It's also a **boilerplate**, and that's the main reason it exists. Apart from one config file and one API file, everything here is generic infrastructure. Swap those two and you have an HTTPS and SSE streaming client for a different API, running on a 1989 Mac.

When I first asked Claude to help me get a compact Mac onto HTTPS, it told me to use a proxy. This is a Claude client built without one.

> **See its sibling run in your browser at [classic.slkk.nl](https://classic.slkk.nl).** Claude 68 uses the same engine but isn't in the demo, because every request costs API tokens. [Download it](#download) and bring your own key. The backstory is in [the story](https://github.com/SLKK-design/NotionClassic/blob/main/docs/the-story.md).

---

## How fast

A modern TLS handshake was never meant to run on a 1989 CPU. Stock mbedTLS needs 49 seconds of math for one handshake on an SE/30, and Cloudflare hangs up after about 15. With this engine's curve code (micro-ecc, 68020 assembly, a custom X25519) and session tickets, measured on real hardware:

| Machine | Cold handshake | Resumed handshake |
|---|---|---|
| Mac SE/30 (68030/16, 32-bit bus) | 6.9 s | 0.93 s |
| Mac Classic II (68030/16, 16-bit bus) | 11.0 s | 0.87 s |

Cold means no session ticket, with the certificate check cached. The very first connection also checks the certificate and takes about 8–11 s on the SE/30 and 13–14 s on the Classic II. The session ticket is saved to disk, survives a restart and is renewed while the app sits idle, so day to day every connection is the resumed one. How it got there, with every step measured, is in [HTTPS on a 37-Year-Old Mac](https://github.com/SLKK-design/HTTPS-68K-Speedtest/blob/main/docs/modern-HTTPS-on-68k.md); the logs, the tests for this engine and the timing app are in [HTTPS-68K-Speedtest](https://github.com/SLKK-design/HTTPS-68K-Speedtest).

## Features

- **Real TLS 1.2 over MacTCP**, directly to the Anthropic API
- **SSE streaming**: tokens appear as they're generated
- **Cancel mid-stream** with Esc or Cmd-.
- **Styled transcript**: bold user turns, plain assistant turns, scrollable
- **Conversation memory**: the recent transcript is sent as context with each message
- **Settings dialog**: the API key is stored in a prefs file in the Preferences folder
- **Debug console**: a live, timestamped log, toggled from the menu. It shares the chat window, so Claude gets to read it too, and will kindly offer to help you debug it.
- **Low-memory safety net**: a 32 KB grow-zone reserve absorbs heap pressure and re-arms itself

## What it runs on

| | |
|---|---|
| CPU | 68020 or later. Tested on the SE/30, the Classic II and the Macintosh mini (a Pi Zero running Basilisk II). |
| App partition | 2 MB preferred, 1.5 MB minimum |
| System RAM | 8 MB or more. MacTCP needs system heap, and heavy extension sets want 10 MB. |
| OS | System 7.5 to 8.1 (8.1 tested on PPC in SheepShaver, through Open Transport's MacTCP compatibility layer) |
| Network | Any MacTCP-compatible Ethernet card or adapter, or a SCSI network card emulated by [PiSCSI](https://github.com/PiSCSI/piscsi) or [BlueSCSI](https://bluescsi.com/) |

On a 68k Mac with Open Transport 1.3, run [Open Transport Bug Patcher](https://github.com/SLKK-design/HTTPS-68K-Speedtest#open-transport-bug-patcher) once. Anthropic is behind Cloudflare, and Open Transport 1.3 mishandles Cloudflare's TCP window scale, so without the patch every connection waits about 3 seconds.

## Download

Get [Claude68.sit](https://github.com/SLKK-design/Claude68/releases/latest/download/Claude68.sit) (StuffIt, for the Mac itself) or [Claude68.zip](https://github.com/SLKK-design/Claude68/releases/latest/download/Claude68.zip) (for a modern Mac, which keeps the resource fork when it unzips) from the [latest release](https://github.com/SLKK-design/Claude68/releases/latest), and copy the app to the Mac. Launch Claude 68, paste your Anthropic API key into Settings, and say hello.

## Building

You need [Retro68](https://github.com/autc04/Retro68). mbedTLS for 68k and MacTCPHelper come pre-built in `src/core/lib/`.

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<Retro68-build>/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make -C build
```

The app ends up in `build/` as `Claude 68.APPL`, with its resource fork, and as `Claude68.bin` (MacBinary).

Don't trust emulator timings. Basilisk II finishes a handshake in 0.2 s, and a real 68030 needs 7 to 11.

## Using it as a boilerplate

The hard part was a full TLS handshake on a 68k Mac that finishes inside Cloudflare's timeout. I solved it first for [Notion Classic](https://github.com/SLKK-design/NotionClassic). After that, nothing tied the engine to one API, so I packaged it here as a starting point. Connecting a different API takes three edits:

| Edit | What it controls |
|---|---|
| `src/config.h` | App name, hostname, endpoint, model, headers, system prompt, UI strings. Every knob is in one file. |
| `src/api/` | `claude_client.cpp` builds the request and parses Anthropic's SSE events. `api_template.cpp` is a commented skeleton for your API. |
| `CMakeLists.txt` | The `project()` name near the top |

If your server isn't behind the Google Trust Services chain that Cloudflare uses, also replace both certificates in `src/core/net/RootCert.cpp`: the root and the intermediate that signs your server's certificate. `config.h` has the `openssl` commands to get them.

Give your app its own four-character Finder creator code too. Claude 68 uses `Cl68`, set in `src/wdefshell.r` (twice) and `CMakeLists.txt`. Two apps with the same code confuse the Finder, which can then show one app's icon on the other.

```
src/
  config.h       all customization knobs
  main.cpp       event loop, stream cancel, idle ticket refresh
  api/           swappable API client (+ template)
  ui/            chat window, settings, menus, debug console
  core/          generic engine: HTTP/TLS/SSE, prefs, allocator, grow-zone
  core/lib/      pre-built 68k mbedTLS + MacTCPHelper
scripts/
  set_finder_flags.py  sets the Finder flags after each build, so the icon shows
  rebuild_mbedtls.sh   rebuilds the pre-built mbedTLS after a config change
```

## Sibling projects

The same `src/core` engine powers **[Notion Classic](https://github.com/SLKK-design/NotionClassic)**, which works it much harder than a chat client does: Notion pages read, edited and written back from a 9-inch CRT, with rich text, toggles, checkboxes, emoji and page icons. Claude 68 is the minimal case, and Notion Classic shows the engine holds up in a real, stateful app.

The engine also brought back **[MacPlayer](https://github.com/SLKK-design/MacPlayer)**, Anthony Super's 2018 Spotify remote for the SE/30. Its login and album covers went through his web service at 68k.io, which no longer exists, so it had stopped working. On this engine the Mac talks to Spotify itself, and decodes and dithers the album covers on its own.

## A note on security

The connection is encrypted. The Mac is not protected. Classic Mac OS has no memory protection and no file permissions, so anything running on the machine can read the prefs file with your API key. Use a dedicated key you can revoke, with a spending limit, and treat this as the toy it is.

## Credits

Built by **Lin van der Slikke** with [Claude Code](https://claude.ai/code), which wrote most of the code, including the 68k assembly, and worked through the papers with me. I ran every build on the real machines, read the tick logs and decided what stayed. mbedTLS runs the protocol and the cipher. The custom parts (the assembly, an X25519 rewrite, a faster signature check and the trust pin) are listed in the [deep-dive](https://github.com/SLKK-design/HTTPS-68K-Speedtest/blob/main/docs/modern-HTTPS-on-68k.md).

Standing on:

- [MacHTTP](https://github.com/antscode/MacHTTP), [mbedtls-Mac-68k](https://github.com/antscode/mbedtls-Mac-68k) and [MacTCPHelper](https://github.com/antscode/MacTCPHelper) by Anthony Super (antscode). MacHTTP is where the HTTP client started, and his Spotify remote for the SE/30 is where this whole project started. Thank you, Anthony.
- [Retro68](https://github.com/autc04/Retro68) by Wolfgang Thaller, the 68k GCC toolchain
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) (ARM/Mbed, Apache 2.0)
- [micro-ecc](https://github.com/kmackay/micro-ecc) (Kenneth MacKay, BSD 2-clause)
- [curve25519-donna](https://github.com/agl/curve25519-donna) (Adam Langley / Google, BSD 3-clause)
- [http-parser](https://github.com/nodejs/http-parser) (Joyent / Node.js contributors, MIT), [yuarel](https://github.com/jackenl/yuarel) (Jack Engqvist Johansson, MIT), [gason](https://github.com/vivkin/gason) (Ivan Vashchaev, MIT)
- [Rivain (2011), *Fast and Regular Algorithms for Scalar Multiplication over Elliptic Curves*](https://eprint.iacr.org/2011/338.pdf), the basis of the ECDSA speed-up
- [PiSCSI](https://github.com/PiSCSI/piscsi) and [BlueSCSI](https://bluescsi.com/), for the SCSI network card emulation that puts the real machines online

## License

My code is released under [0BSD](LICENSE). Take it, change it, ship it; no credit needed. The bundled libraries keep their own licenses (see [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES)).

---

*With a nod to a certain 1997 Apple ad: here's to the round pegs in the square holes, the ones daft enough to think a Mac from 1989 should stream from a frontier model. [It does now.](https://github.com/SLKK-design/Claude68/releases)*
