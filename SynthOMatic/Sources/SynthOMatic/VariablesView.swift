import AppKit

/// A small picture of a table, envelope or sample.
final class WaveThumbnailView: NSView {
    var points: [Float] = [] { didSet { needsDisplay = true } }
    var lolim: Double = -1
    var hilim: Double = 1
    var color: NSColor = .labelColor
    var drawsFrame = false

    override func draw(_ dirtyRect: NSRect) {
        if drawsFrame {
            NSColor.textBackgroundColor.setFill()
            bounds.fill()
            NSColor.separatorColor.setStroke()
            NSBezierPath(rect: bounds.insetBy(dx: 0.5, dy: 0.5)).stroke()
        }
        let r = bounds.insetBy(dx: drawsFrame ? 4 : 1, dy: drawsFrame ? 4 : 2)
        guard !points.isEmpty, hilim > lolim, r.width > 2 else { return }
        let range = hilim - lolim
        func y(_ v: Float) -> CGFloat { r.minY + CGFloat((Double(v) - lolim) / range) * r.height }
        if lolim < 0 && hilim > 0 {
            NSColor.separatorColor.setFill()
            NSRect(x: r.minX, y: y(0), width: r.width, height: 0.5).fill()
        }
        color.withAlphaComponent(0.85).set()
        let columns = Int(r.width)
        let per = Double(points.count) / Double(columns)
        let path = NSBezierPath()
        path.lineWidth = 1
        if per <= 1.5 {
            for (i, v) in points.enumerated() {
                let p = NSPoint(x: r.minX + CGFloat(i) / CGFloat(max(points.count - 1, 1)) * r.width, y: y(v))
                if i == 0 { path.move(to: p) } else { path.line(to: p) }
            }
        } else {
            for c in 0..<columns {
                let a = Int(Double(c) * per), b = min(points.count, Int(Double(c + 1) * per) + 1)
                guard a < b else { continue }
                var lo = points[a], hi = points[a]
                for k in a..<b { let v = points[k]; if v.isFinite { lo = min(lo, v); hi = max(hi, v) } }
                let x = r.minX + CGFloat(c) + 0.5
                path.move(to: NSPoint(x: x, y: max(r.minY, min(r.maxY, y(lo)))))
                path.line(to: NSPoint(x: x, y: max(r.minY, min(r.maxY, y(hi)) + 0.5)))
            }
        }
        NSGraphicsContext.saveGraphicsState()
        NSBezierPath(rect: bounds).addClip()
        path.stroke()
        NSGraphicsContext.restoreGraphicsState()
    }
}

/// The table of variables.  Keys play the selected sample as in the original.
final class VariablesTableView: NSTableView {
    weak var somDocument: SOMDocument?

    override func keyDown(with event: NSEvent) {
        guard let doc = somDocument else { return super.keyDown(with: event) }
        let flags = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        if flags.contains(.command), event.charactersIgnoringModifiers == "." {
            doc.stopCalculating()
            return
        }
        if flags.isDisjoint(with: [.command, .control, .option]) {
            switch event.keyCode {
            case 36, 76:	// return, enter
                if let n = doc.targetNames().first { doc.openEditor(for: n) }
                return
            case 49:		// space
                doc.play(nil)
                return
            case 51, 117:	// delete
                doc.removeVars(nil)
                return
            default:
                if let c = event.characters?.first, doc.playKey(c) { return }
            }
        }
        super.keyDown(with: event)
    }

    @objc func copy(_ sender: Any?) {
        guard let n = somDocument?.targetNames().first else { NSSound.beep(); return }
        somDocument?.copyVariable(n)
    }

    /// the original's Cut copied
    @objc func cut(_ sender: Any?) { copy(sender) }

    @objc func paste(_ sender: Any?) {
        guard let n = somDocument?.targetNames().first else { NSSound.beep(); return }
        somDocument?.pasteVariable(into: n)
    }

    override func validateUserInterfaceItem(_ item: NSValidatedUserInterfaceItem) -> Bool {
        if item.action == #selector(copy(_:)) || item.action == #selector(cut(_:)) { return selectedRow >= 0 }
        if item.action == #selector(paste(_:)) { return selectedRow >= 0 && SOMDocument.scrap != nil }
        return super.validateUserInterfaceItem(item)
    }

    override func menu(for event: NSEvent) -> NSMenu? {
        let row = self.row(at: convert(event.locationInWindow, from: nil))
        if row >= 0 && !selectedRowIndexes.contains(row) {
            selectRowIndexes(IndexSet(integer: row), byExtendingSelection: false)
        }
        return super.menu(for: event)
    }
}

/// Data source and delegate of the variable table.
final class VariablesController: NSObject, NSTableViewDataSource, NSTableViewDelegate, NSTextFieldDelegate {
    let tableView = VariablesTableView()
    let scrollView = NSScrollView()
    private weak var document: SOMDocument?
    private var rows: [VarInfo] = []
    private var dragSnapshot: VarSnapshot?

    enum Column: String, CaseIterable {
        case random, name, type, value, control, range

        var title: String {
            switch self {
            case .random: return ""
            case .name: return "Name"
            case .type: return "Type"
            case .value: return "Value"
            case .control: return ""
            case .range: return "Range"
            }
        }
    }

    init(document: SOMDocument) {
        self.document = document
        super.init()
        tableView.somDocument = document
        for c in Column.allCases {
            let col = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(c.rawValue))
            col.title = c.title
            switch c {
            case .random: col.width = 22; col.minWidth = 22; col.maxWidth = 22
                col.headerToolTip = "Member of the random set (Synth > Randomize)"
            case .name: col.width = 150; col.minWidth = 80
            case .type: col.width = 80; col.minWidth = 60
            case .value: col.width = 130; col.minWidth = 70
            case .control: col.width = 160; col.minWidth = 60
            case .range: col.width = 120; col.minWidth = 60
            }
            tableView.addTableColumn(col)
        }
        tableView.dataSource = self
        tableView.delegate = self
        tableView.allowsMultipleSelection = true
        tableView.usesAlternatingRowBackgroundColors = true
        tableView.rowHeight = 22
        tableView.style = .fullWidth
        tableView.target = self
        tableView.doubleAction = #selector(doubleClicked(_:))
        tableView.columnAutoresizingStyle = .lastColumnOnlyAutoresizingStyle
        tableView.setAccessibilityLabel("Variables")
        tableView.menu = makeContextMenu()
        scrollView.documentView = tableView
        scrollView.hasVerticalScroller = true
        scrollView.borderType = .noBorder
        reload()
    }

    private func makeContextMenu() -> NSMenu {
        let m = NSMenu()
        m.addItem(withTitle: "Edit…", action: #selector(SOMDocument.editVariable(_:)), keyEquivalent: "")
        m.addItem(withTitle: "Play", action: #selector(SOMDocument.play(_:)), keyEquivalent: "")
        m.addItem(withTitle: "Export Sample…", action: #selector(SOMDocument.exportSample(_:)), keyEquivalent: "")
        m.addItem(withTitle: "Import Sound…", action: #selector(SOMDocument.importSound(_:)), keyEquivalent: "")
        m.addItem(.separator())
        let type = NSMenuItem(title: "Set Type", action: nil, keyEquivalent: "")
        let sub = NSMenu()
        sub.addItem(withTitle: "Wavetable", action: #selector(SOMDocument.setTypeToWavetable(_:)), keyEquivalent: "")
        sub.addItem(withTitle: "Variable", action: #selector(SOMDocument.setTypeToVariable(_:)), keyEquivalent: "")
        sub.addItem(withTitle: "Envelope", action: #selector(SOMDocument.setTypeToEnvelope(_:)), keyEquivalent: "")
        sub.addItem(withTitle: "Sample", action: #selector(SOMDocument.setTypeToSample(_:)), keyEquivalent: "")
        type.submenu = sub
        m.addItem(type)
        m.addItem(.separator())
        m.addItem(withTitle: "Copy", action: #selector(VariablesTableView.copy(_:)), keyEquivalent: "")
        m.addItem(withTitle: "Paste", action: #selector(VariablesTableView.paste(_:)), keyEquivalent: "")
        m.addItem(.separator())
        m.addItem(withTitle: "Remove Vars", action: #selector(SOMDocument.removeVars(_:)), keyEquivalent: "")
        return m
    }

    var selectedNames: [String] {
        tableView.selectedRowIndexes.compactMap { $0 < rows.count ? rows[$0].name : nil }
    }

    func select(_ names: [String]) {
        let set = Set(names)
        let idx = IndexSet(rows.indices.filter { set.contains(rows[$0].name) })
        tableView.selectRowIndexes(idx, byExtendingSelection: false)
        if let first = idx.first { tableView.scrollRowToVisible(first) }
    }

    func reload(names: Set<String>? = nil) {
        guard let document else { return }
        let selected = selectedNames
        let new = document.vars
        if let names, new.map(\.name) == rows.map(\.name) {
            // just refresh the changed rows
            rows = new
            let changed = IndexSet(rows.indices.filter { names.contains(rows[$0].name) })
            tableView.reloadData(forRowIndexes: changed, columnIndexes: IndexSet(integersIn: 0..<tableView.numberOfColumns))
            return
        }
        rows = new
        tableView.reloadData()
        select(selected)
    }

    // MARK: data source

    func numberOfRows(in tableView: NSTableView) -> Int { rows.count }

    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard let id = tableColumn?.identifier, let column = Column(rawValue: id.rawValue), row < rows.count else { return nil }
        let v = rows[row]
        switch column {
        case .random:
            guard v.isParameter else { return nil }
            let b = NSButton(checkboxWithTitle: "", target: self, action: #selector(randomToggled(_:)))
            b.state = v.randomset ? .on : .off
            b.tag = row
            b.toolTip = "Randomize this parameter"
            return b
        case .name:
            let cell = label(id)
            let s = NSMutableAttributedString(string: v.name, attributes: [
                .font: NSFont.monospacedSystemFont(ofSize: 12, weight: v.isParameter ? .semibold : .regular),
                .foregroundColor: Palette.color(for: v),
            ])
            if v.pfield > 0 {
                s.append(NSAttributedString(string: "  p\(v.pfield)", attributes: [
                    .font: NSFont.systemFont(ofSize: 10), .foregroundColor: NSColor.tertiaryLabelColor]))
            }
            cell.textField?.attributedStringValue = s
            return cell
        case .type:
            let cell = label(id)
            cell.textField?.stringValue = v.typeName
            cell.textField?.textColor = .secondaryLabelColor
            return cell
        case .value:
            if v.type == .scalar {
                let tf = NSTextField(string: format(v.value))
                tf.isBordered = false
                tf.drawsBackground = false
                tf.font = .monospacedDigitSystemFont(ofSize: 12, weight: .regular)
                tf.alignment = .right
                tf.isEditable = v.isParameter
                tf.textColor = v.isParameter ? .labelColor : .secondaryLabelColor
                tf.tag = row
                tf.target = self
                tf.action = #selector(valueEdited(_:))
                tf.delegate = self
                tf.lineBreakMode = .byTruncatingTail
                return wrap(tf)
            }
            let thumb = WaveThumbnailView()
            if let doc = document {
                var pts = doc.engine.points(v.index)
                if v.type == .table && pts.count > 512 { pts.removeLast(pts.count - 512) }
                thumb.points = pts
            }
            thumb.lolim = v.lolim
            thumb.hilim = v.hilim
            thumb.color = Palette.color(for: v)
            return thumb
        case .control:
            guard v.isParameter, v.hilim > v.lolim else { return nil }
            let s = NSSlider(value: v.value, minValue: v.lolim, maxValue: v.hilim, target: self, action: #selector(sliderMoved(_:)))
            s.controlSize = .small
            s.tag = row
            s.isContinuous = true
            if v.stepsiz > 0 {
                let steps = Int(((v.hilim - v.lolim) / v.stepsiz).rounded())
                if steps <= 40 && steps > 0 {
                    s.numberOfTickMarks = steps + 1
                    s.allowsTickMarkValuesOnly = true
                }
            }
            return wrap(s, inset: 4)
        case .range:
            let cell = label(id)
            if v.type == .sample {
                cell.textField?.stringValue = String(format: "%d samples", v.length)
            } else if v.type == .scalar && !v.isParameter {
                cell.textField?.stringValue = ""
            } else {
                cell.textField?.stringValue = "\(format(v.lolim)) … \(format(v.hilim))"
            }
            cell.textField?.textColor = .tertiaryLabelColor
            cell.textField?.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
            return cell
        }
    }

    private func label(_ id: NSUserInterfaceItemIdentifier) -> NSTableCellView {
        if let cell = tableView.makeView(withIdentifier: id, owner: self) as? NSTableCellView { return cell }
        let cell = NSTableCellView()
        cell.identifier = id
        let tf = NSTextField(labelWithString: "")
        tf.lineBreakMode = .byTruncatingTail
        tf.translatesAutoresizingMaskIntoConstraints = false
        cell.addSubview(tf)
        cell.textField = tf
        NSLayoutConstraint.activate([
            tf.leadingAnchor.constraint(equalTo: cell.leadingAnchor, constant: 2),
            tf.trailingAnchor.constraint(equalTo: cell.trailingAnchor, constant: -2),
            tf.centerYAnchor.constraint(equalTo: cell.centerYAnchor),
        ])
        return cell
    }

    private func wrap(_ view: NSView, inset: CGFloat = 2) -> NSView {
        let box = NSView()
        view.translatesAutoresizingMaskIntoConstraints = false
        box.addSubview(view)
        NSLayoutConstraint.activate([
            view.leadingAnchor.constraint(equalTo: box.leadingAnchor, constant: inset),
            view.trailingAnchor.constraint(equalTo: box.trailingAnchor, constant: -inset),
            view.centerYAnchor.constraint(equalTo: box.centerYAnchor),
        ])
        return box
    }

    private func format(_ x: Double) -> String {
        if x == 0 { return "0" }
        if abs(x) >= 1e6 || abs(x) < 1e-4 { return String(format: "%.4g", x) }
        return FormSheet.format((x * 1e6).rounded() / 1e6)
    }

    func tableView(_ tableView: NSTableView, shouldEdit tableColumn: NSTableColumn?, row: Int) -> Bool { false }

    func tableViewSelectionDidChange(_ notification: Notification) {
        NotificationCenter.default.post(name: .selectionDidChange, object: document)
    }

    // MARK: actions

    @objc private func doubleClicked(_ sender: Any?) {
        let row = tableView.clickedRow
        guard row >= 0, row < rows.count else { return }
        document?.openEditor(for: rows[row].name)
    }

    @objc private func randomToggled(_ sender: NSButton) {
        guard sender.tag < rows.count else { return }
        document?.setRandomSet(rows[sender.tag].name, on: sender.state == .on)
    }

    @objc private func valueEdited(_ sender: NSTextField) {
        guard sender.tag < rows.count, let doc = document else { return }
        let name = rows[sender.tag].name
        guard let x = FormSheet.parseNumber(sender.stringValue) else {
            NSSound.beep()
            sender.stringValue = format(rows[sender.tag].value)
            return
        }
        doc.setValue(x, of: name)
    }

    @objc private func sliderMoved(_ sender: NSSlider) {
        guard sender.tag < rows.count, let doc = document else { return }
        let v = rows[sender.tag]
        var x = sender.doubleValue
        if v.stepsiz > 0 {
            // the original's slider snapped to steps from the lower limit
            x = v.lolim + ((x - v.lolim) / v.stepsiz).rounded() * v.stepsiz
        }
        if dragSnapshot == nil { dragSnapshot = doc.beginContinuousChange(v.name) }
        doc.setValueLive(x, of: v.name)
        rows[sender.tag].value = x
        if let valueView = tableView.view(atColumn: tableView.column(withIdentifier: NSUserInterfaceItemIdentifier(Column.value.rawValue)),
                                          row: sender.tag, makeIfNecessary: false),
           let tf = valueView.subviews.first as? NSTextField {
            tf.stringValue = format(x)
        }
        let type = NSApp.currentEvent?.type
        if type == .leftMouseUp || type == .keyDown || type == nil {
            finishSlider()
        } else {
            // in case the last action doesn't come with the mouse up
            DispatchQueue.main.async { [weak self] in
                if NSEvent.pressedMouseButtons == 0 { self?.finishSlider() }
            }
        }
    }

    private func finishSlider() {
        guard let snapshot = dragSnapshot else { return }
        dragSnapshot = nil
        document?.endContinuousChange(snapshot, actionName: "Slider")
    }
}

extension Notification.Name {
    static let selectionDidChange = Notification.Name("SOMSelectionDidChange")
}
