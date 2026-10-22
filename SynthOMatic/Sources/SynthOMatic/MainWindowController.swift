import AppKit

/// The document window.  The original had three windows - Program, Variables
/// and Message - which are the three panes here.
final class MainWindowController: NSWindowController, NSWindowDelegate, NSToolbarDelegate, NSSplitViewDelegate {
    private var editor: ProgramEditorView!
    private var variables: VariablesController!
    private let split = NSSplitView()
    private let globalsButton = NSButton()
    private var progressSheet: NSWindow?
    private var progressBar: NSProgressIndicator?
    private var progressLabel: NSTextField?
    private var progressTimer: Timer?

    var somDocument: SOMDocument? { document as? SOMDocument }

    init() {
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 880, height: 720),
                              styleMask: [.titled, .closable, .miniaturizable, .resizable],
                              backing: .buffered, defer: false)
        window.minSize = NSSize(width: 560, height: 420)
        window.tabbingMode = .preferred
        super.init(window: window)
        window.delegate = self
        shouldCascadeWindows = true
        // closing the patch window closes the patch and its editors
        shouldCloseDocument = true
    }

    required init?(coder: NSCoder) { fatalError() }

    deinit { NotificationCenter.default.removeObserver(self) }

    override var document: AnyObject? {
        didSet { if let doc = document as? SOMDocument, editor == nil { build(doc) } }
    }

    private func build(_ doc: SOMDocument) {
        guard let window else { return }
        editor = ProgramEditorView(document: doc)
        variables = VariablesController(document: doc)

        let consoleScroll = NSScrollView()
        let storage = doc.messages
        let layout = NSLayoutManager()
        storage.addLayoutManager(layout)
        let container = NSTextContainer(size: NSSize(width: 0, height: CGFloat.greatestFiniteMagnitude))
        container.widthTracksTextView = true
        layout.addTextContainer(container)
        let consoleView = NSTextView(frame: .zero, textContainer: container)
        consoleView.isEditable = false
        consoleView.isSelectable = true
        consoleView.textContainerInset = NSSize(width: 4, height: 4)
        consoleView.autoresizingMask = [.width]
        consoleView.isVerticallyResizable = true
        consoleView.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        consoleView.setAccessibilityLabel("Messages")
        consoleScroll.documentView = consoleView
        consoleScroll.hasVerticalScroller = true
        consoleScroll.borderType = .noBorder

        split.isVertical = false
        split.dividerStyle = .thin
        split.delegate = self
        split.addArrangedSubview(pane("Program", editor))
        split.addArrangedSubview(pane("Variables", variables.scrollView))
        split.addArrangedSubview(pane("Messages", consoleScroll, clear: true))
        split.setHoldingPriority(.init(250), forSubviewAt: 0)
        split.setHoldingPriority(.init(249), forSubviewAt: 1)
        split.setHoldingPriority(.init(260), forSubviewAt: 2)
        window.contentView = split
        window.initialFirstResponder = editor.textView

        let toolbar = NSToolbar(identifier: "SOMMainToolbar")
        toolbar.delegate = self
        toolbar.displayMode = .iconOnly
        toolbar.allowsUserCustomization = true
        toolbar.autosavesConfiguration = true
        window.toolbar = toolbar
        window.toolbarStyle = .unified

        updateGlobals()
        setInitialSplit()
        DispatchQueue.main.async { [weak self] in self?.setInitialSplit() }
        let nc = NotificationCenter.default
        nc.addObserver(self, selector: #selector(variablesChanged(_:)), name: .variablesDidChange, object: doc)
        nc.addObserver(self, selector: #selector(globalsChanged(_:)), name: .globalsDidChange, object: doc)
        nc.addObserver(self, selector: #selector(messagesChanged(_:)), name: .messagesDidChange, object: doc)
        self.consoleTextView = consoleView

    }

    private var consoleTextView: NSTextView?
    private var didSetInitialSplit = false

    override func showWindow(_ sender: Any?) {
        super.showWindow(sender)
        setInitialSplit()
    }

    /// program 45%, variables 37%, messages the rest
    func setInitialSplit() {
        guard !didSetInitialSplit, split.subviews.count == 3, let window else { return }
        if split.bounds.height < 300 { split.frame = window.contentLayoutRect }
        split.layoutSubtreeIfNeeded()
        let h = split.bounds.height
        guard h > 300 else { return }
        didSetInitialSplit = true
        split.setPosition((h * 0.45).rounded(), ofDividerAt: 0)
        split.setPosition((h * 0.82).rounded(), ofDividerAt: 1)
    }

    /// A pane with a small title bar.
    private func pane(_ title: String, _ content: NSView, clear: Bool = false) -> NSView {
        let v = NSView()
        let header = NSTextField(labelWithString: title.uppercased())
        header.font = .systemFont(ofSize: 10, weight: .semibold)
        header.textColor = .secondaryLabelColor
        header.translatesAutoresizingMaskIntoConstraints = false
        let bar = NSVisualEffectView()
        bar.material = .headerView
        bar.translatesAutoresizingMaskIntoConstraints = false
        bar.addSubview(header)
        content.translatesAutoresizingMaskIntoConstraints = false
        v.addSubview(bar)
        v.addSubview(content)
        var constraints = [
            bar.topAnchor.constraint(equalTo: v.topAnchor),
            bar.leadingAnchor.constraint(equalTo: v.leadingAnchor),
            bar.trailingAnchor.constraint(equalTo: v.trailingAnchor),
            bar.heightAnchor.constraint(equalToConstant: 20),
            header.leadingAnchor.constraint(equalTo: bar.leadingAnchor, constant: 8),
            header.centerYAnchor.constraint(equalTo: bar.centerYAnchor),
            content.topAnchor.constraint(equalTo: bar.bottomAnchor),
            content.leadingAnchor.constraint(equalTo: v.leadingAnchor),
            content.trailingAnchor.constraint(equalTo: v.trailingAnchor),
            content.bottomAnchor.constraint(equalTo: v.bottomAnchor),
        ]
        if clear {
            let b = NSButton(title: "Clear", target: self, action: #selector(clearMessages(_:)))
            b.bezelStyle = .inline
            b.controlSize = .mini
            b.font = .systemFont(ofSize: 10)
            b.translatesAutoresizingMaskIntoConstraints = false
            bar.addSubview(b)
            constraints += [b.trailingAnchor.constraint(equalTo: bar.trailingAnchor, constant: -8),
                            b.centerYAnchor.constraint(equalTo: bar.centerYAnchor)]
        }
        NSLayoutConstraint.activate(constraints)
        return v
    }

    func splitView(_ splitView: NSSplitView, constrainMinCoordinate proposedMinimumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        proposedMinimumPosition + 80
    }

    func splitView(_ splitView: NSSplitView, constrainMaxCoordinate proposedMaximumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        proposedMaximumPosition - 60
    }

    // MARK: titles

    override func windowTitle(forDocumentDisplayName displayName: String) -> String { displayName }

    private func updateGlobals() {
        guard let doc = somDocument else { return }
        let sr = doc.engine.sampleRate
        let srText = sr == sr.rounded() ? String(format: "%.0f Hz", sr) : String(format: "%.2f Hz", sr)
        globalsButton.title = "\(srText)  ·  \(String(format: "%.3f", doc.engine.duration)) s"
        globalsButton.toolTip = "Sample rate and sample time.  Click to set the globals."
    }

    // MARK: selection

    var selectedVariableNames: [String] { variables?.selectedNames ?? [] }

    func selectVariables(_ names: [String]) { variables?.select(names) }

    // MARK: notifications

    @objc private func variablesChanged(_ n: Notification) {
        variables.reload(names: n.userInfo?["names"] as? Set<String>)
        updateGlobals()
    }

    @objc private func globalsChanged(_ n: Notification) { updateGlobals() }

    @objc private func messagesChanged(_ n: Notification) {
        guard let tv = consoleTextView else { return }
        tv.scrollToEndOfDocument(nil)
    }

    @objc private func clearMessages(_ sender: Any?) { somDocument?.clearMessages() }

    @objc func showGlobals(_ sender: Any?) { somDocument?.setGlobals(sender) }

    @objc func focusProgram(_ sender: Any?) { window?.makeFirstResponder(editor.textView) }

    @objc func focusVariables(_ sender: Any?) { window?.makeFirstResponder(variables.tableView) }

    // MARK: calculation progress

    /// The original showed "Blocks Left to Calculate".  Calculations are
    /// fast now, so the sheet only appears when one takes a while.
    func calculationStarted() {
        progressTimer?.invalidate()
        progressTimer = Timer.scheduledTimer(withTimeInterval: 0.4, repeats: false) { [weak self] _ in
            self?.showProgressSheet()
        }
    }

    private func showProgressSheet() {
        guard let window, progressSheet == nil, somDocument?.isCalculating == true else { return }
        let panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 360, height: 110), styleMask: [.titled], backing: .buffered, defer: false)
        let label = NSTextField(labelWithString: "Blocks Left to Calculate :")
        let bar = NSProgressIndicator()
        bar.isIndeterminate = false
        bar.minValue = 0
        bar.maxValue = 1
        let hint = NSTextField(labelWithString: "⌘. to stop.")
        hint.textColor = .secondaryLabelColor
        hint.font = .systemFont(ofSize: 11)
        let stop = NSButton(title: "Stop", target: self, action: #selector(stopCalculation(_:)))
        stop.keyEquivalent = "\u{1b}"
        let stack = NSStackView(views: [label, bar, NSStackView(views: [hint, NSView(), stop])])
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.edgeInsets = NSEdgeInsets(top: 16, left: 20, bottom: 16, right: 20)
        stack.spacing = 10
        bar.widthAnchor.constraint(equalTo: stack.widthAnchor, constant: -40).isActive = true
        (stack.arrangedSubviews[2] as! NSStackView).widthAnchor.constraint(equalTo: bar.widthAnchor).isActive = true
        panel.contentView = stack
        progressSheet = panel
        progressBar = bar
        progressLabel = label
        window.beginSheet(panel)
    }

    @objc private func stopCalculation(_ sender: Any?) { somDocument?.stopCalculating() }

    func calculationProgress(done: Int, total: Int) {
        progressBar?.doubleValue = total > 0 ? Double(done) / Double(total) : 0
        progressLabel?.stringValue = "Blocks Left to Calculate :  \(max(0, total - done))"
    }

    func calculationFinished() {
        progressTimer?.invalidate()
        progressTimer = nil
        if let sheet = progressSheet {
            window?.endSheet(sheet)
            progressSheet = nil
        }
    }

    // MARK: window

    func windowDidResize(_ notification: Notification) { setInitialSplit() }

    func windowWillClose(_ notification: Notification) {
        somDocument?.stopCalculating()
        if AudioPlayer.shared.owner === somDocument { AudioPlayer.shared.stop() }
    }

    func windowWillReturnUndoManager(_ window: NSWindow) -> UndoManager? { somDocument?.undoManager }

    // MARK: toolbar

    private enum Item {
        static let compile = NSToolbarItem.Identifier("compile")
        static let calculate = NSToolbarItem.Identifier("calculate")
        static let calculateAndPlay = NSToolbarItem.Identifier("calculateAndPlay")
        static let play = NSToolbarItem.Identifier("play")
        static let stop = NSToolbarItem.Identifier("stop")
        static let globals = NSToolbarItem.Identifier("globals")
        static let randomize = NSToolbarItem.Identifier("randomize")
        static let reference = NSToolbarItem.Identifier("reference")
    }

    func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        [Item.compile, Item.calculateAndPlay, Item.play, Item.stop, .flexibleSpace, Item.globals, .flexibleSpace, Item.randomize, Item.reference]
    }

    func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        [Item.compile, Item.calculate, Item.calculateAndPlay, Item.play, Item.stop, Item.globals, Item.randomize, Item.reference,
         .flexibleSpace, .space]
    }

    func toolbar(_ toolbar: NSToolbar, itemForItemIdentifier id: NSToolbarItem.Identifier, willBeInsertedIntoToolbar flag: Bool) -> NSToolbarItem? {
        func button(_ label: String, _ symbol: String, _ action: Selector, _ tip: String) -> NSToolbarItem {
            let item = NSToolbarItem(itemIdentifier: id)
            item.label = label
            item.paletteLabel = label
            item.toolTip = tip
            item.image = NSImage(systemSymbolName: symbol, accessibilityDescription: label)
            item.action = action
            item.isBordered = true
            return item
        }
        switch id {
        case Item.compile: return button("Compile", "hammer", #selector(SOMDocument.compile(_:)), "Compile the program (⌘K)")
        case Item.calculate: return button("Calculate", "function", #selector(SOMDocument.calculate(_:)), "Calculate (⌘/)")
        case Item.calculateAndPlay: return button("Calculate & Play", "play.circle", #selector(SOMDocument.calculateAndPlay(_:)), "Calculate and play the output (⌘↩)")
        case Item.play: return button("Play", "speaker.wave.2", #selector(SOMDocument.play(_:)), "Play the selected sample (space)")
        case Item.stop: return button("Stop", "stop.fill", #selector(SOMDocument.stop(_:)), "Stop calculating or playing (⌘.)")
        case Item.randomize: return button("Randomize", "dice", #selector(SOMDocument.randomize(_:)), "Randomize the random set (⇧⌘R)")
        case Item.reference: return button("Reference", "book", #selector(AppDelegate.showQuickReference(_:)), "Quick Reference")
        case Item.globals:
            let item = NSToolbarItem(itemIdentifier: id)
            item.label = "Globals"
            item.paletteLabel = "Globals"
            globalsButton.bezelStyle = .texturedRounded
            globalsButton.target = self
            globalsButton.action = #selector(showGlobals(_:))
            globalsButton.font = .monospacedDigitSystemFont(ofSize: 12, weight: .regular)
            item.view = globalsButton
            updateGlobals()
            return item
        default: return nil
        }
    }
}
