#include "audio_in.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <cmath>
#include <cstring>

static OSStatus inputCB(void* refCon, AudioUnitRenderActionFlags* flags,
                        const AudioTimeStamp* ts, UInt32 bus, UInt32 nFrames, AudioBufferList*) {
    auto* self = (InputUnit*)refCon;
    int ch = self->deviceChannels;
    if (ch <= 0 || !self->ring()) return noErr;
    self->scratch_.resize((size_t)nFrames * ch);
    AudioBufferList abl;
    abl.mNumberBuffers = 1;
    abl.mBuffers[0].mNumberChannels = (UInt32)ch;
    abl.mBuffers[0].mDataByteSize = (UInt32)(nFrames * ch * sizeof(float));
    abl.mBuffers[0].mData = self->scratch_.data();
    OSStatus err = AudioUnitRender((AudioUnit)self->unit_, flags, ts, bus, nFrames, &abl);
    if (err != noErr) return err;
    int cl = std::min(std::max(self->chanL.load(), 0), ch - 1);
    int cr = std::min(std::max(self->chanR.load(), 0), ch - 1);
    float g = self->gain.load();
    static thread_local std::vector<float> L, R;
    L.resize(nFrames); R.resize(nFrames);
    float pk = 0;
    for (UInt32 i = 0; i < nFrames; i++) {
        float l = self->scratch_[i * ch + cl] * g, r = self->scratch_[i * ch + cr] * g;
        L[i] = l; R[i] = r;
        pk = std::max(pk, std::max(std::fabs(l), std::fabs(r)));
    }
    float prev = self->peak.load() * 0.9f;
    self->peak.store(pk > prev ? pk : prev);
    self->ring()->pushPlanar(L.data(), R.data(), (int)nFrames);
    return noErr;
}

bool InputUnit::start(unsigned deviceID, StereoRing* ring) {
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
