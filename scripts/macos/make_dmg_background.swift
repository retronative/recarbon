#!/usr/bin/env swift
// ReCarbon. GPL-3.0-or-later (see COPYING).
//
// make_dmg_background.swift [output dir]: the background of the dmg's Finder window,
// scripts/macos/DmgBackground.png (640 x 400) and DmgBackground@2x.png (1280 x 800, for Retina
// screens), which make_app.py joins into one TIFF. Drawn from the project's own mark
// (website/public/favicon.svg) and the website's colours (website/src/styles/tokens.css: asphalt
// #0A0A0C, graphite #1D1E22, racing red #E10600 and #FF4336, silver #C2C6CD, text #E6E8EC), never
// the game's or SpeedBreaker's art: a dark gradient with a faint carbon-fibre twill, a red glow
// and a few slanted red and silver speed streaks, the mark and "ReCarbon" ("Re" silver, "Carbon"
// red) at the top, a red arrow with a silver highlight from the app's icon to the Applications
// shortcut, a light plate under each icon's name, and "Drag ReCarbon onto Applications" below them.
// The window's size and the two icons' centres are make_app.py's DMG_WINDOW and DMG_ICONS
// (tests/dmg_layout_test.py checks the images' sizes against them).
// Needs only macOS (AppKit draws the SVG, macOS 14+).
// From the repo root: swift scripts/macos/make_dmg_background.swift
import AppKit

let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
let outputDir = CommandLine.arguments.count > 1 ? URL(fileURLWithPath: CommandLine.arguments[1])
                                                 : root.appendingPathComponent("scripts/macos")
let svg = root.appendingPathComponent("website/public/favicon.svg").path
guard let mark = NSImage(contentsOfFile: svg), mark.size.width > 0 else {
    FileHandle.standardError.write("make_dmg_background.swift: can't read \(svg)\n".data(using: .utf8)!)
    exit(1)
}

func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> NSColor {
    NSColor(srgbRed: CGFloat(hex >> 16 & 0xFF) / 255, green: CGFloat(hex >> 8 & 0xFF) / 255,
            blue: CGFloat(hex & 0xFF) / 255, alpha: alpha)
}

// In points, from the window's top left, as Finder places icons: make_app.py's DMG_WINDOW and DMG_ICONS.
let width: CGFloat = 640, height: CGFloat = 400
let app = NSPoint(x: 170, y: 190), applications = NSPoint(x: 470, y: 190)
let iconSize: CGFloat = 128
// The centre of the names Finder writes under the icons (13 pt, below the 128 pt icons).
let labelCentre: CGFloat = 273

// AppKit draws from the bottom left.
func flip(_ p: NSPoint) -> NSPoint { NSPoint(x: p.x, y: height - p.y) }

// A parallelogram slanted like the mark (its sides lean right by `lean` over its height).
func slanted(_ rect: NSRect, lean: CGFloat) -> NSBezierPath {
    let path = NSBezierPath()
    path.move(to: NSPoint(x: rect.minX, y: rect.minY))
    path.line(to: NSPoint(x: rect.maxX, y: rect.minY))
    path.line(to: NSPoint(x: rect.maxX + lean, y: rect.maxY))
    path.line(to: NSPoint(x: rect.minX + lean, y: rect.maxY))
    path.close()
    return path
}

func text(_ string: String, size: CGFloat, weight: NSFont.Weight, colour: NSColor, centreX: CGFloat, top: CGFloat,
          kern: CGFloat = 0) {
    let attributes: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: size, weight: weight),
                                                     .foregroundColor: colour, .kern: kern]
    let s = NSAttributedString(string: string, attributes: attributes)
    let bounds = s.size()
    s.draw(at: NSPoint(x: centreX - bounds.width / 2, y: height - top - bounds.height))
}

func draw() {
    // The ground: asphalt, lighter towards the top, a faint twill (tokens.css's --rn-weave), and a
    // red glow behind the icons' row.
    let full = NSRect(x: 0, y: 0, width: width, height: height)
    NSGradient(starting: color(0x0A0A0C), ending: color(0x1D1E22))!.draw(in: full, angle: 90)
    // 45-degree bands 6 points apart horizontally (whole pixels at 1x and 2x), so the pattern
    // repeats exactly and the PNGs stay small.
    let cg = NSGraphicsContext.current!.cgContext, period: CGFloat = 6
    for (slope, shade) in [(CGFloat(1), color(0xFFFFFF, 0.02)), (CGFloat(-1), color(0x000000, 0.22))] {
        cg.setFillColor(shade.cgColor)
        var x = -height
        while x < width + height {
            let band = CGMutablePath()
            band.move(to: CGPoint(x: x, y: 0))
            band.addLine(to: CGPoint(x: x + period / 2, y: 0))
            band.addLine(to: CGPoint(x: x + period / 2 + slope * height, y: height))
            band.addLine(to: CGPoint(x: x + slope * height, y: height))
            band.closeSubpath()
            cg.addPath(band)
            cg.fillPath()
            x += period
        }
    }
    let row = flip(NSPoint(x: width / 2, y: app.y))
    NSGradient(starting: color(0xE10600, 0.14), ending: color(0xE10600, 0))!
        .draw(fromCenter: row, radius: 0, toCenter: row, radius: 300, options: [])

    // Speed streaks, slanted like the mark: top right and bottom left, faint.
    let streaks: [(NSRect, UInt32, CGFloat)] = [
        (NSRect(x: 452, y: 356, width: 150, height: 3), 0xE10600, 0.70),
        (NSRect(x: 500, y: 344, width: 118, height: 3), 0xC2C6CD, 0.35),
        (NSRect(x: 478, y: 332, width: 90, height: 2), 0xFF4336, 0.40),
        (NSRect(x: 30, y: 30, width: 120, height: 3), 0xC2C6CD, 0.30),
        (NSRect(x: 54, y: 20, width: 160, height: 3), 0xE10600, 0.60),
        (NSRect(x: 18, y: 10, width: 96, height: 2), 0xC2C6CD, 0.20),
    ]
    for (rect, hex, alpha) in streaks {
        NSGradient(starting: color(hex, 0), ending: color(hex, alpha))!.draw(in: slanted(rect, lean: 2), angle: 0)
    }

    // A thin silver line under the icons' row: the track's edge.
    let track = NSRect(x: 60, y: height - 300, width: width - 120, height: 1)
    NSGradient(colors: [color(0xC2C6CD, 0), color(0xC2C6CD, 0.35), color(0xC2C6CD, 0)])!.draw(in: track, angle: 0)

    // A light plate under each icon's name: Finder writes the names in black over a picture,
    // whatever the appearance, so they need a light ground to be read on this dark one.
    for centre in [app, applications] {
        let plate = NSRect(x: centre.x - 58, y: height - labelCentre - 11, width: 112, height: 22)
        color(0xE6E8EC, 0.92).setFill()
        slanted(plate, lean: 4).fill()
    }

    // The mark and the name, at the top.
    let markHeight: CGFloat = 26, markWidth = markHeight * mark.size.width / mark.size.height
    let name = NSMutableAttributedString(string: "ReCarbon", attributes: [
        .font: NSFont.systemFont(ofSize: 22, weight: .heavy), .foregroundColor: color(0xE6E8EC), .kern: 0.5])
    name.addAttribute(.foregroundColor, value: color(0xFF4336), range: NSRange(location: 2, length: 6))
    let nameSize = name.size(), gap: CGFloat = 10
    let left = (width - markWidth - gap - nameSize.width) / 2, middle = height - 44
    mark.draw(in: NSRect(x: left, y: middle - markHeight / 2, width: markWidth, height: markHeight))
    name.draw(at: NSPoint(x: left + markWidth + gap, y: middle - nameSize.height / 2))

    // The arrow: three slanted red chevrons, brighter towards Applications, between the two icons,
    // each with a silver highlight along its upper edge.
    let start = app.x + iconSize / 2 + 24, end = applications.x - iconSize / 2 - 24
    let y = height - app.y, step = (end - start) / 3
    for i in 0..<3 {
        let x = start + CGFloat(i) * step + 6
        let chevron = NSBezierPath()
        chevron.move(to: NSPoint(x: x, y: y + 18))
        chevron.line(to: NSPoint(x: x + 12, y: y + 18))
        chevron.line(to: NSPoint(x: x + 30, y: y))
        chevron.line(to: NSPoint(x: x + 12, y: y - 18))
        chevron.line(to: NSPoint(x: x, y: y - 18))
        chevron.line(to: NSPoint(x: x + 18, y: y))
        chevron.close()
        color(0xE10600, 0.45 + 0.275 * CGFloat(i)).setFill()
        chevron.fill()
        let highlight = NSBezierPath()
        highlight.move(to: NSPoint(x: x, y: y + 18))
        highlight.line(to: NSPoint(x: x + 12, y: y + 18))
        highlight.line(to: NSPoint(x: x + 30, y: y))
        highlight.lineWidth = 1.5
        highlight.lineJoinStyle = .round
        color(0xC2C6CD, 0.35 + 0.30 * CGFloat(i)).setStroke()
        highlight.stroke()
    }

    // What to do, below the icons' names.
    text("Drag ReCarbon onto Applications to install it", size: 14, weight: .semibold,
         colour: color(0xE6E8EC, 0.85), centreX: width / 2, top: 318)
    text("Need for Speed: Carbon, native on Apple silicon. Unofficial fan project.", size: 11, weight: .regular,
         colour: color(0xE6E8EC, 0.45), centreX: width / 2, top: 344)
}

// 16-bit RGBA to an 8-bit PNG, each channel rounded (no dithering: smooth gradients, small files),
// `width` x `height` points (72 dpi at 1x, 144 at 2x: the same size in points).
func toPNG(_ wide: NSBitmapImageRep) -> Data {
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
    narrow.size = NSSize(width: width, height: height)
    return narrow.representation(using: .png, properties: [:])!
}

for scale in [1, 2] {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(width) * scale, pixelsHigh: Int(height) * scale,
                               bitsPerSample: 16, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                               colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!.retagging(with: .sRGB)!
    rep.size = NSSize(width: width, height: height)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSGraphicsContext.current!.imageInterpolation = .high
    draw()
    NSGraphicsContext.restoreGraphicsState()
    let file = outputDir.appendingPathComponent(scale == 1 ? "DmgBackground.png" : "DmgBackground@2x.png")
    let png = toPNG(rep)
    try png.write(to: file)
    print("\(file.path): \(rep.pixelsWide) x \(rep.pixelsHigh)")
}
