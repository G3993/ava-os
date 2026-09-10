#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/CATapDescription.h>
#import <CoreAudio/AudioHardwareTapping.h>
#include <unistd.h>
#include <vector>
#include <cmath>
#include "capture_tap.h"

static OSStatus tapIOProc(AudioObjectID inDevice, const AudioTimeStamp*,
                          const AudioBufferList* inInputData, const AudioTimeStamp*,
                          AudioBufferList*, const AudioTimeStamp*, void* clientData) {
    auto* self = (SystemTap*)clientData;
    if (!inInputData || inInputData->mNumberBuffers == 0) return noErr;

    const AudioBuffer& b = inInputData->mBuffers[0];
    const float* data = (const float*)b.mData;
    int ch = (int)b.mNumberChannels;
    int frames = (int)(b.mDataByteSize / sizeof(float) / (ch > 0 ? ch : 1));
    if (!data || frames <= 0) return noErr;

    float peak = 0;
    // build interleaved stereo regardless of source layout
    static thread_local std::vector<float> lr;
    lr.resize((size_t)frames * 2);
    if (ch >= 2) {
        for (int i = 0; i < frames; i++) {
            lr[i * 2] = data[i * ch];
            lr[i * 2 + 1] = data[i * ch + 1];
        }
    } else {
        // mono tap or planar multi-buffer: mix buffers
        for (int i = 0; i < frames; i++) { lr[i * 2] = data[i]; lr[i * 2 + 1] = data[i]; }
        if (inInputData->mNumberBuffers >= 2) {
            const float* d2 = (const float*)inInputData->mBuffers[1].mData;
            if (d2) for (int i = 0; i < frames; i++) lr[i * 2 + 1] = d2[i];
        }
    }
    for (int i = 0; i < frames; i++)
        peak = std::max(peak, std::max(std::fabs(lr[i * 2]), std::fabs(lr[i * 2 + 1])));
    self->inputPeak.store(std::max(peak, self->inputPeak.load() * 0.9f));

    if (self->ring()) self->ring()->push(lr.data(), frames);
    return noErr;
}

bool SystemTap::start(StereoRing* ring) {
    if (running_.load()) return true;
    ring_ = ring;
    lastError.clear();

    @autoreleasepool {
        // translate our own PID to a process object so we can exclude it
        NSArray<NSNumber*>* excluded = @[];
        {
            AudioObjectPropertyAddress addr = {
                kAudioHardwarePropertyTranslatePIDToProcessObject,
                kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
            pid_t pid = getpid();
            AudioObjectID procObj = kAudioObjectUnknown;
            UInt32 sz = sizeof(procObj);
            if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                           sizeof(pid), &pid, &sz, &procObj) == noErr &&
                procObj != kAudioObjectUnknown) {
                excluded = @[@(procObj)];
            }
        }

        CATapDescription* desc =
            [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:excluded];
        desc.name = @"AVASystemTap";
        desc.privateTap = YES;
        desc.muteBehavior = CATapUnmuted;

        AudioObjectID tapID = kAudioObjectUnknown;
        OSStatus err = AudioHardwareCreateProcessTap(desc, &tapID);
        if (err != noErr || tapID == kAudioObjectUnknown) {
            lastError = "AudioHardwareCreateProcessTap failed (" + std::to_string(err) +
                        ") — check System Audio Recording permission";
            return false;
        }
        tapID_ = tapID;

        NSDictionary* aggDesc = @{
            @(kAudioAggregateDeviceNameKey): @"AVA Tap Aggregate",
            @(kAudioAggregateDeviceUIDKey):
                [NSString stringWithFormat:@"nyc.soundtemple.tapagg.%d", getpid()],
            @(kAudioAggregateDeviceIsPrivateKey): @YES,
            @(kAudioAggregateDeviceTapAutoStartKey): @YES,
            @(kAudioAggregateDeviceTapListKey): @[@{
                @(kAudioSubTapUIDKey): desc.UUID.UUIDString,
                @(kAudioSubTapDriftCompensationKey): @YES,
            }],
        };
        AudioObjectID aggID = kAudioObjectUnknown;
        err = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDesc, &aggID);
        if (err != noErr || aggID == kAudioObjectUnknown) {
            lastError = "aggregate device failed (" + std::to_string(err) + ")";
            AudioHardwareDestroyProcessTap(tapID_);
            tapID_ = 0;
            return false;
        }
        aggID_ = aggID;

        // read aggregate rate for resample awareness (engine runs at 48k)
        {
            AudioObjectPropertyAddress addr = {kAudioDevicePropertyNominalSampleRate,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
            Float64 rate = 48000;
            UInt32 sz = sizeof(rate);
            if (AudioObjectGetPropertyData(aggID_, &addr, 0, nullptr, &sz, &rate) == noErr)
                tapSampleRate = rate;
        }

        AudioDeviceIOProcID procID = nullptr;
        err = AudioDeviceCreateIOProcID(aggID_, tapIOProc, this, &procID);
        if (err != noErr) {
            lastError = "IOProc create failed (" + std::to_string(err) + ")";
            stop();
            return false;
        }
        procID_ = (void*)procID;

        err = AudioDeviceStart(aggID_, procID);
        if (err != noErr) {
            lastError = "device start failed (" + std::to_string(err) + ")";
            stop();
            return false;
        }
    }
    running_.store(true);
    return true;
}

void SystemTap::stop() {
    if (aggID_) {
        if (procID_) {
            AudioDeviceStop(aggID_, (AudioDeviceIOProcID)procID_);
            AudioDeviceDestroyIOProcID(aggID_, (AudioDeviceIOProcID)procID_);
            procID_ = nullptr;
        }
        AudioHardwareDestroyAggregateDevice(aggID_);
        aggID_ = 0;
    }
    if (tapID_) {
        AudioHardwareDestroyProcessTap(tapID_);
        tapID_ = 0;
    }
    running_.store(false);
}
