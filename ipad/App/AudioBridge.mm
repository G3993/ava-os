// AVA OS iPad — audio plumbing.
// Input: iPad microphone (the room IS the source; iOS cannot tap other apps'
// audio) → lock-free ring → shared Engine. Output: AVAudioSourceNode with as
// many channels as the connected interface offers; zones land on the same
// device channels as the Mac build (params.zoneChan, default ch 3-7). On a
// plain iPad speaker (≤2 ch) the zones fold into a stereo vibration monitor.
#import "AudioBridge.h"
#import <AVFoundation/AVFoundation.h>
#include "engine.h"
#include "ringbuf.h"
#include <vector>
#include <algorithm>

static const int kMaxFrames = 8192;

@implementation AVABridge {
    Engine eng_;
    StereoRing micRing_;
    AVAudioEngine *av_;
    AVAudioSourceNode *src_;
    std::vector<float> inL_, inR_;
    std::vector<float> zoneBuf_[NZONES];
    int outCh_;
    BOOL running_;
    // per-buffer linear resampler state for non-48k mic routes
    double rsPos_;
    float rsLastL_, rsLastR_;
}

+ (instancetype)shared {
    static AVABridge *s;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ s = [[AVABridge alloc] init]; });
    return s;
}

- (instancetype)init {
    if ((self = [super init])) {
        eng_.init();
        micRing_.init(48000); // 1 s
        inL_.assign(kMaxFrames, 0);
        inR_.assign(kMaxFrames, 0);
        for (int z = 0; z < NZONES; z++) zoneBuf_[z].assign(kMaxFrames, 0);
        _lastError = @"";
        [[NSNotificationCenter defaultCenter]
            addObserver:self selector:@selector(routeChanged:)
                   name:AVAudioSessionRouteChangeNotification object:nil];
    }
    return self;
}

- (void)routeChanged:(NSNotification *)n {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->running_) { [self stop]; [self start]; }
    });
}

- (BOOL)start {
    if (running_) return YES;
    NSError *err = nil;
    AVAudioSession *s = [AVAudioSession sharedInstance];
    [s setCategory:AVAudioSessionCategoryPlayAndRecord
       withOptions:AVAudioSessionCategoryOptionMixWithOthers error:&err];
    [s setPreferredSampleRate:48000 error:nil];
    [s setPreferredIOBufferDuration:0.008 error:nil];
    if (![s setActive:YES error:&err]) {
        _lastError = [NSString stringWithFormat:@"session: %@", err.localizedDescription];
        return NO;
    }
    NSInteger maxCh = s.maximumOutputNumberOfChannels;
    [s setPreferredOutputNumberOfChannels:MIN(12, maxCh) error:nil];

    av_ = [[AVAudioEngine alloc] init];

    // ── mic → ring (convert to 48k stereo if the route runs elsewhere) ──
    AVAudioInputNode *inNode = av_.inputNode;
    AVAudioFormat *inFmt = [inNode outputFormatForBus:0];
    StereoRing *ring = &micRing_;
    double inRate = inFmt.sampleRate;
    rsPos_ = 0; rsLastL_ = rsLastR_ = 0;
    __block double rsPos = 0;
    __block float rsL = 0, rsR = 0;
    [inNode installTapOnBus:0 bufferSize:512 format:inFmt
                      block:^(AVAudioPCMBuffer *buf, AVAudioTime *when) {
        int n = (int)buf.frameLength;
        if (n <= 0 || !buf.floatChannelData) return;
        const float *L = buf.floatChannelData[0];
        const float *R = buf.format.channelCount > 1 ? buf.floatChannelData[1] : L;
        if (inRate == 48000.0) {
            ring->pushPlanar(L, R, n);
        } else { // linear resample to 48k
            double step = inRate / 48000.0;
            float tmpL[2048], tmpR[2048];
            int out = 0;
            double p = rsPos;
            while (out < 2048) {
                int i = (int)p;
                if (i >= n - 1) break;
                float f = (float)(p - i);
                tmpL[out] = L[i] + f * (L[i + 1] - L[i]);
                tmpR[out] = R[i] + f * (R[i + 1] - R[i]);
                out++; p += step;
            }
            // carry the sub-sample phase into the next buffer
            rsPos = p - (double)(n - 1);
            if (rsPos < 0) rsPos = 0;
            if (rsPos >= 1.0) rsPos -= (long)rsPos;
            ring->pushPlanar(tmpL, tmpR, out);
            rsL = L[n - 1]; rsR = R[n - 1]; (void)rsL; (void)rsR;
        }
    }];

    // ── engine render → device channels ──
    AVAudioFormat *hw = [av_.outputNode outputFormatForBus:0];
    outCh_ = (int)hw.channelCount;
    int N = std::max(2, std::min(outCh_, 12));
    AVAudioFormat *fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000
                                                                        channels:(AVAudioChannelCount)N];
    Engine *eng = &eng_;
    float *inL = inL_.data(), *inR = inR_.data();
    float *zp[NZONES];
    for (int z = 0; z < NZONES; z++) zp[z] = zoneBuf_[z].data();
    float *z0 = zp[0], *z1 = zp[1], *z2 = zp[2], *z3 = zp[3], *z4 = zp[4];

    src_ = [[AVAudioSourceNode alloc] initWithFormat:fmt renderBlock:
            ^OSStatus(BOOL *isSilence, const AudioTimeStamp *ts,
                      AVAudioFrameCount frameCount, AudioBufferList *out) {
        int n = std::min((int)frameCount, kMaxFrames);
        // latency policy: never let the mic ring back up past ~100 ms
        int avail = ring->available();
        if (avail > 4800) ring->discard(avail - 2400);
        ring->pop(inL, inR, n);
        float *zones[NZONES] = {z0, z1, z2, z3, z4};
        eng->process(inL, inR, zones, n);
        float master = eng->params.masterVolume.load();
        int nb = (int)out->mNumberBuffers;
        for (int b = 0; b < nb; b++)
            memset(out->mBuffers[b].mData, 0, out->mBuffers[b].mDataByteSize);
        if (nb >= 7) {
            for (int z = 0; z < NZONES; z++) {
                int c = eng->params.zoneChan[z].load();
                if (c < 0 || c >= nb) continue;
                float *dst = (float *)out->mBuffers[c].mData;
                float *sz = zones[z];
                for (int i = 0; i < n; i++) dst[i] = sz[i] * master;
            }
        } else { // stereo vibration monitor
            float *dl = (float *)out->mBuffers[0].mData;
            float *dr = nb > 1 ? (float *)out->mBuffers[1].mData : dl;
            for (int i = 0; i < n; i++) {
                float m = 0;
                for (int z = 0; z < NZONES; z++) m += zones[z][i];
                m *= 0.30f * master;
                dl[i] = m; dr[i] = m;
            }
        }
        *isSilence = NO;
        return noErr;
    }];

    [av_ attachNode:src_];
    [av_ connect:src_ to:av_.outputNode format:fmt];

    if (![av_ startAndReturnError:&err]) {
        _lastError = [NSString stringWithFormat:@"engine: %@", err.localizedDescription];
        [inNode removeTapOnBus:0];
        av_ = nil; src_ = nil;
        return NO;
    }
    running_ = YES;
    _lastError = @"";
    NSLog(@"[AVA] audio up: in %.0f Hz %dch → out %d ch @ %@ (graph 48k, %d engine ch)",
          inRate, (int)inFmt.channelCount, outCh_, [self outputName], N);
    return YES;
}

- (void)stop {
    if (!running_) return;
    [av_.inputNode removeTapOnBus:0];
    [av_ stop];
    av_ = nil; src_ = nil;
    running_ = NO;
    [self padAllUp];
}

// ── pads: same feel as the Mac octagon ──
- (void)padDownZone:(int)zone k:(int)k frac:(float)frac {
    if (zone < 0 || zone >= NZONES) return;
    int patch = std::min(std::max(eng_.params.padPatch.load(), 0), NPATCHES - 1);
    float pbase = kPadPatches[patch].baseHz;
    float hz = pbase > 0
        ? pbase * std::pow(2.0f, k / 12.0f)
        : std::min(200.0f, eng_.zoneHz[zone].load() * std::pow(2.0f, k / 8.0f));
    float vel = 0.55f + 0.45f * std::min(std::max(frac, 0.0f), 1.0f);
    bool allZones = patch == PAD_WHALE;
    for (int z = 0; z < NZONES; z++) {
        if (!allZones && z != zone) continue;
        eng_.params.padHz[z].store(hz);
        eng_.params.padVel[z].store(vel);
        eng_.params.padGate[z].store(1);
    }
}
- (void)padUpZone:(int)zone {
    if (zone < 0 || zone >= NZONES) return;
    eng_.params.padGate[zone].store(0);
}
- (void)padAllUp {
    for (int z = 0; z < NZONES; z++) eng_.params.padGate[z].store(0);
}
- (void)setPatch:(int)p { eng_.params.padPatch.store(std::min(std::max(p, 0), NPATCHES - 1)); }
- (int)patch { return eng_.params.padPatch.load(); }

- (void)setIntensity:(float)v { eng_.params.intensity.store(v); }
- (void)setVoidAmt:(float)v { eng_.params.voidAmt.store(v); }
- (void)setMaster:(float)v { eng_.params.masterVolume.store(v); }

- (float)meterZone:(int)z {
    return (z >= 0 && z < NZONES) ? eng_.meter[2 + z].load() : 0;
}
- (float)level { return eng_.audioLevel.load(); }
- (float)bpm { return eng_.analyzer.out.bpm.load(); }
- (float)rootHz { return eng_.analyzer.out.midOctaveHz.load(); }
- (float)voidNow { return eng_.voidNow.load(); }
- (int)outputChannels { return outCh_; }
- (NSString *)outputName {
    AVAudioSessionRouteDescription *r = [AVAudioSession sharedInstance].currentRoute;
    return r.outputs.firstObject ? r.outputs.firstObject.portName : @"—";
}
@end
