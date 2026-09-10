// AVA OS iPad — B&W control surface: octagon pads + the essential params.
import SwiftUI
import AVFoundation

struct ContentView: View {
    @State private var intensity: Double = 0.7
    @State private var voidAmt: Double = 0.0
    @State private var master: Double = 1.0
    @State private var patch: Int = 0
    @State private var bpm: Float = 120
    @State private var rootHz: Float = 55
    @State private var level: Float = 0
    @State private var route = "—"
    @State private var channels = 0
    @State private var started = false
    @State private var errorText = ""

    private let patches = ["PURE", "WHALE", "QUAKE", "HEART", "PURR", "DROP"]
    private let poll = Timer.publish(every: 0.25, on: .main, in: .common).autoconnect()

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 14) {
                HStack {
                    Text("AVA OS").font(.system(size: 22, weight: .semibold, design: .monospaced))
                        .foregroundColor(.white.opacity(0.95))
                    Spacer()
                    Circle().fill(level > 0.02 ? Color.white : Color.white.opacity(0.2))
                        .frame(width: 9, height: 9)
                    Text(started ? "\(route) · \(channels)ch" : "starting…")
                        .font(.system(size: 13, design: .monospaced))
                        .foregroundColor(.white.opacity(0.45))
                }
                .padding(.horizontal, 26).padding(.top, 14)

                OctagonPadView()
                    .aspectRatio(1, contentMode: .fit)
                    .frame(maxWidth: 560)

                Text(String(format: "%.0f BPM   ·   %.0f Hz", bpm, rootHz))
                    .font(.system(size: 14, design: .monospaced))
                    .foregroundColor(.white.opacity(0.55))

                HStack(spacing: 10) {
                    ForEach(patches.indices, id: \.self) { i in
                        Button(patches[i]) {
                            patch = i
                            AVABridge.shared().setPatch(Int32(i))
                        }
                        .font(.system(size: 13, weight: .medium, design: .monospaced))
                        .foregroundColor(patch == i ? .black : .white.opacity(0.7))
                        .padding(.horizontal, 14).padding(.vertical, 8)
                        .background(patch == i ? Color.white : Color.white.opacity(0.08))
                        .cornerRadius(16)
                    }
                }

                VStack(spacing: 10) {
                    paramSlider("INTENSITY", $intensity) { AVABridge.shared().setIntensity(Float($0)) }
                    paramSlider("VOID", $voidAmt) { AVABridge.shared().setVoidAmt(Float($0)) }
                    paramSlider("MASTER", $master) { AVABridge.shared().setMaster(Float($0)) }
                }
                .frame(maxWidth: 560)
                .padding(.bottom, 18)

                if !errorText.isEmpty {
                    Text(errorText).font(.system(size: 12, design: .monospaced))
                        .foregroundColor(.red.opacity(0.8))
                }
            }
        }
        .statusBarHidden()
        .onAppear {
            AVAudioSessionHelper.requestMic { granted in
                let ok = AVABridge.shared().start()
                started = ok
                if !ok { errorText = AVABridge.shared().lastError }
                if !granted { errorText = "microphone denied — pads only" }
            }
            UIApplication.shared.isIdleTimerDisabled = true
        }
        .onReceive(poll) { _ in
            let b = AVABridge.shared()
            bpm = b.bpm(); rootHz = b.rootHz(); level = b.level()
            route = b.outputName(); channels = Int(b.outputChannels())
        }
    }

    private func paramSlider(_ label: String, _ v: Binding<Double>,
                             onChange: @escaping (Double) -> Void) -> some View {
        HStack(spacing: 14) {
            Text(label).font(.system(size: 11, design: .monospaced))
                .foregroundColor(.white.opacity(0.4)).frame(width: 84, alignment: .leading)
            Slider(value: v).tint(.white.opacity(0.85))
                .onChange(of: v.wrappedValue) { onChange($0) }
            Text(String(format: "%.0f", v.wrappedValue * 100))
                .font(.system(size: 11, design: .monospaced))
                .foregroundColor(.white.opacity(0.4)).frame(width: 30, alignment: .trailing)
        }
        .padding(.horizontal, 26)
    }
}

enum AVAudioSessionHelper {
    static func requestMic(_ done: @escaping (Bool) -> Void) {
        AVAudioSession.sharedInstance().requestRecordPermission { g in
            DispatchQueue.main.async { done(g) }
        }
    }
}
