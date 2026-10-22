// swift-tools-version:5.9
//
// Synth-O-Matic for modern macOS.
//
//   swift build                      builds the engine, the app executable and somrender
//   swift test                       runs the engine tests
//   scripts/build_app.sh             assembles Synth-O-Matic.app
//
import PackageDescription

let package = Package(
    name: "SynthOMatic",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "SynthOMatic", targets: ["SynthOMatic"]),
        .executable(name: "somrender", targets: ["somrender"]),
        .library(name: "SOMEngine", targets: ["SOMEngine"]),
    ],
    targets: [
        .target(
            name: "SOMEngine",
            path: "Sources/SOMEngine",
            cSettings: [
                .headerSearchPath("."),
                // the ported 1990 code is kept close to the original
                .unsafeFlags([
                    "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                    "-Wno-parentheses", "-Wno-dangling-else",
                    "-Wno-unused-function", "-Wno-sometimes-uninitialized",
                    "-Wno-conditional-uninitialized", "-Wno-uninitialized",
                    "-Wno-shorten-64-to-32", "-Wno-unused-label",
                ]),
            ],
            linkerSettings: [.linkedFramework("CoreFoundation")]
        ),
        .executableTarget(
            name: "SynthOMatic",
            dependencies: ["SOMEngine"],
            path: "Sources/SynthOMatic",
            linkerSettings: [
                .linkedFramework("AppKit"),
                .linkedFramework("AVFoundation"),
                .linkedFramework("UniformTypeIdentifiers"),
            ]
        ),
        .executableTarget(
            name: "somrender",
            dependencies: ["SOMEngine"],
            path: "Sources/somrender"
        ),
        .testTarget(
            name: "SOMEngineTests",
            dependencies: ["SOMEngine"],
            path: "Tests/SOMEngineTests",
            resources: [.copy("Patches")]
        ),
    ]
)
