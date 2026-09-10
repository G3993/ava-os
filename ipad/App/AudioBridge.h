// AVA OS iPad — ObjC facade over the shared C++ vibroacoustic engine.
// Mic listens to the room (iOS has no system-audio tap); pads play directly.
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVABridge : NSObject
+ (instancetype)shared;

- (BOOL)start;                 // configure session + graph; NO on failure
- (void)stop;
@property (readonly) NSString *lastError;

// octagon pads (zone 0=HEAD..4=FEET; k = slice 0..7; frac = radial 0..1)
- (void)padDownZone:(int)zone k:(int)k frac:(float)frac;
- (void)padUpZone:(int)zone;
- (void)padAllUp;
- (void)setPatch:(int)p;
- (int)patch;

// engine params
- (void)setIntensity:(float)v;
- (void)setVoidAmt:(float)v;
- (void)setMaster:(float)v;

// readouts for UI
- (float)meterZone:(int)z;     // 0..1 live zone output
- (float)level;                // input loudness envelope
- (float)bpm;
- (float)rootHz;
- (float)voidNow;
- (int)outputChannels;
- (NSString *)outputName;
@end

NS_ASSUME_NONNULL_END
