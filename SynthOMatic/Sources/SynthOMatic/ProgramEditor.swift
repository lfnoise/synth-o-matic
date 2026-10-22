import AppKit

/// Colors for the program editor and the variable list.
enum Palette {
    static let comment = NSColor.systemGreen
    static let number = NSColor.systemPurple
    static let function = NSColor.systemBlue
    static let keyword = NSColor.systemPink
    static let table = NSColor.systemOrange
    static let envelope = NSColor.systemTeal
    static let sample = NSColor.systemRed
    static let parameter = NSColor.systemBrown
    static let unknownCall = NSColor.systemRed

    static func color(for v: VarInfo) -> NSColor {
        switch v.type {
        case .table: return table
        case .envelope: return envelope
        case .sample: return sample
        case .scalar: return v.isParameter ? parameter : .labelColor
        }
    }

    static let editorFont = NSFont.monospacedSystemFont(ofSize: 13, weight: .regular)
}

/// A token of the Synth-O-Matic language, enough to color and to find calls.
struct Token {
    enum Kind { case identifier, number, comment, punct, other }
    var kind: Kind
    var range: NSRange
}

enum Lexer {
    /// Splits a program the way the original lexer would, closely enough for coloring.
    static func tokens(_ s: NSString) -> [Token] {
        var out: [Token] = []
        let n = s.length
        var i = 0
        func ch(_ k: Int) -> unichar { k < n ? s.character(at: k) : 0 }
        func isAlpha(_ c: unichar) -> Bool { (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95 }
        func isDigit(_ c: unichar) -> Bool { c >= 48 && c <= 57 }
        while i < n {
            let c = ch(i)
            if c == 47 && ch(i+1) == 47 {				// //
                var j = i
                while j < n && ch(j) != 10 && ch(j) != 13 { j += 1 }
                out.append(Token(kind: .comment, range: NSRange(location: i, length: j - i)))
                i = j
            } else if c == 47 && ch(i+1) == 42 {		// /*
                var j = i + 2
                while j < n && !(ch(j) == 42 && ch(j+1) == 47) { j += 1 }
                j = min(n, j + 2)
                out.append(Token(kind: .comment, range: NSRange(location: i, length: j - i)))
                i = j
            } else if isAlpha(c) || (c == 64 && isAlpha(ch(i+1))) {	// identifiers, @names
                var j = i + 1
                while j < n && (isAlpha(ch(j)) || isDigit(ch(j))) { j += 1 }
                if ch(j) == 39 && ch(j+1) >= 49 && ch(j+1) <= 52 { j += 2 }		// name'1 .. name'4
                out.append(Token(kind: .identifier, range: NSRange(location: i, length: j - i)))
                i = j
            } else if isDigit(c) || (c == 46 && isDigit(ch(i+1))) {
                var j = i
                while j < n && (isDigit(ch(j)) || ch(j) == 46) { j += 1 }
                if ch(j) == 101 || ch(j) == 69 {
                    var k = j + 1
                    if ch(k) == 43 || ch(k) == 45 { k += 1 }
                    if isDigit(ch(k)) { j = k; while j < n && isDigit(ch(j)) { j += 1 } }
                }
                out.append(Token(kind: .number, range: NSRange(location: i, length: j - i)))
                i = j
            } else if c == 32 || c == 9 || c == 10 || c == 13 {
                i += 1
            } else {
                out.append(Token(kind: .punct, range: NSRange(location: i, length: 1)))
                i += 1
            }
        }
        return out
    }
}

/// The text view of the program.
final class CodeTextView: NSTextView {
    weak var somDocument: SOMDocument?
    var onCursorMove: (() -> Void)?

    override func insertNewline(_ sender: Any?) {
        // keep the indentation of the line
        let s = string as NSString
        let lineRange = s.lineRange(for: NSRange(location: selectedRange().location, length: 0))
        var indent = ""
        var k = lineRange.location
        while k < min(s.length, selectedRange().location) {
            let c = s.character(at: k)
            guard c == 32 || c == 9 else { break }
            indent.append(c == 32 ? " " : "\t")
            k += 1
        }
        super.insertNewline(sender)
        if !indent.isEmpty { insertText(indent, replacementRange: selectedRange()) }
    }

    override func completions(forPartialWordRange charRange: NSRange, indexOfSelectedItem index: UnsafeMutablePointer<Int>) -> [String]? {
        let prefix = (string as NSString).substring(with: charRange)
        guard !prefix.isEmpty else { return nil }
        var names = Engine.functionNames
        names += somDocument?.vars.map(\.name) ?? []
        names += ["if", "else", "sr", "nyq", "tempo", "dur", "note"]
        let matches = Array(Set(names.filter { $0.hasPrefix(prefix) && $0 != prefix })).sorted()
        index.pointee = matches.isEmpty ? -1 : 0
        return matches
    }

    override func setSelectedRanges(_ ranges: [NSValue], affinity: NSSelectionAffinity, stillSelecting: Bool) {
        super.setSelectedRanges(ranges, affinity: affinity, stillSelecting: stillSelecting)
        onCursorMove?()
    }

    override func keyDown(with event: NSEvent) {
        // the original's command-period stops a calculation
        if event.modifierFlags.contains(.command), event.charactersIgnoringModifiers == "." {
            somDocument?.stopCalculating()
            return
        }
        super.keyDown(with: event)
    }
}

/// Line numbers, with a mark on the line of a compile error.
final class LineNumberRuler: NSRulerView {
    weak var textView: NSTextView?
    var errorLine = 0 { didSet { needsDisplay = true } }

    init(textView: NSTextView, scrollView: NSScrollView) {
        self.textView = textView
        super.init(scrollView: scrollView, orientation: .verticalRuler)
        clientView = textView
        ruleThickness = 40
    }

    required init(coder: NSCoder) { fatalError() }

    override func drawHashMarksAndLabels(in rect: NSRect) {
        guard let tv = textView, let lm = tv.layoutManager, let tc = tv.textContainer else { return }
        NSColor.textBackgroundColor.setFill()
        bounds.fill()
        NSColor.separatorColor.setFill()
        NSRect(x: bounds.maxX - 1, y: bounds.minY, width: 1, height: bounds.height).fill()

        let s = tv.string as NSString
        let visible = tv.visibleRect
        let glyphRange = lm.glyphRange(forBoundingRect: visible, in: tc)
        let charRange = lm.characterRange(forGlyphRange: glyphRange, actualGlyphRange: nil)
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedDigitSystemFont(ofSize: 10, weight: .regular),
            .foregroundColor: NSColor.tertiaryLabelColor,
        ]
        let errorAttrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedDigitSystemFont(ofSize: 10, weight: .bold),
            .foregroundColor: NSColor.white,
        ]
        var line = 1
        // count lines before the visible range
        s.enumerateSubstrings(in: NSRange(location: 0, length: charRange.location), options: [.byLines, .substringNotRequired]) { _, _, _, _ in
            line += 1
        }
        let yOffset = convert(NSPoint.zero, from: tv).y
        var index = charRange.location
        func drawNumber(_ rectInText: NSRect) {
            let y = rectInText.minY + yOffset + tv.textContainerInset.height
            let label = "\(line)" as NSString
            let isError = line == errorLine
            let a = isError ? errorAttrs : attrs
            let size = label.size(withAttributes: a)
            let r = NSRect(x: ruleThickness - size.width - 8, y: y + (rectInText.height - size.height) / 2, width: size.width, height: size.height)
            if isError {
                NSColor.systemRed.setFill()
                NSBezierPath(roundedRect: r.insetBy(dx: -4, dy: -1), xRadius: 6, yRadius: 6).fill()
            }
            label.draw(at: r.origin, withAttributes: a)
        }
        while index < NSMaxRange(charRange) {
            let lineRange = s.lineRange(for: NSRange(location: index, length: 0))
            let glyphs = lm.glyphRange(forCharacterRange: NSRange(location: lineRange.location, length: 0), actualCharacterRange: nil)
            let rect = lm.lineFragmentRect(forGlyphAt: glyphs.location, effectiveRange: nil)
            drawNumber(rect)
            line += 1
            index = NSMaxRange(lineRange)
        }
        // the empty last line
        if index == s.length && (s.length == 0 || s.hasSuffix("\n")) {
            let rect = lm.extraLineFragmentRect
            if rect.height > 0 { drawNumber(rect) }
        }
    }
}

/// The program editor: text, line numbers and a status line with signature help.
final class ProgramEditorView: NSView, NSTextViewDelegate, NSTextStorageDelegate {
    let textView: CodeTextView
    let scrollView = NSScrollView()
    let ruler: LineNumberRuler
    private let status = NSTextField(labelWithString: "")
    private weak var document: SOMDocument?
    private var highlightPending = false

    init(document: SOMDocument) {
        self.document = document
        let storage = document.programText
        let layout = NSLayoutManager()
        storage.addLayoutManager(layout)
        let container = NSTextContainer(size: NSSize(width: 0, height: CGFloat.greatestFiniteMagnitude))
        container.widthTracksTextView = true
        layout.addTextContainer(container)
        textView = CodeTextView(frame: .zero, textContainer: container)
        ruler = LineNumberRuler(textView: textView, scrollView: scrollView)
        super.init(frame: .zero)

        textView.somDocument = document
        textView.isRichText = false
        textView.allowsUndo = true
        textView.font = Palette.editorFont
        textView.typingAttributes = [.font: Palette.editorFont, .foregroundColor: NSColor.textColor]
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        textView.isAutomaticSpellingCorrectionEnabled = false
        textView.isContinuousSpellCheckingEnabled = false
        textView.smartInsertDeleteEnabled = false
        textView.usesFindBar = true
        textView.isIncrementalSearchingEnabled = true
        textView.textContainerInset = NSSize(width: 4, height: 6)
        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        textView.autoresizingMask = [.width]
        textView.minSize = NSSize(width: 0, height: 0)
        textView.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        textView.delegate = self
        textView.onCursorMove = { [weak self] in self?.updateStatus() }
        textView.setAccessibilityLabel("Program")
        storage.delegate = self
        // the document's text may have been set before the font
        storage.addAttributes([.font: Palette.editorFont, .foregroundColor: NSColor.textColor],
                              range: NSRange(location: 0, length: storage.length))

        scrollView.documentView = textView
        scrollView.hasVerticalScroller = true
        scrollView.hasVerticalRuler = true
        scrollView.verticalRulerView = ruler
        scrollView.rulersVisible = true
        scrollView.borderType = .noBorder
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(scrollView)

        status.font = .systemFont(ofSize: 11)
        status.textColor = .secondaryLabelColor
        status.lineBreakMode = .byTruncatingTail
        status.translatesAutoresizingMaskIntoConstraints = false
        status.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        let statusBar = NSVisualEffectView()
        statusBar.material = .headerView
        statusBar.translatesAutoresizingMaskIntoConstraints = false
        statusBar.addSubview(status)
        addSubview(statusBar)
        let line = NSBox()
        line.boxType = .separator
        line.translatesAutoresizingMaskIntoConstraints = false
        addSubview(line)

        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: trailingAnchor),
            line.topAnchor.constraint(equalTo: scrollView.bottomAnchor),
            line.leadingAnchor.constraint(equalTo: leadingAnchor),
            line.trailingAnchor.constraint(equalTo: trailingAnchor),
            statusBar.topAnchor.constraint(equalTo: line.bottomAnchor),
            statusBar.leadingAnchor.constraint(equalTo: leadingAnchor),
            statusBar.trailingAnchor.constraint(equalTo: trailingAnchor),
            statusBar.bottomAnchor.constraint(equalTo: bottomAnchor),
            statusBar.heightAnchor.constraint(equalToConstant: 22),
            status.leadingAnchor.constraint(equalTo: statusBar.leadingAnchor, constant: 8),
            status.trailingAnchor.constraint(lessThanOrEqualTo: statusBar.trailingAnchor, constant: -8),
            status.centerYAnchor.constraint(equalTo: statusBar.centerYAnchor),
        ])

        NotificationCenter.default.addObserver(self, selector: #selector(variablesChanged(_:)), name: .variablesDidChange, object: document)
        NotificationCenter.default.addObserver(self, selector: #selector(compileFinished(_:)), name: .compileDidFinish, object: document)
        highlight()
        updateStatus()
    }

    required init?(coder: NSCoder) { fatalError() }

    deinit { NotificationCenter.default.removeObserver(self) }

    // MARK: coloring

    func textStorage(_ textStorage: NSTextStorage, didProcessEditing editedMask: NSTextStorageEditActions, range editedRange: NSRange, changeInLength delta: Int) {
        guard editedMask.contains(.editedCharacters) else { return }
        if ruler.errorLine != 0 { ruler.errorLine = 0 }
        scheduleHighlight()
    }

    private func scheduleHighlight() {
        guard !highlightPending else { return }
        highlightPending = true
        DispatchQueue.main.async { [weak self] in
            self?.highlightPending = false
            self?.highlight()
            self?.ruler.needsDisplay = true
        }
    }

    @objc private func variablesChanged(_ n: Notification) { scheduleHighlight() }

    @objc private func compileFinished(_ n: Notification) {
        let line = n.userInfo?["errorLine"] as? Int ?? 0
        ruler.errorLine = line
        if line > 0 { reveal(line: line) }
        scheduleHighlight()
    }

    func highlight() {
        guard let lm = textView.layoutManager, let document else { return }
        let s = textView.string as NSString
        let all = NSRange(location: 0, length: s.length)
        lm.removeTemporaryAttribute(.underlineStyle, forCharacterRange: all)
        lm.addTemporaryAttribute(.foregroundColor, value: NSColor.textColor, forCharacterRange: all)
        let byName = Dictionary(document.vars.map { ($0.name, $0) }, uniquingKeysWith: { a, _ in a })
        let functions = Set(Engine.functionNames)
        let tokens = Lexer.tokens(s)
        for (k, t) in tokens.enumerated() {
            switch t.kind {
            case .comment:
                lm.addTemporaryAttribute(.foregroundColor, value: Palette.comment, forCharacterRange: t.range)
            case .number:
                lm.addTemporaryAttribute(.foregroundColor, value: Palette.number, forCharacterRange: t.range)
            case .identifier:
                let word = s.substring(with: t.range)
                let nextIsParen = k + 1 < tokens.count && tokens[k+1].kind == .punct && s.character(at: tokens[k+1].range.location) == 40
                if word == "if" || word == "else" {
                    lm.addTemporaryAttribute(.foregroundColor, value: Palette.keyword, forCharacterRange: t.range)
                } else if functions.contains(word) {
                    lm.addTemporaryAttribute(.foregroundColor, value: Palette.function, forCharacterRange: t.range)
                } else if nextIsParen {
                    lm.addTemporaryAttribute(.foregroundColor, value: Palette.unknownCall, forCharacterRange: t.range)
                    lm.addTemporaryAttribute(.underlineStyle, value: NSUnderlineStyle.patternDot.rawValue | NSUnderlineStyle.single.rawValue, forCharacterRange: t.range)
                } else if let v = byName[word] {
                    lm.addTemporaryAttribute(.foregroundColor, value: Palette.color(for: v), forCharacterRange: t.range)
                }
            default:
                break
            }
        }
    }

    // MARK: status line

    /// Finds the call the cursor is in and which argument.
    private func callContext() -> (name: String, argument: Int)? {
        let s = textView.string as NSString
        var i = min(textView.selectedRange().location, s.length) - 1
        var depth = 0, commas = 0
        while i >= 0 {
            let c = s.character(at: i)
            if c == 41 { depth += 1 }					// )
            else if c == 40 {							// (
                if depth == 0 {
                    var j = i - 1
                    while j >= 0 && (s.character(at: j) == 32 || s.character(at: j) == 9) { j -= 1 }
                    var k = j
                    while k >= 0, let u = UnicodeScalar(s.character(at: k)), CharacterSet.alphanumerics.contains(u) || u == "_" { k -= 1 }
                    if j > k {
                        let name = s.substring(with: NSRange(location: k + 1, length: j - k))
                        if Reference.entriesByName[name] != nil { return (name, commas) }
                    }
                    // a parenthesized expression: keep looking outward
                    commas = 0
                } else { depth -= 1 }
            } else if c == 44 && depth == 0 { commas += 1 }		// ,
            else if c == 59 { return nil }						// ;
            i -= 1
        }
        return nil
    }

    func updateStatus() {
        let s = textView.string as NSString
        let loc = min(textView.selectedRange().location, s.length)
        var line = 1
        s.enumerateSubstrings(in: NSRange(location: 0, length: loc), options: [.byLines, .substringNotRequired]) { _, _, _, _ in line += 1 }
        if loc > 0, s.character(at: loc - 1) != 10 { line -= 1 }
        let lineStart = s.lineRange(for: NSRange(location: loc, length: 0)).location
        let column = loc - lineStart + 1
        let result = NSMutableAttributedString(string: "Line \(max(line, 1)), Col \(column)",
                                               attributes: [.foregroundColor: NSColor.tertiaryLabelColor, .font: NSFont.systemFont(ofSize: 11)])
        if let (name, arg) = callContext(), let entry = Reference.entriesByName[name] {
            result.append(NSAttributedString(string: "    "))
            let mono = NSFont.monospacedSystemFont(ofSize: 11, weight: .regular)
            let bold = NSFont.monospacedSystemFont(ofSize: 11, weight: .bold)
            result.append(NSAttributedString(string: name + "(", attributes: [.font: mono, .foregroundColor: Palette.function]))
            let args = entry.arguments
            for (k, a) in args.enumerated() {
                let isCurrent = k == arg
                result.append(NSAttributedString(string: a, attributes: [.font: isCurrent ? bold : mono,
                                                                        .foregroundColor: isCurrent ? NSColor.labelColor : NSColor.secondaryLabelColor]))
                if k < args.count - 1 { result.append(NSAttributedString(string: ", ", attributes: [.font: mono, .foregroundColor: NSColor.secondaryLabelColor])) }
            }
            result.append(NSAttributedString(string: ")", attributes: [.font: mono, .foregroundColor: Palette.function]))
            result.append(NSAttributedString(string: "  — " + entry.summary, attributes: [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor.secondaryLabelColor]))
        }
        status.attributedStringValue = result
    }

    func reveal(line: Int) {
        let s = textView.string as NSString
        var current = 1
        var location = 0
        while current < line && location < s.length {
            location = NSMaxRange(s.lineRange(for: NSRange(location: location, length: 0)))
            current += 1
        }
        let range = s.lineRange(for: NSRange(location: min(location, s.length), length: 0))
        textView.scrollRangeToVisible(range)
        textView.showFindIndicator(for: range)
    }
}
