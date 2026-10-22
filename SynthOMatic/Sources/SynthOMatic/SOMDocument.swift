import AppKit
import AVFoundation
import UniformTypeIdentifiers
import SOMEngine

extension Notification.Name {
    /// posted by a document when variables were added, removed or changed;
    /// userInfo["names"] is a Set<String> of changed names, absent when everything may have changed
    static let variablesDidChange = Notification.Name("SOMVariablesDidChange")
    static let globalsDidChange = Notification.Name("SOMGlobalsDidChange")
    static let compileDidFinish = Notification.Name("SOMCompileDidFinish")
    static let messagesDidChange = Notification.Name("SOMMessagesDidChange")
    static let calculationStateDidChange = Notification.Name("SOMCalculationStateDidChange")
}

/// Window controllers that edit one variable answer the menu commands for it.
protocol VariableEditor: AnyObject {
    var variableName: String { get }
}

/// One Synth-O-Matic patch: a program, its variables and its globals.
@objc(SOMDocument)
final class SOMDocument: NSDocument {
    static let patchType = "com.jamesmccartney.synthomatic.patch"

    let engine = Engine()
    let programText = NSTextStorage()
    let messages = NSTextStorage()

    /// the variables, in the order the original showed them
    private(set) var vars: [VarInfo] = []
    private(set) var isCalculating = false
    private var cancelRequested = false
    private var editors: [String: NSWindowController] = [:]

    override init() {
        super.init()
        engine.onMessage = { [weak self] text in
            if Thread.isMainThread {
                self?.appendMessage(text)
            } else {
                DispatchQueue.main.async { self?.appendMessage(text) }
            }
        }
        engine.newDocument()
        setProgramText(engine.program)
        reloadVariables(notify: false)
    }

    override class var autosavesInPlace: Bool { false }

    override class var readableTypes: [String] { [patchType, "public.data"] }
    override class var writableTypes: [String] { [patchType] }
    override class func isNativeType(_ type: String) -> Bool { type == patchType }

    override func writableTypes(for saveOperation: NSDocument.SaveOperationType) -> [String] { [SOMDocument.patchType] }

    override func makeWindowControllers() {
        addWindowController(MainWindowController())
    }

    var mainWindowController: MainWindowController? {
        windowControllers.first { $0 is MainWindowController } as? MainWindowController
    }

    var mainWindow: NSWindow? { mainWindowController?.window }

    // MARK: reading and writing

    override func read(from data: Data, ofType typeName: String) throws {
        guard Engine.isPatch(data) else {
            throw NSError(domain: NSCocoaErrorDomain, code: NSFileReadCorruptFileError,
                          userInfo: [NSLocalizedDescriptionKey: "This is not a Synth-O-Matic patch.",
                                     NSLocalizedRecoverySuggestionErrorKey: "Synth-O-Matic patches begin with 'SOM1'."])
        }
        messages.setAttributedString(NSAttributedString())
        guard engine.read(data) else {
            throw NSError(domain: NSCocoaErrorDomain, code: NSFileReadCorruptFileError,
                          userInfo: [NSLocalizedDescriptionKey: "The patch could not be read.",
                                     NSLocalizedRecoverySuggestionErrorKey: messages.string])
        }
        setProgramText(engine.program)
        // the original compiled every file it opened
        compile()
        reloadVariables(notify: true)
        fileType = SOMDocument.patchType
    }

    override func data(ofType typeName: String) throws -> Data {
        engine.program = programText.string
        return engine.write()
    }

    /// Prints the program and the values of the variables.
    override func printOperation(withSettings printSettings: [NSPrintInfo.AttributeKey: Any]) throws -> NSPrintOperation {
        let info = printInfo.copy() as! NSPrintInfo
        info.horizontalPagination = .fit
        info.isVerticallyCentered = false
        let width = info.paperSize.width - info.leftMargin - info.rightMargin
        let view = NSTextView(frame: NSRect(x: 0, y: 0, width: width, height: 100))
        let mono = NSFont.monospacedSystemFont(ofSize: 10, weight: .regular)
        let text = NSMutableAttributedString(string: displayName + "\n\n", attributes: [.font: NSFont.boldSystemFont(ofSize: 12)])
        text.append(NSAttributedString(string: programText.string + "\n\n", attributes: [.font: mono]))
        var table = String(format: "sample rate %.2f Hz, sample time %.3f s\n\n", engine.sampleRate, engine.duration)
        for v in vars {
            let value = v.type == .scalar ? FormSheet.format(v.value) : ""
            table += String(format: "%-20@ %-10@ %14@   [%@ .. %@]\n", v.name as NSString, v.typeName as NSString, value as NSString,
                            FormSheet.format(v.lolim) as NSString, FormSheet.format(v.hilim) as NSString)
        }
        text.append(NSAttributedString(string: table, attributes: [.font: mono]))
        view.textStorage?.setAttributedString(text)
        view.sizeToFit()
        return NSPrintOperation(view: view, printInfo: info)
    }

    private func setProgramText(_ s: String) {
        programText.beginEditing()
        programText.replaceCharacters(in: NSRange(location: 0, length: programText.length), with: s)
        programText.endEditing()
    }

    // MARK: messages

    func appendMessage(_ text: String) {
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedSystemFont(ofSize: 11, weight: .regular),
            .foregroundColor: NSColor.labelColor,
        ]
        messages.append(NSAttributedString(string: text, attributes: attrs))
        NotificationCenter.default.post(name: .messagesDidChange, object: self)
    }

    func clearMessages() {
        messages.setAttributedString(NSAttributedString())
        NotificationCenter.default.post(name: .messagesDidChange, object: self)
    }

    // MARK: variables

    func reloadVariables(notify: Bool = true, names: Set<String>? = nil) {
        guard !isCalculating else { return }
        vars = engine.sortedVars()
        let present = Set(vars.map(\.name))
        for (name, wc) in editors where !present.contains(name) {
            wc.close()
            editors[name] = nil
        }
        if notify {
            var info: [String: Any] = [:]
            if let names { info["names"] = names }
            NotificationCenter.default.post(name: .variablesDidChange, object: self, userInfo: info)
        }
    }

    func info(named name: String) -> VarInfo? { vars.first { $0.name == name } }

    func index(named name: String) -> Int? { info(named: name)?.index }

    /// The variables the menu commands apply to: the one in the front editor
    /// window, or the ones selected in the variable list.
    func targetNames() -> [String] {
        if let editor = NSApp.keyWindow?.windowController as? VariableEditor,
           (editor as? NSWindowController)?.document === self {
            return [editor.variableName]
        }
        return mainWindowController?.selectedVariableNames ?? []
    }

    func targetInfos() -> [VarInfo] { targetNames().compactMap(info(named:)) }

    func targetTable() -> VarInfo? { targetInfos().first { $0.type == .table || $0.type == .envelope } }

    func targetSample() -> VarInfo? { targetInfos().first { $0.type == .sample } }

    // MARK: undo

    /// Makes a change to variables undoable.  `body` changes them; the state
    /// before is restored on undo (and the state after on redo).
    func changeVariables(_ names: [String], actionName: String, _ body: () -> Void) {
        let before = names.compactMap { name in engine.index(named: name).map { engine.snapshot($0) } }
        body()
        registerUndo(before, actionName: actionName)
        reloadVariables(names: Set(names))
    }

    private func registerUndo(_ snapshots: [VarSnapshot], actionName: String) {
        guard let undoManager, !snapshots.isEmpty else { return }
        undoManager.registerUndo(withTarget: self) { doc in
            guard !doc.isCalculating else { return }
            let current = snapshots.compactMap { s in doc.engine.index(named: s.info.name).map { doc.engine.snapshot($0) } }
            snapshots.forEach(doc.engine.restore)
            doc.registerUndo(current, actionName: actionName)
            doc.reloadVariables(names: Set(snapshots.map(\.info.name)))
        }
        undoManager.setActionName(actionName)
    }

    // MARK: editing variables (used by the views)

    func setValue(_ value: Double, of name: String, actionName: String = "Set Value") {
        guard let i = engine.index(named: name) else { return }
        changeVariables([name], actionName: actionName) { engine.setValue(i, value) }
    }

    /// For continuous controls: the caller snapshots once at the start of a drag.
    func beginContinuousChange(_ name: String) -> VarSnapshot? {
        engine.index(named: name).map { engine.snapshot($0) }
    }

    func endContinuousChange(_ before: VarSnapshot?, actionName: String) {
        guard let before else { return }
        registerUndo([before], actionName: actionName)
        reloadVariables(names: [before.info.name])
    }

    /// Sets a value without undo or a full reload, while dragging.
    func setValueLive(_ value: Double, of name: String) {
        guard let i = engine.index(named: name) else { return }
        engine.setValue(i, value)
        if let k = vars.firstIndex(where: { $0.name == name }) { vars[k].value = value }
    }

    func setRandomSet(_ name: String, on: Bool) {
        var set = vars.filter(\.randomset).map(\.name)
        if on { set.append(name) } else { set.removeAll { $0 == name } }
        let all = vars.map(\.name)
        changeVariables(all, actionName: on ? "Add to Random Set" : "Remove from Random Set") {
            engine.makeRandomSet(set.compactMap { engine.index(named: $0) })
        }
    }

    // MARK: editor windows

    func openEditor(for name: String) {
        guard let v = info(named: name) else { return }
        if v.type == .scalar {
            mainWindowController?.window?.makeKeyAndOrderFront(nil)
            showSetValueSheet(for: name)
            return
        }
        if let wc = editors[name] {
            wc.showWindow(nil)
            return
        }
        let wc: NSWindowController = v.type == .sample ? SampleWindowController(document: self, name: name)
                                                       : TableEditorWindowController(document: self, name: name)
        editors[name] = wc
        addWindowController(wc)
        wc.showWindow(nil)
    }

    func editorClosed(_ name: String) {
        editors[name] = nil
    }

    func closeEditor(_ name: String) {
        if let wc = editors[name] {
            wc.close()
            editors[name] = nil
        }
    }

    override func removeWindowController(_ windowController: NSWindowController) {
        if let e = windowController as? VariableEditor, editors[e.variableName] === windowController {
            editors[e.variableName] = nil
        }
        super.removeWindowController(windowController)
    }

    // MARK: compile and calculate

    /// Compiles the program in the editor.  Returns success.
    @discardableResult
    func compile(quiet: Bool = false) -> Bool {
        guard !isCalculating else { return false }
        engine.program = programText.string
        let ok = engine.compile()
        if ok && !quiet { appendMessage("Compile done.\n") }
        reloadVariables()
        NotificationCenter.default.post(name: .compileDidFinish, object: self,
                                        userInfo: ["errorLine": ok ? 0 : engine.errorLine])
        return ok
    }

    func calculate(thenPlay: Bool) {
        guard !isCalculating else { return }
        // as the original: Calculate compiles first
        guard compile(quiet: true) else {
            NSSound.beep()
            return
        }
        isCalculating = true
        cancelRequested = false
        NotificationCenter.default.post(name: .calculationStateDidChange, object: self)
        mainWindowController?.calculationStarted()
        let engine = self.engine
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            var last = Date.distantPast
            let result = engine.calculate { done, total in
                let now = Date()
                if now.timeIntervalSince(last) > 0.05 || done == total {
                    last = now
                    DispatchQueue.main.async { self?.mainWindowController?.calculationProgress(done: done, total: total) }
                }
                return self?.cancelRequested ?? true
            }
            DispatchQueue.main.async { self?.calculationFinished(result, thenPlay: thenPlay) }
        }
    }

    private func calculationFinished(_ result: Engine.CalcResult, thenPlay: Bool) {
        isCalculating = false
        mainWindowController?.calculationFinished()
        reloadVariables()
        NotificationCenter.default.post(name: .calculationStateDidChange, object: self)
        // redraw editors that skipped drawing while calculating
        for wc in windowControllers { wc.window?.contentView?.needsDisplay = true; wc.window?.contentView?.subviews.forEach { $0.needsDisplay = true } }
        guard result == .ok else { return }
        if let first = engine.firstOutputSample {
            let name = engine.info(first).name
            mainWindowController?.selectVariables([name])
            if thenPlay { play(name) }
        }
    }

    func stopCalculating() {
        if isCalculating { cancelRequested = true }
        AudioPlayer.shared.stop()
    }

    // MARK: playing

    func play(_ name: String, rate: Double = 1) {
        guard !isCalculating, let i = engine.index(named: name) else { return }
        let v = engine.info(i)
        guard v.type == .sample else { NSSound.beep(); return }
        var samples = engine.points(i)
        if samples.count > engine.sampleLength { samples.removeLast(samples.count - engine.sampleLength) }
        AudioPlayer.shared.play(samples, sampleRate: engine.sampleRate, rate: rate,
                                normalize: UserDefaults.standard.object(forKey: "NormalizePlayback") as? Bool ?? true,
                                owner: self, tag: name)
    }

    /// The keyboard plays the selected sample transposed, as the original did.
    func playKey(_ key: Character) -> Bool {
        guard let rate = AudioPlayer.rate(forKey: key) else { return false }
        guard let s = targetSample() ?? info(named: playableDefault() ?? "") else { return false }
        play(s.name, rate: rate)
        return true
    }

    private func playableDefault() -> String? {
        engine.firstOutputSample.map { engine.info($0).name }
    }

    // MARK: validation

    override func validateUserInterfaceItem(_ item: NSValidatedUserInterfaceItem) -> Bool {
        guard let action = item.action else { return super.validateUserInterfaceItem(item) }
        if isCalculating {
            if action == #selector(stop(_:)) { return true }
            if [#selector(compile(_:)), #selector(calculate(_:)), #selector(calculateAndPlay(_:)),
                #selector(save(_:)), #selector(saveAs(_:)), #selector(revertToSaved(_:))].contains(action) { return false }
            if SOMDocument.variableActions.contains(action) { return false }
        }
        switch action {
        case #selector(stop(_:)):
            return isCalculating || AudioPlayer.shared.isPlaying
        case #selector(play(_:)), #selector(exportSample(_:)):
            return targetSample() != nil || (action == #selector(play(_:)) && engine.firstOutputSample != nil)
        case #selector(importSound(_:)):
            return !targetNames().isEmpty
        case #selector(makeRandomSet(_:)):
            return true
        case #selector(randomize(_:)):
            return vars.contains(where: \.randomset)
        case #selector(removeVars(_:)), #selector(setTypeToWavetable(_:)), #selector(setTypeToVariable(_:)),
             #selector(setTypeToEnvelope(_:)), #selector(setTypeToSample(_:)), #selector(editVariable(_:)):
            return !targetNames().isEmpty
        case #selector(addClipboard(_:)), #selector(multClipboard(_:)):
            return targetTable() != nil && (SOMDocument.scrap?.info.type == .table || SOMDocument.scrap?.info.type == .envelope)
        default:
            if SOMDocument.tableActions.contains(action) { return targetTable() != nil }
        }
        return super.validateUserInterfaceItem(item)
    }

    static let tableActions: Set<Selector> = [
        #selector(fourierFill(_:)), #selector(fmTableFill(_:)), #selector(gbuzzTableFill(_:)),
        #selector(grainEnvelopeFill(_:)), #selector(reverseTable(_:)), #selector(invertTable(_:)),
        #selector(absTable(_:)), #selector(clipNegative(_:)), #selector(squareTable(_:)),
        #selector(squashUp(_:)), #selector(squashDown(_:)), #selector(squashIn(_:)), #selector(squashOut(_:)),
        #selector(hanningWindow(_:)), #selector(parzenWindow(_:)), #selector(welchWindow(_:)),
        #selector(normalizeTable(_:)), #selector(combTable(_:)), #selector(smoothTable(_:)),
        #selector(analyzeTable(_:)), #selector(autoScale(_:)), #selector(setBounds(_:)),
    ]

    static let variableActions: Set<Selector> = tableActions.union([
        #selector(removeVars(_:)), #selector(removeAllUnusedVars(_:)), #selector(setTypeToWavetable(_:)),
        #selector(setTypeToVariable(_:)), #selector(setTypeToEnvelope(_:)), #selector(setTypeToSample(_:)),
        #selector(randomize(_:)), #selector(makeRandomSet(_:)), #selector(setGlobals(_:)),
        #selector(instrumentIO(_:)), #selector(addClipboard(_:)), #selector(multClipboard(_:)),
        #selector(importSound(_:)), #selector(editVariable(_:)), #selector(play(_:)),
    ])
}

// MARK: - Synth menu

extension SOMDocument {
    @objc func compile(_ sender: Any?) {
        clearMessages()
        compile()
    }

    @objc func calculate(_ sender: Any?) { calculate(thenPlay: false) }

    @objc func calculateAndPlay(_ sender: Any?) { calculate(thenPlay: true) }

    @objc func stop(_ sender: Any?) { stopCalculating() }

    @objc func play(_ sender: Any?) {
        if let s = targetSample() { play(s.name) } else if let n = playableDefault() { play(n) }
    }

    @objc func makeRandomSet(_ sender: Any?) {
        let names = targetNames()
        changeVariables(vars.map(\.name), actionName: "Make Random Set") {
            engine.makeRandomSet(names.compactMap { engine.index(named: $0) })
        }
    }

    @objc func randomize(_ sender: Any?) {
        changeVariables(vars.filter(\.randomset).map(\.name), actionName: "Randomize") {
            engine.randomize()
        }
    }

    @objc func removeVars(_ sender: Any?) { removeUnused(all: false) }

    @objc func removeAllUnusedVars(_ sender: Any?) { removeUnused(all: true) }

    private func removeUnused(all: Bool) {
        let selected = targetNames().compactMap { engine.index(named: $0) }
        let n = engine.removeUnused(selected, all: all)
        if n == 0 {
            appendMessage(all ? "There are no unused variables.\n" : "Only variables the program doesn't use can be removed.\n")
            NSSound.beep()
        } else {
            appendMessage("Removed \(n) unused variable\(n == 1 ? "" : "s").\n")
            undoManager?.removeAllActions()
            updateChangeCount(.changeDone)
            compile(quiet: true)
        }
        reloadVariables()
    }

    @objc func memoryReport(_ sender: Any?) {
        clearMessages()
        engine.memoryReport()
    }
}

// MARK: - Signals menu

extension SOMDocument {
    @objc func setGlobals(_ sender: Any?) {
        guard let window = mainWindow else { return }
        let fields = [
            FormSheet.Field(label: "Sample Rate :", value: String(format: "%.2f", engine.sampleRate), unit: "Hz"),
            FormSheet.Field(label: "Sample Time :", value: String(format: "%.3f", engine.duration), unit: "sec."),
            FormSheet.Field(label: "Max Delay Time :", value: String(format: "%.3f", engine.maxDelay), unit: "sec."),
        ]
        FormSheet.present(on: window, title: "Set Globals :", fields: fields) { [weak self] r in
            guard let self, let r, let sr = r.number(0), let dur = r.number(1), let dly = r.number(2) else { return }
            guard sr >= 100, sr <= 768000, dur > 0, dur <= 3600 else {
                NSSound.beep()
                self.appendMessage("The sample rate must be between 100 and 768000 Hz and the time between 0 and 3600 seconds.\n")
                return
            }
            self.changeGlobals(actionName: "Set Globals") {
                self.engine.setGlobals(sampleRate: sr, duration: dur, maxDelay: max(0, dly))
            }
        }
    }

    func changeGlobals(actionName: String, _ body: () -> Void) {
        let before = (engine.sampleRate, engine.duration, engine.maxDelay)
        body()
        undoManager?.registerUndo(withTarget: self) { doc in
            doc.changeGlobals(actionName: actionName) {
                doc.engine.setGlobals(sampleRate: before.0, duration: before.1, maxDelay: before.2)
            }
        }
        undoManager?.setActionName(actionName)
        reloadVariables()
        NotificationCenter.default.post(name: .globalsDidChange, object: self)
    }

    @objc func instrumentIO(_ sender: Any?) {
        guard let window = mainWindow else { return }
        let fields = [
            FormSheet.Field(label: "Sound Input File :", value: engine.inFilename, isNumber: false),
            FormSheet.Field(label: "Sound Output File :", value: engine.outFilename, isNumber: false),
        ]
        FormSheet.present(on: window, title: "Instrument I/O :",
                          message: "The file names the original used for its disk based mode. They are kept with the patch.",
                          fields: fields) { [weak self] r in
            guard let self, let r else { return }
            let old = (self.engine.inFilename, self.engine.outFilename)
            self.engine.setIOFilenames(input: r.strings[0], output: r.strings[1])
            self.undoManager?.registerUndo(withTarget: self) { doc in doc.engine.setIOFilenames(input: old.0, output: old.1) }
            self.undoManager?.setActionName("Instrument I/O")
        }
    }

    private func setType(_ type: VarType, _ title: String) {
        let names = targetNames()
        for n in names where info(named: n)?.type != type { closeEditor(n) }
        changeVariables(names, actionName: title) {
            for n in names { if let i = engine.index(named: n) { engine.setType(i, type) } }
        }
    }

    @objc func setTypeToWavetable(_ sender: Any?) { setType(.table, "Set Type to Wavetable") }
    @objc func setTypeToVariable(_ sender: Any?) { setType(.scalar, "Set Type to Variable") }
    @objc func setTypeToEnvelope(_ sender: Any?) { setType(.envelope, "Set Type to Envelope") }
    @objc func setTypeToSample(_ sender: Any?) { setType(.sample, "Set Type to Sample") }

    @objc func editVariable(_ sender: Any?) {
        if let n = targetNames().first { openEditor(for: n) }
    }

    /// Loads a sound file into a sample variable, mixed to mono at the patch's sample rate.
    @objc func importSound(_ sender: Any?) {
        guard let target = targetNames().first, let window = NSApp.keyWindow ?? mainWindow else { return }
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.audio]
        panel.message = "Choose a sound to load into '\(target)'."
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url, let self else { return }
            do {
                let samples = try SOMDocument.readSound(url, sampleRate: self.engine.sampleRate)
                self.changeVariables([target], actionName: "Import Sound") {
                    if let i = self.engine.index(named: target) { self.engine.setSampleData(i, samples) }
                }
                let secs = Double(samples.count) / self.engine.sampleRate
                self.appendMessage(String(format: "Imported %@ into '%@' (%.3f seconds).\n", url.lastPathComponent, target, secs))
                if secs > self.engine.duration + 0.001 {
                    self.appendMessage(String(format: "The sound is longer than the Sample Time (%.3f s); set the globals to use all of it.\n", self.engine.duration))
                }
            } catch {
                self.presentError(error)
            }
        }
    }

    static func readSound(_ url: URL, sampleRate: Double) throws -> [Float] {
        let file = try AVAudioFile(forReading: url)
        let inFormat = file.processingFormat
        guard let inBuffer = AVAudioPCMBuffer(pcmFormat: inFormat, frameCapacity: AVAudioFrameCount(file.length)) else {
            throw NSError(domain: NSOSStatusErrorDomain, code: -1, userInfo: [NSLocalizedDescriptionKey: "The sound is too long."])
        }
        try file.read(into: inBuffer)
        // mix every channel down evenly
        let channels = Int(inFormat.channelCount), frames = Int(inBuffer.frameLength)
        var mono = [Float](repeating: 0, count: frames)
        if let data = inBuffer.floatChannelData {
            for c in 0..<channels {
                for f in 0..<frames { mono[f] += data[c][f] }
            }
            if channels > 1 { for f in 0..<frames { mono[f] /= Float(channels) } }
        }
        if abs(inFormat.sampleRate - sampleRate) < 0.5 { return mono }

        guard let monoFormat = AVAudioFormat(standardFormatWithSampleRate: inFormat.sampleRate, channels: 1),
              let outFormat = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 1),
              let converter = AVAudioConverter(from: monoFormat, to: outFormat),
              let monoBuffer = AVAudioPCMBuffer(pcmFormat: monoFormat, frameCapacity: AVAudioFrameCount(max(frames, 1))) else {
            throw NSError(domain: NSOSStatusErrorDomain, code: -1, userInfo: [NSLocalizedDescriptionKey: "Can't convert this sound."])
        }
        monoBuffer.frameLength = AVAudioFrameCount(frames)
        mono.withUnsafeBufferPointer { monoBuffer.floatChannelData![0].update(from: $0.baseAddress!, count: frames) }
        let capacity = AVAudioFrameCount(Double(frames) * sampleRate / inFormat.sampleRate + 4096)
        guard let outBuffer = AVAudioPCMBuffer(pcmFormat: outFormat, frameCapacity: capacity) else { return mono }
        var fed = false
        var error: NSError?
        converter.convert(to: outBuffer, error: &error) { _, status in
            if fed { status.pointee = .endOfStream; return nil }
            fed = true
            status.pointee = .haveData
            return monoBuffer
        }
        if let error { throw error }
        return Array(UnsafeBufferPointer(start: outBuffer.floatChannelData![0], count: Int(outBuffer.frameLength)))
    }

    @objc func exportSample(_ sender: Any?) {
        guard let s = targetSample(), let window = NSApp.keyWindow ?? mainWindow else { return }
        let others = vars.filter { $0.type == .sample && $0.name != s.name }
        let accessory = ExportAccessory(stereoCandidates: others.map(\.name), suggested: SOMDocument.stereoPartner(of: s.name, among: others.map(\.name)))
        let panel = NSSavePanel()
        panel.nameFieldStringValue = (displayName as NSString).deletingPathExtension + " " + s.name
        panel.accessoryView = accessory.view
        accessory.panel = panel
        accessory.update()
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, var url = panel.url, let self else { return }
            let format = accessory.format
            if url.pathExtension.lowercased() != format.fileExtension {
                url = url.deletingPathExtension().appendingPathExtension(format.fileExtension)
            }
            let right = accessory.stereoPartner.flatMap { self.engine.index(named: $0) }
            if self.engine.export(s.index, stereoWith: right, to: url, format: format, normalize: accessory.normalize) {
                self.appendMessage("Wrote \(url.lastPathComponent)\n")
            } else {
                NSSound.beep()
            }
        }
    }

    static func stereoPartner(of name: String, among names: [String]) -> String? {
        for (l, r) in [("L", "R"), ("l", "r"), ("1", "2"), ("left", "right")] where name.hasSuffix(l) {
            let candidate = String(name.dropLast(l.count)) + r
            if names.contains(candidate) { return candidate }
        }
        return nil
    }
}

// MARK: - Fill and Alter menus

extension SOMDocument {
    private func alter(_ op: SOMAlterOp, _ title: String) {
        guard let t = targetTable() else { return }
        let clip = SOMDocument.scrap?.data
        changeVariables([t.name], actionName: title) { engine.alter(t.index, op, clip: clip) }
    }

    @objc func reverseTable(_ sender: Any?) { alter(SOMAlterReverse, "Reverse") }
    @objc func invertTable(_ sender: Any?) { alter(SOMAlterInvert, "Invert") }
    @objc func absTable(_ sender: Any?) { alter(SOMAlterAbs, "Absolute Value") }
    @objc func clipNegative(_ sender: Any?) { alter(SOMAlterClipNegative, "Clip Negative") }
    @objc func squareTable(_ sender: Any?) { alter(SOMAlterSquare, "Square") }
    @objc func addClipboard(_ sender: Any?) { alter(SOMAlterAdd, "Add Clipboard") }
    @objc func multClipboard(_ sender: Any?) { alter(SOMAlterMultiply, "Mult Clipboard") }
    @objc func squashUp(_ sender: Any?) { alter(SOMAlterSquashUp, "Squash Up") }
    @objc func squashDown(_ sender: Any?) { alter(SOMAlterSquashDown, "Squash Down") }
    @objc func squashIn(_ sender: Any?) { alter(SOMAlterSquashIn, "Squash In") }
    @objc func squashOut(_ sender: Any?) { alter(SOMAlterSquashOut, "Squash Out") }
    @objc func hanningWindow(_ sender: Any?) { alter(SOMAlterHanning, "Hanning Window") }
    @objc func parzenWindow(_ sender: Any?) { alter(SOMAlterParzen, "Parzen Window") }
    @objc func welchWindow(_ sender: Any?) { alter(SOMAlterWelch, "Welch Window") }
    @objc func normalizeTable(_ sender: Any?) { alter(SOMAlterNormalize, "Normalize") }
    @objc func smoothTable(_ sender: Any?) { alter(SOMAlterSmooth, "Smooth Table") }
    @objc func analyzeTable(_ sender: Any?) { alter(SOMAlterAnalyze, "Analyze Table") }

    @objc func autoScale(_ sender: Any?) {
        guard let t = targetTable() ?? targetSample() else { return }
        changeVariables([t.name], actionName: "Auto Scale") {
            if t.type == .sample { engine.autoScaleExact(t.index) } else { engine.autoScale(t.index) }
        }
    }

    @objc func setBounds(_ sender: Any?) {
        guard let t = targetTable() ?? targetSample() else { return }
        showBoundsSheet(for: t.name, on: NSApp.keyWindow ?? mainWindow)
    }

    @objc func fourierFill(_ sender: Any?) {
        guard let t = targetTable() else { return }
        let wc = FillTableWindowController(document: self, name: t.name)
        addWindowController(wc)
        wc.showWindow(nil)
    }

    private static var fmValues = (1.0, 1.0, 1.0)
    private static var gbuzzValues = (16.0, 1.0, 1.0)
    private static var grainValues = (0, 50.0)
    private static var combValues = (0, 4.0)

    @objc func fmTableFill(_ sender: Any?) {
        guard let t = targetTable(), let window = NSApp.keyWindow ?? mainWindow else { return }
        let v = SOMDocument.fmValues
        FormSheet.present(on: window, title: "FM table fill routine :", fields: [
            .init(label: "Carrier Freq :", value: FormSheet.format(v.0)),
            .init(label: "Modulator Freq :", value: FormSheet.format(v.1)),
            .init(label: "Modulation Index :", value: String(format: "%.2f", v.2)),
        ]) { [weak self] r in
            guard let self, let r, let c = r.number(0), let m = r.number(1), let x = r.number(2) else { return }
            SOMDocument.fmValues = (c, m, x)
            self.changeVariables([t.name], actionName: "FM Fill") { self.engine.fillFM(t.index, carrier: c, modulator: m, index: x) }
        }
    }

    @objc func gbuzzTableFill(_ sender: Any?) {
        guard let t = targetTable(), let window = NSApp.keyWindow ?? mainWindow else { return }
        let v = SOMDocument.gbuzzValues
        FormSheet.present(on: window, title: "GBuzz table fill routine :", fields: [
            .init(label: "Number of Harmonics :", value: FormSheet.format(v.0)),
            .init(label: "Lowest Harmonic :", value: FormSheet.format(v.1)),
            .init(label: "Amplitude Series Coef. :", value: String(format: "%.2f", v.2)),
        ]) { [weak self] r in
            guard let self, let r, let n = r.number(0), let k = r.number(1), let c = r.number(2) else { return }
            guard n >= 1 else { NSSound.beep(); return }
            SOMDocument.gbuzzValues = (n, k, c)
            self.changeVariables([t.name], actionName: "GBuzz Fill") { self.engine.fillGBuzz(t.index, n: n, k: k, r: c) }
        }
    }

    @objc func grainEnvelopeFill(_ sender: Any?) {
        guard let t = targetTable(), let window = NSApp.keyWindow ?? mainWindow else { return }
        let v = SOMDocument.grainValues
        FormSheet.present(on: window, title: "Grain Envelope Table Fill :",
                          fields: [.init(label: "Grain Duration :", value: FormSheet.format(v.1), unit: "%")],
                          choice: .init(label: "Attack/Decay Shape :", options: ["Hanning", "Welch", "Parzen"], selected: v.0)) { [weak self] r in
            guard let self, let r, let d = r.number(0) else { return }
            SOMDocument.grainValues = (r.choice, d)
            self.changeVariables([t.name], actionName: "Grain Env. Fill") { self.engine.fillGrainEnv(t.index, shape: r.choice, duration: d) }
        }
    }

    @objc func combTable(_ sender: Any?) {
        guard let t = targetTable(), let window = NSApp.keyWindow ?? mainWindow else { return }
        let v = SOMDocument.combValues
        FormSheet.present(on: window, title: "Comb Table :",
                          fields: [.init(label: "Number of Teeth :", value: String(format: "%.2f", v.1))],
                          choice: .init(label: "Harmonics :", options: ["Odd (subtract)", "Even (add)"], selected: v.0)) { [weak self] r in
            guard let self, let r, let teeth = r.number(0), teeth > 0 else { return }
            SOMDocument.combValues = (r.choice, teeth)
            self.changeVariables([t.name], actionName: "Comb Table") { self.engine.combTable(t.index, even: r.choice == 1, teeth: teeth) }
        }
    }
}

// MARK: - dialogs for one variable

extension SOMDocument {
    func showSetValueSheet(for name: String) {
        guard let v = info(named: name), let window = mainWindow else { return }
        let steps = v.stepsiz != 0 ? ((v.hilim - v.lolim) / v.stepsiz).rounded() : 0
        FormSheet.present(on: window, title: "Set Value of :  \(name)", fields: [
            .init(label: "Value :", value: FormSheet.format(v.value)),
            .init(label: "Lower Limit :", value: FormSheet.format(v.lolim)),
            .init(label: "Upper Limit :", value: FormSheet.format(v.hilim)),
            .init(label: "Number of Steps :", value: String(format: "%.0f", steps), unit: "0 = continuous"),
            .init(label: "P-Field :", value: "\(v.pfield)", unit: "0 = none"),
        ]) { [weak self] r in
            guard let self, let r, let value = r.number(0), let lo = r.number(1), let hi = r.number(2),
                  let n = r.number(3), let p = r.number(4) else { return }
            self.changeVariables([name], actionName: "Set Value") {
                if let i = self.engine.index(named: name) {
                    self.engine.setScalarParams(i, value: value, lo: lo, hi: hi, steps: n, pfield: Int(p))
                }
            }
        }
    }

    func showBoundsSheet(for name: String, on window: NSWindow?) {
        guard let v = info(named: name), let window else { return }
        FormSheet.present(on: window, title: "Set Graph Bounds :  \(name)", fields: [
            .init(label: "Upper Limit :", value: FormSheet.format(v.hilim)),
            .init(label: "Lower Limit :", value: FormSheet.format(v.lolim)),
        ], actions: [
            .init(title: "Auto Scale") { [weak self] sheet in
                guard let self, let i = self.engine.index(named: name) else { return }
                let l = self.engine.findLimits(i)
                sheet.setValue(FormSheet.format(l.max), at: 0)
                sheet.setValue(FormSheet.format(l.min), at: 1)
            },
            .init(title: "Current Settings") { sheet in
                sheet.setValue(FormSheet.format(v.hilim), at: 0)
                sheet.setValue(FormSheet.format(v.lolim), at: 1)
            },
        ]) { [weak self] r in
            guard let self, let r, let hi = r.number(0), let lo = r.number(1) else { return }
            self.changeVariables([name], actionName: "Set Bounds") {
                if let i = self.engine.index(named: name) { self.engine.setGraphBounds(i, hi: hi, lo: lo) }
            }
        }
    }
}

// MARK: - copying variables

extension SOMDocument {
    /// The original's private scrap for variables (also used by Add/Mult Clipboard).
    static var scrap: VarSnapshot?
    static let pasteboardType = NSPasteboard.PasteboardType("com.jamesmccartney.synthomatic.variable")

    func copyVariable(_ name: String) {
        guard let i = engine.index(named: name) else { return }
        let s = engine.snapshot(i)
        guard s.info.type != .sample else { NSSound.beep(); return }
        SOMDocument.scrap = s
        let pb = NSPasteboard.general
        pb.clearContents()
        let text: String
        if s.info.type == .scalar {
            text = FormSheet.format(s.info.value)
        } else {
            text = s.data.map { String(format: "%g", $0) }.joined(separator: "\n")
        }
        pb.setString(text, forType: .string)
        pb.setString(name, forType: SOMDocument.pasteboardType)
    }

    func pasteVariable(into name: String) {
        guard let scrap = SOMDocument.scrap, let i = engine.index(named: name) else { NSSound.beep(); return }
        let target = engine.info(i)
        guard target.type != .sample else { NSSound.beep(); return }
        if target.type != scrap.info.type { closeEditor(name) }
        changeVariables([name], actionName: "Paste") {
            var s = scrap
            s.info.name = name
            s.info.pfield = target.pfield
            s.info.randomset = target.randomset
            engine.restore(s)
        }
    }
}
