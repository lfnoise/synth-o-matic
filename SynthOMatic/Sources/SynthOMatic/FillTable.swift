import AppKit

/// The original's harmonic gadget: 32 values from 0 to 99, drawn as bars
/// (amplitudes) or as marks (phases).
final class HarmonicsView: NSView {
    enum Style { case bars, marks }
    var style: Style = .bars
    var values = [UInt8](repeating: 0, count: 32) { didSet { needsDisplay = true } }
    var onChange: (() -> Void)?

    override var intrinsicContentSize: NSSize { NSSize(width: 32 * 10 + 8, height: 260) }

    private var plot: NSRect { bounds.insetBy(dx: 4, dy: 6) }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.textBackgroundColor.setFill()
        bounds.fill()
        let r = plot
        NSColor.separatorColor.setStroke()
        NSBezierPath(rect: r.insetBy(dx: -0.5, dy: -0.5)).stroke()
        // amplitudes: the original's scale marks, 3 dB apart; phases: 90, 180 and 270 degrees
        NSColor.separatorColor.withAlphaComponent(0.5).setFill()
        for level in style == .bars ? [71, 50, 35, 25, 18, 13, 9, 6] : [25, 50, 75] {
            let y = r.minY + CGFloat(level) / 99 * r.height
            NSRect(x: r.minX, y: y, width: r.width, height: 0.5).fill()
        }
        let w = r.width / 32
        for (i, v) in values.enumerated() {
            let x = r.minX + CGFloat(i) * w
            let h = CGFloat(v) / 99 * r.height
            if style == .bars {
                (i % 8 == 7 ? NSColor.controlAccentColor : NSColor.controlAccentColor.withAlphaComponent(0.75)).setFill()
                NSRect(x: x + 1.5, y: r.minY, width: w - 3, height: h).fill()
            } else {
                NSColor.controlAccentColor.setFill()
                NSRect(x: x + 1.5, y: r.minY + h - 1.5, width: w - 3, height: 3).fill()
            }
        }
        // harmonic numbers
        let attrs: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 8), .foregroundColor: NSColor.tertiaryLabelColor]
        for i in stride(from: 0, to: 32, by: 4) {
            ("\(i + 1)" as NSString).draw(at: NSPoint(x: r.minX + CGFloat(i) * w + 2, y: r.maxY - 10), withAttributes: attrs)
        }
    }

    private func set(_ event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        let r = plot
        let i = min(31, max(0, Int((p.x - r.minX) / (r.width / 32))))
        var v: Int
        if event.modifierFlags.contains(.command) { v = 0 }
        else if event.modifierFlags.contains(.shift) { v = 99 }
        else { v = Int(((p.y - r.minY) / r.height * 99).rounded()) }
        v = min(99, max(0, v))
        if values[i] != UInt8(v) { values[i] = UInt8(v) }
    }

    override func mouseDown(with event: NSEvent) {
        if event.modifierFlags.contains(.option) {
            // the original: option click randomizes
            for i in 0..<32 {
                values[i] = UInt8(style == .bars ? 100 * Double.random(in: 0..<1) / sqrt(Double(i) + 1) : 100 * Double.random(in: 0..<1))
            }
            onChange?()
            return
        }
        if style == .marks && event.modifierFlags.contains(.control) {
            // control click rotates the phases by 90 degrees
            values = values.map { ($0 + 25) % 100 }
            onChange?()
            return
        }
        set(event)
        onChange?()
    }

    override func mouseDragged(with event: NSEvent) {
        set(event)
        onChange?()
    }

    override func rightMouseDown(with event: NSEvent) {
        if style == .marks { values = values.map { ($0 + 25) % 100 }; onChange?() }
    }
}

/// The original's "Fill Table" window: Fourier (sine) and Chebyshev fill
/// from 32 harmonic amplitudes and phases.
final class FillTableWindowController: NSWindowController, NSWindowDelegate, VariableEditor {
    let variableName: String
    private let amplitudes = HarmonicsView()
    private let phases = HarmonicsView()
    private let preview = WaveThumbnailView()

    /// set at init: `document` is only set once the document adds the controller
    private(set) weak var somDocument: SOMDocument?

    init(document: SOMDocument, name: String) {
        variableName = name
        somDocument = document
        let window = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 720, height: 230),
                             styleMask: [.titled, .closable, .utilityWindow], backing: .buffered, defer: false)
        window.isFloatingPanel = false
        window.hidesOnDeactivate = false
        super.init(window: window)
        window.delegate = self
        build(document)
    }

    required init?(coder: NSCoder) { fatalError() }

    deinit { NotificationCenter.default.removeObserver(self) }

    override func windowTitle(forDocumentDisplayName displayName: String) -> String { "Fill Table — \(variableName)" }

    private func build(_ doc: SOMDocument) {
        guard let window, let v = doc.info(named: variableName) else { return }
        let analysis = doc.engine.analyzeHarmonics(v.index)
        amplitudes.values = analysis.ampl
        phases.values = analysis.phas
        phases.style = .marks
        amplitudes.setAccessibilityLabel("Harmonic amplitudes")
        phases.setAccessibilityLabel("Harmonic phases")

        func titled(_ title: String, _ view: NSView) -> NSView {
            let label = NSTextField(labelWithString: title)
            label.font = .systemFont(ofSize: 11, weight: .semibold)
            label.textColor = .secondaryLabelColor
            let s = NSStackView(views: [label, view])
            s.orientation = .vertical
            s.alignment = .leading
            s.spacing = 4
            view.widthAnchor.constraint(equalToConstant: 328).isActive = true
            view.heightAnchor.constraint(equalToConstant: 240).isActive = true
            return s
        }
        let sine = NSButton(title: "Sine Fill", target: self, action: #selector(sineFill(_:)))
        sine.keyEquivalent = "\r"
        let cheby = NSButton(title: "Cheby Fill", target: self, action: #selector(chebyFill(_:)))
        cheby.toolTip = "Fill with a Chebyshev transfer function for waveshaping; also fills the table named \(variableName)ps, if there is one."
        preview.color = Palette.color(for: v)
        preview.drawsFrame = true
        preview.widthAnchor.constraint(equalToConstant: 120).isActive = true
        preview.heightAnchor.constraint(equalToConstant: 60).isActive = true
        let buttons = NSStackView(views: [sine, cheby, preview])
        buttons.orientation = .vertical
        buttons.alignment = .centerX
        sine.widthAnchor.constraint(equalTo: cheby.widthAnchor).isActive = true

        let hint = NSTextField(wrappingLabelWithString: "Drag to set harmonics.  ⇧-drag: maximum.  ⌘-drag: zero.  ⌥-click: random.  ⌃-click the phases: rotate 90°.")
        hint.font = .systemFont(ofSize: 10)
        hint.textColor = .tertiaryLabelColor

        let row = NSStackView(views: [titled("Amplitudes", amplitudes), titled("Phases", phases), buttons])
        row.alignment = .top
        row.spacing = 16
        let col = NSStackView(views: [row, hint])
        col.orientation = .vertical
        col.alignment = .leading
        col.edgeInsets = NSEdgeInsets(top: 14, left: 16, bottom: 12, right: 16)
        window.contentView = col
        window.setContentSize(col.fittingSize)
        updatePreview()
        NotificationCenter.default.addObserver(self, selector: #selector(variablesChanged(_:)), name: .variablesDidChange, object: doc)
    }

    @objc private func variablesChanged(_ n: Notification) {
        guard let doc = somDocument else { return }
        if doc.info(named: variableName).map({ $0.type == .table || $0.type == .envelope }) != true { close(); return }
        updatePreview()
    }

    private func updatePreview() {
        guard let doc = somDocument, let v = doc.info(named: variableName) else { return }
        var pts = doc.engine.points(v.index)
        if pts.count > 512 { pts.removeLast(pts.count - 512) }
        preview.lolim = v.lolim
        preview.hilim = v.hilim
        preview.points = pts
    }

    @objc private func sineFill(_ sender: Any?) {
        guard let doc = somDocument, let v = doc.info(named: variableName) else { return }
        doc.changeVariables([variableName], actionName: "Fourier Fill") {
            doc.engine.sineFill(v.index, ampl: amplitudes.values, phas: phases.values)
        }
    }

    @objc private func chebyFill(_ sender: Any?) {
        guard let doc = somDocument, let v = doc.info(named: variableName) else { return }
        let ps = variableName + "ps"
        doc.changeVariables([variableName, ps], actionName: "Chebyshev Fill") {
            if let filled = doc.engine.chebyFill(v.index, ampl: amplitudes.values, phas: phases.values) {
                doc.appendMessage("Filled the power scaling table '\(doc.engine.info(filled).name)'.\n")
            }
        }
    }

    func windowWillReturnUndoManager(_ window: NSWindow) -> UndoManager? { somDocument?.undoManager }
}
