// Set the Mac's default output device by name substring.
//   swift tools/set_default_output.swift "BlackHole 2ch"
//   swift tools/set_default_output.swift UMC1820
import CoreAudio
import Foundation

let want = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "BlackHole 2ch"

var addr = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices,
                                      mScope: kAudioObjectPropertyScopeGlobal,
                                      mElement: kAudioObjectPropertyElementMain)
var size: UInt32 = 0
AudioObjectGetPropertyDataSize(AudioObjectID(kAudioObjectSystemObject), &addr, 0, nil, &size)
var ids = [AudioObjectID](repeating: 0, count: Int(size) / MemoryLayout<AudioObjectID>.size)
AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &addr, 0, nil, &size, &ids)

func name(_ id: AudioObjectID) -> String {
    var a = AudioObjectPropertyAddress(mSelector: kAudioObjectPropertyName,
                                       mScope: kAudioObjectPropertyScopeGlobal,
                                       mElement: kAudioObjectPropertyElementMain)
    var s: CFString = "" as CFString
    var sz = UInt32(MemoryLayout<CFString>.size)
    AudioObjectGetPropertyData(id, &a, 0, nil, &sz, &s)
    return s as String
}
func outputs(_ id: AudioObjectID) -> Int {
    var a = AudioObjectPropertyAddress(mSelector: kAudioDevicePropertyStreamConfiguration,
                                       mScope: kAudioObjectPropertyScopeOutput,
                                       mElement: kAudioObjectPropertyElementMain)
    var sz: UInt32 = 0
    AudioObjectGetPropertyDataSize(id, &a, 0, nil, &sz)
    let buf = UnsafeMutablePointer<AudioBufferList>.allocate(capacity: Int(sz))
    defer { buf.deallocate() }
    AudioObjectGetPropertyData(id, &a, 0, nil, &sz, buf)
    return UnsafeMutableAudioBufferListPointer(buf).reduce(0) { $0 + Int($1.mNumberChannels) }
}

guard let target = ids.first(where: { outputs($0) > 0 && name($0).contains(want) }) else {
    print("no output device matching \"\(want)\""); exit(1)
}
var def = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDefaultOutputDevice,
                                     mScope: kAudioObjectPropertyScopeGlobal,
                                     mElement: kAudioObjectPropertyElementMain)
var t = target
let err = AudioObjectSetPropertyData(AudioObjectID(kAudioObjectSystemObject), &def, 0, nil,
                                     UInt32(MemoryLayout<AudioObjectID>.size), &t)
print(err == noErr ? "default output -> \(name(target))" : "failed (\(err))")
exit(err == noErr ? 0 : 1)
