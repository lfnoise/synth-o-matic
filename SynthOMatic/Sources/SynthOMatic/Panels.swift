import AppKit
import UniformTypeIdentifiers
import SOMEngine

/// Options under the save panel when exporting a sample.
final class ExportAccessory: NSObject {
    let view = NSView(frame: NSRect(x: 0, y: 0, width: 440, height: 92))
    weak var panel: NSSavePanel?
    private let formatPopup = NSPopUpButton()
    private let normalizeBox = NSButton(checkboxWithTitle: "Normalize to full scale (as the original)", target: nil, action: nil)
    private let stereoPopup = NSPopUpButton()
    private let candidates: [String]

    init(stereoCandidates: [String], suggested: String?) {
        candidates = stereoCandidates
        super.init()
        for f in Engine.ExportFormat.allCases { formatPopup.addItem(withTitle: f.title) }
        formatPopup.selectItem(at: UserDefaults.standard.integer(forKey: "ExportFormat"))
        formatPopup.target = self
        formatPopup.action = #selector(update)
        normalizeBox.state = (UserDefaults.standard.object(forKey: "ExportNormalize") as? Bool ?? true) ? .on : .off
        stereoPopup.addItem(withTitle: "Mono")
        for c in stereoCandidates { stereoPopup.addItem(withTitle: "Stereo, with \(c) on the right") }
        if let suggested, let i = stereoCandidates.firstIndex(of: suggested) { stereoPopup.selectItem(at: i + 1) }
        stereoPopup.isEnabled = !stereoCandidates.isEmpty

        let grid = NSGridView(views: [
            [NSTextField(labelWithString: "Format:"), formatPopup],
            [NSTextField(labelWithString: "Channels:"), stereoPopup],
            [NSGridCell.emptyContentView, normalizeBox],
        ])
        grid.column(at: 0).xPlacement = .trailing
        grid.rowSpacing = 6
        grid.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(grid)
        NSLayoutConstraint.activate([
            grid.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            grid.topAnchor.constraint(equalTo: view.topAnchor, constant: 10),
            grid.bottomAnchor.constraint(equalTo: view.bottomAnchor, constant: -10),
        ])
    }

    var format: Engine.ExportFormat {
        Engine.ExportFormat(rawValue: formatPopup.indexOfSelectedItem) ?? .aiff16
    }

    var normalize: Bool { normalizeBox.state == .on }

    var stereoPartner: String? {
        let i = stereoPopup.indexOfSelectedItem
        return i > 0 ? candidates[i - 1] : nil
    }

    @objc func update() {
        UserDefaults.standard.set(formatPopup.indexOfSelectedItem, forKey: "ExportFormat")
        UserDefaults.standard.set(normalize, forKey: "ExportNormalize")
        switch format {
        case .aiff16: panel?.allowedContentTypes = [.aiff]
        case .soundDesigner: panel?.allowedContentTypes = [UTType(filenameExtension: "sd") ?? .data]
        default: panel?.allowedContentTypes = [.wav]
        }
        normalizeBox.isEnabled = true
    }
}

// MARK: - About

/// The original About box: a spirograph drawn from spinner.data on black.
final class SpirographView: NSView {
    private var lines: [(dtheta: Int, domega: Int, age: Int)] = []
    private var timer: Timer?
    private var current: (Int, Int) = (1, 1)
    private var progress = 0

    override var isFlipped: Bool { true }

    override func viewDidMoveToWindow() {
        timer?.invalidate()
        guard window != nil else { return }
        next()
        timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60, repeats: true) { [weak self] _ in self?.tick() }
    }

    private func next() {
        current = (Int.random(in: 1...330), Int.random(in: 1...330))
        progress = 0
    }

    private func tick() {
        progress += 6
        // like the original, a figure stays up for a while and is replaced
        if progress > 331 + 400 { next() }
        needsDisplay = true
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.black.setFill()
        bounds.fill()
        let n = Int(som_spiro_size())
        guard let sinT = som_spiro_sin(), let cosT = som_spiro_cos() else { return }
        let center = NSPoint(x: 136, y: bounds.midY)
        let path = NSBezierPath()
        path.lineWidth = 0.6
        var theta = 0, omega = 0
        path.move(to: center)
        for _ in 0..<min(progress, n) {
            theta += current.0; if theta >= n { theta -= n }
            omega += current.1; if omega >= n { omega -= n }
            let x = CGFloat((Int(sinT[omega]) * Int(cosT[theta])) >> 7)
            let y = CGFloat((Int(sinT[omega]) * Int(sinT[theta])) >> 7)
            path.line(to: NSPoint(x: center.x + x, y: center.y + y))
        }
        NSColor.white.setStroke()
        path.stroke()

        let title: [NSAttributedString.Key: Any] = [.font: NSFont.boldSystemFont(ofSize: 15), .foregroundColor: NSColor.white]
        let body: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 12), .foregroundColor: NSColor.white]
        let small: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor(white: 0.7, alpha: 1)]
        let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0"
        var y: CGFloat = 26
        (String(format: "Synth-O-Matic %5.3f", Engine.version) as NSString).draw(at: NSPoint(x: 296, y: y), withAttributes: title)
        y += 26
        for line in ["Copyright © 1990", "by James McCartney"] {
            (line as NSString).draw(at: NSPoint(x: 308, y: y), withAttributes: body)
            y += 20
        }
        y += 10
        ("macOS port \(version)" as NSString).draw(at: NSPoint(x: 308, y: y), withAttributes: small)
        y += 18
        for _ in 0..<4 {
            ("." as NSString).draw(at: NSPoint(x: 308, y: y), withAttributes: body)
            y += 20
        }
        ("Synth-O-Matic has more unit generators than other leading brands.   ;¬∫" as NSString)
            .draw(at: NSPoint(x: 40, y: bounds.maxY - 34), withAttributes: body)
    }

    override func mouseDown(with event: NSEvent) { window?.close() }
    override func keyDown(with event: NSEvent) { window?.close() }
    override var acceptsFirstResponder: Bool { true }
}

final class AboutWindowController: NSWindowController, NSWindowDelegate {
    static var shared: AboutWindowController?

    static func show() {
        if shared == nil { shared = AboutWindowController() }
        shared?.window?.center()
        shared?.showWindow(nil)
    }

    init() {
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 640, height: 330),
                              styleMask: [.titled, .closable, .fullSizeContentView], backing: .buffered, defer: false)
        window.titlebarAppearsTransparent = true
        window.titleVisibility = .hidden
        window.backgroundColor = .black
        window.isMovableByWindowBackground = true
        let view = SpirographView()
        window.contentView = view
        window.initialFirstResponder = view
        super.init(window: window)
        window.delegate = self
    }

    required init?(coder: NSCoder) { fatalError() }

    func windowWillClose(_ notification: Notification) {
        (window?.contentView as? SpirographView)?.removeFromSuperview()
        AboutWindowController.shared = nil
    }
}

// MARK: - Quick Reference

final class QuickReferenceWindowController: NSWindowController, NSSearchFieldDelegate {
    static var shared: QuickReferenceWindowController?
    private let textView = NSTextView()
    private let search = NSSearchField()

    static func show() {
        if shared == nil { shared = QuickReferenceWindowController() }
        shared?.showWindow(nil)
    }

    init() {
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 640, height: 720),
                              styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "Synth-O-Matic Quick Reference"
        window.setFrameAutosaveName("SOMQuickReference")
        super.init(window: window)

        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.documentView = textView
        textView.isEditable = false
        textView.textContainerInset = NSSize(width: 16, height: 12)
        textView.autoresizingMask = [.width]
        textView.isVerticallyResizable = true
        textView.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        search.placeholderString = "Find a unit generator"
        search.delegate = self
        search.sendsSearchStringImmediately = true
        let stack = NSStackView(views: [search, scroll])
        stack.orientation = .vertical
        stack.edgeInsets = NSEdgeInsets(top: 8, left: 8, bottom: 0, right: 8)
        stack.spacing = 8
        window.contentView = stack
        scroll.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
        render(filter: "")
        window.center()
    }

    required init?(coder: NSCoder) { fatalError() }

    func controlTextDidChange(_ obj: Notification) { render(filter: search.stringValue) }

    private func render(filter: String) {
        let f = filter.trimmingCharacters(in: .whitespaces).lowercased()
        let out = NSMutableAttributedString()
        let h1: [NSAttributedString.Key: Any] = [.font: NSFont.boldSystemFont(ofSize: 18), .foregroundColor: NSColor.labelColor]
        let h2: [NSAttributedString.Key: Any] = [.font: NSFont.boldSystemFont(ofSize: 13), .foregroundColor: NSColor.labelColor]
        let sig: [NSAttributedString.Key: Any] = [.font: NSFont.monospacedSystemFont(ofSize: 12, weight: .semibold), .foregroundColor: Palette.function]
        let body: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 12), .foregroundColor: NSColor.labelColor]
        let detail: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor.secondaryLabelColor]
        func para(_ s: String, _ a: [NSAttributedString.Key: Any], before: CGFloat = 0, indent: CGFloat = 0) {
            let p = NSMutableParagraphStyle()
            p.paragraphSpacingBefore = before
            p.headIndent = indent
            p.firstLineHeadIndent = indent
            var a = a
            a[.paragraphStyle] = p
            out.append(NSAttributedString(string: s + "\n", attributes: a))
        }
        if f.isEmpty {
            para("Synth-O-Matic Quick Reference Guide", h1)
            for (title, text) in Reference.notes {
                para(title, h2, before: 10)
                para(text, body)
            }
        }
        for section in [Reference.operators] + Reference.sections {
            let entries = section.entries.filter { f.isEmpty || $0.signature.lowercased().contains(f) || $0.summary.lowercased().contains(f) }
            guard !entries.isEmpty else { continue }
            para(section.title, h2, before: 14)
            for e in entries {
                para(e.signature, sig, before: 6)
                para(e.summary, body, indent: 18)
                if !e.details.isEmpty { para(e.details, detail, indent: 18) }
            }
        }
        if out.length == 0 { para("Nothing matches \u{201C}\(filter)\u{201D}.", body) }
        textView.textStorage?.setAttributedString(out)
        textView.scrollToBeginningOfDocument(nil)
    }
}
