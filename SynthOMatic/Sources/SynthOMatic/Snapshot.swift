import AppKit

/// A testing aid: `SynthOMatic --snapshot <dir> [patch]` opens a patch,
/// calculates it, opens its editors, writes a PNG of every window and quits.
enum Snapshot {
    static func run(directory: URL, patch: String?) {
        if CommandLine.arguments.contains("--light") { NSApp.appearance = NSAppearance(named: .aqua) }
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let url: URL
        if let patch {
            url = URL(fileURLWithPath: patch)
        } else if let ex = AppDelegate.examplesURL?.appendingPathComponent("Original Patches/flute") {
            url = ex
        } else {
            NSApp.terminate(nil)
            return
        }
        NSDocumentController.shared.openDocument(withContentsOf: url, display: true) { document, _, error in
            guard let doc = document as? SOMDocument else {
                print("snapshot: could not open \(url.path): \(String(describing: error))")
                NSApp.terminate(nil)
                return
            }
            doc.mainWindowController?.window?.setFrame(NSRect(x: 40, y: 80, width: 900, height: 720), display: true)
            doc.calculate(thenPlay: false)
            after(1.0) {
                for v in doc.vars where v.type != .scalar { doc.openEditor(for: v.name) }
                if let t = doc.vars.first(where: { $0.type == .table }) {
                    doc.mainWindowController?.selectVariables([t.name])
                    doc.mainWindowController?.window?.makeKeyAndOrderFront(nil)
                    doc.fourierFill(nil)
                }
                AboutWindowController.show()
                QuickReferenceWindowController.show()
                after(1.5) {
                    for (i, w) in NSApp.windows.enumerated() where w.isVisible {
                        save(w, to: directory.appendingPathComponent(String(format: "%02d-%@.png", i, sanitize(w.title.isEmpty ? "window" : w.title))))
                    }
                    print("snapshot: wrote \(directory.path)")
                    for d in NSDocumentController.shared.documents { d.updateChangeCount(.changeCleared) }
                    NSApp.terminate(nil)
                }
            }
        }
    }

    private static func after(_ t: TimeInterval, _ f: @escaping () -> Void) {
        DispatchQueue.main.asyncAfter(deadline: .now() + t, execute: f)
    }

    private static func sanitize(_ s: String) -> String {
        String(s.map { $0.isLetter || $0.isNumber ? $0 : "_" }.prefix(40))
    }

    static func save(_ window: NSWindow, to url: URL) {
        guard let view = window.contentView?.superview ?? window.contentView else { return }
        guard let rep = view.bitmapImageRepForCachingDisplay(in: view.bounds) else { return }
        view.cacheDisplay(in: view.bounds, to: rep)
        if let data = rep.representation(using: .png, properties: [:]) {
            try? data.write(to: url)
        }
    }
}
