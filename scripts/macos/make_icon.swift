#!/usr/bin/env swift
// ReCarbon. GPL-3.0-or-later (see COPYING).
//
// make_icon.swift [output.png]: ReCarbon's app icon, scripts/macos/AppIcon.png
// (make_app.py makes the app's AppIcon.icns and the dmg's volume icon from it),
// from the project's own mark, website/public/favicon.svg (never the game's or
// SpeedBreaker's art), in the website's colours (website/src/styles/tokens.css).
// 1024 x 1024 on Apple's macOS icon grid: an 824 px rounded square (corner
// radius 185) centred in a transparent margin, with a soft drop shadow. The
// square: asphalt (#0A0A0C) to carbon (#141417) with a faint carbon-fibre twill,
// a racing-red (#E10600) glow around the mark, and a chrome edge (silver
// #FFFFFF / #9AA0AA / #C2C6CD); the mark drawn as vector, centred, 88% of the
// square's width.
// Needs only macOS (AppKit draws the SVG, macOS 14+).
// From the repo root: swift scripts/macos/make_icon.swift
import AppKit

let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
let output = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : root.appendingPathComponent("scripts/macos/AppIcon.png").path
let svg = root.appendingPathComponent("website/public/favicon.svg").path
guard let mark = NSImage(contentsOfFile: svg), mark.size.width > 0 else {
    FileHandle.standardError.write("make_icon.swift: can't read \(svg)\n".data(using: .utf8)!)
    exit(1)
}

func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> NSColor {
    NSColor(srgbRed: CGFloat(hex >> 16 & 0xFF) / 255, green: CGFloat(hex >> 8 & 0xFF) / 255,
            blue: CGFloat(hex & 0xFF) / 255, alpha: alpha)
}

let size = 1024, body: CGFloat = 824, radius: CGFloat = 185
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size, bitsPerSample: 8,
                           samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                           bytesPerRow: 0, bitsPerPixel: 0)!.retagging(with: .sRGB)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
NSGraphicsContext.current!.imageInterpolation = .high

let square = NSRect(x: (CGFloat(size) - body) / 2, y: (CGFloat(size) - body) / 2, width: body, height: body)
let plate = NSBezierPath(roundedRect: square, xRadius: radius, yRadius: radius)
NSGraphicsContext.saveGraphicsState()
let shadow = NSShadow()
shadow.shadowColor = NSColor(white: 0, alpha: 0.35)
shadow.shadowOffset = NSSize(width: 0, height: -10)
shadow.shadowBlurRadius = 24
shadow.set()
color(0x0A0A0C).setFill()
plate.fill()
NSGraphicsContext.restoreGraphicsState()

NSGraphicsContext.saveGraphicsState()
plate.addClip()
NSGradient(starting: color(0x0A0A0C), ending: color(0x141417))!.draw(in: plate, angle: 90)
// The twill: 45-degree bands, light one way and dark the other, as tokens.css's --rn-weave
// (scaled up so it shows at 512 and 1024 and fades to an even carbon below).
let cg = NSGraphicsContext.current!.cgContext, period: CGFloat = 16
for (angle, shade) in [(CGFloat.pi / 4, color(0xFFFFFF, 0.035)), (-CGFloat.pi / 4, color(0x000000, 0.35))] {
    cg.saveGState()
    cg.translateBy(x: CGFloat(size) / 2, y: CGFloat(size) / 2)
    cg.rotate(by: angle)
    cg.setFillColor(shade.cgColor)
    var x = -CGFloat(size)
    while x < CGFloat(size) {
        cg.fill(CGRect(x: x, y: -CGFloat(size), width: period / 2, height: 2 * CGFloat(size)))
        x += period
    }
    cg.restoreGState()
}
// A red glow behind the mark, and a faint lift towards the top.
let centre = NSPoint(x: CGFloat(size) / 2, y: CGFloat(size) / 2 - 20)
NSGradient(colors: [color(0xE10600, 0.45), color(0xE10600, 0.12), color(0xE10600, 0)],
           atLocations: [0, 0.55, 1], colorSpace: .sRGB)!
    .draw(fromCenter: centre, radius: 0, toCenter: centre, radius: body * 0.52, options: [])
NSGradient(starting: color(0xFFFFFF, 0.08), ending: color(0xFFFFFF, 0))!.draw(in: plate, angle: -90)
NSGraphicsContext.restoreGraphicsState()

// The chrome edge just inside the square: bright at the top and bottom, darker at the middle.
let edge = NSBezierPath(roundedRect: square.insetBy(dx: 7, dy: 7), xRadius: radius - 7, yRadius: radius - 7)
cg.saveGState()
cg.addPath(edge.cgPath.copy(strokingWithWidth: 14, lineCap: .butt, lineJoin: .round, miterLimit: 10))
cg.clip()
NSGradient(colors: [color(0xFFFFFF), color(0xC2C6CD), color(0x9AA0AA), color(0xC2C6CD), color(0xFFFFFF)],
           atLocations: [0, 0.3, 0.5, 0.7, 1], colorSpace: .sRGB)!.draw(in: square, angle: -90)
cg.restoreGState()

// The mark lit red from behind: a red glow around its plate.
NSGraphicsContext.saveGraphicsState()
let glow = NSShadow()
glow.shadowColor = color(0xE10600, 0.85)
glow.shadowOffset = .zero
glow.shadowBlurRadius = 48
glow.set()
let width = body * 0.88, height = width * mark.size.height / mark.size.width
mark.draw(in: NSRect(x: (CGFloat(size) - width) / 2, y: (CGFloat(size) - height) / 2, width: width, height: height))
NSGraphicsContext.restoreGraphicsState()
NSGraphicsContext.restoreGraphicsState()

guard let png = rep.representation(using: .png, properties: [:]) else { exit(1) }
try png.write(to: URL(fileURLWithPath: output))
print("\(output): \(size) x \(size)")
