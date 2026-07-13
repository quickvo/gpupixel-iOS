

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

@protocol RTCBeautyFilterDelegate <NSObject>

- (void)didReceivePixelBuffer:(CVPixelBufferRef)pixelBuffer
                        width:(int)width
                       height:(int)height;

@end

@interface RTCBeautyFilter : NSObject

@property (nonatomic, weak) id<RTCBeautyFilterDelegate> delegate;
@property (nonatomic, assign) BOOL logEnabled;
@property (nonatomic, assign) CGFloat beautyValue;
@property (nonatomic, assign) CGFloat whithValue;
@property (nonatomic, assign) CGFloat thinFaceValue;
@property (nonatomic, assign) CGFloat eyeValue;

@property (nonatomic, assign) CGFloat contrastValue;
@property (nonatomic, assign) CGFloat exposureValue;
@property (nonatomic, assign) CGFloat saturationValue;
@property (nonatomic, assign) CGFloat red;
@property (nonatomic, assign) CGFloat green;
@property (nonatomic, assign) CGFloat blue;
@property (nonatomic, assign) CGFloat hueValue;
@property (nonatomic, assign) CGFloat tint;
@property (nonatomic, assign) CGFloat temperature;
@property(nonatomic, assign) CGFloat brightValue;

@property(nonatomic, assign) CGFloat sharpenValue;
@property(nonatomic, assign) CGFloat ruddy;
@property(nonatomic, assign) CGFloat shadowValue;
@property(nonatomic, assign) CGFloat highlightsValue;
@property(nonatomic, assign) CGFloat vignetValue;

@property (nonatomic, assign) CGFloat adjustMouth;
@property (nonatomic, assign) CGFloat thinNose;
@property (nonatomic, assign) CGFloat thinChin;
@property (nonatomic, assign) CGFloat faceShave;
@property (nonatomic, assign) CGFloat eyesDistance;
@property (nonatomic, assign) CGFloat nosePosition;

@property (nonatomic, assign) NSInteger rotation;


@property (nonatomic, assign) CGFloat lipstickValue;
@property (nonatomic, assign) CGFloat blusherValue;
@property (nonatomic, assign) CGFloat lashValue;
@property (nonatomic, assign) CGFloat whitenTeethValue;
@property (nonatomic, assign) CGFloat eyeShadowValue;
@property (nonatomic, assign) CGFloat eyeLinesValue;
@property (nonatomic, assign) CGFloat browValue;
@property (nonatomic, assign) NSInteger lipstickType;
@property (nonatomic, assign) NSInteger blushType;
@property (nonatomic, assign) NSInteger lashType;
@property (nonatomic, assign) NSInteger eyeShadowType;
@property (nonatomic, assign) NSInteger eyeLinesType;
@property (nonatomic, assign) NSInteger browType;




- (instancetype)initWithDelegate:(id<RTCBeautyFilterDelegate>)delegate;
- (void)releaseInstance;

- (void)processVideoFrame:(CVPixelBufferRef)imageBuffer rotation:(NSInteger) rotation;

/// Same as above, but drives time-based effects/animated stickers by an explicit
/// media time (seconds, typically the frame PTS). Pass a negative value to use the
/// wall clock (live-preview default). Used by non-realtime video export so effect
/// animation speed matches the content instead of the export throughput.
- (void)processVideoFrame:(CVPixelBufferRef)imageBuffer
                 rotation:(NSInteger)rotation
                mediaTime:(double)mediaTime;

- (void)setSpecialEffectsSink:(NSInteger )index;
- (void)setLookupPath:(NSString *)path level:(CGFloat)level;
- (void)setStikerPath:(NSString *)path;

- (void)setStyleBeautyIndex:(NSInteger )index;
- (void)setStyleBeautyLevel:(CGFloat )Level;
- (void)setStyleBeautyIntensity:(CGFloat )Intensityl;

+ (void)checkString:(NSString *)key completion:(void (^)(BOOL auth))completion;

+ (BOOL)checkString:(NSString *)key name:(NSString *)name;

// ===== 两级鉴权查询（消费证书授权粒度）=====
// 入参为对应 proto 枚举的整型值 / 资源 id；整体未验签时一律返回 NO。
/// 整体本地验签是否通过。
+ (BOOL)isVerifySuccess;
/// 授权版本号：验签成功时自增，供上层做授权快照失效。
+ (NSUInteger)licenseEpoch;
/// tab 级：证书存在该 ModuleType 且启用。
+ (BOOL)isModuleEnabled:(NSInteger)moduleType;
/// item 级逐项判定。
+ (BOOL)isFaceTypeAuthorized:(NSInteger)faceType;
+ (BOOL)isStyleTypeAuthorized:(NSInteger)styleType;
+ (BOOL)isMakeupMainTypeAuthorized:(NSInteger)mainType;
+ (BOOL)isMakeupSubTypeAuthorized:(NSInteger)mainType subType:(NSInteger)subType;
+ (BOOL)isEffectsTypeAuthorized:(NSInteger)effectType;
+ (BOOL)isColorTypeAuthorized:(NSInteger)colorType;
+ (BOOL)isFilterAuthorized:(NSString *)filterId;
+ (BOOL)isStickerAuthorized:(NSString *)stickerId;
+ (BOOL)isVirtualBlurAuthorized;
+ (BOOL)isVirtualCustomAuthorized;
/// 运行时开关鉴权调试日志（默认关）。开启后 Is*Authorized 查询以【AUTH】前缀
/// 打印返回值，并在开启当下 dump 一次证书授权结构（【AUTH-PB】）。
+ (void)setAuthDebugLog:(BOOL)enabled;

// Room Effect Parameters
- (void)setFlipXFlipFlag:(BOOL)flag;
- (void)setToonThreshold:(CGFloat)threshold;
- (void)setSmoothToonThreshold:(CGFloat)threshold;
- (void)setSmoothToonBlurRadius:(CGFloat)blurRadius;

// MARK: - Raw Pixel Buffer Access (Read-Only)
/// Returns the current RGBA frame buffer, if available.
/// @note The caller does NOT own the returned pointer; it is owned by `SinkRawData` and is only
///       valid until the next call to -processVideoFrame:rotation:.
/// @note On iOS, `rgba_buffer_` is currently not populated by the render path and will
///       contain all-zero contents. Use I420 or CVPixelBuffer delegate paths for real data.
///       This is a known limitation; the data will remain empty until the C++ render path
///       is fixed in a follow-up change.
- (const uint8_t * _Nullable)rgbaBuffer NS_SWIFT_NAME(rgbaBuffer());

/// Returns the current I420 frame buffer, if available.
/// @note The caller does NOT own the returned pointer; it is owned by `SinkRawData` and is only
///       valid until the next call to -processVideoFrame:rotation:.
- (const uint8_t * _Nullable)i420Buffer NS_SWIFT_NAME(i420Buffer());

/// Width of the last processed frame, or 0 if no frame has been processed yet.
- (NSInteger)frameWidth;

/// Height of the last processed frame, or 0 if no frame has been processed yet.
- (NSInteger)frameHeight;

@end
