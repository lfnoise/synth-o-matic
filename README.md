# Synth-O-Matic

Synth-O-Matic is a software synthesis program for the Macintosh, written by James McCartney in 1990–92. You write a small program of unit generators, then draw wavetables and envelopes, calculate the sound, and play it from the keyboard.

- [`SynthOMatic/`](SynthOMatic/) is a port to current macOS. Its [README](SynthOMatic/README.md) covers building, the design of the port, and how it differs from the original.
- [`SOM-classic/`](SOM-classic/) holds the original THINK C sources, the example patches, and the reference guides. The files are MacRoman text with CR line endings, as they were written.

## License

MIT; see [LICENSE](LICENSE). Third-party files in `SOM-classic/` keep their own terms:

- bison.simple, and harp2.tab.c, harp3.c and harp9.tab.c, which were generated from it: Copyright (C) 1984 Bob Corbett and Richard Stallman, GNU General Public License version 1 or later.
- TransSkel.c/.h, TransEdit.c/.h and TransDisplay.c/.h: public domain, written by Paul DuBois.

The macOS port in SynthOMatic/ uses none of these files.
