import Foundation

/// The language reference: the original "Quick Reference Guide" and "slow
/// ref guide", completed with the unit generators the guides never listed.
/// Used by the Quick Reference window, signature help and completion.
enum Reference {
    struct Entry {
        let signature: String
        let summary: String
        var details: String = ""

        var name: String { String(signature.prefix { $0 != "(" }) }
        var arguments: [String] {
            guard let open = signature.firstIndex(of: "("), let close = signature.lastIndex(of: ")") else { return [] }
            let inner = signature[signature.index(after: open)..<close]
            return inner.isEmpty ? [] : inner.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }
        }
    }

    struct Section {
        let title: String
        let entries: [Entry]
    }

    static let operators = Section(title: "Operators", entries: [
        Entry(signature: "a + b", summary: "addition"),
        Entry(signature: "a - b", summary: "subtraction"),
        Entry(signature: "a * b", summary: "multiplication"),
        Entry(signature: "a / b", summary: "division (division by zero gives 1e10)"),
        Entry(signature: "a ^ b", summary: "exponentiation; a negative base gives -(|a| ^ b)"),
        Entry(signature: "a = b", summary: "assignment"),
        Entry(signature: "a += b   a -= b   a *= b   a /= b   a ^= b", summary: "a = a + b, and so on"),
        Entry(signature: "a > b   a < b   a >= b   a <= b", summary: "comparison, 1 or 0"),
        Entry(signature: "a == b   a != b", summary: "equality, 1 or 0"),
        Entry(signature: "a && b   a || b   !a", summary: "logical and, or, not"),
        Entry(signature: "a ? b : c", summary: "conditional expression",
              details: "Note: as in the original, ? : binds more tightly than the comparison and arithmetic operators, so write (a > b) ? c : d."),
        Entry(signature: "tbl[i]   tbl[i] = x", summary: "read or write one of the 512 points of a wavetable or envelope"),
    ])

    static let sections: [Section] = [
        Section(title: "Simple Functions", entries: [
            Entry(signature: "abs(in)", summary: "absolute value of input"),
            Entry(signature: "sgn(in)", summary: "sign function: -1, 0 or +1"),
            Entry(signature: "int(in)", summary: "integer part of input (rounds down)"),
            Entry(signature: "frac(in)", summary: "fractional part of input: in - int(in)"),
            Entry(signature: "rand(hi)", summary: "scalar random number in interval [0, hi]",
                  details: "Chosen once when the calculation starts."),
            Entry(signature: "rand2(lo, hi)", summary: "scalar random number in interval [lo, hi]"),
            Entry(signature: "min(ina, inb)", summary: "minimum of ina and inb"),
            Entry(signature: "max(ina, inb)", summary: "maximum of ina and inb"),
            Entry(signature: "clip(minlim, in, maxlim)", summary: "clips input to min and max limits"),
            Entry(signature: "trig(in)", summary: "1 where the input is positive, else 0"),
            Entry(signature: "sah(in, trigger)", summary: "sample and hold",
                  details: "Samples the input each time the trigger goes from zero or below to above zero."),
            Entry(signature: "gate(in, trigger)", summary: "gate or hold",
                  details: "Passes the input while the trigger is positive, holds the last value otherwise."),
        ]),
        Section(title: "Transcendental Functions", entries: [
            Entry(signature: "sin(in)", summary: "sine of input (radians)"),
            Entry(signature: "cos(in)", summary: "cosine of input"),
            Entry(signature: "tan(in)", summary: "tangent of input"),
            Entry(signature: "log(in)", summary: "natural log of input"),
            Entry(signature: "log10(in)", summary: "base 10 log of input"),
            Entry(signature: "sqrt(in)", summary: "square root of input"),
            Entry(signature: "exp(in)", summary: "e to the power of the input"),
            Entry(signature: "cpsmidi(in)", summary: "convert Hertz to MIDI note numbers",
                  details: "The integer part is the MIDI key number and the fractional part times 100 is the number of cents sharp."),
            Entry(signature: "midicps(in)", summary: "convert MIDI note numbers to Hertz (note 57 is A 220 Hz)"),
            Entry(signature: "dbamp(in)", summary: "convert decibels to linear amplitude"),
            Entry(signature: "ampdb(in)", summary: "convert linear amplitude to decibels"),
        ]),
        Section(title: "Oscillators", entries: [
            Entry(signature: "sinosc(freq, phase)", summary: "sine wave oscillator, computed with sin()",
                  details: "freq in Hz, phase modulation in radians."),
            Entry(signature: "oscil(table, freq, phase)", summary: "table lookup oscillator with freq and phase modulation",
                  details: "'table' is a wavetable. 'freq' is a frequency modulation input in Hz. 'phase' is a phase modulation input in radians."),
            Entry(signature: "oscili(table, freq, phase)", summary: "interpolating oscillator with freq and phase modulation",
                  details: "Same as oscil, but output samples are linearly interpolated from the wavetable, reducing table lookup noise."),
            Entry(signature: "poscil(carfrq, modfrq, index)", summary: "phase modulation oscillator pair",
                  details: "A pair of sine oscillators, one phase modulating the other. 'index' is the modulation index in radians; the spectrum is the same as frequency modulation with that index."),
            Entry(signature: "poscili(carfrq, modfrq, index)", summary: "interpolating phase modulation oscillator pair"),
            Entry(signature: "pmoscil(carfrq, modfrq, index)", summary: "phase modulation oscillator pair using sin()",
                  details: "The most accurate (least noisy) of the three."),
            Entry(signature: "coscil(table, frq, beatfrq)", summary: "detuned pair of oscillators",
                  details: "Exactly equivalent to oscil(table, frq + beatfrq/2, 0) + oscil(table, frq - beatfrq/2, 0), but easier to type."),
            Entry(signature: "coscili(table, frq, beatfrq)", summary: "interpolating detuned pair of oscillators"),
            Entry(signature: "coscil2(table1, table2, frq, beatfrq)", summary: "detuned pair of oscillators, each with its own table"),
            Entry(signature: "coscili2(table1, table2, frq, beatfrq)", summary: "interpolating coscil2"),
            Entry(signature: "xcoscil(table1, table2, frq, beatfrq, xfade)", summary: "detuned pair of wavetable crossfade oscillators",
                  details: "xfade runs from -1 (all table1) to +1 (all table2)."),
            Entry(signature: "soscil(table1, table2, fm1, fm2)", summary: "hard sync oscillator with window function",
                  details: "table1 at fm1 is the master (window); table2 at fm2 is restarted at each master cycle and multiplied by it."),
            Entry(signature: "woscil(shapetbl, pstbl, freq, index)", summary: "waveshaping oscillator",
                  details: "A sine wave scaled by index (0..1) drives the transfer function shapetbl, made with Cheby Fill; pstbl is its power scaling table (the 'ps' table Cheby Fill writes)."),
            Entry(signature: "wcoscil(shapetbl, pstbl, freq, beatfrq, index)", summary: "detuned pair of waveshaping oscillators"),
            Entry(signature: "vosim(table, frq, wavefrq)", summary: "VOSIM-like oscillator",
                  details: "One cycle of 'table' at 'wavefrq' is started at the rate 'frq'."),
        ]),
        Section(title: "Control Functions", entries: [
            Entry(signature: "sinlfo(freq, phase, amp)", summary: "sine lfo"),
            Entry(signature: "lfo(table, freq, phase, amp)", summary: "table lookup lfo"),
            Entry(signature: "oscil1(table, dur, amp)", summary: "one shot table lookup lfo",
                  details: "Reads the table once in 'dur' seconds and then holds its last value."),
            Entry(signature: "triang(freq, amp)", summary: "triangle wave lfo"),
            Entry(signature: "square(freq, amp)", summary: "square wave lfo (-amp..+amp)"),
            Entry(signature: "square0(freq, amp)", summary: "square wave lfo (0..+amp)"),
            Entry(signature: "saw(freq, amp)", summary: "sawtooth wave lfo"),
            Entry(signature: "pulse(freq, pwm, amp)", summary: "variable pulse width lfo (-amp..+amp)",
                  details: "pwm is the pulse width, 0..1."),
            Entry(signature: "pulse0(freq, pwm, amp)", summary: "variable pulse width lfo (0..+amp)"),
            Entry(signature: "blip(freq, amp)", summary: "one sample impulse wave lfo"),
            Entry(signature: "ramp(start, end)", summary: "ramp generator over the whole duration"),
            Entry(signature: "bpenv(table, level, timescale)", summary: "breakpoint envelope generator with level & duration scaling",
                  details: "The table holds pairs of level and duration (seconds): level, time, level, time, ..."),
            Entry(signature: "adsr(lvl1, lvl2, t1, t2, t3, t4)", summary: "old analog 4 stage exponential envelope generator",
                  details: "Attack to lvl1 in t1, decay to lvl2 in t2, sustain for t3, release in t4."),
            Entry(signature: "ar(lvl, t1, t2)", summary: "old analog 2 stage exponential envelope generator",
                  details: "Attack to lvl in t1, then release in t2."),
            Entry(signature: "linen(lvl, t1, t2, t3)", summary: "linear flat top envelope",
                  details: "Rise to lvl in t1, sustain for t2, fall in t3."),
            Entry(signature: "linseg(lvl1, lvl2, lvl3, t1, t2, t3, t4)", summary: "4 stage linear envelope",
                  details: "0 to lvl1 in t1, to lvl2 in t2, to lvl3 in t3, to 0 in t4."),
            Entry(signature: "linseg0(lvl0, lvl1, lvl2, lvl3, lvl4, t1, t2, t3, t4)", summary: "4 stage linear envelope with given end points"),
            Entry(signature: "lag(in, time)", summary: "exponential decay",
                  details: "'time' is the time to decay by 60 dB."),
            Entry(signature: "lag2(in, atktime, dcytime)", summary: "exponential attack and decay"),
            Entry(signature: "dopfreq(speed, distance, time)", summary: "doppler frequency ratio",
                  details: "For a source passing by at 'speed' feet per second, 'distance' feet away at its nearest, at 'time' seconds."),
            Entry(signature: "dopamp(speed, distance, time)", summary: "doppler amplitude for the same passing source"),
        ]),
        Section(title: "Random Generators", entries: [
            Entry(signature: "noise0(freq, amp)", summary: "random noise with no interpolation - \"buzzy\"",
                  details: "Steps to a new random value 'freq' times per second."),
            Entry(signature: "noise1(freq, amp)", summary: "random noise with linear interpolation - \"fizzy\"",
                  details: "Ramps linearly to the next random value. More band limited than noise0."),
            Entry(signature: "noise(freq, amp)", summary: "random noise with quadratic interpolation - \"whirry\"",
                  details: "Swerves parabolically to the next random value. The best noise source to use in most cases."),
            Entry(signature: "wavelets(wavtbl, density, ctrfreq, bw)", summary: "wavelet generator",
                  details: "Starts 'density' single cycles of the table per second at random frequencies, ctrfreq ± bw."),
            Entry(signature: "grains(wavtbl, amptbl, dur, density, ctrfreq, bw)", summary: "granular synthesis generator",
                  details: "Grains of 'dur' seconds (at most 0.2) shaped by amptbl, 'density' grains per second, frequency ctrfreq ± bw."),
        ]),
        Section(title: "Filters", entries: [
            Entry(signature: "diff(in)", summary: "derivative of input"),
            Entry(signature: "integ(in)", summary: "integral of input"),
            Entry(signature: "sum(in)", summary: "two point sum of input"),
            Entry(signature: "avg(in)", summary: "two point average of input"),
            Entry(signature: "flip(in)", summary: "inverts spectrum about SampleRate/4"),
            Entry(signature: "flipsum(in)", summary: "inverts spectrum about SR/4, then notches out SR/2"),
            Entry(signature: "tone(in, cfreq)", summary: "simple first order lowpass filter"),
            Entry(signature: "atone(in, cfreq)", summary: "inverse of tone"),
            Entry(signature: "reson(in, cfreq, bw)", summary: "simple second order resonant filter"),
            Entry(signature: "areson(in, cfreq, bw)", summary: "inverse of reson"),
            Entry(signature: "lpf(in, cfreq)", summary: "Butterworth low pass filter"),
            Entry(signature: "hpf(in, cfreq)", summary: "Butterworth high pass filter"),
            Entry(signature: "bpf(in, cfreq, bw)", summary: "Butterworth band pass filter"),
            Entry(signature: "brf(in, cfreq, bw)", summary: "Butterworth band reject filter"),
            Entry(signature: "string(in, freq, rvt)", summary: "string resonator",
                  details: "A tuned Karplus-Strong delay loop; rvt is the time to decay by 60 dB."),
            Entry(signature: "pipe(in, freq, rvt)", summary: "pipe resonator (odd harmonics)"),
        ]),
        Section(title: "Amplitude Functions", entries: [
            Entry(signature: "rms(in)", summary: "rms of input signal"),
            Entry(signature: "gain(in, level)", summary: "sets rms of input to level"),
            Entry(signature: "balance(in, control)", summary: "balances rms of input to control signal"),
            Entry(signature: "shaper(in, tbl)", summary: "nonlinear transfer function",
                  details: "The input, -1..+1, is looked up across the table."),
            Entry(signature: "tablei(index, tbl)", summary: "interpolated table lookup, index 0..1"),
            Entry(signature: "pan(in, pos, outL, outR)", summary: "stereo panner - assigns panned input to output L & R",
                  details: "pos runs from -1 (left) to +1 (right). outL and outR must be variables. Returns the input."),
            Entry(signature: "panx(in, pos, outL, outR)", summary: "stereo panner - mixes panned input into output L & R"),
            Entry(signature: "xfade(in1, in2, mix)", summary: "cross fade between two inputs, mix -1..+1"),
        ]),
        Section(title: "Delays", entries: [
            Entry(signature: "delay1(in)", summary: "one sample delay"),
            Entry(signature: "delay(in, time)", summary: "delay line",
                  details: "A constant time makes a delay line of exactly that length. A time that varies may be up to the Max Delay Time (Signals > Set Globals)."),
            Entry(signature: "delayi(in, time)", summary: "interpolating delay line"),
            Entry(signature: "flange(in, time, mix)", summary: "flanger"),
            Entry(signature: "flangei(in, time, mix)", summary: "interpolating flanger"),
            Entry(signature: "comb(in, time, rvt)", summary: "comb delay line",
                  details: "rvt is the time for the recirculating signal to decay by 60 dB."),
            Entry(signature: "echo(in, time, rvt)", summary: "echo delay line"),
            Entry(signature: "echoi(in, time, rvt)", summary: "interpolating echo delay line"),
            Entry(signature: "lpcomb(in, time, g, g1)", summary: "lowpass comb delay line",
                  details: "g is the loop gain, g1 the lowpass coefficient."),
            Entry(signature: "allpass(in, time, rvt)", summary: "allpass delay line"),
            Entry(signature: "reverb(in, rvt)", summary: "standard reverberator (4 combs, 2 allpasses)"),
            Entry(signature: "reverb2(in, rvt)", summary: "reverberator with series combs"),
            Entry(signature: "taps(in, fbk, t1, t2, t3, t4, a1, a2, a3, a4)", summary: "multi tap delay line",
                  details: "Four taps at times t1..t4 with amplitudes a1..a4; their sum is fed back with gain fbk."),
        ]),
        Section(title: "Not Yet Done (produce silence)", entries: [
            Entry(signature: "buzz(in)", summary: "band limited pulse"),
            Entry(signature: "pluck(in)", summary: "plucked string"),
            Entry(signature: "pitchshift(in)", summary: "pitch shifter"),
            Entry(signature: "vocoder(in)", summary: "vocoder"),
            Entry(signature: "envfollow(in)", summary: "amplitude follower"),
            Entry(signature: "pchfollow(in)", summary: "pitch follower"),
        ]),
    ]

    static let entriesByName: [String: Entry] = {
        var d = [String: Entry]()
        for s in sections { for e in s.entries { d[e.name] = e } }
        return d
    }()

    static let notes: [(String, String)] = [
        ("Statements",
         "A program is a list of statements, each ending with a semicolon. Braces group statements. "
         + "if (expr) statement [else statement] chooses once, when the calculation starts. "
         + "Comments are written // to the end of the line, or /* like this */."),
        ("Variables",
         "Any new name used in a program becomes a variable when the program is compiled. Names containing "
         + "\"tbl\" become wavetables, names containing \"env\" become envelopes, names containing \"out\" or "
         + "beginning with \"@\" become samples, and all others become scalar variables. "
         + "The type of a variable can be changed from the Signals menu."),
        ("Parameters",
         "A scalar variable that the program reads but does not compute is a parameter: set it with its slider, "
         + "or double-click it to type its value, its limits and its number of steps. "
         + "Make Random Set and Randomize pick new values for chosen parameters."),
        ("Signals",
         "Signals are calculated in blocks of 512 samples. Anything that depends only on numbers and parameters "
         + "is calculated once. A wavetable has 512 points; an envelope's 512 points are stretched over the whole "
         + "duration; a sample has one point per output sample. A sample that the program reads but never "
         + "assigns plays its contents, for instance a sound loaded with File > Import Sound into Sample."),
        ("P-fields",
         "Variables named instr, time, dur, slur, note, volume and veloc are score p-fields 1 to 7, as are p1, p2, .... "
         + "dur is set to the duration of the sound; assigning to dur changes the duration."),
        ("Globals",
         "Variables named sr, nyq and tempo read the sample rate, the Nyquist frequency and the tempo."),
        ("Limits",
         "A program may use at most 64 different numbers."),
    ]
}
