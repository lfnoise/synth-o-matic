# Synth-O-Matic for macOS

A port of Synth-O-Matic (James McCartney, 1990–1992), originally written in THINK C for the 68k Macintosh, to current macOS. The original sources are in `../SOM-classic`.

You write a short program of unit generators, set its parameters and wavetables, calculate it into a sound, and play it from the keyboard.

## Building

You need Xcode 15 or later (it was built with Xcode 26, Swift 6.2).

```sh
scripts/build_app.sh            # release build → build/Synth-O-Matic.app
scripts/build_app.sh debug
swift test                      # engine tests, run against the original patches
swift build --product somrender # the command line renderer
```

`build_app.sh` copies the original example patches into the app; they're under **File ▸ Open Example**. You can also open `Package.swift` in Xcode.

## Layout

| Path | What it is |
|---|---|
| `Sources/SOMEngine` | The synthesis engine in C: compiler, interpreter, unit generators, file format, export |
| `Sources/SOMEngine/ugens` | The original unit generator files. They are produced by `scripts/port_ugens.py`, and every change to them is marked `port:` |
| `grammar/som_grammar.y` | The original grammar (`harp9.y`). Regenerate the parser with the `bison` command in its header |
| `Sources/SynthOMatic` | The AppKit application |
| `Sources/somrender` | Command line renderer: `somrender patch` writes `patch.wav` |
| `Tests/SOMEngineTests` | Engine tests and the original patches they run |

## How the port is built

**The engine is the original code.** The original kept all of its state in globals. That state is now held per document in a `SOMContext`, and the original global names (`vars`, `gSampleRate`, `cps2inc`, …) are macros that refer to the current thread's context. Because of that, the unit generators, compiler and interpreter read almost exactly as they were written, and patches sound as they did. Other changes:
- Numbers are still single precision floats, as in the original, so `.01` seconds is 440 samples at 44.1 kHz.
- Mac Toolbox handles became plain pointers.
- The parser is regenerated from the original grammar, keeping its 43 shift/reduce conflicts and how they resolve.

**Files.** Patches use the original `SOM1` format: big endian, 2 byte ints, and 96-bit 68881 extended doubles. The app reads all three versions: before 50, 50, and 70. It writes version 70, so the original application can read files the port saves. Classic files have no extension; the app recognises them by their contents. New patches are saved as `.som`.

**The application** reproduces the original's three windows (Program, Variables and Message) as panes of one document window. It keeps the original's table and sample windows, the Fourier/Chebyshev harmonic editor, every dialog, and the spirograph About box.

## Additions

- **Program editor:**
  - syntax coloring, with variables colored by signal type
  - line numbers, with the line of a compile error marked
  - signature help for the unit generator under the cursor
  - completion (⌥⎋)
- **Variable list:** live sliders for parameters, waveform thumbnails, random set checkboxes, and multi-level undo for every edit.
- **Wavetable editor:** freehand drawing, as well as the original's click-to-click line drawing (hold ⇧ or choose Lines).
- **Sample window:** zoom, scroll and a play head.
- **Export:** AIFF and Sound Designer files, as the original wrote, plus WAV in 16 bit, 24 bit and float. Two samples can be exported as one stereo file (e.g. `outL`/`outR` from `pan`).
- **File ▸ Import Sound into Sample:** loads a sound file into a sample variable. A sample that the program reads but never assigns now plays its contents, so the vocoder patches work. The original always read zeros there.
- **Quick Reference** (⌘?): the original guides, completed with the unit generators they never listed.
- Dark mode, document tabs, and `somrender` for batch rendering.

## Keyboard

The original's command keys are kept where they don't collide with ones macOS needs:

- **Synth:** Compile ⌘K, Calculate ⌘/, Make Random Set ⌘5
- **Signals:** Set Globals ⌘G, Set Type ⌘1–⌘4
- **Fill:** ⌘F, ⌘T, ⌘Y
- **Alter:** ⌘R, ⌘-, ⌘=, ⌘', ⌘[, ⌘], ⌘L, ⌘;, ⌘U, ⌘\
- **Export:** ⌘E

Some moved:

| Command | Original | Now |
|---|---|---|
| Absolute Value | ⌘A | ⌥⌘A |
| Hanning Window | ⌘H | ⌥⌘H |
| Parzen Window | ⌘P | ⌥⌘P |
| Normalize | ⌘M | ⌥⌘M |
| Randomize | ⌘` | ⇧⌘R |
| Find | — | ⌥⌘F (⌘F stays Fourier Fill) |

Calculate & Play is ⌘↩, and ⌘. stops a calculation or the sound.

As in the original, the letter keys play the selected sample transposed when the variable list or a sample window has focus. Each row of keys is an octave, and **a** is the original pitch. Space plays it untransposed.

## Bugs fixed in the original

Each fix is commented `port:` in the source.

- `a -= b`, `a /= b` and `a ^= b` computed `b - a`, `b / a` and `b ^ a`. No original patch uses them.
- `oscil1` read through an uninitialized pointer instead of its table.
- After `oscil1` finished, and at the end of an envelope, the rest of the block was left with stale data from a reused buffer.
- Reading `tbl[i]` from a table that the program only assigns elements of read through a NULL pointer.
- `linseg` and `linseg0` were documented, and used by the *flute* patch, but missing from the function table.
- `p10`–`p49` got the wrong p-field number.
- A parse error could leave a half-compiled program that would then run.
- A function with assigned arguments (`pan`, `panx`) marked the wrong variable as assigned.
- Zero-length delay lines, all-zero tables in the fills, and flat tables in auto scaling are now guarded.
- The unimplemented unit generators (`buzz`, `pluck`, `pitchshift`, `vocoder`, `envfollow`, `pchfollow`) left garbage in their output; they now produce silence.

The language's other quirks are kept:
- `?:` binds more tightly than the comparison operators, so write `(a > b) ? c : d`.
- `if` decides once, when the calculation starts.
- A program that computes only scalars runs for one block.

## Not carried over

- The disk-based mode for long sounds, and `soundin`. The original never exposed either in its menus.
- The Sound Folder and Instrument Folder settings, which only that mode used. The Instrument I/O file names are still kept in the patch.
- Two of the example patches won't compile. `bwowowo.pch` (`atkdecay`) and `grains.pch` (4-argument `grains`) come from earlier versions, and the final 1992 application rejects them too.
- Samples are not saved in patches, as in the original. Export them, or re-import them.

## Testing

- `swift test`: 22 engine tests. They compile and calculate every original patch, round-trip every file, and check the language, the unit generators, the fills and alters, and export.
- `open build/Synth-O-Matic.app --args --selftest`: 57 checks of the application layer, printed to the console. They cover:
  - editing and undo
  - every Alter command
  - Fourier analysis
  - copying and pasting variables
  - the random set
  - saving and reopening
  - exporting and importing
  - playback over the original's full transposition range
  - menu routing
  - a scan for duplicate shortcuts
- `SynthOMatic --snapshot <dir> [patch]` writes a PNG of every window.
