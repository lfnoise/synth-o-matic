import AppKit

/// Draws a table on the original's grid: a line every step, a heavier one
/// every ten steps, and the zero line.  Shared by the table and sample views.
enum Grid {
    static func draw(in r: NSRect, lolim: Double, hilim: Double, stepsiz: Double) {
        guard hilim > lolim else { return }
        let range = hilim - lolim
        var step = stepsiz > 0 ? stepsiz : range / 10
        // keep the grid readable
        while range / step > Double(r.height) / 4 { step *= 10 }
        var k = (lolim / step).rounded(.up)
        while k * step <= hilim + step / 2 {
            let value = k * step
            let y = r.minY + CGFloat((value - lolim) / range) * r.height
            let index = Int(k.rounded())
            let color: NSColor
            if index == 0 {
                color = NSColor.secondaryLabelColor.withAlphaComponent(0.6)
            } else if index % 10 == 0 {
                color = NSColor.separatorColor.withAlphaComponent(0.9)
            } else {
                color = NSColor.separatorColor.withAlphaComponent(0.35)
            }
            color.setFill()
            NSRect(x: r.minX, y: (y - 0.5).rounded() + 0.5, width: r.width, height: index == 0 ? 1 : 0.5).fill()
            k += 1
        }
    }
}

/// The table drawing area.
final class WaveformEditView: NSView {
    weak var document: SOMDocument?
    var name: String = ""
    var onHover: ((Int?, Double?) -> Void)?

    enum Mode { case freehand, lines }
    var mode: Mode = .freehand

    private var snapshot: VarSnapshot?
    private var lastPoint: (index: Int, value: Double)?
    private var lineAnchor: (index: Int, value: Double)?
    private var rubber: NSPoint?
    private var tracking: NSTrackingArea?

    override var isFlipped: Bool { false }
    override var acceptsFirstResponder: Bool { true }

    private var plot: NSRect { bounds.insetBy(dx: 8, dy: 8) }

    private var info: VarInfo? { document?.info(named: name) }

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
        guard let doc = document, !doc.isCalculating, let v = info, let data = doc.engine.data(v.index) else { return }
        let r = plot
        NSColor.separatorColor.setStroke()
        let frame = NSBezierPath(rect: r.insetBy(dx: -0.5, dy: -0.5))
        frame.lineWidth = 1
        frame.stroke()
        Grid.draw(in: r, lolim: v.lolim, hilim: v.hilim, stepsiz: v.stepsiz)

        let range = v.hilim - v.lolim
        guard range > 0 else { return }
        let n = min(data.count, 513)
        // a wavetable shows its guard point as the start of the next cycle
        let shown = v.type == .table ? 513 : n
        func point(_ i: Int) -> NSPoint {
            let value = Double(i < n ? data[i] : data[0])
            return NSPoint(x: r.minX + CGFloat(i) / CGFloat(shown - 1) * r.width,
                           y: r.minY + CGFloat((value - v.lolim) / range) * r.height)
        }
        NSGraphicsContext.saveGraphicsState()
        NSBezierPath(rect: r).addClip()
        let path = NSBezierPath()
        path.lineWidth = 1.5
        path.lineJoinStyle = .round
        path.move(to: point(0))
        for i in 1..<shown { path.line(to: point(i)) }
        Palette.color(for: v).setStroke()
        path.stroke()
        if let anchor = lineAnchor, let rubber {
            let a = NSPoint(x: r.minX + CGFloat(anchor.index) / CGFloat(shown - 1) * r.width,
                            y: r.minY + CGFloat((anchor.value - v.lolim) / range) * r.height)
            let line = NSBezierPath()
            line.move(to: a)
            line.line(to: rubber)
            line.setLineDash([4, 3], count: 2, phase: 0)
            NSColor.labelColor.withAlphaComponent(0.6).setStroke()
            line.stroke()
        }
        NSGraphicsContext.restoreGraphicsState()
    }

    /// table index and value under a point
    private func locate(_ p: NSPoint) -> (index: Int, value: Double)? {
        guard let v = info else { return nil }
        let r = plot
        let x = min(max(p.x, r.minX), r.maxX), y = min(max(p.y, r.minY), r.maxY)
        let index = Int(((x - r.minX) / r.width * 511).rounded())
        let value = v.lolim + Double((y - r.minY) / r.height) * (v.hilim - v.lolim)
        return (min(max(index, 0), 511), value)
    }

    override func mouseMoved(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        if let l = locate(p) { onHover?(l.index, l.value) }
        if lineAnchor != nil { rubber = p; needsDisplay = true }
    }

    override func mouseExited(with event: NSEvent) { onHover?(nil, nil) }

    override func mouseDown(with event: NSEvent) {
        guard let doc = document, !doc.isCalculating, let l = locate(convert(event.locationInWindow, from: nil)) else { return }
        window?.makeFirstResponder(self)
        if mode == .lines || event.modifierFlags.contains(.shift) {
            // the original: click points, double click to finish
            if event.clickCount >= 2 {
                finishLines()
                return
            }
            if snapshot == nil { snapshot = doc.beginContinuousChange(name) }
            if let a = lineAnchor { drawSegment(from: a, to: l) } else { set(l.index, l.value) }
            lineAnchor = l
            rubber = convert(event.locationInWindow, from: nil)
            needsDisplay = true
            return
        }
        snapshot = doc.beginContinuousChange(name)
        lastPoint = l
        set(l.index, l.value)
        needsDisplay = true
    }

    override func mouseDragged(with event: NSEvent) {
        guard mode == .freehand, lineAnchor == nil, let last = lastPoint,
              let l = locate(convert(event.locationInWindow, from: nil)) else { return }
        drawSegment(from: last, to: l)
        lastPoint = l
        onHover?(l.index, l.value)
        needsDisplay = true
    }

    override func mouseUp(with event: NSEvent) {
        guard lineAnchor == nil, snapshot != nil else { return }
        lastPoint = nil
        document?.endContinuousChange(snapshot, actionName: "Edit Wave")
        snapshot = nil
    }

    override func keyDown(with event: NSEvent) {
        if event.keyCode == 53 || event.keyCode == 36 {	// escape, return end a line drawing
            finishLines()
            return
        }
        super.keyDown(with: event)
    }

    func finishLines() {
        guard lineAnchor != nil else { return }
        lineAnchor = nil
        rubber = nil
        document?.endContinuousChange(snapshot, actionName: "Edit Wave")
        snapshot = nil
        needsDisplay = true
    }

    private func set(_ index: Int, _ value: Double) {
        guard let doc = document, let v = info, let data = doc.engine.data(v.index) else { return }
        data[index] = Float(value)
        if v.type == .table && index == 0 && data.count > 512 { data[512] = data[0] }
    }

    private func drawSegment(from a: (index: Int, value: Double), to b: (index: Int, value: Double)) {
        let steps = abs(b.index - a.index)
        if steps == 0 { set(b.index, b.value); return }
        let dir = b.index > a.index ? 1 : -1
        for k in 0...steps {
            set(a.index + k * dir, a.value + (b.value - a.value) * Double(k) / Double(steps))
        }
    }
}

/// A window editing one wavetable or envelope, like the original's table windows.
final class TableEditorWindowController: NSWindowController, NSWindowDelegate, VariableEditor {
    let variableName: String
    private let waveView = WaveformEditView()
    private let limits = ClickableLabel(labelWithString: "")
    private let coords = NSTextField(labelWithString: "")
    private let modeControl = NSSegmentedControl(labels: ["Freehand", "Lines"], trackingMode: .selectOne, target: nil, action: nil)

    /// set at init: `document` is only set once the document adds the controller
    private(set) weak var somDocument: SOMDocument?

    init(document: SOMDocument, name: String) {
        variableName = name
        somDocument = document
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 600, height: 380),
                              styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.minSize = NSSize(width: 420, height: 260)
        super.init(window: window)
        window.delegate = self
        window.setFrameAutosaveName("SOMTable \(name)")
        build(document)
    }

    required init?(coder: NSCoder) { fatalError() }

    deinit { NotificationCenter.default.removeObserver(self) }

    override func windowTitle(forDocumentDisplayName displayName: String) -> String {
        let type = somDocument?.info(named: variableName)?.type.displayName ?? ""
        return "\(variableName) — \(type) — \(displayName)"
    }

    private func build(_ doc: SOMDocument) {
        guard let window else { return }
        waveView.document = doc
        waveView.name = variableName
        waveView.onHover = { [weak self] index, value in self?.showCoords(index, value) }
        waveView.translatesAutoresizingMaskIntoConstraints = false
        waveView.setAccessibilityLabel("\(variableName) table")

        limits.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
        limits.textColor = .secondaryLabelColor
        limits.toolTip = "Double click to set the graph bounds; ⌥-click to auto scale."
        // clicking the limits: double click sets bounds, option click auto scales (as the original)
        limits.onClick = { [weak self] event in
            if event.modifierFlags.contains(.option) { self?.somDocument?.autoScale(nil) }
            else if event.clickCount == 2 { self?.somDocument?.setBounds(nil) }
        }
        coords.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
        coords.textColor = .secondaryLabelColor

        modeControl.selectedSegment = 0
        modeControl.target = self
        modeControl.action = #selector(modeChanged(_:))
        modeControl.controlSize = .small
        modeControl.toolTip = "Freehand: drag to draw.  Lines: click points, double click to finish (or hold ⇧)."

        func small(_ title: String, _ action: Selector) -> NSButton {
            let b = NSButton(title: title, target: nil, action: action)
            b.controlSize = .small
            b.bezelStyle = .rounded
            return b
        }
        let bounds = small("Bounds…", #selector(SOMDocument.setBounds(_:)))
        let auto = small("Auto Scale", #selector(SOMDocument.autoScale(_:)))
        let fill = small("Fill…", #selector(SOMDocument.fourierFill(_:)))
        let alter = NSPopUpButton(frame: .zero, pullsDown: true)
        alter.controlSize = .small
        alter.addItem(withTitle: "Alter")
        if let alterMenu = AppDelegate.makeAlterMenu() {
            for item in alterMenu.items { alterMenu.removeItem(item); item.keyEquivalent = ""; alter.menu?.addItem(item) }
        }
        let top = NSStackView()
        top.setViews([modeControl], in: .leading)
        top.setViews([bounds, auto, fill, alter], in: .trailing)
        top.orientation = .horizontal
        top.spacing = 8
        top.edgeInsets = NSEdgeInsets(top: 6, left: 10, bottom: 6, right: 10)
        top.translatesAutoresizingMaskIntoConstraints = false
        let bottom = NSStackView()
        bottom.setViews([coords], in: .leading)
        bottom.setViews([limits], in: .trailing)
        coords.setContentCompressionResistancePriority(.init(100), for: .horizontal)
        bottom.edgeInsets = NSEdgeInsets(top: 4, left: 10, bottom: 6, right: 10)
        bottom.translatesAutoresizingMaskIntoConstraints = false

        let content = NSView()
        content.addSubview(top)
        content.addSubview(waveView)
        content.addSubview(bottom)
        NSLayoutConstraint.activate([
            top.topAnchor.constraint(equalTo: content.topAnchor),
            top.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            top.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            waveView.topAnchor.constraint(equalTo: top.bottomAnchor),
            waveView.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            waveView.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            bottom.topAnchor.constraint(equalTo: waveView.bottomAnchor),
            bottom.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            bottom.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            bottom.bottomAnchor.constraint(equalTo: content.bottomAnchor),
        ])
        window.contentView = content
        window.initialFirstResponder = waveView

        NotificationCenter.default.addObserver(self, selector: #selector(variablesChanged(_:)), name: .variablesDidChange, object: doc)
        refresh()
        showCoords(nil, nil)
    }

    @objc private func modeChanged(_ sender: NSSegmentedControl) {
        waveView.finishLines()
        waveView.mode = sender.selectedSegment == 0 ? .freehand : .lines
    }

    @objc private func variablesChanged(_ n: Notification) {
        if let names = n.userInfo?["names"] as? Set<String>, !names.contains(variableName) { return }
        refresh()
    }

    private func refresh() {
        guard let v = somDocument?.info(named: variableName) else { return }
        limits.stringValue = "upper limit = \(FormSheet.format(v.hilim))    lower limit = \(FormSheet.format(v.lolim))"
        waveView.needsDisplay = true
        synchronizeWindowTitleWithDocumentName()
    }

    private func showCoords(_ index: Int?, _ value: Double?) {
        guard let index, let value, let doc = somDocument, let v = doc.info(named: variableName) else {
            coords.stringValue = "Click and drag to draw.  Double click the limits to set them; ⌥-click them to auto scale."
            return
        }
        var s = String(format: "Horiz = %3d   Vert = %12f", index, value)
        if v.type == .envelope {
            s += String(format: "   Time = %.3f s", Double(index) / 512 * doc.engine.duration)
        }
        coords.stringValue = s
    }

    func windowWillClose(_ notification: Notification) {
        waveView.finishLines()
        somDocument?.editorClosed(variableName)
    }

    func windowWillReturnUndoManager(_ window: NSWindow) -> UndoManager? { somDocument?.undoManager }
}

/// A label that reports clicks.
final class ClickableLabel: NSTextField {
    var onClick: ((NSEvent) -> Void)?

    override func mouseDown(with event: NSEvent) {
        if let onClick { onClick(event) } else { super.mouseDown(with: event) }
    }
}
