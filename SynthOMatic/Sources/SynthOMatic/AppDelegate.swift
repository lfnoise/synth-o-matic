import AppKit
import UniformTypeIdentifiers

/// Recognizes patches by their contents: classic Mac files have no extension.
final class SOMDocumentController: NSDocumentController {
    override func typeForContents(of url: URL) throws -> String {
        if let h = try? FileHandle(forReadingFrom: url) {
            defer { try? h.close() }
            if let head = try? h.read(upToCount: 8), Engine.isPatch(head) { return SOMDocument.patchType }
        }
        return try super.typeForContents(of: url)
    }

    override func documentClass(forType typeName: String) -> AnyClass? { SOMDocument.self }

    override var defaultType: String? { SOMDocument.patchType }
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuItemValidation {
    func applicationWillFinishLaunching(_ notification: Notification) {
        NSApp.mainMenu = makeMainMenu()
        UserDefaults.standard.register(defaults: ["NormalizePlayback": true, "ExportNormalize": true, "NSQuitAlwaysKeepsWindows": false])
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        if CommandLine.arguments.contains("--selftest") {
            // a timer, not the main queue: the test waits for main queue blocks
            Timer.scheduledTimer(withTimeInterval: 0, repeats: false) { _ in SelfTest.run() }
            return
        }
        if let i = CommandLine.arguments.firstIndex(of: "--snapshot"), i + 1 < CommandLine.arguments.count {
            Snapshot.run(directory: URL(fileURLWithPath: CommandLine.arguments[i + 1]),
                         patch: i + 2 < CommandLine.arguments.count ? CommandLine.arguments[i + 2] : nil)
        }
    }

    func applicationShouldOpenUntitledFile(_ sender: NSApplication) -> Bool {
        !CommandLine.arguments.contains("--snapshot") && !CommandLine.arguments.contains("--selftest")
    }

    // MARK: actions

    @objc func showAbout(_ sender: Any?) { AboutWindowController.show() }

    @objc func showQuickReference(_ sender: Any?) { QuickReferenceWindowController.show() }

    @objc func openExample(_ sender: NSMenuItem) {
        guard let url = sender.representedObject as? URL else { return }
        do {
            _ = try NSDocumentController.shared.duplicateDocument(withContentsOf: url, copying: true, displayName: url.lastPathComponent)
        } catch {
            NSApp.presentError(error)
        }
    }

    @objc func toggleNormalizePlayback(_ sender: Any?) {
        let d = UserDefaults.standard
        d.set(!d.bool(forKey: "NormalizePlayback"), forKey: "NormalizePlayback")
    }

    func validateMenuItem(_ item: NSMenuItem) -> Bool {
        if item.action == #selector(toggleNormalizePlayback(_:)) {
            item.state = UserDefaults.standard.bool(forKey: "NormalizePlayback") ? .on : .off
        }
        return true
    }

    static var examplesURL: URL? { Bundle.main.resourceURL?.appendingPathComponent("Examples") }

    // MARK: menus

    private func item(_ title: String, _ action: Selector?, _ key: String = "", _ mods: NSEvent.ModifierFlags = .command) -> NSMenuItem {
        let i = NSMenuItem(title: title, action: action, keyEquivalent: key)
        i.keyEquivalentModifierMask = key.isEmpty ? [] : mods
        return i
    }

    private func menu(_ title: String, _ items: [NSMenuItem]) -> NSMenuItem {
        let m = NSMenu(title: title)
        items.forEach(m.addItem)
        let top = NSMenuItem(title: title, action: nil, keyEquivalent: "")
        top.submenu = m
        return top
    }

    private func makeMainMenu() -> NSMenu {
        let main = NSMenu()
        let app = ProcessInfo.processInfo.processName == "SynthOMatic" ? "Synth-O-Matic" : ProcessInfo.processInfo.processName

        let services = NSMenuItem(title: "Services", action: nil, keyEquivalent: "")
        services.submenu = NSMenu()
        NSApp.servicesMenu = services.submenu
        main.addItem(menu(app, [
            item("About \(app)", #selector(showAbout(_:))),
            .separator(),
            services,
            .separator(),
            item("Hide \(app)", #selector(NSApplication.hide(_:)), "h"),
            item("Hide Others", #selector(NSApplication.hideOtherApplications(_:)), "h", [.command, .option]),
            item("Show All", #selector(NSApplication.unhideAllApplications(_:))),
            .separator(),
            item("Quit \(app)", #selector(NSApplication.terminate(_:)), "q"),
        ]))

        let recent = NSMenuItem(title: "Open Recent", action: nil, keyEquivalent: "")
        let recentMenu = NSMenu(title: "Open Recent")
        recentMenu.addItem(item("Clear Menu", #selector(NSDocumentController.clearRecentDocuments(_:))))
        recent.submenu = recentMenu

        main.addItem(menu("File", [
            item("New", #selector(NSDocumentController.newDocument(_:)), "n"),
            item("Open…", #selector(NSDocumentController.openDocument(_:)), "o"),
            recent,
            makeExamplesItem(),
            .separator(),
            item("Close", #selector(NSWindow.performClose(_:)), "w"),
            item("Save", #selector(NSDocument.save(_:)), "s"),
            item("Save As…", #selector(NSDocument.saveAs(_:)), "s", [.command, .shift]),
            item("Revert to Saved", #selector(NSDocument.revertToSaved(_:))),
            .separator(),
            item("Export Sample…", #selector(SOMDocument.exportSample(_:)), "e"),
            item("Import Sound into Sample…", #selector(SOMDocument.importSound(_:)), "i", [.command, .shift]),
            .separator(),
            item("Page Setup…", #selector(NSDocument.runPageLayout(_:)), "p", [.command, .shift]),
            item("Print…", #selector(NSDocument.printDocument(_:)), "p"),
        ]))

        let find = NSMenuItem(title: "Find", action: nil, keyEquivalent: "")
        let findMenu = NSMenu(title: "Find")
        for (title, tag, key, mods) in [("Find…", NSTextFinder.Action.showFindInterface, "f", NSEvent.ModifierFlags([.command, .option])),
                                        ("Find Next", .nextMatch, "g", [.command, .option]),
                                        ("Find Previous", .previousMatch, "g", [.command, .option, .shift]),
                                        ("Use Selection for Find", .setSearchString, "e", [.command, .option])] {
            let i = item(title, #selector(NSResponder.performTextFinderAction(_:)), key, mods)
            i.tag = tag.rawValue
            findMenu.addItem(i)
        }
        find.submenu = findMenu

        main.addItem(menu("Edit", [
            item("Undo", Selector(("undo:")), "z"),
            item("Redo", Selector(("redo:")), "z", [.command, .shift]),
            .separator(),
            item("Cut", #selector(NSText.cut(_:)), "x"),
            item("Copy", #selector(NSText.copy(_:)), "c"),
            item("Paste", #selector(NSText.paste(_:)), "v"),
            item("Delete", #selector(NSText.delete(_:))),
            item("Select All", #selector(NSText.selectAll(_:)), "a"),
            .separator(),
            find,
            item("Complete", #selector(NSTextView.complete(_:)), "\u{1b}", .option),
        ]))

        let removeAll = item("Remove All Unused Vars", #selector(SOMDocument.removeAllUnusedVars(_:)))
        removeAll.isAlternate = true
        removeAll.keyEquivalentModifierMask = .option
        main.addItem(menu("Synth", [
            item("Compile", #selector(SOMDocument.compile(_:)), "k"),
            item("Calculate", #selector(SOMDocument.calculate(_:)), "/"),
            item("Calculate & Play", #selector(SOMDocument.calculateAndPlay(_:)), "\r"),
            item("Play", #selector(SOMDocument.play(_:))),
            item("Stop", #selector(SOMDocument.stop(_:)), "."),
            .separator(),
            item("Make Random Set", #selector(SOMDocument.makeRandomSet(_:)), "5"),
            item("Randomize", #selector(SOMDocument.randomize(_:)), "r", [.command, .shift]),
            .separator(),
            item("Remove Vars", #selector(SOMDocument.removeVars(_:))),
            removeAll,
            item("Count Memory", #selector(SOMDocument.memoryReport(_:))),
            .separator(),
            item("Normalize Playback", #selector(toggleNormalizePlayback(_:))),
        ]))

        main.addItem(menu("Signals", [
            item("Set Globals…", #selector(SOMDocument.setGlobals(_:)), "g"),
            item("Instrument I/O…", #selector(SOMDocument.instrumentIO(_:))),
            .separator(),
            item("Set Type to Wavetable", #selector(SOMDocument.setTypeToWavetable(_:)), "1"),
            item("Set Type to Variable", #selector(SOMDocument.setTypeToVariable(_:)), "2"),
            item("Set Type to Envelope", #selector(SOMDocument.setTypeToEnvelope(_:)), "3"),
            item("Set Type to Sample", #selector(SOMDocument.setTypeToSample(_:)), "4"),
            .separator(),
            item("Edit Variable…", #selector(SOMDocument.editVariable(_:)), "e", [.command, .control]),
            item("Set Graph Bounds…", #selector(SOMDocument.setBounds(_:)), "b", [.command, .control]),
            item("Auto Scale", #selector(SOMDocument.autoScale(_:)), "a", [.command, .control]),
        ]))

        main.addItem(menu("Fill", [
            item("Fourier & Chebyshev Fill…", #selector(SOMDocument.fourierFill(_:)), "f"),
            item("FM Table Fill…", #selector(SOMDocument.fmTableFill(_:)), "t"),
            item("GBuzz Table Fill…", #selector(SOMDocument.gbuzzTableFill(_:)), "y"),
            item("Grain Envelope Fill…", #selector(SOMDocument.grainEnvelopeFill(_:))),
        ]))

        if let alter = AppDelegate.makeAlterMenu() {
            let top = NSMenuItem(title: "Alter", action: nil, keyEquivalent: "")
            top.submenu = alter
            main.addItem(top)
        }

        let window = menu("Window", [
            item("Minimize", #selector(NSWindow.performMiniaturize(_:)), "m"),
            item("Zoom", #selector(NSWindow.performZoom(_:))),
            .separator(),
            item("Program", #selector(MainWindowController.focusProgram(_:)), "0"),
            item("Variables", #selector(MainWindowController.focusVariables(_:)), "9"),
            .separator(),
            item("Bring All to Front", #selector(NSApplication.arrangeInFront(_:))),
        ])
        NSApp.windowsMenu = window.submenu
        main.addItem(window)

        let help = menu("Help", [
            item("Synth-O-Matic Quick Reference", #selector(showQuickReference(_:)), "?"),
        ])
        NSApp.helpMenu = help.submenu
        main.addItem(help)
        return main
    }

    /// The Alter menu; the original's command keys, except where macOS needs them
    /// (⌘A, ⌘H, ⌘P, ⌘M gained an option key).
    static func makeAlterMenu() -> NSMenu? {
        let m = NSMenu(title: "Alter")
        func add(_ title: String, _ action: Selector, _ key: String = "", _ mods: NSEvent.ModifierFlags = .command) {
            let i = NSMenuItem(title: title, action: action, keyEquivalent: key)
            i.keyEquivalentModifierMask = key.isEmpty ? [] : mods
            m.addItem(i)
        }
        add("Reverse", #selector(SOMDocument.reverseTable(_:)), "r")
        add("Invert", #selector(SOMDocument.invertTable(_:)), "-")
        add("Absolute Value", #selector(SOMDocument.absTable(_:)), "a", [.command, .option])
        add("Clip Negative", #selector(SOMDocument.clipNegative(_:)))
        add("Square", #selector(SOMDocument.squareTable(_:)))
        add("Add Clipboard", #selector(SOMDocument.addClipboard(_:)), "+")
        add("Mult Clipboard", #selector(SOMDocument.multClipboard(_:)), "*")
        m.addItem(.separator())
        add("Squash Up", #selector(SOMDocument.squashUp(_:)), "=")
        add("Squash Down", #selector(SOMDocument.squashDown(_:)), "'")
        add("Squash In", #selector(SOMDocument.squashIn(_:)), "[")
        add("Squash Out", #selector(SOMDocument.squashOut(_:)), "]")
        m.addItem(.separator())
        add("Hanning Window", #selector(SOMDocument.hanningWindow(_:)), "h", [.command, .option])
        add("Parzen Window", #selector(SOMDocument.parzenWindow(_:)), "p", [.command, .option])
        add("Welch Window", #selector(SOMDocument.welchWindow(_:)), "l")
        add("Normalize", #selector(SOMDocument.normalizeTable(_:)), "m", [.command, .option])
        m.addItem(.separator())
        add("Comb Table…", #selector(SOMDocument.combTable(_:)), ";")
        add("Smooth Table", #selector(SOMDocument.smoothTable(_:)), "u")
        add("Analyze Table", #selector(SOMDocument.analyzeTable(_:)), "\\")
        return m
    }

    private func makeExamplesItem() -> NSMenuItem {
        let top = NSMenuItem(title: "Open Example", action: nil, keyEquivalent: "")
        let m = NSMenu(title: "Open Example")
        top.submenu = m
        guard let root = AppDelegate.examplesURL,
              let folders = try? FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil) else { return top }
        for folder in folders.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
            guard let files = try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil) else { continue }
            let sub = NSMenu(title: folder.lastPathComponent)
            for f in files.sorted(by: { $0.lastPathComponent.localizedCaseInsensitiveCompare($1.lastPathComponent) == .orderedAscending }) {
                let i = NSMenuItem(title: f.lastPathComponent, action: #selector(openExample(_:)), keyEquivalent: "")
                i.representedObject = f
                i.target = self
                sub.addItem(i)
            }
            let item = NSMenuItem(title: folder.lastPathComponent, action: nil, keyEquivalent: "")
            item.submenu = sub
            m.addItem(item)
        }
        return top
    }
}
