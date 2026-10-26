#!/usr/bin/env swift
// ReCarbon. GPL-3.0-or-later (see COPYING).
//
// make_icon.swift [output dir]: ReCarbon's app icon, scripts/macos/AppIcon.iconset: every size
// macOS asks for (icon_<n>x<n>[@2x].png, 16 to 512 at @1x and @2x), each drawn at its own pixel
// size rather than scaled down from one large picture, so every size is sharp. make_app.py makes
// the app's AppIcon.icns, and with it the dmg's volume icon, from the set (iconutil). Drawn from
// the project's own mark, website/public/favicon.svg (never the game's or SpeedBreaker's art), in
// the website's colours (website/src/styles/tokens.css), on Apple's macOS icon grid: in 1024
// units, an 824 square (corner radius 185) centred in a transparent margin, with a soft drop
// shadow. The square: asphalt (#0A0A0C) to carbon (#141417), a racing-red (#E10600) glow around
// the mark, and a chrome edge (#FFFFFF / #C2C6CD / #9AA0AA); the mark centred, 88% of the square's
// width. The 1024-pixel size (512 points at 2x, the largest Finder shows) has a faint carbon-fibre
// twill, a pattern that repeats on whole pixels; smaller sizes leave it out, where it would only
// cost file size. At 64 pixels and below the glows are lighter and the mark a little larger, so
// the R still reads at 16 and 32. Each size is drawn at 16 bits a channel and rounded to 8 without
// dithering: smooth gradients, small files (the whole set about 0.7 MB).
// Needs only macOS (AppKit draws the SVG, macOS 14+).
// From the repo root: swift scripts/macos/make_icon.swift
import AppKit

let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
let outputDir = CommandLine.arguments.count > 1 ? URL(fileURLWithPath: CommandLine.arguments[1])
                                                 : root.appendingPathComponent("scripts/macos/AppIcon.iconset")
let svg = root.appendingPathComponent("website/public/favicon.svg").path
guard let mark = NSImage(contentsOfFile: svg), mark.size.width > 0 else {
    FileHandle.standardError.write("make_icon.swift: can't read \(svg)\n".data(using: .utf8)!)
    exit(1)
}

func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> NSColor {
    NSColor(srgbRed: CGFloat(hex >> 16 & 0xFF) / 255, green: CGFloat(hex >> 8 & 0xFF) / 255,
            blue: CGFloat(hex & 0xFF) / 255, alpha: alpha)
}

// One icon, `points` wide at `scale` (1 or 2: its dpi, 72 or 144), drawn in 1024 units scaled to
// its pixels.
func icon(points: Int, scale density: Int) -> Data {
    let pixels = points * density
    // Drawn at 16 bits a channel, then rounded to 8 (toPNG): the gradients come out smooth and
    // undithered, which keeps the PNGs small.
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels, bitsPerSample: 16,
                               samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                               bytesPerRow: 0, bitsPerPixel: 0)!.retagging(with: .sRGB)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSGraphicsContext.current!.imageInterpolation = .high
    let cg = NSGraphicsContext.current!.cgContext
    let scale = CGFloat(pixels) / 1024
    let small = pixels <= 64
    cg.scaleBy(x: scale, y: scale)

    let size: CGFloat = 1024, body: CGFloat = 824, radius: CGFloat = 185
    let square = NSRect(x: (size - body) / 2, y: (size - body) / 2, width: body, height: body)
    let plate = NSBezierPath(roundedRect: square, xRadius: radius, yRadius: radius)
    // Shadows are in device pixels: scale them with the icon.
    NSGraphicsContext.saveGraphicsState()
    let shadow = NSShadow()
    shadow.shadowColor = NSColor(white: 0, alpha: 0.35)
    shadow.shadowOffset = NSSize(width: 0, height: -10 * scale)
    shadow.shadowBlurRadius = 24 * scale
    shadow.set()
    color(0x0A0A0C).setFill()
    plate.fill()
    NSGraphicsContext.restoreGraphicsState()

    NSGraphicsContext.saveGraphicsState()
    plate.addClip()
    NSGradient(starting: color(0x0A0A0C), ending: color(0x141417))!.draw(in: plate, angle: 90)
    if pixels >= 1024 {
        // The twill: 45-degree bands, light one way and dark the other (tokens.css's --rn-weave),
        // 24 pixels apart horizontally, a whole number, so the pattern repeats exactly.
        let period = 24 / scale
        for (slope, shade) in [(CGFloat(1), color(0xFFFFFF, 0.04)), (CGFloat(-1), color(0x000000, 0.30))] {
            cg.setFillColor(shade.cgColor)
            var x = -size
            while x < 2 * size {
                let band = CGMutablePath()
                band.move(to: CGPoint(x: x, y: 0))
                band.addLine(to: CGPoint(x: x + period / 2, y: 0))
                band.addLine(to: CGPoint(x: x + period / 2 + slope * size, y: size))
                band.addLine(to: CGPoint(x: x + slope * size, y: size))
                band.closeSubpath()
                cg.addPath(band)
                cg.fillPath()
                x += period
            }
        }
    }
    // A red glow behind the mark, and a faint lift towards the top.
    let centre = NSPoint(x: size / 2, y: size / 2 - 20)
    NSGradient(colors: [color(0xE10600, small ? 0.30 : 0.45), color(0xE10600, small ? 0.06 : 0.12), color(0xE10600, 0)],
               atLocations: [0, 0.55, 1], colorSpace: .sRGB)!
        .draw(fromCenter: centre, radius: 0, toCenter: centre, radius: body * 0.52, options: [])
    NSGradient(starting: color(0xFFFFFF, 0.08), ending: color(0xFFFFFF, 0))!.draw(in: plate, angle: -90)
    NSGraphicsContext.restoreGraphicsState()

    // The chrome edge just inside the square (at least a pixel wide): bright at the top and
    // bottom, darker at the middle.
    let edgeWidth = max(14, 1.2 / scale)
    let edge = NSBezierPath(roundedRect: square.insetBy(dx: edgeWidth / 2, dy: edgeWidth / 2),
                            xRadius: radius - edgeWidth / 2, yRadius: radius - edgeWidth / 2)
    cg.saveGState()
    cg.addPath(edge.cgPath.copy(strokingWithWidth: edgeWidth, lineCap: .butt, lineJoin: .round, miterLimit: 10))
    cg.clip()
    NSGradient(colors: [color(0xFFFFFF), color(0xC2C6CD), color(0x9AA0AA), color(0xC2C6CD), color(0xFFFFFF)],
               atLocations: [0, 0.3, 0.5, 0.7, 1], colorSpace: .sRGB)!.draw(in: square, angle: -90)
    cg.restoreGState()

    // The mark lit red from behind: a red glow around its plate.
    NSGraphicsContext.saveGraphicsState()
    let glow = NSShadow()
    glow.shadowColor = color(0xE10600, small ? 0.6 : 0.85)
    glow.shadowOffset = .zero
    glow.shadowBlurRadius = 48 * scale
    glow.set()
    let width = body * (small ? 0.94 : 0.88), height = width * mark.size.height / mark.size.width
    mark.draw(in: NSRect(x: (size - width) / 2, y: (size - height) / 2, width: width, height: height))
    NSGraphicsContext.restoreGraphicsState()
    NSGraphicsContext.restoreGraphicsState()
    return toPNG(rep, points: CGFloat(points))
}

// 16-bit RGBA to an 8-bit PNG, each channel rounded (no dithering), at `points` (its dpi).
func toPNG(_ wide: NSBitmapImageRep, points: CGFloat) -> Data {
    let w = wide.pixelsWide, h = wide.pixelsHigh
    let narrow = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: w, pixelsHigh: h, bitsPerSample: 8,
                                  samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                                  bytesPerRow: w * 4, bitsPerPixel: 32)!.retagging(with: .sRGB)!
    let src = wide.bitmapData!.withMemoryRebound(to: UInt16.self, capacity: wide.bytesPerRow / 2 * h) { $0 }
    let dst = narrow.bitmapData!
    let srcRow = wide.bytesPerRow / 2
    for y in 0..<h {
        for x in 0..<w * 4 {
            dst[y * w * 4 + x] = UInt8((UInt32(src[y * srcRow + x]) * 255 + 32767) / 65535)
        }
    }
    narrow.size = NSSize(width: points, height: points)
    return narrow.representation(using: .png, properties: [:])!
}

try FileManager.default.createDirectory(at: outputDir, withIntermediateDirectories: true)
for points in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let name = "icon_\(points)x\(points)" + (scale == 2 ? "@2x" : "") + ".png"
        let data = icon(points: points, scale: scale)
        try data.write(to: outputDir.appendingPathComponent(name))
        print("\(name): \(points * scale) px, \(data.count / 1024) KB")
    }
}
