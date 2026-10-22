import XCTest
import SOMEngine

/// Tests of the ported engine, run against the original 1990-1994 patches.
final class SOMEngineTests: XCTestCase {
    var ctx: OpaquePointer!

    override func setUp() {
        ctx = som_create()
        som_set_random_seed(ctx, 12345)
        som_set_message_callback(ctx, { _, _ in }, nil)
    }

    override func tearDown() {
        som_destroy(ctx)
    }

    // MARK: helpers

    static var patchURLs: [URL] {
        let dir = Bundle.module.url(forResource: "Patches", withExtension: nil)!
        return try! FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)
            .sorted { $0.lastPathComponent < $1.lastPathComponent }
    }

    func load(_ url: URL) -> Bool {
        let data = try! Data(contentsOf: url)
        return data.withUnsafeBytes { som_read_patch(ctx, $0.bindMemory(to: UInt8.self).baseAddress, $0.count) } == 0
    }

    func run(_ program: String, duration: Double = 0.1) -> [Float] {
        som_new_document(ctx)
        som_set_globals(ctx, 44100, duration, 1)
        som_set_program(ctx, program)
        XCTAssertEqual(som_compile(ctx), 0, "compile: \(program)")
        XCTAssertEqual(som_calculate(ctx, nil, nil), Int32(SOMCalcOK))
        let out = som_find_var(ctx, "out")
        guard out >= 0 else { return [] }
        return samples(out)
    }

    func samples(_ i: Int32) -> [Float] {
        var info = SOMVarInfo()
        som_get_var_info(ctx, i, &info)
        guard let p = som_var_data(ctx, i) else { return [] }
        return Array(UnsafeBufferPointer(start: p, count: info.length))
    }

    func value(_ name: String) -> Double {
        var info = SOMVarInfo()
        som_get_var_info(ctx, som_find_var(ctx, name), &info)
        return info.value
    }

    // MARK: the original patches

    /// Patches that the final 1992 build itself could not compile: they use
    /// unit generators from earlier versions.
    let incompatible: Set<String> = ["bwowowo.pch", "grains.pch"]

    func testEveryPatchCompilesAndCalculates() {
        var rendered = 0
        for url in Self.patchURLs {
            let name = url.lastPathComponent
            XCTAssertTrue(load(url), "read \(name)")
            let ok = som_compile(ctx) == 0 && som_is_compiled(ctx) != 0
            if incompatible.contains(name) {
                XCTAssertFalse(ok, "\(name) should not compile")
                continue
            }
            XCTAssertTrue(ok, "compile \(name)")
            XCTAssertEqual(som_calculate(ctx, nil, nil), Int32(SOMCalcOK), name)
            let out = som_first_output_sample(ctx)
            XCTAssertGreaterThanOrEqual(out, 0, "\(name) has an output")
            let s = samples(out)
            XCTAssertFalse(s.isEmpty, name)
            XCTAssertTrue(s.allSatisfy(\.isFinite), "\(name) output is finite")
            XCTAssertLessThan(s.map(abs).max() ?? 0, 1000, "\(name) output is sane")
            rendered += 1
        }
        XCTAssertGreaterThan(rendered, 35)
    }

    func testPatchRoundTrip() {
        for url in Self.patchURLs where !incompatible.contains(url.lastPathComponent) {
            XCTAssertTrue(load(url))
            let program = String(cString: som_get_program(ctx))
            let rate = som_sample_rate(ctx), length = som_sample_length(ctx)
            var before: [String: (SOMVarInfo, [Float])] = [:]
            for i in 0..<som_var_count(ctx) {
                var info = SOMVarInfo()
                som_get_var_info(ctx, i, &info)
                before[name(info)] = (info, samples(i))
            }
            var len = 0
            let bytes = som_write_patch(ctx, &len)!
            let data = Data(bytes: bytes, count: len)
            free(bytes)
            XCTAssertEqual(data.withUnsafeBytes { som_read_patch(ctx, $0.bindMemory(to: UInt8.self).baseAddress, $0.count) }, 0)
            XCTAssertEqual(String(cString: som_get_program(ctx)), program, url.lastPathComponent)
            XCTAssertEqual(som_sample_rate(ctx), rate, accuracy: 1e-9)
            XCTAssertEqual(som_sample_length(ctx), length)
            XCTAssertEqual(Int(som_var_count(ctx)), before.count)
            for i in 0..<som_var_count(ctx) {
                var info = SOMVarInfo()
                som_get_var_info(ctx, i, &info)
                guard let (old, data) = before[name(info)] else { XCTFail("lost \(name(info))"); continue }
                XCTAssertEqual(info.type, old.type)
                XCTAssertEqual(info.value, old.value, "\(url.lastPathComponent) \(name(info))")
                XCTAssertEqual(info.lolim, old.lolim)
                XCTAssertEqual(info.hilim, old.hilim)
                XCTAssertEqual(info.pfield, old.pfield)
                if info.type == SOMTypeTable.rawValue || info.type == SOMTypeEnvelope.rawValue {
                    XCTAssertEqual(samples(i), data)
                }
            }
        }
    }

    func name(_ info: SOMVarInfo) -> String {
        withUnsafeBytes(of: info.name) { String(cString: $0.bindMemory(to: CChar.self).baseAddress!) }
    }

    func testReadsAllThreeFileVersions() {
        // version 10 (no version word, 42 byte variables), 50 (46 bytes) and 70 (48 bytes, file names)
        let byName = Dictionary(uniqueKeysWithValues: Self.patchURLs.map { ($0.lastPathComponent, $0) })
        XCTAssertTrue(load(byName["4waves.pch"]!))
        XCTAssertEqual(som_sample_rate(ctx), 22000)
        XCTAssertTrue(load(byName["flute"]!))
        XCTAssertEqual(som_sample_rate(ctx), 39063)
        XCTAssertEqual(value("pitch"), 84)
        XCTAssertTrue(load(byName["crix.pch"]!))
        XCTAssertEqual(String(cString: som_in_filename(ctx)), "sndin")
        XCTAssertEqual(String(cString: som_out_filename(ctx)), "sndout")
    }

    // MARK: the language

    func testArithmeticAndAssignment() {
        // the original computed b - a for a -= b (and likewise /= and ^=)
        _ = run("x = 2; x += 3; x *= 4; x -= 1; x /= 2; y = x; y ^= 2;")
        XCTAssertEqual(value("x"), 9.5, accuracy: 1e-6)
        XCTAssertEqual(value("y"), 90.25, accuracy: 1e-6)
        _ = run("tbl[3] = 10; tbl[3] -= 4; tbl[3] /= 2; x = tbl[3];")
        XCTAssertEqual(value("x"), 3, accuracy: 1e-6)
        _ = run("a = 2 ^ 3; b = -2 ^ 2; c = 7 / 0;")
        XCTAssertEqual(value("a"), 8)
        XCTAssertEqual(value("b"), -4)		// the original's signed power
        XCTAssertEqual(value("c"), 1e10)
    }

    func testOriginalConditionalPrecedence() {
        // ? : binds tighter than comparison in the original grammar
        _ = run("x = 1 > 0 ? 5 : 7;")
        XCTAssertEqual(value("x"), 0)
        _ = run("x = (1 > 0) ? 5 : 7;")
        XCTAssertEqual(value("x"), 5)
    }

    func testIfElse() {
        XCTAssertEqual(run("out = 1; if (0) { out = 2; } else { out = 3; }")[10], 3)
        XCTAssertEqual(run("out = 1; if (1) out = 2;")[10], 2)
    }

    func testTableIndexing() {
        _ = run("tbl[3] = 0.5; x = tbl[3];")
        XCTAssertEqual(value("x"), 0.5)
    }

    func testAutoDeclaration() {
        som_new_document(ctx)
        som_set_program(ctx, "out = oscil(mytbl, 440, 0) * myenv; @snd = out;")
        XCTAssertEqual(som_compile(ctx), 0)
        func type(_ n: String) -> Int32 { var i = SOMVarInfo(); som_get_var_info(ctx, som_find_var(ctx, n), &i); return i.type }
        XCTAssertEqual(type("mytbl"), Int32(SOMTypeTable.rawValue))
        XCTAssertEqual(type("myenv"), Int32(SOMTypeEnvelope.rawValue))
        XCTAssertEqual(type("out"), Int32(SOMTypeSample.rawValue))
        XCTAssertEqual(type("@snd"), Int32(SOMTypeSample.rawValue))
    }

    func testCompileErrorsReportTheLine() {
        som_new_document(ctx)
        som_set_program(ctx, "a = 1;\nb = oscil(tbl, 440);\n")
        XCTAssertNotEqual(som_compile(ctx), 0)
        XCTAssertEqual(som_error_line(ctx), 2)
        XCTAssertEqual(som_is_compiled(ctx), 0)
        som_set_program(ctx, "a = 1;\nb = 2 $ 3;\n")
        XCTAssertNotEqual(som_compile(ctx), 0)
        XCTAssertEqual(som_error_line(ctx), 2)
    }

    func testCompileAfterAnError() {
        som_new_document(ctx)
        som_set_program(ctx, "out = sinosc(440, 0);")
        XCTAssertEqual(som_compile(ctx), 0)
        som_set_program(ctx, "a = 1;\nb = 2 +;\n")
        XCTAssertNotEqual(som_compile(ctx), 0)
        som_set_program(ctx, "out = sinosc(440, 0);")
        XCTAssertEqual(som_compile(ctx), 0, "compiles again")
        XCTAssertNotEqual(som_is_compiled(ctx), 0)
    }

    func testPFieldsAndGlobals() {
        som_new_document(ctx)
        som_set_globals(ctx, 22050, 0.5, 1)
        som_set_program(ctx, "a = dur; b = sr; c = nyq; d = p12;")
        XCTAssertEqual(som_compile(ctx), 0)
        XCTAssertEqual(som_calculate(ctx, nil, nil), Int32(SOMCalcOK))
        XCTAssertEqual(value("a"), 0.5, accuracy: 1e-9)
        XCTAssertEqual(value("b"), 22050)
        XCTAssertEqual(value("c"), 11025)
        var info = SOMVarInfo()
        som_get_var_info(ctx, som_find_var(ctx, "p12"), &info)
        XCTAssertEqual(info.pfield, 12)
    }

    // MARK: unit generators

    func testSineOscillatorsAgree() {
        let a = run("out = sinosc(441, 0);")
        let b = run("out = oscili(tbl, 441, 0);")
        XCTAssertEqual(a.count, b.count)
        var maxDiff: Float = 0
        for i in 0..<a.count { maxDiff = max(maxDiff, abs(a[i] - b[i])) }
        XCTAssertLessThan(maxDiff, 0.001)
        XCTAssertEqual(a.map(abs).max()!, 1, accuracy: 0.001)
    }

    func testSumsOfSines() {
        let s = run("out = sinosc(440, 0); out += sinosc(210, 0);", duration: 1)
        let rms = sqrt(s.reduce(0) { $0 + Double($1) * Double($1) } / Double(s.count))
        XCTAssertEqual(rms, 1, accuracy: 0.01)
    }

    func testEnvelopes() {
        // numbers are single precision, as in the original: .02 s is 881 samples
        let s = run("out = linen(1, .02, .05, .02);")
        XCTAssertEqual(s[0], 0)
        XCTAssertEqual(Double(s[Int(0.01 * 44100)]), 0.5, accuracy: 0.01)
        XCTAssertEqual(Double(s[Int(0.05 * 44100)]), 1, accuracy: 0.002)
        XCTAssertEqual(Double(s[Int(0.08 * 44100)]), 0.5, accuracy: 0.02)
        XCTAssertEqual(s[Int(0.095 * 44100)], 0)
        let r = run("out = ramp(0, 1);")
        XCTAssertEqual(Double(r[0]), 0, accuracy: 1e-9)
        XCTAssertEqual(Double(r[Int(som_sample_length(ctx)) - 1]), 1, accuracy: 1e-6)
    }

    func testOneShotHoldsItsLastValue() {
        // oscil1 used to leave stale data after the one shot ended
        let s = run("out = oscil1(shapetbl, .02, 1);", duration: 0.1)
        let last = s[Int(0.09 * 44100)]
        for i in Int(0.03 * 44100)..<Int(0.09 * 44100) { XCTAssertEqual(s[i], last) }
    }

    func testFiltersAreStable() {
        for program in ["out = lpf(noise0(10000, 1), 1000);", "out = hpf(noise0(10000, 1), 1000);",
                        "out = bpf(noise0(10000, 1), 1000, 100);", "out = brf(noise0(10000, 1), 1000, 100);",
                        "out = reson(noise0(10000, 1), 1000, 100);", "out = reverb(blip(5, 1), 2);",
                        "out = string(noise0(5000, 1) * linen(1, .001, 0, .001), 220, 1);"] {
            let s = run(program, duration: 0.5)
            XCTAssertTrue(s.allSatisfy(\.isFinite), program)
            XCTAssertLessThan(s.map(abs).max()!, 100, program)
        }
    }

    func testDelay() {
        // .01 is the float 0.0099999998, so the line is 440 samples long
        let s = run("out = delay(blip(10, 1), .01);", duration: 0.05)
        XCTAssertEqual(s[0], 0)
        XCTAssertEqual(s[440], 1)
    }

    func testPan() {
        _ = run("out = pan(sinosc(100, 0), -1, outL, outR);")
        let l = samples(som_find_var(ctx, "outL")), r = samples(som_find_var(ctx, "outR"))
        XCTAssertGreaterThan(l.map(abs).max()!, 0.99)
        XCTAssertLessThan(r.map(abs).max()!, 0.001)
    }

    func testInputSampleIsPlayed() {
        som_new_document(ctx)
        som_set_globals(ctx, 1000, 1, 1)
        som_set_program(ctx, "out = 2 * insnd;")
        let i = som_define_var(ctx, "insnd", Int32(SOMTypeSample.rawValue))
        let input = (0..<1000).map { Float($0) / 1000 }
        input.withUnsafeBufferPointer { som_set_sample_data(ctx, i, $0.baseAddress, $0.count) }
        XCTAssertEqual(som_compile(ctx), 0)
        XCTAssertEqual(som_calculate(ctx, nil, nil), Int32(SOMCalcOK))
        let out = samples(som_find_var(ctx, "out"))
        XCTAssertEqual(Double(out[600]), 1.2, accuracy: 1e-6)
    }

    func testStop() {
        som_new_document(ctx)
        som_set_globals(ctx, 44100, 10, 1)
        som_set_program(ctx, "out = sinosc(440, 0);")
        XCTAssertEqual(som_compile(ctx), 0)
        let r = som_calculate(ctx, { _, done, _ in done >= 3 ? 1 : 0 }, nil)
        XCTAssertEqual(r, Int32(SOMCalcStopped))
    }

    // MARK: tables

    func testFillsAndAlters() {
        som_new_document(ctx)
        let t = som_define_var(ctx, "tbl", Int32(SOMTypeTable.rawValue))
        som_fill_fm(ctx, t, 1, 0, 0)
        let sine = samples(t)
        XCTAssertEqual(Double(sine[128]), 1, accuracy: 1e-6)
        som_alter_table(ctx, t, SOMAlterReverse, nil)
        som_alter_table(ctx, t, SOMAlterReverse, nil)
        XCTAssertEqual(samples(t), sine)
        som_alter_table(ctx, t, SOMAlterInvert, nil)
        XCTAssertEqual(Double(samples(t)[128]), -1, accuracy: 1e-6)
        som_alter_table(ctx, t, SOMAlterAbs, nil)
        XCTAssertTrue(samples(t).allSatisfy { $0 >= 0 })

        var ampl = [UInt8](repeating: 0, count: 32), phas = [UInt8](repeating: 0, count: 32)
        ampl[0] = 99; ampl[2] = 50
        som_sine_fill(ctx, t, ampl, phas)
        var a2 = [UInt8](repeating: 0, count: 32), p2 = [UInt8](repeating: 0, count: 32)
        som_analyze_harmonics(ctx, t, &a2, &p2)
        XCTAssertEqual(a2[0], 99)
        XCTAssertGreaterThan(a2[2], 40)
        XCTAssertLessThan(a2[1], 3)

        let ps = som_define_var(ctx, "tblps", Int32(SOMTypeTable.rawValue))
        XCTAssertEqual(som_cheby_fill(ctx, t, ampl, phas), ps)
        XCTAssertTrue(samples(t).allSatisfy { $0.isFinite && abs($0) <= 1.0001 })

        som_fill_gbuzz(ctx, t, 16, 1, 0.9)
        som_fill_grain_env(ctx, t, 0, 50)
        XCTAssertEqual(samples(t)[256], 1)
        som_comb_table(ctx, t, 1, 4)
        for op in [SOMAlterSquashUp, SOMAlterSquashDown, SOMAlterSquashIn, SOMAlterSquashOut, SOMAlterHanning,
                   SOMAlterParzen, SOMAlterWelch, SOMAlterNormalize, SOMAlterSmooth, SOMAlterAnalyze, SOMAlterSquare,
                   SOMAlterClipNegative] {
            som_alter_table(ctx, t, op, nil)
            XCTAssertTrue(samples(t).allSatisfy(\.isFinite), "\(op)")
        }
    }

    // MARK: export

    func testExportFormats() throws {
        _ = run("out = 0.5 * sinosc(441, 0);")
        let out = som_find_var(ctx, "out")
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("somtests-\(getpid())")
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: dir) }
        let frames = Int(som_sample_length(ctx))
        for (format, ext, header, bytes) in [(SOMExportAIFF16, "aiff", 54, 2), (SOMExportWAV16, "wav", 44, 2),
                                             (SOMExportWAV24, "wav", 44, 3), (SOMExportWAVFloat, "wav", 44, 4),
                                             (SOMExportSoundDesigner, "sd", 1336, 2)] {
            let url = dir.appendingPathComponent("t\(format.rawValue).\(ext)")
            XCTAssertEqual(url.withUnsafeFileSystemRepresentation { som_export_sample(ctx, out, $0, format, 1) }, 0)
            let data = try Data(contentsOf: url)
            XCTAssertEqual(data.count, header + frames * bytes, "\(format)")
        }
    }
}
