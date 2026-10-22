import AppKit

/// A modal sheet of labelled text fields, standing in for the original's
/// DLOG/DITL dialogs.
final class FormSheet: NSWindowController, NSTextFieldDelegate {
    struct Field {
        var label: String
        var value: String
        var unit: String = ""
        var isNumber = true
    }

    struct Choice {
        var label: String
        var options: [String]
        var selected: Int
    }

    struct Result {
        var strings: [String]
        var choice: Int

        func number(_ i: Int) -> Double? { FormSheet.parseNumber(strings[i]) }
    }

    /// An extra button (like "Auto Scale") that can rewrite the fields.
    struct Action {
        var title: String
        var handler: (FormSheet) -> Void
    }

    private var textFields: [NSTextField] = []
    private var radios: [NSButton] = []
    private var fieldSpecs: [Field] = []
    private var completion: ((Result?) -> Void)?
    private var actions: [Action] = []
    private weak var parentWindow: NSWindow?
    private var okButton: NSButton!

    static func parseNumber(_ s: String) -> Double? {
        let t = s.trimmingCharacters(in: .whitespaces)
        if let d = Double(t) { return d }
        // accept "1/3" and similar simple ratios
        let parts = t.split(separator: "/")
        if parts.count == 2, let a = Double(parts[0].trimmingCharacters(in: .whitespaces)),
           let b = Double(parts[1].trimmingCharacters(in: .whitespaces)), b != 0 { return a / b }
        return nil
    }

    static func format(_ x: Double) -> String {
        if x == x.rounded() && abs(x) < 1e15 { return String(format: "%.0f", x) }
        var s = String(format: "%.6f", x)
        while s.hasSuffix("0") { s.removeLast() }
        if s.hasSuffix(".") { s.removeLast() }
        return s
    }

    /// Presents the sheet; `completion` gets nil on Cancel.
    @discardableResult
    static func present(on window: NSWindow, title: String, message: String? = nil,
                        fields: [Field], choice: Choice? = nil, actions: [Action] = [],
                        okTitle: String = "OK", completion: @escaping (Result?) -> Void) -> FormSheet {
        let sheet = FormSheet(window: nil)
        sheet.build(title: title, message: message, fields: fields, choice: choice, actions: actions, okTitle: okTitle)
        sheet.completion = completion
        sheet.parentWindow = window
        window.beginSheet(sheet.window!) { _ in _ = sheet }
        return sheet
    }

    func setValue(_ value: String, at i: Int) {
        guard i < textFields.count else { return }
        textFields[i].stringValue = value
    }

    func value(at i: Int) -> String { textFields[i].stringValue }

    private func build(title: String, message: String?, fields: [Field], choice: Choice?, actions: [Action], okTitle: String) {
        fieldSpecs = fields
        self.actions = actions
        let content = NSView()
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false
        content.addSubview(stack)

        let titleLabel = NSTextField(labelWithString: title)
        titleLabel.font = .boldSystemFont(ofSize: 13)
        stack.addArrangedSubview(titleLabel)
        if let message {
            let m = NSTextField(wrappingLabelWithString: message)
            m.font = .systemFont(ofSize: 11)
            m.textColor = .secondaryLabelColor
            m.preferredMaxLayoutWidth = 320
            stack.addArrangedSubview(m)
        }

        if let choice {
            let row = NSStackView()
            row.orientation = .horizontal
            row.alignment = .firstBaseline
            let label = NSTextField(labelWithString: choice.label)
            label.alignment = .right
            label.widthAnchor.constraint(equalToConstant: 150).isActive = true
            row.addArrangedSubview(label)
            let col = NSStackView()
            col.orientation = .vertical
            col.alignment = .leading
            col.spacing = 4
            for (i, option) in choice.options.enumerated() {
                let b = NSButton(radioButtonWithTitle: option, target: self, action: #selector(radioChanged(_:)))
                b.tag = i
                b.state = i == choice.selected ? .on : .off
                radios.append(b)
                col.addArrangedSubview(b)
            }
            row.addArrangedSubview(col)
            stack.addArrangedSubview(row)
        }

        let grid = NSGridView()
        grid.rowSpacing = 8
        grid.columnSpacing = 8
        for f in fields {
            let label = NSTextField(labelWithString: f.label)
            label.alignment = .right
            let tf = NSTextField(string: f.value)
            tf.delegate = self
            tf.widthAnchor.constraint(equalToConstant: f.isNumber ? 130 : 200).isActive = true
            if f.isNumber { tf.alignment = .right }
            textFields.append(tf)
            let unit = NSTextField(labelWithString: f.unit)
            unit.textColor = .secondaryLabelColor
            grid.addRow(with: [label, tf, unit])
        }
        if fields.count > 0 {
            grid.column(at: 0).xPlacement = .trailing
            grid.column(at: 0).width = 150
            grid.rowAlignment = .firstBaseline
            stack.addArrangedSubview(grid)
        }

        let buttons = NSStackView()
        buttons.orientation = .horizontal
        buttons.spacing = 8
        for (i, a) in actions.enumerated() {
            let b = NSButton(title: a.title, target: self, action: #selector(runAction(_:)))
            b.tag = i
            buttons.addArrangedSubview(b)
        }
        let spacer = NSView()
        spacer.setContentHuggingPriority(.defaultLow, for: .horizontal)
        buttons.addArrangedSubview(spacer)
        let cancel = NSButton(title: "Cancel", target: self, action: #selector(cancel(_:)))
        cancel.keyEquivalent = "\u{1b}"
        let ok = NSButton(title: okTitle, target: self, action: #selector(ok(_:)))
        ok.keyEquivalent = "\r"
        okButton = ok
        buttons.addArrangedSubview(cancel)
        buttons.addArrangedSubview(ok)
        stack.addArrangedSubview(buttons)
        buttons.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true

        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 20),
            stack.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -20),
            stack.topAnchor.constraint(equalTo: content.topAnchor, constant: 20),
            stack.bottomAnchor.constraint(equalTo: content.bottomAnchor, constant: -16),
            stack.widthAnchor.constraint(greaterThanOrEqualToConstant: 340),
        ])
        let panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 400, height: 200),
                            styleMask: [.titled], backing: .buffered, defer: false)
        panel.contentView = content
        panel.title = title
        window = panel
        if let first = textFields.first {
            panel.initialFirstResponder = first
        }
        validate()
    }

    @objc private func radioChanged(_ sender: NSButton) {
        for b in radios { b.state = b == sender ? .on : .off }
    }

    @objc private func runAction(_ sender: NSButton) {
        actions[sender.tag].handler(self)
        validate()
    }

    func controlTextDidChange(_ obj: Notification) { validate() }

    private func validate() {
        var ok = true
        for (i, f) in fieldSpecs.enumerated() where f.isNumber {
            let valid = FormSheet.parseNumber(textFields[i].stringValue) != nil
            textFields[i].textColor = valid ? .labelColor : .systemRed
            ok = ok && valid
        }
        okButton?.isEnabled = ok
    }

    private func finish(_ result: Result?) {
        guard let window, let parent = window.sheetParent ?? parentWindow else { return }
        parent.endSheet(window)
        completion?(result)
        completion = nil
    }

    @objc private func ok(_ sender: Any?) {
        window?.makeFirstResponder(nil)
        let choice = radios.firstIndex { $0.state == .on } ?? 0
        finish(Result(strings: textFields.map(\.stringValue), choice: choice))
    }

    @objc private func cancel(_ sender: Any?) {
        finish(nil)
    }
}
