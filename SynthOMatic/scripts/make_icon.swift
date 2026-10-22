// Renders the Synth-O-Matic app icon: a figure from the original About box
// spirograph (x = sin(w)cos(t), y = sin(w)sin(t) over 331 steps) on a dark tile.
//
//   swift scripts/make_icon.swift <output.iconset> [dtheta domega]
//   iconutil -c icns <output.iconset>
import AppKit

let args = CommandLine.arguments
let out = URL(fileURLWithPath: args.count > 1 ? args[1] : "AppIcon.iconset")
let dtheta = args.count > 3 ? Int(args[2])! : 11
let domega = args.count > 3 ? Int(args[3])! : 4
let n = 331

func render(size: Int) -> NSBitmapImageRep {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size, bitsPerSample: 8,
                               samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                               bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    let s = CGFloat(size) / 1024
    let ctx = NSGraphicsContext.current!.cgContext
    ctx.scaleBy(x: s, y: s)

    // the tile, on the macOS icon grid
    let tile = NSRect(x: 100, y: 100, width: 824, height: 824)
    let shape = NSBezierPath(roundedRect: tile, xRadius: 185, yRadius: 185)
    NSGraphicsContext.saveGraphicsState()
    let shadow = NSShadow()
    shadow.shadowColor = NSColor.black.withAlphaComponent(0.45)
    shadow.shadowOffset = NSSize(width: 0, height: -12)
    shadow.shadowBlurRadius = 24
    shadow.set()
    NSColor.black.setFill()
    shape.fill()
    NSGraphicsContext.restoreGraphicsState()
    NSGradient(colors: [NSColor(calibratedRed: 0.13, green: 0.11, blue: 0.24, alpha: 1),
                        NSColor(calibratedRed: 0.03, green: 0.03, blue: 0.07, alpha: 1)])!
        .draw(in: shape, angle: -90)

    // the figure
    let center = NSPoint(x: 512, y: 512)
    let radius: CGFloat = 330
    var theta = 0, omega = 0
    var points = [center]
    for _ in 0..<n {
        theta += dtheta; if theta >= n { theta -= n }
        omega += domega; if omega >= n { omega -= n }
        let w = 2 * Double.pi * Double(omega) / Double(n), t = 2 * Double.pi * Double(theta) / Double(n)
        points.append(NSPoint(x: center.x + radius * CGFloat(sin(w) * cos(t)), y: center.y + radius * CGFloat(sin(w) * sin(t))))
    }
    let path = NSBezierPath()
    path.move(to: points[0])
    for p in points.dropFirst() { path.line(to: p) }
    path.lineWidth = 3.2
    path.lineJoinStyle = .round

    NSGraphicsContext.saveGraphicsState()
    shape.addClip()
    // a soft glow, then the line in a warm gradient
    let glow = NSShadow()
    glow.shadowColor = NSColor(calibratedRed: 1.0, green: 0.55, blue: 0.25, alpha: 0.9)
    glow.shadowBlurRadius = 18
    glow.set()
    NSColor(calibratedRed: 1.0, green: 0.78, blue: 0.45, alpha: 0.9).setStroke()
    path.stroke()
    NSGraphicsContext.restoreGraphicsState()

    NSGraphicsContext.saveGraphicsState()
    ctx.addPath(path.cgPath)
    ctx.setLineWidth(2.2)
    ctx.replacePathWithStrokedPath()
    ctx.clip()
    NSGradient(colors: [NSColor(calibratedRed: 1.0, green: 0.93, blue: 0.70, alpha: 1),
                        NSColor(calibratedRed: 1.0, green: 0.55, blue: 0.30, alpha: 1),
                        NSColor(calibratedRed: 0.95, green: 0.30, blue: 0.55, alpha: 1)])!
        .draw(in: tile, angle: -60)
    NSGraphicsContext.restoreGraphicsState()

    // a fine inner edge
    NSColor.white.withAlphaComponent(0.08).setStroke()
    let edge = NSBezierPath(roundedRect: tile.insetBy(dx: 2, dy: 2), xRadius: 183, yRadius: 183)
    edge.lineWidth = 3
    edge.stroke()
    NSGraphicsContext.restoreGraphicsState()
    return rep
}

try? FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
for (name, size) in [("icon_16x16", 16), ("icon_16x16@2x", 32), ("icon_32x32", 32), ("icon_32x32@2x", 64),
                     ("icon_128x128", 128), ("icon_128x128@2x", 256), ("icon_256x256", 256), ("icon_256x256@2x", 512),
                     ("icon_512x512", 512), ("icon_512x512@2x", 1024)] {
    let data = render(size: size).representation(using: .png, properties: [:])!
    try! data.write(to: out.appendingPathComponent(name + ".png"))
}
print("wrote \(out.path)")
