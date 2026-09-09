//
//  RTCFaceLandmarkDetector.h
//
//  只做人脸关键点检测的独立入口。
//

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// framework 以隐藏可见性构建，不加这个标注的 ObjC 类符号不会被导出，
/// 外部模块能编译（头文件可见）却链接不上。
#if defined(__has_attribute) && __has_attribute(visibility)
#define RTC_FACE_LANDMARK_EXPORT __attribute__((visibility("default")))
#else
#define RTC_FACE_LANDMARK_EXPORT
#endif

/// 一次检测的产出。
RTC_FACE_LANDMARK_EXPORT
@interface RTCFaceLandmarkResult : NSObject

/// 交错 `[x,y,...]`，相对**输入帧**归一化到 `[0,1]`。无脸时为 `NULL` 且 `count == 0`。
@property (nonatomic, readonly, nullable) const float *landmarks NS_RETURNS_INNER_POINTER;
/// float 个数（点数 × 2）。
@property (nonatomic, readonly) NSUInteger count;
/// 检出所用的旋转角（0/90/180/270）。须原样透传给注入 API 的 `usedRot:`——
/// 它的下游是贴纸精灵的 Y 翻转判定，把旋转烘进坐标是不够的。
@property (nonatomic, readonly) NSInteger usedRotation;
/// 未检出人脸。
@property (nonatomic, readonly, getter=isEmpty) BOOL empty;

@end

/// 独立的人脸关键点检测器：**不建滤镜链、不碰 GL、无任何进程级副作用**。
///
/// ## 为什么要它
///
/// `RTCBeautyFilter` 的检测嵌在 `-processVideoFrame:` / `-processBGRAPixelBufferAsTexture:`
/// 内同步执行，处理链必须等它跑完。它是整条链上最贵的串行化点。把检测搬到调用方的
/// 独立队列，链上就只剩「拿最近一次结果」，代价从「等一次推理」降到「读一个指针」。
///
/// 搬走的前提是有一个能单独调用的检测入口——就是本类。
///
/// ## 与滤镜链内那个检测器的关系
///
/// 二者是**不同实例**，时序状态（跟踪、信任度、朝向锁）各自独立。但 Mars/MNN 的
/// session 由一把进程级互斥锁保护，两个实例**不能真正并发**：一方在推理，另一方
/// 就在锁上等。
///
/// 因此使用本类时，必须同时把注入 API 切到「外部关键点权威」模式
/// （`RTCBeautyFilter.externalLandmarksAuthoritative = YES`），让链内检测器彻底不跑。
/// 否则链内那次自检会阻塞在本类持有的锁上——串行化点不但没消除，还多了一次推理。
///
/// ## 线程
///
/// 实例非线程安全：`-detectInPixelBuffer:` 须始终在同一个串行队列上调用。
/// 内部缩放缓冲是 `thread_local` 的，跨线程调用只会多分配几份，不会崩；
/// 但时序状态（朝向锁）会因交错而失准。
RTC_FACE_LANDMARK_EXPORT
@interface RTCFaceLandmarkDetector : NSObject

/// 检测输入的长边上限，超过则先降采样。默认 320。
///
/// 关键点检测不需要全分辨率：720p 下整帧推理与 320 长边推理的点位差异远小于
/// 逐帧抖动本身，而像素量差 16 倍。上限只影响推理输入，输出坐标始终归一化到原图。
@property (nonatomic, assign) NSInteger maxDetectLongSide;

/// 检测一帧。支持 32BGRA 与 NV12 / 420f / 420v 双平面。
///
/// NV12 且当前朝向锁为 0 时走**双平面直读**：先缩 YUV 再只对小图转 RGB，
/// 不产生整帧格式转换。其余情形回退到「转 BGRA（必要时旋转）再检」。
///
/// `hintRotation` 是宿主给的朝向提示（0/90/180/270）。首次检出前会按提示 + 常见角度
/// 依次试检，成功后锁定；锁定角度连续失效则解锁重搜。
///
/// - Returns: 恒非 nil。未检出人脸时返回 `empty == YES` 的结果——
///   「这一帧确实没脸」与「还没检过」是两回事，调用方的调度策略依赖这个区分。
- (RTCFaceLandmarkResult *)detectInPixelBuffer:(CVPixelBufferRef)pixelBuffer
                                  hintRotation:(NSInteger)hintRotation
    NS_SWIFT_NAME(detect(in:hintRotation:));

/// 清掉朝向锁与跟踪时序。分辨率或摄像头切换后调用。
- (void)reset;

@end

NS_ASSUME_NONNULL_END
