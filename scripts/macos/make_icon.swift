#!/usr/bin/env swift
// ReCarbon. GPL-3.0-or-later (see COPYING).
//
// make_icon.swift [output.png]: ReCarbon's app icon, scripts/macos/AppIcon.png
// (make_app.py makes the app's AppIcon.icns and the dmg's volume icon from it),
// from the project's own mark, website/public/favicon.svg (never the game's or
// SpeedBreaker's art). 1024 x 1024 on Apple's macOS icon grid: an 824 px
// rounded square (corner radius 185) centred in a transparent margin, with a
// soft drop shadow, filled with the mark's dark (#15181D) and edged with its
// cyan (#19E3FF); the mark drawn as vector, centred, 88% of the square's width.
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
color(0x15181D).setFill()
plate.fill()
NSGraphicsContext.restoreGraphicsState()
// A faint lift towards the top, and the cyan edge just inside the square.
NSGradient(starting: color(0xFFFFFF, 0.07), ending: color(0xFFFFFF, 0))!.draw(in: plate, angle: -90)
let edge = NSBezierPath(roundedRect: square.insetBy(dx: 6, dy: 6), xRadius: radius - 6, yRadius: radius - 6)
edge.lineWidth = 12
color(0x19E3FF, 0.55).setStroke()
edge.stroke()

let width = body * 0.88, height = width * mark.size.height / mark.size.width
mark.draw(in: NSRect(x: (CGFloat(size) - width) / 2, y: (CGFloat(size) - height) / 2, width: width, height: height))
NSGraphicsContext.restoreGraphicsState()

guard let png = rep.representation(using: .png, properties: [:]) else { exit(1) }
try png.write(to: URL(fileURLWithPath: output))
print("\(output): \(size) x \(size)")
