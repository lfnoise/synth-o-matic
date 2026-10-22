import AppKit

/// `SynthOMatic --selftest` exercises the document layer the way the menus
/// do - editing, undo, the Fill and Alter commands, saving and reopening,
/// exporting and importing - and prints a report.  Exit status 0 if all passed.
enum SelfTest {
    private static var failures = 0
    private static var checks = 0

    private static func check(_ ok: Bool, _ what: String) {
        checks += 1
        if !ok { failures += 1 }
        print((ok ? "  ok   " : "  FAIL ") + what)
    }

    private static func waitUntil(_ condition: () -> Bool, timeout: TimeInterval = 10) {
        let end = Date().addingTimeInterval(timeout)
        while !condition() && Date() < end {
            RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.01))
        }
    }

    static func run() {
        setvbuf(stdout, nil, _IONBF, 0)
        let tmp = FileManager.default.temporaryDirectory.appendingPathComponent("som-selftest-\(getpid())")
        try? FileManager.default.createDirectory(at: tmp, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: tmp) }

        print("new document")
        guard let doc = try? NSDocumentController.shared.makeUntitledDocument(ofType: SOMDocument.patchType) as? SOMDocument else {
            print("  FAIL can't make a document"); exit(1)
        }
        NSDocumentController.shared.addDocument(doc)
        // one undo group per action, as each menu command or click would be
        let um = doc.undoManager!
        um.groupsByEvent = false
        func act(_ f: () -> Void) { um.beginUndoGrouping(); f(); um.endUndoGrouping() }
        doc.makeWindowControllers()
        doc.showWindows()
        check(doc.programText.string == "freq = midicps(note);\n", "File > New program")
        check(doc.info(named: "note")?.value == 60 && doc.info(named: "note")?.pfield == 5, "note is 60, p5")

        print("compile and calculate")
        doc.engine.setGlobals(sampleRate: 22050, duration: 0.5, maxDelay: 1)
        doc.programText.replaceCharacters(in: NSRange(location: 0, length: doc.programText.length), with:
            "freq = midicps(note);\nout = amp * oscili(wavetbl, freq, 0) * ampenv;\nout = pan(out, 0, outL, outR);\n")
        check(doc.compile(), "compile")
        check(doc.info(named: "wavetbl")?.type == .table, "wavetbl is a wavetable")
        check(doc.info(named: "ampenv")?.type == .envelope, "ampenv is an envelope")
        check(doc.info(named: "outL")?.type == .sample, "outL is a sample")
        check(doc.info(named: "amp")?.isParameter == true, "amp is a parameter")
        check(doc.info(named: "freq")?.isParameter == false, "freq is computed")
        act { doc.setValue(0.5, of: "amp") }
        doc.calculate(thenPlay: false)
        waitUntil { !doc.isCalculating }
        let out = doc.engine.points(doc.index(named: "out")!)
        let peak = out.map(abs).max() ?? 0
        check(abs(peak - 0.5) < 0.01, "output peak is amp (\(peak))")
        check(doc.mainWindowController?.selectedVariableNames == ["out"], "first output selected after calculating")

        print("a compile error")
        // NSTextStorage.string is the live backing store: copy it
        let good = String(doc.programText.string.unicodeScalars)
        doc.programText.replaceCharacters(in: NSRange(location: 0, length: doc.programText.length), with: "a = 1;\nb = 2 +;\n")
        check(!doc.compile(), "error found")
        check(doc.engine.errorLine == 2, "on line 2")
        doc.programText.replaceCharacters(in: NSRange(location: 0, length: doc.programText.length), with: good)
        check(doc.compile(), "compiles again")

        print("undo")
        act { doc.setValue(0.25, of: "amp") }
        check(doc.info(named: "amp")?.value == 0.25, "set value")
        um.undo()
        check(doc.info(named: "amp")?.value == 0.5, "undo set value")
        um.redo()
        check(doc.info(named: "amp")?.value == 0.25, "redo set value")

        print("alter and fill")
        doc.mainWindowController?.selectVariables(["wavetbl"])
        let sine = doc.engine.points(doc.index(named: "wavetbl")!)
        act { doc.reverseTable(nil) }
        let reversed = doc.engine.points(doc.index(named: "wavetbl")!)
        check(reversed != sine && reversed[1] == sine[510] && reversed[0] == sine[0], "reverse (the original swaps i and 511 - i)")
        um.undo()
        check(doc.engine.points(doc.index(named: "wavetbl")!) == sine, "undo reverse")
        for (name, action) in [("invert", #selector(SOMDocument.invertTable(_:))), ("hanning", #selector(SOMDocument.hanningWindow(_:))),
                               ("smooth", #selector(SOMDocument.smoothTable(_:))), ("normalize", #selector(SOMDocument.normalizeTable(_:))),
                               ("squash up", #selector(SOMDocument.squashUp(_:))), ("analyze", #selector(SOMDocument.analyzeTable(_:)))] {
            act { _ = doc.perform(action, with: nil) }
            check(doc.engine.points(doc.index(named: "wavetbl")!).allSatisfy(\.isFinite), name)
        }
        for _ in 0..<6 { um.undo() }
        check(doc.engine.points(doc.index(named: "wavetbl")!) == sine, "undo six alterations")
        let t = doc.index(named: "wavetbl")!
        act { doc.changeVariables(["wavetbl"], actionName: "FM Fill") { doc.engine.fillFM(t, carrier: 1, modulator: 2, index: 3) } }
        check(doc.engine.points(t) != sine, "FM fill")
        var ampl = [UInt8](repeating: 0, count: 32); ampl[0] = 99; ampl[1] = 60
        let phas = [UInt8](repeating: 0, count: 32)
        act { doc.changeVariables(["wavetbl"], actionName: "Fourier Fill") { doc.engine.sineFill(t, ampl: ampl, phas: phas) } }
        let h = doc.engine.analyzeHarmonics(t)
        check(h.ampl[0] == 99 && abs(Int(h.ampl[1]) - 60) <= 2, "sine fill then analysis (\(h.ampl[0]), \(h.ampl[1]))")

        print("types, copy and paste")
        doc.mainWindowController?.selectVariables(["amp"])
        act { doc.setTypeToWavetable(nil) }
        check(doc.info(named: "amp")?.type == .table, "set type to wavetable")
        um.undo()
        check(doc.info(named: "amp")?.type == .scalar && doc.info(named: "amp")?.value == 0.25, "undo set type")
        doc.copyVariable("wavetbl")
        _ = doc.engine.defineVar("othertbl", type: .table)
        doc.reloadVariables()
        act { doc.pasteVariable(into: "othertbl") }
        guard let other = doc.index(named: "othertbl") else {
            check(false, "othertbl exists: \(doc.vars.map(\.name))"); exit(1)
        }
        check(doc.engine.points(other) == doc.engine.points(t), "paste a wavetable")
        doc.mainWindowController?.selectVariables(["othertbl"])
        doc.removeVars(nil)
        check(doc.info(named: "othertbl") == nil, "remove an unused variable")
        check(doc.engine.isCompiled, "recompiled after removing")

        print("random set")
        doc.mainWindowController?.selectVariables(["amp", "note"])
        act { doc.makeRandomSet(nil) }
        check(doc.info(named: "amp")?.randomset == true && doc.info(named: "note")?.randomset == true, "make random set")
        act { doc.randomize(nil) }
        let note = doc.info(named: "note")!.value
        check(note >= 20 && note <= 108 && note == note.rounded(), "randomize respects limits and steps (\(note))")

        print("save and reopen")
        let file = tmp.appendingPathComponent("test.som")
        do {
            try doc.write(to: file, ofType: SOMDocument.patchType)
            let reopened = SOMDocument()
            try reopened.read(from: Data(contentsOf: file), ofType: SOMDocument.patchType)
            check(reopened.programText.string == doc.programText.string, "program survives")
            check(reopened.info(named: "amp")?.value == doc.info(named: "amp")?.value, "values survive")
            check(reopened.engine.points(reopened.index(named: "wavetbl")!) == doc.engine.points(t), "tables survive")
            check(reopened.engine.sampleRate == 22050 && abs(reopened.engine.duration - 0.5) < 0.001, "globals survive")
            check(reopened.engine.isCompiled, "compiled on opening")
        } catch {
            check(false, "save: \(error)")
        }

        print("export and import")
        let wav = tmp.appendingPathComponent("out.wav")
        let outIndex = doc.index(named: "outL")!
        check(doc.engine.export(outIndex, stereoWith: doc.index(named: "outR"), to: wav, format: .wav16, normalize: true), "export stereo WAV")
        do {
            let samples = try SOMDocument.readSound(wav, sampleRate: 22050)
            check(abs(samples.count - doc.engine.sampleLength) <= 1, "import length (\(samples.count))")
            let resampled = try SOMDocument.readSound(wav, sampleRate: 44100)
            check(abs(resampled.count - 2 * doc.engine.sampleLength) < 64, "import with resampling (\(resampled.count))")
            let i = doc.engine.defineVar("insnd", type: .sample)
            doc.reloadVariables()
            act { doc.changeVariables(["insnd"], actionName: "Import Sound") { doc.engine.setSampleData(i, samples) } }
            check(doc.engine.points(i).map(abs).max() ?? 0 > 0.9, "imported sound is loud")
        } catch {
            check(false, "import: \(error)")
        }

        print("playing")
        doc.play("out", rate: 1)
        waitUntil({ AudioPlayer.shared.position ?? 0 > 0 }, timeout: 3)
        check(AudioPlayer.shared.isPlaying && (AudioPlayer.shared.position ?? 0) > 0, "plays (position \(AudioPlayer.shared.position ?? -1))")
        AudioPlayer.shared.stop()
        check(!AudioPlayer.shared.isPlaying, "stops")
        check(AudioPlayer.rate(forKey: "a") == 1 && AudioPlayer.rate(forKey: "s") == 1.125 && AudioPlayer.rate(forKey: "q") == 2
              && AudioPlayer.rate(forKey: "z") == 0.5 && AudioPlayer.rate(forKey: "!") == 16, "the original key layout")
        doc.play("out", rate: 16)
        waitUntil({ AudioPlayer.shared.position ?? 0 > 0 }, timeout: 3)
        check(AudioPlayer.shared.isPlaying, "plays four octaves up")
        doc.play("out", rate: 0.125)
        waitUntil({ AudioPlayer.shared.position ?? 0 > 0 }, timeout: 3)
        check(AudioPlayer.shared.isPlaying, "plays three octaves down")
        AudioPlayer.shared.stop()

        print("menus")
        NSApp.activate(ignoringOtherApps: true)
        doc.mainWindow?.makeKeyAndOrderFront(nil)
        waitUntil({ NSApp.keyWindow === doc.mainWindow }, timeout: 2)
        if NSApp.keyWindow == nil {
            // launched from a terminal, macOS may not let the app become active
            print("  skip the app isn't active, so menus can't be tested (run it with 'open')")
        } else {
        check(NSApp.target(forAction: #selector(SOMDocument.compile(_:))) as AnyObject === doc, "Compile reaches the document")
        func enabled(_ menu: String, _ title: String) -> Bool? {
            guard let m = NSApp.mainMenu?.item(withTitle: menu)?.submenu else { return nil }
            m.update()
            return m.item(withTitle: title)?.isEnabled
        }
        doc.mainWindowController?.selectVariables(["wavetbl"])
        check(enabled("Alter", "Reverse") == true, "Alter enabled for a selected wavetable")
        check(enabled("File", "Export Sample…") == false, "Export disabled for a wavetable")
        doc.mainWindowController?.selectVariables(["amp"])
        check(enabled("Alter", "Reverse") == false, "Alter disabled for a parameter")
        doc.mainWindowController?.selectVariables(["out"])
        check(enabled("File", "Export Sample…") == true, "Export enabled for a sample")
        doc.openEditor(for: "wavetbl")
        waitUntil({ NSApp.keyWindow?.windowController is TableEditorWindowController }, timeout: 2)
        check(doc.targetNames() == ["wavetbl"], "an editor window is the target")
        check(NSApp.target(forAction: #selector(SOMDocument.reverseTable(_:))) as AnyObject === doc, "Alter reaches the document from an editor")
        NSApp.keyWindow?.performClose(nil)
        }

        var seen: [String: String] = [:]
        var duplicates: [String] = []
        func scan(_ menu: NSMenu) {
            for item in menu.items {
                if let sub = item.submenu { scan(sub) }
                guard !item.keyEquivalent.isEmpty, !item.isAlternate else { continue }
                let key = "\(item.keyEquivalentModifierMask.rawValue)-\(item.keyEquivalent.lowercased())-\(item.keyEquivalent)"
                if let other = seen[key] { duplicates.append("\(other) / \(item.title)") }
                seen[key] = item.title
            }
        }
        if let main = NSApp.mainMenu { scan(main) }
        check(duplicates.isEmpty, "no duplicate keyboard shortcuts \(duplicates)")

        print("examples")
        if let root = AppDelegate.examplesURL, let folders = try? FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil) {
            var opened = 0
            for folder in folders {
                for f in (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)) ?? [] {
                    let d = SOMDocument()
                    if (try? d.read(from: Data(contentsOf: f), ofType: "public.data")) != nil { opened += 1 }
                }
            }
            check(opened >= 50, "opened \(opened) example patches")
        }

        doc.updateChangeCount(.changeCleared)
        print("\(checks - failures) of \(checks) checks passed")
        exit(failures == 0 ? 0 : 1)
    }
}
