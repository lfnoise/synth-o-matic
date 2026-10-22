import AppKit

/// Draws a sample as the original did - the minimum and maximum of each
/// column - with zooming, scrolling and a play head.
final class SampleView: NSView {
    weak var document: SOMDocument?
    var name: String = ""
    var onHover: ((Double?, Double?) -> Void)?

    /// the visible part of the sound, in samples
    private(set) var visibleStart: Double = 0
    private(set) var visibleLength: Double = 0
    private var tracking: NSTrackingArea?
    private var playTimer: Timer?

    override var acceptsFirstResponder: Bool { true }

    private var plot: NSRect { bounds.insetBy(dx: 8, dy: 8) }
    private var info: VarInfo? { document?.info(named: name) }
    private var totalLength: Double { Double(max(1, min(info?.length ?? 1, document?.engine.sampleLength ?? 1))) }

    func resetZoom() {
        visibleStart = 0
        visibleLength = totalLength
        needsDisplay = true
    }

    private func clampView() {
        let total = totalLength
        visibleLength = min(max(visibleLength, min(64, total)), total)
        visibleStart = min(max(0, visibleStart), total - visibleLength)
    }

    func zoom(by factor: Double, around fraction: Double = 0.5) {
        if visibleLength == 0 { resetZoom() }
        let center = visibleStart + visibleLength * fraction
        visibleLength /= factor
        clampView()
        visibleStart = center - visibleLength * fraction
        clampView()
        needsDisplay = true
    }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        if let tracking { removeTrackingArea(tracking) }
        let t = NSTrackingArea(rect: bounds, options: [.mouseMoved, .mouseEnteredAndExited, .activeInKeyWindow, .inVisibleRect], owner: self)
        addTrackingArea(t)
        tracking = t
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.textBackgroundColor.setFill()
        bounds.fill()
        // the calculation may be reallocating the sample
        guard let doc = document, !doc.isCalculating, let v = info, let data = doc.engine.data(v.index) else { return }
        if visibleLength == 0 { resetZoom() }
        clampView()
        let r = plot
        NSColor.separatorColor.setStroke()
        NSBezierPath(rect: r.insetBy(dx: -0.5, dy: -0.5)).stroke()
        let range = v.hilim - v.lolim
        guard range > 0 else { return }
        if v.lolim < 0 && v.hilim > 0 {
            let y = r.minY + CGFloat(-v.lolim / range) * r.height
            NSColor.secondaryLabelColor.withAlphaComponent(0.5).setFill()
            NSRect(x: r.minX, y: y, width: r.width, height: 1).fill()
        }
        func y(_ value: Float) -> CGFloat {
            min(r.maxY, max(r.minY, r.minY + CGFloat((Double(value) - v.lolim) / range) * r.height))
        }
        NSGraphicsContext.saveGraphicsState()
        NSBezierPath(rect: r).addClip()
        let columns = max(1, Int(r.width))
        let per = visibleLength / Double(columns)
        let path = NSBezierPath()
        path.lineWidth = 1
        let count = min(data.count, Int(totalLength))
        if per < 1 {
            // few samples: connect the points
            let first = Int(visibleStart), last = min(count - 1, Int(visibleStart + visibleLength) + 1)
            if first <= last {
                for i in first...last {
                    let p = NSPoint(x: r.minX + CGFloat((Double(i) - visibleStart) / visibleLength) * r.width, y: y(data[i]))
                    if i == first { path.move(to: p) } else { path.line(to: p) }
                }
            }
        } else {
            for c in 0..<columns {
                let a = Int(visibleStart + Double(c) * per)
                let b = min(count, Int(visibleStart + Double(c + 1) * per) + 1)
                guard a < b else { continue }
                var lo = data[a], hi = data[a]
                for k in a..<b { let s = data[k]; if s < lo { lo = s } else if s > hi { hi = s } }
                let x = r.minX + CGFloat(c) + 0.5
                path.move(to: NSPoint(x: x, y: y(lo)))
                path.line(to: NSPoint(x: x, y: max(y(hi), y(lo) + 1)))
            }
        }
        Palette.color(for: v).setStroke()
        path.stroke()

        // the play head
        let player = AudioPlayer.shared
        if player.owner === doc, player.ownerTag == name, let pos = player.position {
            let x = r.minX + CGFloat((pos - visibleStart) / visibleLength) * r.width
            if x >= r.minX && x <= r.maxX {
                NSColor.controlAccentColor.setFill()
                NSRect(x: x, y: r.minY, width: 1.5, height: r.height).fill()
            }
        }
        NSGraphicsContext.restoreGraphicsState()
    }

    func playbackChanged() {
        let playing = AudioPlayer.shared.owner === document && AudioPlayer.shared.ownerTag == name
        if playing && playTimer == nil {
            playTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30, repeats: true) { [weak self] _ in self?.needsDisplay = true }
        } else if !playing {
            playTimer?.invalidate()
            playTimer = nil
        }
        needsDisplay = true
    }

    override func viewWillMove(toWindow newWindow: NSWindow?) {
        if newWindow == nil { playTimer?.invalidate(); playTimer = nil }
    }

    override func mouseMoved(with event: NSEvent) {
        guard let v = info, let doc = document else { return }
        let p = convert(event.locationInWindow, from: nil)
        let r = plot
        let sample = visibleStart + Double((p.x - r.minX) / r.width) * visibleLength
        let value = v.lolim + Double((p.y - r.minY) / r.height) * (v.hilim - v.lolim)
        onHover?(max(0, sample) / doc.engine.sampleRate, value)
    }

    override func mouseExited(with event: NSEvent) { onHover?(nil, nil) }

    override func scrollWheel(with event: NSEvent) {
        if event.modifierFlags.contains(.command) || event.modifierFlags.contains(.option) {
            zoom(by: pow(1.01, Double(event.scrollingDeltaY)), around: fraction(event))
            return
        }
        let dx = event.scrollingDeltaX != 0 ? event.scrollingDeltaX : -event.scrollingDeltaY
        visibleStart -= Double(dx) / Double(max(plot.width, 1)) * visibleLength
        clampView()
        needsDisplay = true
    }

    override func magnify(with event: NSEvent) {
        zoom(by: 1 + Double(event.magnification), around: fraction(event))
    }

    private func fraction(_ event: NSEvent) -> Double {
        let p = convert(event.locationInWindow, from: nil)
        return Double(min(max((p.x - plot.minX) / plot.width, 0), 1))
    }

    override func keyDown(with event: NSEvent) {
        let flags = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        if flags.isDisjoint(with: [.command, .control, .option]) {
            if event.keyCode == 49 { document?.play(name); return }
            if let c = event.characters?.first, let rate = AudioPlayer.rate(forKey: c) {
                document?.play(name, rate: rate)
                return
            }
        }
        super.keyDown(with: event)
    }
}

/// A window showing one sample variable.
final class SampleWindowController: NSWindowController, NSWindowDelegate, VariableEditor {
    let variableName: String
    private let sampleView = SampleView()
    private let limits = ClickableLabel(labelWithString: "")
    private let status = NSTextField(labelWithString: "")
    private let summary = NSTextField(labelWithString: "")

    /// set at init: `document` is only set once the document adds the controller
    private(set) weak var somDocument: SOMDocument?

    init(document: SOMDocument, name: String) {
        variableName = name
        somDocument = document
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 720, height: 360),
                              styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.minSize = NSSize(width: 460, height: 240)
        super.init(window: window)
        window.delegate = self
        window.setFrameAutosaveName("SOMSample \(name)")
        build(document)
    }

    required init?(coder: NSCoder) { fatalError() }

    deinit { NotificationCenter.default.removeObserver(self) }

    override func windowTitle(forDocumentDisplayName displayName: String) -> String {
        "\(variableName) — Sample — \(displayName)"
    }

    private func build(_ doc: SOMDocument) {
        guard let window else { return }
        sampleView.document = doc
        sampleView.name = variableName
        sampleView.translatesAutoresizingMaskIntoConstraints = false
        sampleView.setAccessibilityLabel("\(variableName) waveform")
        sampleView.onHover = { [weak self] t, v in
            guard let self else { return }
            if let t, let v { self.status.stringValue = String(format: "Time = %.4f s   Value = %12f", t, v) }
            else { self.status.stringValue = "Space plays; the letter keys play transposed ('a' is the original pitch).  ⌥-scroll or pinch to zoom." }
        }
        sampleView.onHover?(nil, nil)

        for label in [limits, status, summary] {
            label.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
            label.textColor = .secondaryLabelColor
            label.lineBreakMode = .byTruncatingTail
        }
        limits.toolTip = "Double click to set the graph bounds; ⌥-click to auto scale."
        limits.onClick = { [weak self] event in
            if event.modifierFlags.contains(.option) { self?.somDocument?.autoScale(nil) }
            else if event.clickCount == 2 { self?.somDocument?.setBounds(nil) }
        }

        func button(_ symbol: String, _ tip: String, _ target: AnyObject?, _ action: Selector) -> NSButton {
            let b = NSButton(image: NSImage(systemSymbolName: symbol, accessibilityDescription: tip)!, target: target, action: action)
            b.bezelStyle = .texturedRounded
            b.toolTip = tip
            return b
        }
        let play = button("play.fill", "Play", self, #selector(playSample(_:)))
        let stop = button("stop.fill", "Stop", nil, #selector(SOMDocument.stop(_:)))
        let zoomIn = button("plus.magnifyingglass", "Zoom In", self, #selector(zoomIn(_:)))
        let zoomOut = button("minus.magnifyingglass", "Zoom Out", self, #selector(zoomOut(_:)))
        let fit = button("arrow.left.and.right", "Show All", self, #selector(showAll(_:)))
        let auto = NSButton(title: "Auto Scale", target: nil, action: #selector(SOMDocument.autoScale(_:)))
        auto.controlSize = .small
        let export = NSButton(title: "Export…", target: nil, action: #selector(SOMDocument.exportSample(_:)))
        export.controlSize = .small

        let top = NSStackView()
        top.setViews([play, stop, summary], in: .leading)
        top.setViews([zoomOut, zoomIn, fit, auto, export], in: .trailing)
        top.spacing = 6
        top.edgeInsets = NSEdgeInsets(top: 6, left: 10, bottom: 6, right: 10)
        top.translatesAutoresizingMaskIntoConstraints = false
        let bottom = NSStackView()
        bottom.setViews([status], in: .leading)
        bottom.setViews([limits], in: .trailing)
        status.setContentCompressionResistancePriority(.init(100), for: .horizontal)
        bottom.edgeInsets = NSEdgeInsets(top: 4, left: 10, bottom: 6, right: 10)
        bottom.translatesAutoresizingMaskIntoConstraints = false

        let content = NSView()
        content.addSubview(top)
        content.addSubview(sampleView)
        content.addSubview(bottom)
        NSLayoutConstraint.activate([
            top.topAnchor.constraint(equalTo: content.topAnchor),
            top.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            top.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            sampleView.topAnchor.constraint(equalTo: top.bottomAnchor),
            sampleView.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            sampleView.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            bottom.topAnchor.constraint(equalTo: sampleView.bottomAnchor),
            bottom.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            bottom.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            bottom.bottomAnchor.constraint(equalTo: content.bottomAnchor),
        ])
        window.contentView = content
        window.initialFirstResponder = sampleView

        let nc = NotificationCenter.default
        nc.addObserver(self, selector: #selector(variablesChanged(_:)), name: .variablesDidChange, object: doc)
        nc.addObserver(self, selector: #selector(playbackChanged(_:)), name: AudioPlayer.didChange, object: nil)
        refresh()
    }

    @objc private func variablesChanged(_ n: Notification) {
        if let names = n.userInfo?["names"] as? Set<String>, !names.contains(variableName) { return }
        refresh()
    }

    @objc private func playbackChanged(_ n: Notification) { sampleView.playbackChanged() }

    private func refresh() {
        guard let doc = somDocument, let v = doc.info(named: variableName) else { return }
        limits.stringValue = "upper limit = \(FormSheet.format(v.hilim))    lower limit = \(FormSheet.format(v.lolim))"
        var peak: Float = 0
        if let data = doc.engine.data(v.index) { for s in data where s.isFinite { peak = max(peak, abs(s)) } }
        summary.stringValue = String(format: "%.3f s   peak %.4g", doc.engine.duration, peak)
        if sampleView.visibleLength == 0 || sampleView.visibleLength > Double(v.length) { sampleView.resetZoom() }
        sampleView.needsDisplay = true
    }

    @objc private func playSample(_ sender: Any?) { somDocument?.play(variableName) }
    @objc private func zoomIn(_ sender: Any?) { sampleView.zoom(by: 2) }
    @objc private func zoomOut(_ sender: Any?) { sampleView.zoom(by: 0.5) }
    @objc private func showAll(_ sender: Any?) { sampleView.resetZoom() }

    func windowWillClose(_ notification: Notification) {
        if AudioPlayer.shared.owner === somDocument, AudioPlayer.shared.ownerTag == variableName { AudioPlayer.shared.stop() }
        somDocument?.editorClosed(variableName)
    }

    func windowWillReturnUndoManager(_ window: NSWindow) -> UndoManager? { somDocument?.undoManager }
}
