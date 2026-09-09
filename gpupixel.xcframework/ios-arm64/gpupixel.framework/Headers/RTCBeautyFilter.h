

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

@protocol RTCBeautyFilterDelegate <NSObject>

/// 远端/主输出：BASE 上按原始关键点绘制贴纸（未镜像，可读），NV12。
- (void)didReceivePixelBuffer:(CVPixelBufferRef)pixelBuffer
                        width:(int)width
                       height:(int)height;

@optional
/// 本端自视图输出：镜像 BASE 上按镜像关键点绘制贴纸（自拍朝向 + 贴纸可读），NV12。
/// 仅当 setStickerDualPresent:YES 时才会回调；否则本端应复用主输出。
- (void)didReceiveLocalPixelBuffer:(CVPixelBufferRef)pixelBuffer
                             width:(int)width
                            height:(int)height;

/// 零拷贝主输出：**IOSurface 背书的 32BGRA**，即滤镜链的渲染目标本身。
///
/// 实现本方法即选择零拷贝路径，此时 `didReceivePixelBuffer:` 不再回调。二者互斥：
/// NV12 路径需要 `ARGBToI420` + `I420ToNV12` 两趟 CPU 转换，而链路本来就渲染在这块
/// IOSurface 上，交出它无需任何像素搬运。
///
/// 调用方可用 `CVMetalTextureCache` 从同一块内存绑出 `MTLTexture`，或直接投给只吃
/// `CVPixelBuffer` 的第三方 SDK（声网等）。
///
/// 生命周期分两种情形：
/// - 未用 `-setNextOutputPixelBuffer:` 指定输出时，缓冲来自 SDK 内部深度 3 的环形池，
///   仅在本回调内有效；持有超过 2 帧会看到正在被写入的那一块——需要留存请
///   `CVPixelBufferRetain`，并保证不超过该窗口。
/// - 指定过输出时，回调交回的就是你给的那块缓冲，生命周期归你，无持有窗口约束。
///
/// 同步：回调前已 `glFlush()`。iOS 没有 EAGL↔Metal fence，这是跨 API 消费能拿到的
/// 最强保证；若观察到撕裂，消费侧需自行加同步。
- (void)didReceiveBGRAPixelBuffer:(CVPixelBufferRef)pixelBuffer
                            width:(int)width
                           height:(int)height;

@end

@interface RTCBeautyFilter : NSObject

@property (nonatomic, weak) id<RTCBeautyFilterDelegate> delegate;

/// 是否**选用**零拷贝 BGRA 输出。默认 `NO`。
///
/// 为什么需要这个开关，而不是「delegate 实现了那个方法就启用」：
/// `respondsToSelector:` 在方法**存在**时即为真，而 delegate 往往是一个长期存在的
/// 包装对象（如 Swift 侧的 `FiterHandle`）。一旦它实现了该方法，零拷贝就永久开启，
/// NV12 回调对所有既有消费方同时停止——预览、导出、推流会一起黑掉，且无法回滚。
///
/// 拆成显式开关后，两条输出路径可在迁移期共存：按需切换、可 A/B、可回退。
///
/// 仅当本属性为 `YES` **且** delegate 实现了 `-didReceiveBGRAPixelBuffer:width:height:`
/// 时才走零拷贝路径（此时 NV12 回读不再挂载）。
@property (nonatomic, assign) BOOL zeroCopyOutputPreferred;

/// 调用方是关键点的**唯一权威**。默认 `NO`。
///
/// 置 `YES` 后，注入 API 收到空关键点即当作「本帧没有人脸」，SDK **不再自检**，
/// 也不再为取关键点做整屏回读。
///
/// 为什么必须能关掉自检，而不是「没给就自己检一下」这种看起来更友好的回退：
///
/// 1. Mars/MNN 的 session 由一把**进程级互斥锁**保护。调用方把检测搬到自己的队列
///    异步跑之后，链内这次自检会阻塞在那把锁上——本来要消除的串行化点原地复活，
///    还多付一次推理。
/// 2. 两个检测器实例各有各的跟踪时序与朝向锁。交替喂帧会让两边的时序都失准，
///    表现为关键点在两套结论之间跳。
///
/// 置 `YES` 的调用方须**每帧**注入（无脸时传空），否则效果会停在无脸状态。
@property (nonatomic, assign) BOOL externalLandmarksAuthoritative;

/// 本帧关键点的可信度门控系数 `[0,1]`。默认 `1`，即不衰减。
///
/// 每帧在 `process…` 之前设置，随该帧的关键点一起生效。
///
/// ## 只作用于对关键点错位敏感的那两级
///
/// - **受门控**：贴纸（硬边几何——偏几个像素就整体脱离脸部）、
///   唇彩/眼影/眼线/腮红/眉/睫毛/瞳孔（精细区域——涂出边界）。
/// - **不受门控**：瘦脸/大眼等 mesh 变形（错位只表现为形变量略有偏差）、
///   磨皮/美白/LUT/调色（不依赖关键点或只依赖粗区域）。
///
/// 对所有效果施加同一策略是错的：磨皮跟着一起淡出，代价远大于收益。
///
/// ## 它不是模型给的置信度
///
/// 底层检测器不输出置信度（`RTCFaceLandmarkResult` 没有该字段）。这个系数应由**调用方**
/// 按自己知道的东西算：坐标是新检出的还是外推的、外推了多久、人脸整体位移多快、
/// 是否已进入「检不到」的容忍窗。SDK 不猜这些——调度策略在调用方手里，
/// 只有它知道当前这批坐标有多新。
///
/// ## 系数不改任何用户可见的值
///
/// 它在下发 uniform 时与妆容 `blend_level_` / 贴纸 `layerBlend` 相乘，
/// 两者本身一动不动。否则淡出会把面板滑块拽着一起动，且恢复时无从知道该回到多少。
@property (nonatomic, assign) float landmarkTrust;

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

/// 指定**下一次**处理的输出缓冲：滤镜链直接渲染进这块内存，不再经 SDK 内部环形池。
///
/// 解决的是所有权而非拷贝：内部环形池只有 3 块并按帧轮转，交出去的缓冲隔两帧即被覆写，
/// 想持有更久（送进 `AVSampleBufferDisplayLayer`、异步合成、编码器）就必须先拷一份。
/// 由调用方提供输出帧后，回收时机变成纯 ARC，那条持有窗口约束整个消失。
///
/// - 一次性：每次 `-processVideoFrame:` 前都要重新指定，处理一次即失效。未指定则回退环形池。
/// - 须是 IOSurface 背书、尺寸与输出一致的 `kCVPixelFormatType_32BGRA` 缓冲；
///   不满足则本帧静默回退环形池（`-didReceiveBGRAPixelBuffer:` 交回的将不是你这一块，
///   按缓冲身份即可判别）。
/// - 传 `NULL` 撤销尚未消费的指定。
/// - SDK 会持有到下一次处理为止，故调用方的池需为此多留一帧余量。
- (void)setNextOutputPixelBuffer:(nullable CVPixelBufferRef)pixelBuffer;

/// Process one video frame for beauty + face landmarks.
///
/// Output pixels keep the **input buffer orientation** (video is not rotated).
/// Face detect may temporarily rotate a BGRA copy when the face is sideways
/// (e.g. portrait .mov stored as 1920x1080); landmarks are mapped back into
/// the source coordinate space before filters run.
///
/// `rotation` (0/90/180/270) is optional host hint / display metadata
/// (`AVAssetTrack.preferredTransform`); it is stored on `self.rotation` for
/// the host and does not rotate the processed output.
///
/// `mediaTime` (seconds, typically PTS) drives time-based effects/stickers.
/// Pass a negative value to use the wall clock (live-preview default).
///
/// Supported input formats: 32BGRA, NV12 / 420f / 420v (biplanar).
- (void)processVideoFrame:(CVPixelBufferRef)imageBuffer
                 rotation:(NSInteger)rotation
                mediaTime:(double)mediaTime;

/// GLES 纹理输入（iOS 仅 `textureType=0` → `GL_TEXTURE_2D`）。
/// 须在宿主 **EAGL current** 线程调用；GPUPixel 会加入当前 `sharegroup` 以便采样该纹理。
///
/// `landmarks`：可选，归一化 `[0,1]` 交错 `x,y`（与纹理像素坐标系一致）；
/// `landmarkCount` 为 float 个数。传 `NULL`/`0` 时：
/// `externalLandmarksAuthoritative == NO` 会读回纹理做检测（有 CPU/`glReadPixels` 开销），
/// 为 `YES` 则直接按「无人脸」处理，不回读也不检测。
///
/// `usedRot`：产出这批关键点时检测器实际所用的旋转角（0/90/180/270）。
/// 它**不能**烘进坐标——下游只有一个用途，即贴纸精灵是否需要 Y 翻转，
/// 而那个判定依赖角度本身。自检时由 SDK 内部得出；注入时须由调用方给。
///
/// `rotation` / `mediaTime` 语义与 `-processVideoFrame:rotation:mediaTime:` 相同。
- (BOOL)processTexture:(unsigned int)textureId
                 width:(int)width
                height:(int)height
           textureType:(int)textureType
              rotation:(NSInteger)rotation
             mediaTime:(double)mediaTime
             landmarks:(nullable const float *)landmarks
         landmarkCount:(NSUInteger)landmarkCount
               usedRot:(NSInteger)usedRot;

/// 在 GPUPixel 自己的 EAGL Context 上从 BGRA 上传 `GL_TEXTURE_2D` 再处理。
/// `pixelBuffer` 须为 `kCVPixelFormatType_32BGRA`。
/// `landmarks` / `usedRot` 语义同 `-processTexture:...`。
- (BOOL)processBGRAPixelBufferAsTexture:(CVPixelBufferRef)pixelBuffer
                               rotation:(NSInteger)rotation
                              mediaTime:(double)mediaTime
                              landmarks:(nullable const float *)landmarks
                          landmarkCount:(NSUInteger)landmarkCount
                                usedRot:(NSInteger)usedRot;

/// NV12 双平面零拷贝输入。`pixelBuffer` 须为 `420f` / `420v` 且 IOSurface 背书。
///
/// 与 `-processBGRAPixelBufferAsTexture:` 的区别只在入口：那条路要求调用方先把 NV12
/// 转成 BGRA，而在 iOS 上那次转换只能用 Metal，于是每帧要跨 Metal↔GL 边界两次。
/// 这两套驱动之间没有可表达的 GPU 侧依赖，中间那次跨界只能靠 CPU 阻塞来排序。
/// 本入口把 Y / UV 两个平面直接绑成 GL 纹理、在 GL 里合成 RGBA，跨界降到一次，
/// 那个阻塞随之消失。
///
/// `ycbcrParameters`：**8 个** float，前 4 个是反变换系数（R←Cr, G←Cb, G←Cr, B←Cb），
/// 后 4 个是 scale/bias（亮度 scale, 亮度 bias, 色度 scale, 色度 bias）。
/// 布局与 Kit 侧 `YCbCrToRGBParameters`（两个 `SIMD4<Float>`）逐字节一致，
/// 调用方应直接传那个结构体的字节而不是另算一份。
///
/// 为什么这些数不在 SDK 内部算：同一进程里另有一条 Metal 的 YCbCr→RGB 实现，
/// 两处各写一份常数的话，改了一边忘了另一边的表现是往返后整体偏色，而那种色偏
/// 无从定位。矩阵的选择还依赖 attachment 与分辨率启发式，那些信息在调用方手里。
///
/// **要求 `externalLandmarksAuthoritative == YES`**，否则返回 `NO`。自检需要 BGRA
/// 像素，在此补上就等于做一次 NV12→BGRA 的 CPU 转换，正是本入口要消除的东西。
///
/// `rotation` / `mediaTime` / `landmarks` / `usedRot` 语义同 `-processTexture:...`。
- (BOOL)processNV12PixelBufferAsTexture:(CVPixelBufferRef)pixelBuffer
                               rotation:(NSInteger)rotation
                              mediaTime:(double)mediaTime
                        ycbcrParameters:(const float *)ycbcrParameters
                              landmarks:(nullable const float *)landmarks
                          landmarkCount:(NSUInteger)landmarkCount
                                usedRot:(NSInteger)usedRot;

- (void)setSpecialEffectsSink:(NSInteger )index;
- (void)setLookupPath:(NSString *)path level:(CGFloat)level;
- (void)setStikerPath:(NSString *)path;

/// 开/关「双输出」：开启后除主输出外，额外产出本端镜像输出（didReceiveLocalPixelBuffer）。
/// 默认关闭；关闭时严格等价旧单路径行为（无第二次绘制/回读开销）。
/// 由上层在「前置 && 贴纸激活 && 本端镜像开」三条件同时满足时开启。
- (void)setStickerDualPresent:(BOOL)enabled;

/// 主贴纸「镜像补偿」：对主输出贴纸精灵的最终水平翻转结论取反（透传给主 stiker 的
/// SetInvertFlipTextureXEnabled）。用于「变换式镜像」宿主（LanvizeBeautyKit 编辑器）——
/// 引擎产出未镜像像素，输出侧（预览层 / EXIF / 导出容器 transform）对整帧做几何翻转，
/// 精灵需预翻转一次才能在翻转后保持文字可读。默认 NO。
/// 仅作用于贴纸精灵朝向，与美颜参数正交；实时音视频（QuickVO）不调用此入口，
/// 主输出/远端保持未镜像可读。宿主退出时须复位为 NO，避免共享引擎单例状态串台。
- (void)setStickerMirrorCompensation:(BOOL)on;

- (void)setStyleBeautyIndex:(NSInteger )index;
- (void)setStyleBeautyLevel:(CGFloat )Level;
- (void)setStyleBeautyIntensity:(CGFloat )Intensityl;

/// 在线复核。completion 回传具体 certificateStatus 码（1=通过 8=服务端判否
/// 2=解析失败 9=本地模式 -1=网络失败 0=未知），供上层按 definitive/transient 分流。
+ (void)checkString:(NSString *)key completion:(void (^)(NSInteger status))completion;

+ (BOOL)checkString:(NSString *)key name:(NSString *)name;

// ===== 两级鉴权查询（消费证书授权粒度）=====
// 入参为对应 proto 枚举的整型值 / 资源 id；整体未验签时一律返回 NO。
/// 整体本地验签是否通过。
+ (BOOL)isVerifySuccess;
/// 授权版本号：验签成功时自增，供上层做授权快照失效。
+ (NSUInteger)licenseEpoch;
/// 证书是否为在线验证模式（verifyMode == "online"）。仅本地验签通过后有效，
/// 否则返回 NO。上层据此决定 create 后是否需要立即联网复核。
+ (BOOL)isOnlineVerifyMode;
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

/// 注册授权状态变更回调（C++ 真值源 isVerifySuccess_ 每次变化时触发）。
/// verified=YES 已授权，NO 已撤销。回调可能来自非主线程，上层需自行派发到主线程。
/// 传 nil 注销。
+ (void)setAuthStatusChangedHandler:(void (^)(BOOL verified))handler;

// Room Effect Parameters
- (void)setFlipXFlipFlag:(BOOL)flag;
- (void)setFlipXFlipYFlag:(BOOL)flag;
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
