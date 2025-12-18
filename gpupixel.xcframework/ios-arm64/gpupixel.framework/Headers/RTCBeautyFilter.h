

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

@protocol RTCBeautyFilterDelegate <NSObject>

- (void)didReceivePixelBuffer:(CVPixelBufferRef)pixelBuffer
                        width:(int)width
                       height:(int)height;

@end

@interface RTCBeautyFilter : NSObject

@property (nonatomic, weak) id<RTCBeautyFilterDelegate> delegate;
@property (nonatomic, assign) CGFloat beautyValue;
@property (nonatomic, assign) CGFloat whithValue;
@property (nonatomic, assign) CGFloat thinFaceValue;
@property (nonatomic, assign) CGFloat eyeValue;
@property (nonatomic, assign) CGFloat lipstickValue;
@property (nonatomic, assign) CGFloat blusherValue;

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

@property(nonatomic, assign) CGFloat effectSweetValue;
@property(nonatomic, assign) CGFloat effectWhiteValue;

@property (nonatomic, assign) CGFloat adjustMouth;
@property (nonatomic, assign) CGFloat thinNose;
@property (nonatomic, assign) CGFloat thinChin;
@property (nonatomic, assign) CGFloat faceShave;
@property (nonatomic, assign) CGFloat eyesDistance;
@property (nonatomic, assign) CGFloat nosePosition;

@property (nonatomic, assign) NSInteger rotation;


- (instancetype)initWithDelegate:(id<RTCBeautyFilterDelegate>)delegate;
- (void)releaseInstance;

- (void)processVideoFrame:(CVPixelBufferRef)imageBuffer rotation:(NSInteger) rotation;

- (void)setSpecialEffectsSink:(NSInteger )index;
- (void)setStyleSink:(NSInteger )index level:(CGFloat )level;
- (void)setStikerPath:(NSString *)path;


+ (void)checkString:(NSString *)key completion:(void (^)(BOOL auth))completion;

+ (BOOL)checkString:(NSString *)key name:(NSString *)name;
@end
