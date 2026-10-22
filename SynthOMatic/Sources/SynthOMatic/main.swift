import AppKit

// The test modes neither restore nor leave behind window state.
if CommandLine.arguments.contains("--selftest") || CommandLine.arguments.contains("--snapshot") {
    UserDefaults.standard.register(defaults: ["ApplePersistenceIgnoreState": true])
}

// The first document controller created becomes the shared one.
_ = SOMDocumentController()

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
