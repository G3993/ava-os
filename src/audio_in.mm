#include "audio_in.h"
#import <AVFoundation/AVFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <cmath>
#include <cstring>

static OSStatus inputCB(void* refCon, AudioUnitRenderActionFlags* flags,
                        const AudioTimeStamp* ts, UInt32 bus, UInt32 nFrames, AudioBufferList*) {
    auto* self = (InputUnit*)refCon;
    self->cbCount.fetch_add(1, std::memory_order_relaxed);
    int ch = self->deviceChannels;
    if (ch <= 0 || !self->ring()) return noErr;
    self->scratch_.resize((size_t)nFrames * ch);
    AudioBufferList abl;
    abl.mNumberBuffers = 1;
    abl.mBuffers[0].mNumberChannels = (UInt32)ch;
    abl.mBuffers[0].mDataByteSize = (UInt32)(nFrames * ch * sizeof(float));
    abl.mBuffers[0].mData = self->scratch_.data();
    OSStatus err = AudioUnitRender((AudioUnit)self->unit_, flags, ts, bus, nFrames, &abl);
    self->lastErr.store((int)err, std::memory_order_relaxed);
    if (err != noErr) return err;
    self->processBlock(self->scratch_.data(), ch, (int)nFrames);
    return noErr;
}

// The HAL does its device start-up and notification work on the process's
// main run loop by default. Our main loop is a GLFW frame loop: when it is
// blocked (vsync on a sleeping display, a long frame) an input unit that
// was just started never delivers a single callback — the mic reads as
// silence with no error. Hand the HAL its own thread instead, once.
static void halOwnThread() {
    static bool done = false;
    if (done) return;
    done = true;
    CFRunLoopRef none = nullptr;
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectSetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, sizeof(none), &none);
}

bool InputUnit::start(unsigned deviceID, StereoRing* ring) {
    halOwnThread();
    stop();
    ring_ = ring;
    lastError.clear();
    AudioComponentDescription desc = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput,
                                      kAudioUnitManufacturer_Apple, 0, 0};
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    AudioUnit unit = nullptr;
    if (!comp || AudioComponentInstanceNew(comp, &unit) != noErr) { lastError = "no input unit"; return false; }
    UInt32 on = 1, off = 0;
    AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &on, sizeof(on));
    AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &off, sizeof(off));
    AudioDeviceID dev = (AudioDeviceID)deviceID;
    if (AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev, sizeof(dev)) != noErr) {
        lastError = "device has no input"; AudioComponentInstanceDispose(unit); return false;
    }
    // how many input channels the device offers
    AudioStreamBasicDescription devFmt{}; UInt32 sz = sizeof(devFmt);
    AudioUnitGetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 1, &devFmt, &sz);
    deviceChannels = (int)devFmt.mChannelsPerFrame;
    hwRate = devFmt.mSampleRate;
    cbCount.store(0); lastErr.store(0);
    if (deviceChannels <= 0) { lastError = "device has no input"; AudioComponentInstanceDispose(unit); return false; }
    AudioStreamBasicDescription fmt{};
    fmt.mSampleRate = 48000; fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mChannelsPerFrame = (UInt32)deviceChannels; fmt.mBitsPerChannel = 32;
    fmt.mFramesPerPacket = 1; fmt.mBytesPerFrame = 4 * fmt.mChannelsPerFrame; fmt.mBytesPerPacket = fmt.mBytesPerFrame;
    if (AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &fmt, sizeof(fmt)) != noErr) {
        lastError = "input format refused"; AudioComponentInstanceDispose(unit); return false;
    }
    { UInt32 bufFrames = 128;
      AudioObjectPropertyAddress bufAddr = {kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
      AudioObjectSetPropertyData(dev, &bufAddr, 0, nullptr, sizeof(bufFrames), &bufFrames); }
    AURenderCallbackStruct cb = {inputCB, this};
    AudioUnitSetProperty(unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &cb, sizeof(cb));
    unit_ = unit;
    if (AudioUnitInitialize(unit) != noErr || AudioOutputUnitStart(unit) != noErr) {
        lastError = "input start failed"; AudioComponentInstanceDispose(unit); unit_ = nullptr; return false;
    }
    running_.store(true);
    return true;
}

void InputUnit::stop() {
    if (!unit_) return;
    AudioOutputUnitStop((AudioUnit)unit_);
    AudioUnitUninitialize((AudioUnit)unit_);
    AudioComponentInstanceDispose((AudioUnit)unit_);
    unit_ = nullptr;
    running_.store(false);
    peak.store(0);
}

// ── live bed input ──
static OSStatus bedCB(void* refCon, AudioUnitRenderActionFlags* flags,
                      const AudioTimeStamp* ts, UInt32 bus, UInt32 nFrames, AudioBufferList*) {
    auto* self = (BedInput*)refCon;
    int ch = self->deviceChannels;
    if (ch <= 0) return noErr;
    self->scratch_.resize((size_t)nFrames * ch);
    AudioBufferList abl;
    abl.mNumberBuffers = 1;
    abl.mBuffers[0].mNumberChannels = (UInt32)ch;
    abl.mBuffers[0].mDataByteSize = (UInt32)(nFrames * ch * sizeof(float));
    abl.mBuffers[0].mData = self->scratch_.data();
    OSStatus err = AudioUnitRender((AudioUnit)self->unit_, flags, ts, bus, nFrames, &abl);
    if (err != noErr) return err;
    int first = std::min(std::max(self->firstChan.load(), 0), std::max(0, ch - 1));
    self->ring.pushInterleaved(self->scratch_.data(), (int)nFrames, ch, first);
    for (int c = 0; c < BedInput::kCh; c++) {
        int sc = first + c;
        float pk = 0;
        if (sc < ch) for (UInt32 i = 0; i < nFrames; i++) pk = std::max(pk, std::fabs(self->scratch_[i * ch + sc]));
        float prev = self->peak[c].load(std::memory_order_relaxed) * 0.9f;
        self->peak[c].store(pk > prev ? pk : prev, std::memory_order_relaxed);
    }
    return noErr;
}

bool BedInput::start(unsigned devID) {
    halOwnThread();
    stop();
    lastError.clear();
    deviceID = devID;
    AudioComponentDescription desc = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput,
                                      kAudioUnitManufacturer_Apple, 0, 0};
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    AudioUnit unit = nullptr;
    if (!comp || AudioComponentInstanceNew(comp, &unit) != noErr) { lastError = "no input unit"; return false; }
    UInt32 on = 1, off = 0;
    AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &on, sizeof(on));
    AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &off, sizeof(off));
    AudioDeviceID dev = (AudioDeviceID)devID;
    if (AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev, sizeof(dev)) != noErr) {
        lastError = "device has no input"; AudioComponentInstanceDispose(unit); return false;
    }
    AudioStreamBasicDescription devFmt{}; UInt32 sz = sizeof(devFmt);
    AudioUnitGetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 1, &devFmt, &sz);
    deviceChannels = (int)devFmt.mChannelsPerFrame;
    if (deviceChannels < 2) { lastError = "device has fewer than 2 inputs"; AudioComponentInstanceDispose(unit); return false; }
    AudioStreamBasicDescription fmt{};
    fmt.mSampleRate = 48000; fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mChannelsPerFrame = (UInt32)deviceChannels; fmt.mBitsPerChannel = 32;
    fmt.mFramesPerPacket = 1; fmt.mBytesPerFrame = 4 * fmt.mChannelsPerFrame; fmt.mBytesPerPacket = fmt.mBytesPerFrame;
    if (AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &fmt, sizeof(fmt)) != noErr) {
        lastError = "input format refused"; AudioComponentInstanceDispose(unit); return false;
    }
    { UInt32 bufFrames = 128;
      AudioObjectPropertyAddress bufAddr = {kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
      AudioObjectSetPropertyData(dev, &bufAddr, 0, nullptr, sizeof(bufFrames), &bufFrames); }
    ring.init(48000, kCh);
    for (int c = 0; c < kCh; c++) peak[c].store(0.0f);
    AURenderCallbackStruct cb = {bedCB, this};
    AudioUnitSetProperty(unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &cb, sizeof(cb));
    unit_ = unit;
    if (AudioUnitInitialize(unit) != noErr || AudioOutputUnitStart(unit) != noErr) {
        lastError = "input start failed"; AudioComponentInstanceDispose(unit); unit_ = nullptr; return false;
    }
    setRunning(true);
    return true;
}

void BedInput::stop() {
    if (!unit_) return;
    setRunning(false);
    AudioOutputUnitStop((AudioUnit)unit_);
    AudioUnitUninitialize((AudioUnit)unit_);
    AudioComponentInstanceDispose((AudioUnit)unit_);
    unit_ = nullptr;
    for (int c = 0; c < kCh; c++) peak[c].store(0.0f);
}

std::vector<OutDevice> listInputDevices() {
    std::vector<OutDevice> out;
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32 sz = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &sz) != noErr) return out;
    std::vector<AudioObjectID> ids(sz / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &sz, ids.data()) != noErr) return out;
    for (AudioObjectID id : ids) {
        AudioObjectPropertyAddress cfgAddr = {kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
        UInt32 cfgSz = 0;
        if (AudioObjectGetPropertyDataSize(id, &cfgAddr, 0, nullptr, &cfgSz) != noErr) continue;
        std::vector<uint8_t> cfgBuf(cfgSz);
        auto* abl = (AudioBufferList*)cfgBuf.data();
        if (AudioObjectGetPropertyData(id, &cfgAddr, 0, nullptr, &cfgSz, abl) != noErr) continue;
        int ch = 0;
        for (UInt32 b = 0; b < abl->mNumberBuffers; b++) ch += abl->mBuffers[b].mNumberChannels;
        if (ch == 0) continue;
        CFStringRef nameRef = nullptr; UInt32 nameSz = sizeof(nameRef);
        AudioObjectPropertyAddress nameAddr = {kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        std::string name = "Unknown";
        if (AudioObjectGetPropertyData(id, &nameAddr, 0, nullptr, &nameSz, &nameRef) == noErr && nameRef) {
            char buf[256] = {0};
            CFStringGetCString(nameRef, buf, sizeof(buf), kCFStringEncodingUTF8);
            name = buf; CFRelease(nameRef);
        }
        if (name.find("AVA Tap") != std::string::npos) continue;
        Float64 rate = 48000; UInt32 rateSz = sizeof(rate);
        AudioObjectPropertyAddress rateAddr = {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        AudioObjectGetPropertyData(id, &rateAddr, 0, nullptr, &rateSz, &rate);
        out.push_back({(unsigned)id, name, ch, rate});
    }
    return out;
}

unsigned defaultInputDevice() {
    AudioDeviceID id = kAudioObjectUnknown; UInt32 sz = sizeof(id);
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &sz, &id) != noErr) return 0;
    return id == kAudioObjectUnknown ? 0 : (unsigned)id;
}

int micAuthStatus() {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
        case AVAuthorizationStatusAuthorized: return 2;
        case AVAuthorizationStatusNotDetermined: return 0;
        default: return 1;
    }
}
void micRequestAccess() {
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
        fprintf(stderr, "[input] microphone access %s\n", granted ? "granted" : "denied");
    }];
}
