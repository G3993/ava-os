#include "audio_out.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <vector>
#include <cmath>
#include <cstring>

unsigned defaultOutputDevice() {
    AudioObjectID id = kAudioObjectUnknown;
    UInt32 sz = sizeof(id);
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultOutputDevice,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &sz, &id) != noErr)
        return 0;
    return id == kAudioObjectUnknown ? 0 : (unsigned)id;
}

std::vector<OutDevice> listOutputDevices() {
    std::vector<OutDevice> out;
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDevices,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    UInt32 sz = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &sz) != noErr)
        return out;
    std::vector<AudioObjectID> ids(sz / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &sz, ids.data()) != noErr)
        return out;

    for (AudioObjectID id : ids) {
        // output channel count
        AudioObjectPropertyAddress cfgAddr = {kAudioDevicePropertyStreamConfiguration,
                                              kAudioObjectPropertyScopeOutput,
                                              kAudioObjectPropertyElementMain};
        UInt32 cfgSz = 0;
        if (AudioObjectGetPropertyDataSize(id, &cfgAddr, 0, nullptr, &cfgSz) != noErr) continue;
        std::vector<uint8_t> cfgBuf(cfgSz);
        auto* abl = (AudioBufferList*)cfgBuf.data();
        if (AudioObjectGetPropertyData(id, &cfgAddr, 0, nullptr, &cfgSz, abl) != noErr) continue;
        int ch = 0;
        for (UInt32 b = 0; b < abl->mNumberBuffers; b++) ch += abl->mBuffers[b].mNumberChannels;
        if (ch == 0) continue;

        // name
        CFStringRef nameRef = nullptr;
        UInt32 nameSz = sizeof(nameRef);
        AudioObjectPropertyAddress nameAddr = {kAudioObjectPropertyName,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
        std::string name = "Unknown";
        if (AudioObjectGetPropertyData(id, &nameAddr, 0, nullptr, &nameSz, &nameRef) == noErr && nameRef) {
            char buf[256] = {0};
            CFStringGetCString(nameRef, buf, sizeof(buf), kCFStringEncodingUTF8);
            name = buf;
            CFRelease(nameRef);
        }
        if (name.find("AVA Tap") != std::string::npos) continue; // hide our aggregate

        Float64 rate = 48000;
        UInt32 rateSz = sizeof(rate);
        AudioObjectPropertyAddress rateAddr = {kAudioDevicePropertyNominalSampleRate,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
        AudioObjectGetPropertyData(id, &rateAddr, 0, nullptr, &rateSz, &rate);

        out.push_back({id, name, ch, rate});
    }
    return out;
}

static OSStatus renderCB(void* inRefCon, AudioUnitRenderActionFlags*,
                         const AudioTimeStamp*, UInt32, UInt32 inNumberFrames,
                         AudioBufferList* ioData) {
    auto* self = (OutputUnit*)inRefCon;
    AudioBuffer& b = ioData->mBuffers[0];
    renderOutputBlock(self, self->engine(), (float*)b.mData, (int)inNumberFrames, (int)b.mNumberChannels,
                      (int)inNumberFrames + 128);
    return noErr;
}

float deviceHwVolume(unsigned deviceID) {
    float sum = 0;
    int cnt = 0;
    for (UInt32 el = 0; el < 32; el++) {
        AudioObjectPropertyAddress va = {kAudioDevicePropertyVolumeScalar,
                                         kAudioObjectPropertyScopeOutput, el};
        if (!AudioObjectHasProperty(deviceID, &va)) continue;
        Float32 v = 0;
        UInt32 sz = sizeof(v);
        if (AudioObjectGetPropertyData(deviceID, &va, 0, nullptr, &sz, &v) == noErr) {
            sum += v;
            cnt++;
        }
    }
    return cnt ? sum / cnt : -1.0f;
}

bool maxDeviceHwVolume(unsigned deviceID) {
    bool any = false;
    for (UInt32 el = 0; el < 32; el++) {
        AudioObjectPropertyAddress va = {kAudioDevicePropertyVolumeScalar,
                                         kAudioObjectPropertyScopeOutput, el};
        Boolean settable = 0;
        if (AudioObjectHasProperty(deviceID, &va) &&
            AudioObjectIsPropertySettable(deviceID, &va, &settable) == noErr && settable) {
            Float32 one = 1.0f;
            if (AudioObjectSetPropertyData(deviceID, &va, 0, nullptr, sizeof(one), &one) == noErr)
                any = true;
        }
        AudioObjectPropertyAddress ma = {kAudioDevicePropertyMute,
                                         kAudioObjectPropertyScopeOutput, el};
        if (AudioObjectHasProperty(deviceID, &ma) &&
            AudioObjectIsPropertySettable(deviceID, &ma, &settable) == noErr && settable) {
            UInt32 zero = 0;
            AudioObjectSetPropertyData(deviceID, &ma, 0, nullptr, sizeof(zero), &zero);
        }
    }
    return any;
}

bool OutputUnit::start(unsigned deviceID, Engine* engine, StereoRing* ring) {
    tapMutesSource = true; // CATapMutedWhenTapped in capture_tap.mm
    stop();
    engine_ = engine;
    ring_ = ring;
    lastError.clear();
    for (int z = 0; z < NZONES; z++) bedDelay_[z].init(16384);   // > 300 ms of alignment
    for (int c = 0; c < kMaxCh; c++) { chanPeak[c].store(0.0f); chanHeat[c].store(0.0f); chanThermGain[c].store(1.0f); heatMs_[c] = 0; thermG_[c] = 1.0f; }

    AudioComponentDescription desc = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput,
                                      kAudioUnitManufacturer_Apple, 0, 0};
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) { lastError = "no HAL output component"; return false; }

    AudioUnit unit = nullptr;
    if (AudioComponentInstanceNew(comp, &unit) != noErr) {
        lastError = "AU instantiate failed"; return false;
    }

    AudioDeviceID dev = deviceID;
    OSStatus err = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice,
                                        kAudioUnitScope_Global, 0, &dev, sizeof(dev));
    if (err != noErr) { lastError = "set device failed (" + std::to_string(err) + ")"; goto fail; }

    {
        // small IO buffer for low output latency (~5.8 ms @ 44.1k)
        UInt32 bufFrames = 128;
        AudioObjectPropertyAddress bufAddr = {kAudioDevicePropertyBufferFrameSize,
                                              kAudioObjectPropertyScopeOutput,
                                              kAudioObjectPropertyElementMain};
        AudioObjectSetPropertyData(dev, &bufAddr, 0, nullptr, sizeof(bufFrames), &bufFrames);
    }

    {
        // channel count of the device
        int devCh = 2;
        for (auto& d : listOutputDevices()) if (d.id == deviceID) devCh = d.channels;
        activeChannels = std::min(devCh, 16); // full UMC1820 (12 out) reachable

        AudioStreamBasicDescription fmt = {};
        fmt.mSampleRate = Engine::kSR;
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        fmt.mChannelsPerFrame = (UInt32)activeChannels;
        fmt.mBitsPerChannel = 32;
        fmt.mBytesPerFrame = 4 * fmt.mChannelsPerFrame;
        fmt.mFramesPerPacket = 1;
        fmt.mBytesPerPacket = fmt.mBytesPerFrame;
        err = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input, 0, &fmt, sizeof(fmt));
        if (err != noErr) { lastError = "set format failed (" + std::to_string(err) + ")"; goto fail; }

        AURenderCallbackStruct cb = {renderCB, this};
        err = AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback,
                                   kAudioUnitScope_Input, 0, &cb, sizeof(cb));
        if (err != noErr) { lastError = "set callback failed"; goto fail; }

        err = AudioUnitInitialize(unit);
        if (err != noErr) { lastError = "AU init failed (" + std::to_string(err) + ")"; goto fail; }
        err = AudioOutputUnitStart(unit);
        if (err != noErr) { lastError = "AU start failed (" + std::to_string(err) + ")"; goto fail; }
    }

    unit_ = unit;
    running_.store(true);
    return true;

fail:
    AudioComponentInstanceDispose(unit);
    return false;
}

void OutputUnit::stop() {
    if (unit_) {
        AudioOutputUnitStop((AudioUnit)unit_);
        AudioUnitUninitialize((AudioUnit)unit_);
        AudioComponentInstanceDispose((AudioUnit)unit_);
        unit_ = nullptr;
    }
    running_.store(false);
}
