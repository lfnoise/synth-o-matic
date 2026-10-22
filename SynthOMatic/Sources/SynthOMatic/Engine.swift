import Foundation
import SOMEngine

/// The four kinds of signal a variable can hold.
enum VarType: Int, CaseIterable {
    case scalar = 0, table = 1, envelope = 2, sample = 3

    var displayName: String {
        switch self {
        case .scalar: return "Variable"
        case .table: return "Wavetable"
        case .envelope: return "Envelope"
        case .sample: return "Sample"
        }
    }

    var hasPoints: Bool { self != .scalar }
}

/// A snapshot of one variable's description, as the engine reports it.
struct VarInfo: Equatable {
    var index: Int
    var name: String
    var type: VarType
    var value: Double
    var lolim: Double
    var hilim: Double
    var stepsiz: Double
    var pfield: Int
    var assigned: Int
    var referenced: Bool
    var randomset: Bool
    var length: Int

    /// The original showed scalars the program doesn't compute as "Parameter"s
    /// with a slider; computed ones as "Variable"s.
    var isParameter: Bool { type == .scalar && assigned != 2 }
    var isOutput: Bool { assigned != 0 }

    var typeName: String { isParameter ? "Parameter" : type.displayName }
}

/// Everything needed to put a variable back the way it was (for undo).
struct VarSnapshot {
    var info: VarInfo
    var data: [Float]
}

/// Swift face of one SOMContext, i.e. one document's synthesizer.
///
/// All calls must come from one thread at a time.  The document makes the
/// main thread the owner, except while a calculation runs in the background.
final class Engine {
    let ctx: OpaquePointer
    var onMessage: ((String) -> Void)?

    init() {
        ctx = som_create()
        som_set_message_callback(ctx, { user, text in
            guard let user, let text else { return }
            let engine = Unmanaged<Engine>.fromOpaque(user).takeUnretainedValue()
            engine.onMessage?(String(cString: text))
        }, Unmanaged.passUnretained(self).toOpaque())
    }

    deinit {
        som_destroy(ctx)
    }

    func newDocument() { som_new_document(ctx) }

    // MARK: program & files

    var program: String {
        get { String(cString: som_get_program(ctx)) }
        set { som_set_program(ctx, newValue) }
    }

    @discardableResult
    func read(_ data: Data) -> Bool {
        data.withUnsafeBytes { buf in
            som_read_patch(ctx, buf.bindMemory(to: UInt8.self).baseAddress, buf.count) == 0
        }
    }

    func write() -> Data {
        var length = 0
        guard let bytes = som_write_patch(ctx, &length) else { return Data() }
        defer { free(bytes) }
        return Data(bytes: bytes, count: length)
    }

    static func isPatch(_ data: Data) -> Bool {
        data.withUnsafeBytes { buf in
            som_is_patch_data(buf.bindMemory(to: UInt8.self).baseAddress, buf.count) != 0
        }
    }

    // MARK: compile & calculate

    @discardableResult
    func compile() -> Bool { som_compile(ctx) == 0 && som_is_compiled(ctx) != 0 }
    var isCompiled: Bool { som_is_compiled(ctx) != 0 }
    var errorLine: Int { Int(som_error_line(ctx)) }

    enum CalcResult { case ok, stopped, error }

    /// Runs the program.  `progress` is called on the calculating thread;
    /// return true from it to stop.
    func calculate(progress: @escaping (Int, Int) -> Bool) -> CalcResult {
        final class Box { let fn: (Int, Int) -> Bool; init(_ fn: @escaping (Int, Int) -> Bool) { self.fn = fn } }
        let box = Box(progress)
        let user = Unmanaged.passRetained(box).toOpaque()
        defer { Unmanaged<Box>.fromOpaque(user).release() }
        let r = som_calculate(ctx, { user, done, total in
            let box = Unmanaged<Box>.fromOpaque(user!).takeUnretainedValue()
            return box.fn(done, total) ? 1 : 0
        }, user)
        switch r {
        case Int32(SOMCalcOK): return .ok
        case Int32(SOMCalcStopped): return .stopped
        default: return .error
        }
    }

    var firstOutputSample: Int? {
        let i = Int(som_first_output_sample(ctx))
        return i >= 0 ? i : nil
    }

    // MARK: globals

    var sampleRate: Double { som_sample_rate(ctx) }
    var duration: Double { som_duration(ctx) }
    var maxDelay: Double { som_max_delay(ctx) }
    var sampleLength: Int { som_sample_length(ctx) }

    func setGlobals(sampleRate: Double, duration: Double, maxDelay: Double) {
        som_set_globals(ctx, sampleRate, duration, maxDelay)
    }

    var inFilename: String { String(cString: som_in_filename(ctx)) }
    var outFilename: String { String(cString: som_out_filename(ctx)) }
    func setIOFilenames(input: String, output: String) { som_set_io_filenames(ctx, input, output) }

    // MARK: variables

    var varCount: Int { Int(som_var_count(ctx)) }

    /// Variables in the order the original's variable window showed them.
    func sortedVars() -> [VarInfo] {
        (0..<varCount).compactMap { row in
            let i = Int(som_var_at_row(ctx, Int32(row)))
            return i >= 0 ? info(i) : nil
        }
    }

    func info(_ i: Int) -> VarInfo {
        var c = SOMVarInfo()
        som_get_var_info(ctx, Int32(i), &c)
        let name = withUnsafeBytes(of: c.name) { raw -> String in
            let bytes = raw.bindMemory(to: UInt8.self)
            let end = bytes.firstIndex(of: 0) ?? bytes.count
            return String(decoding: bytes[..<end], as: UTF8.self)
        }
        return VarInfo(index: i, name: name, type: VarType(rawValue: Int(c.type)) ?? .scalar,
                       value: c.value, lolim: c.lolim, hilim: c.hilim, stepsiz: c.stepsiz,
                       pfield: Int(c.pfield), assigned: Int(c.assigned), referenced: c.referenced != 0,
                       randomset: c.randomset != 0, length: c.length)
    }

    func index(named name: String) -> Int? {
        let i = Int(som_find_var(ctx, name))
        return i >= 0 ? i : nil
    }

    /// The table, envelope or sample points.  Valid until the variable changes type
    /// or the duration changes.
    func data(_ i: Int) -> UnsafeMutableBufferPointer<Float>? {
        let n = info(i).length
        guard n > 0, let p = som_var_data(ctx, Int32(i)) else { return nil }
        return UnsafeMutableBufferPointer(start: p, count: n)
    }

    func points(_ i: Int) -> [Float] { data(i).map { Array($0) } ?? [] }

    @discardableResult
    func defineVar(_ name: String, type: VarType) -> Int {
        Int(som_define_var(ctx, name, Int32(type.rawValue)))
    }

    func setValue(_ i: Int, _ v: Double) { som_set_var_value(ctx, Int32(i), v) }
    func setLimits(_ i: Int, lo: Double, hi: Double, step: Double) { som_set_var_limits(ctx, Int32(i), lo, hi, step) }
    func setPField(_ i: Int, _ p: Int) { som_set_var_pfield(ctx, Int32(i), Int32(p)) }
    func setType(_ i: Int, _ t: VarType) { som_set_var_type(ctx, Int32(i), Int32(t.rawValue)) }

    func setScalarParams(_ i: Int, value: Double, lo: Double, hi: Double, steps: Double, pfield: Int) {
        som_set_scalar_params(ctx, Int32(i), value, lo, hi, steps, Int32(pfield))
    }

    func setGraphBounds(_ i: Int, hi: Double, lo: Double) { som_set_graph_bounds(ctx, Int32(i), hi, lo) }

    func findLimits(_ i: Int) -> (min: Double, max: Double, step: Double) {
        var lo = 0.0, hi = 0.0, step = 0.0
        som_find_limits(ctx, Int32(i), &lo, &hi, &step)
        return (lo, hi, step)
    }

    func autoScale(_ i: Int) { som_auto_scale(ctx, Int32(i)) }
    func autoScaleExact(_ i: Int) { som_auto_scale_exact(ctx, Int32(i)) }

    func snapshot(_ i: Int) -> VarSnapshot { VarSnapshot(info: info(i), data: points(i)) }

    func restore(_ s: VarSnapshot) {
        guard let i = index(named: s.info.name) else { return }
        var c = SOMVarInfo()
        som_get_var_info(ctx, Int32(i), &c)
        c.type = Int32(s.info.type.rawValue)
        c.value = s.info.value
        c.lolim = s.info.lolim
        c.hilim = s.info.hilim
        c.stepsiz = s.info.stepsiz
        c.pfield = Int32(s.info.pfield)
        c.randomset = s.info.randomset ? 1 : 0
        s.data.withUnsafeBufferPointer { buf in
            som_restore_var(ctx, Int32(i), &c, s.data.isEmpty ? nil : buf.baseAddress, s.data.count)
        }
    }

    func setSampleData(_ i: Int, _ samples: [Float]) {
        samples.withUnsafeBufferPointer { som_set_sample_data(ctx, Int32(i), $0.baseAddress, $0.count) }
    }

    func makeRandomSet(_ indices: [Int]) {
        let list = indices.map(Int32.init)
        list.withUnsafeBufferPointer { som_make_random_set(ctx, $0.baseAddress, Int32($0.count)) }
    }

    func randomize() { som_randomize(ctx) }

    @discardableResult
    func removeUnused(_ indices: [Int], all: Bool) -> Int {
        let list = indices.map(Int32.init)
        return Int(list.withUnsafeBufferPointer { som_remove_unused(ctx, $0.baseAddress, Int32($0.count), all ? 1 : 0) })
    }

    func memoryReport() { som_memory_report(ctx) }

    // MARK: tables

    func alter(_ i: Int, _ op: SOMAlterOp, clip: [Float]? = nil) {
        if let clip, clip.count >= 513 {
            clip.withUnsafeBufferPointer { som_alter_table(ctx, Int32(i), op, $0.baseAddress) }
        } else {
            som_alter_table(ctx, Int32(i), op, nil)
        }
    }

    func fillFM(_ i: Int, carrier: Double, modulator: Double, index: Double) { som_fill_fm(ctx, Int32(i), carrier, modulator, index) }
    func fillGBuzz(_ i: Int, n: Double, k: Double, r: Double) { som_fill_gbuzz(ctx, Int32(i), n, k, r) }
    func fillGrainEnv(_ i: Int, shape: Int, duration: Double) { som_fill_grain_env(ctx, Int32(i), Int32(shape), duration) }
    func combTable(_ i: Int, even: Bool, teeth: Double) { som_comb_table(ctx, Int32(i), even ? 1 : 0, teeth) }

    func analyzeHarmonics(_ i: Int) -> (ampl: [UInt8], phas: [UInt8]) {
        var a = [UInt8](repeating: 0, count: 32), p = [UInt8](repeating: 0, count: 32)
        som_analyze_harmonics(ctx, Int32(i), &a, &p)
        return (a, p)
    }

    func sineFill(_ i: Int, ampl: [UInt8], phas: [UInt8]) { som_sine_fill(ctx, Int32(i), ampl, phas) }

    /// returns the index of the power scaling table it filled, if any
    func chebyFill(_ i: Int, ampl: [UInt8], phas: [UInt8]) -> Int? {
        let r = Int(som_cheby_fill(ctx, Int32(i), ampl, phas))
        return r >= 0 ? r : nil
    }

    // MARK: export

    enum ExportFormat: Int, CaseIterable {
        case aiff16, wav16, wav24, wavFloat, soundDesigner

        var title: String {
            switch self {
            case .aiff16: return "AIFF, 16 bit"
            case .wav16: return "WAV, 16 bit"
            case .wav24: return "WAV, 24 bit"
            case .wavFloat: return "WAV, 32 bit float"
            case .soundDesigner: return "Sound Designer (classic Mac)"
            }
        }

        var fileExtension: String {
            switch self {
            case .aiff16: return "aiff"
            case .wav16, .wav24, .wavFloat: return "wav"
            case .soundDesigner: return "sd"
            }
        }

        var cValue: SOMExportFormat {
            switch self {
            case .aiff16: return SOMExportAIFF16
            case .wav16: return SOMExportWAV16
            case .wav24: return SOMExportWAV24
            case .wavFloat: return SOMExportWAVFloat
            case .soundDesigner: return SOMExportSoundDesigner
            }
        }
    }

    func export(_ i: Int, stereoWith right: Int? = nil, to url: URL, format: ExportFormat, normalize: Bool) -> Bool {
        url.withUnsafeFileSystemRepresentation { path in
            guard let path else { return false }
            if let right {
                return som_export_stereo(ctx, Int32(i), Int32(right), path, format.cValue, normalize ? 1 : 0) == 0
            }
            return som_export_sample(ctx, Int32(i), path, format.cValue, normalize ? 1 : 0) == 0
        }
    }

    // MARK: language

    static let functionNames: [String] = (0..<Int(som_function_count())).map { String(cString: som_function_name(Int32($0))) }

    static func argCount(_ name: String) -> Int? {
        guard let i = functionNames.firstIndex(of: name) else { return nil }
        return Int(som_function_arg_count(Int32(i)))
    }

    static var version: Double { som_version() }
}
