// AVA OS iPad — the octagon as a multi-touch pad surface.
// Same geometry as the Mac build: center = HEAD, rings out to FEET,
// 8 slices = pitch steps, radial position = velocity. Every finger is a voice.
import SwiftUI
import UIKit

final class PadUIView: UIView {
    private var touchZone: [UITouch: Int] = [:]
    private var link: CADisplayLink?

    // ring radius fractions, matching the Mac octagon
    private let bounds5: [CGFloat] = [0.36, 0.50, 0.65, 0.80, 0.96]
    private let ringZone = [1, 2, 3, 4] // HEART BELLY ROOT FEET
    private let ringAlpha: [CGFloat] = [0.84, 0.72, 0.61, 0.49]

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        backgroundColor = .clear
        contentMode = .redraw
        link = CADisplayLink(target: self, selector: #selector(tick))
        link?.preferredFramesPerSecond = 30
        link?.add(to: .main, forMode: .common)
    }
    required init?(coder: NSCoder) { fatalError() }
    deinit { link?.invalidate() }
    @objc private func tick() { setNeedsDisplay() }

    private func padAt(_ p: CGPoint) -> (zone: Int, k: Int, frac: Float)? {
        let c = CGPoint(x: bounds.midX, y: bounds.midY)
        let R = min(bounds.width, bounds.height) * 0.5 * 0.98
        let dx = p.x - c.x, dy = p.y - c.y
        let dist = hypot(dx, dy)
        if dist < 1 { return (0, 0, 0.5) }
        let theta = atan2(dy, dx)
        var rel = theta - (-.pi / 2 + .pi / 8)
        rel -= floor(rel / (2 * .pi)) * 2 * .pi
        let k = min(7, Int(rel / (.pi / 4)))
        let u = (theta - (-.pi / 2 + .pi / 4)) / (.pi / 4)
        let delta = (u - u.rounded()) * (.pi / 4)
        let f = dist * cos(delta) / (R * cos(.pi / 8))
        if f < bounds5[0] { return (0, k, 0.5) }
        for ring in 0..<4 where f < bounds5[ring + 1] {
            let frac = Float((f - bounds5[ring]) / (bounds5[ring + 1] - bounds5[ring]))
            return (ringZone[ring], k, frac)
        }
        return nil
    }

    private func apply(_ touches: Set<UITouch>) {
        let bridge = AVABridge.shared()
        for t in touches {
            guard let pad = padAt(t.location(in: self)) else {
                if let old = touchZone.removeValue(forKey: t) { bridge.padUpZone(Int32(old)) }
                continue
            }
            if let old = touchZone[t], old != pad.zone { bridge.padUpZone(Int32(old)) }
            touchZone[t] = pad.zone
            bridge.padDownZone(Int32(pad.zone), k: Int32(pad.k), frac: pad.frac)
        }
    }
    private func lift(_ touches: Set<UITouch>) {
        let bridge = AVABridge.shared()
        for t in touches {
            if let z = touchZone.removeValue(forKey: t) { bridge.padUpZone(Int32(z)) }
        }
        if touchZone.isEmpty {
            bridge.padAllUp()
        } else { // re-assert held pads (covers WHALE's all-zone gating)
            for t in touchZone.keys {
                if let pad = padAt(t.location(in: self)) {
                    bridge.padDownZone(Int32(pad.zone), k: Int32(pad.k), frac: pad.frac)
                }
            }
        }
    }
    override func touchesBegan(_ t: Set<UITouch>, with e: UIEvent?) { apply(t) }
    override func touchesMoved(_ t: Set<UITouch>, with e: UIEvent?) { apply(t) }
    override func touchesEnded(_ t: Set<UITouch>, with e: UIEvent?) { lift(t) }
    override func touchesCancelled(_ t: Set<UITouch>, with e: UIEvent?) { lift(t) }

    private func octPath(_ c: CGPoint, _ r: CGFloat) -> UIBezierPath {
        let path = UIBezierPath()
        for i in 0..<8 {
            let a = -CGFloat.pi / 2 + .pi / 8 + CGFloat(i) * .pi / 4
            let pt = CGPoint(x: c.x + r * cos(a), y: c.y + r * sin(a))
            i == 0 ? path.move(to: pt) : path.addLine(to: pt)
        }
        path.close()
        return path
    }

    override func draw(_ rect: CGRect) {
        let bridge = AVABridge.shared()
        let c = CGPoint(x: bounds.midX, y: bounds.midY)
        let R = min(bounds.width, bounds.height) * 0.5 * 0.98
        let voidDim = CGFloat(1.0 - 0.65 * bridge.voidNow())
        // rings outer → inner (FEET → HEART), then HEAD center
        for ring in stride(from: 3, through: 0, by: -1) {
            let z = ringZone[ring]
            let meter = CGFloat(min(1, bridge.meterZone(Int32(z))))
            let a = (0.10 + 0.16 * ringAlpha[ring] + 0.45 * meter) * voidDim
            let path = octPath(c, R * bounds5[ring + 1])
            UIColor(white: 1, alpha: min(0.92, a)).setFill()
            path.fill()
            UIColor(white: 0, alpha: 0.55).setStroke()
            path.lineWidth = 2
            path.stroke()
        }
        let headMeter = CGFloat(min(1, bridge.meterZone(0)))
        let head = octPath(c, R * bounds5[0])
        UIColor(white: 1, alpha: min(0.98, (0.34 + 0.5 * headMeter) * voidDim)).setFill()
        head.fill()
        // held pads flash
        for (t, _) in touchZone {
            let p = t.location(in: self)
            let dot = UIBezierPath(arcCenter: p, radius: 26, startAngle: 0,
                                   endAngle: 2 * .pi, clockwise: true)
            UIColor(white: 1, alpha: 0.35).setFill()
            dot.fill()
        }
    }
}

struct OctagonPadView: UIViewRepresentable {
    func makeUIView(context: Context) -> PadUIView { PadUIView() }
    func updateUIView(_ v: PadUIView, context: Context) {}
}
