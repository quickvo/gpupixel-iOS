# GPUPixel iOS SDK 接入文档

`gpupixel.xcframework` 是一套基于 GPU 的实时视频美颜 / 滤镜 / 特效 SDK，提供人脸美颜、美型、美妆、风格滤镜、虚拟背景、动态贴纸、画质调节等能力。SDK 以 CVPixelBuffer 为输入输出单元，可无缝对接摄像头采集、RTC 推流、本地渲染等场景。

---

## 一、环境要求

| 项目 | 要求 |
| --- | --- |
| 平台 | iOS 15.0 及以上 |
| 架构 | `arm64`（**仅真机**，当前未提供模拟器切片） |
| 语言 | Swift 5 / Objective-C 混编，Swift 工具链 ≥ 5.9 |
| 依赖框架 | `Foundation`、`CoreVideo`、`UIKit`、`Combine`、`Metal/OpenGLES`（系统自带） |
| 相机权限 | `Info.plist` 中需配置 `NSCameraUsageDescription` |

> ⚠️ 由于当前 xcframework 只包含 `ios-arm64` 切片，无法在 iOS 模拟器上运行，请使用真机调试。

---

## 二、集成方式

### 方式一：Swift Package Manager（推荐）

SDK 根目录已提供 `Package.swift`，以本地二进制目标形式引入：

```swift
// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "gpupixel",
    platforms: [.iOS(.v15)],
    products: [
        .library(name: "gpupixel", targets: ["gpupixel"]),
    ],
    targets: [
        .binaryTarget(name: "gpupixel", path: "gpupixel.xcframework"),
    ],
    swiftLanguageVersions: [.v5]
)
```

在你的工程中：`File → Add Package Dependencies… → Add Local…`，选择包含 `Package.swift` 的目录即可；或在自己工程的 `Package.swift` 中通过 `.package(path:)` 引用。

### 方式二：手动拖入 xcframework

1. 将 `gpupixel.xcframework` 拖入 Xcode 工程。
2. 在 `Target → General → Frameworks, Libraries, and Embedded Content` 中，将其设置为 **Embed & Sign**。
3. `Build Settings` 中确认 `Enable Bitcode = No`。

### 资源文件说明

framework 内已内置运行所需资源，无需额外拷贝：

- `models/`：人脸检测与关键点对齐模型（`face_det.mars_model`、`face_align.mars_model`）。
- `res/`：美妆 / 贴纸 / 滤镜等内置素材，以及网络请求所需的 `ca-bundle.crt` 根证书。

如果你使用的是**手动裁剪**或自定义资源目录，可通过 C++ 接口指定资源根路径：

```cpp
gpupixel::GPUPixel::SetResourcePath("/path/to/res");
```

---

## 三、鉴权（License 授权）

SDK 在使用前需要完成一次**异步授权**。授权基于 `key` 与 `appID`，通过 `FilterEngine.create(key:appID:)` 发起，成功后 `FilterEngine.auth` 返回 `true`。

```swift
import gpupixel

// App 启动或进入美颜页面前调用一次即可
Task {
    let ok = await FilterEngine.create(key: "你的License Key",
                                       appID: "你的AppID")
    if ok {
        print("GPUPixel 授权成功")
    } else {
        print("GPUPixel 授权失败，请检查 key / appID / 网络")
    }
}

// 任意时刻查询授权状态
if FilterEngine.auth {
    // 已授权
}
```

可选的全局开关：

```swift
FilterEngine.logEnabled = true            // 打开 SDK 日志
FilterEngine.networkLoggingEnabled = true // 打开网络请求日志
```

---

## 四、快速开始（Swift 推荐流程）

推荐使用 `FilterEngine` 现代 API。核心对象：

- `FilterEngine.engine`：全局单例引擎。
- `engine.filterHandle`（`FiterHandle`）：负责逐帧处理与结果回调。
- `engine.filter`（`RoomVideoFilterConfig`）：所有滤镜参数的配置入口。

### 1. 初始化与结果回调

```swift
import gpupixel
import CoreVideo

final class BeautyManager {
    private let engine = FilterEngine.engine

    func setup() {
        // 处理结果通过闭包回调（已美颜的 CVPixelBuffer + 旋转方向）
        engine.filterHandle.handleBuffer = { [weak self] (buffer, rotation) in
            // buffer: 处理后的 CVPixelBuffer，可用于渲染 / 推流
            self?.render(buffer)
        }

        // 可选：预热，减少首帧卡顿
        engine.filterHandle.warmup()
    }

    private func render(_ buffer: CVPixelBuffer) {
        // 交给你的渲染层 / RTC SDK
    }
}
```

### 2. 逐帧处理摄像头数据

在 `AVCaptureVideoDataOutputSampleBufferDelegate` 中，把每一帧的 `CVPixelBuffer` 交给 SDK：

```swift
func captureOutput(_ output: AVCaptureOutput,
                   didOutput sampleBuffer: CMSampleBuffer,
                   from connection: AVCaptureConnection) {
    guard let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }

    // rotaion 传 nil 时使用 currentRotation
    engine.filterHandle.process(buffer: pixelBuffer, rotaion: .rotation0)
}
```

### 3. 设置画面旋转

```swift
// 根据设备方向 / 摄像头方向设置
engine.filterHandle.updateRotation(.rotation90)
// 或
engine.filterHandle.currentRotation = .rotation90
```

`FilterVideoRotation` 取值：`.rotation0` / `.rotation90` / `.rotation180` / `.rotation270`。

### 4. 获取裸数据（可选）

若渲染层需要裸数据而非 CVPixelBuffer：

```swift
if let size = engine.filterHandle.frameSize() {
    let w = size.width, h = size.height
    if let i420 = engine.filterHandle.i420Buffer() {
        // i420 数据，长度 = w * h * 3 / 2
    }
    if let rgba = engine.filterHandle.rgbaBuffer() {
        // rgba 数据（iOS 上 RGBA 路径可能为空，推荐使用 I420 / CVPixelBuffer 回调）
    }
}
```

> ⚠️ 裸数据指针由 SDK 内部持有，**仅在下一帧处理前有效**，请勿缓存或释放该指针。

---

## 五、滤镜配置总览（`RoomVideoFilterConfig`）

通过 `FilterEngine.engine.filter` 访问，包含以下九大子滤镜模块。每个子模块都实现 `RoomVideoFilterEnable` 协议，具有统一的 `enable`（开关）、`reset()`（重置）与 `event`（Combine 变更事件流）。

| 属性 | 类型 | 作用 |
| --- | --- | --- |
| `face` | `RoomFaceFilter` | 基础美颜与美型（磨皮、美白、瘦脸、大眼、瘦鼻等） |
| `beauty` | `RoomBeautyFilter` | 美妆（口红、腮红、眼线、眼影、睫毛、眉毛）及其风格 |
| `styleBeauty` | `RoomStyleBeautyFilter` | 一键风格美颜（甜美、气质、白皙、烟熏等） |
| `style` | `RoomStyleFilter` | 风格滤镜（按素材路径加载） |
| `styleEffect` | `RoomStyleEffectFilter` | 云端风格特效素材（支持异步下载与应用） |
| `effect` | `RoomEffectFilter` | 动态特效（灵魂出窍、抖动、故障、马赛克、卡通等） |
| `quality` | `RoomQulityFilter` | 画质调节（色温、色调、曝光、饱和度、对比度等） |
| `virtualBackground` | `RoomVirtualBackFilter` | 虚拟背景（图片 / 模糊 / 关闭） |
| `stiker` | `RoomStickerFilter` | 动态贴纸（按素材路径加载） |

统一操作示例：

```swift
let filter = FilterEngine.engine.filter

filter.face.enable = true       // 开启某模块
filter.face.reset()             // 重置该模块参数
filter.clear()                  // 清空全部滤镜
filter.reset()                  // 重置全部滤镜到初始值
```

### 5.1 基础美颜 / 美型（`RoomFaceFilter`）

以 `subscript(key:)` 读写，值为 `Float`，取值范围由 `key.range` 提供，可用 `normalizedValue(from:)` / `rawValue(fromNormalized:)` 在归一化值与原始值之间转换。

```swift
let face = FilterEngine.engine.filter.face
face.enable = true
face[.buffing] = 0.6   // 磨皮
face[.white]   = 0.4   // 美白
face[.thin]    = 0.3   // 瘦脸
face[.eye]     = 0.2   // 大眼
```

可用 `Key`：

| Key | 含义 | Key | 含义 |
| --- | --- | --- | --- |
| `.buffing` | 磨皮 | `.thinNose` | 瘦鼻 |
| `.white` | 美白 | `.thinChin` | 下巴 |
| `.thin` | 瘦脸 | `.faceShave` | 削脸 |
| `.eye` | 大眼 | `.adjustMouth` | 嘴型 |
| `.sharpen` | 锐化 | `.nosePosition` | 鼻子位置 |
| `.ruddy` | 红润 | `.eyesDistance` | 眼距 |
| `.whitenTeeth` | 美牙 | | |

### 5.2 美妆（`RoomBeautyFilter`）

美妆包含「强度」与「风格」两个维度。强度通过 `subscript(key:)` 设置，风格通过独立属性设置：

```swift
let beauty = FilterEngine.engine.filter.beauty
beauty.enable = true

// 强度（Key: .lipstick / .blush / .eyeLines / .eyeShadow / .lash / .brow）
beauty[.lipstick] = 0.8

// 风格
beauty.lipstickStyle = .velvetRed   // BeautyLipstickStyle
beauty.blushStyle    = .peach       // BeautyBlushStyle
beauty.browStyle     = .natural     // BeautyBrowStyle
beauty.eyeLinesStyle = .elegant     // BeautyEyelinesStyle
beauty.eyeShadowStyle = .mochaBrown // BeautyEyeshadowStyle
beauty.lashStyle     = .curled      // BeautyLashStyle
```

各风格枚举可选值：

| 部位 | 枚举 | 可选值 |
| --- | --- | --- |
| 口红 | `BeautyLipstickStyle` | none, roseBean, coral, velvetRed, sweetOrange, rustRed |
| 腮红 | `BeautyBlushStyle` | none, apricotPink, milkOrange, peach, tipsy, sweetOrange |
| 眉毛 | `BeautyBrowStyle` | none, natural, thickFlat, downTail, distant, sword, feather |
| 眼线 | `BeautyEyelinesStyle` | none, natural, playful, sly, elegant, wild, shadowplume |
| 眼影 | `BeautyEyeshadowStyle` | none, mistPink, mochaBrown, glowHoney, warmTea, energyOrange, violet, delicateRose |
| 睫毛 | `BeautyLashStyle` | none, natural, gentle, curled, long, dense |

### 5.3 一键风格美颜（`RoomStyleBeautyFilter`）

```swift
let styleBeauty = FilterEngine.engine.filter.styleBeauty
styleBeauty.enable = true
styleBeauty.value = .sweet          // RoomStyleBeautyCase
styleBeauty[.sweet, .value] = 0.7   // 调节该风格强度
```

`RoomStyleBeautyCase` 可选：`none, sweet, temperament, fairSkinned, flicker, paleColored, smokyEyes, Charm`。

### 5.4 画质调节（`RoomQulityFilter`）

```swift
let quality = FilterEngine.engine.filter.quality
quality.enable = true
quality[.temperature] = 0.5
quality[.contrast]    = 0.2
```

可用 `Key`：`temperature`（色温）、`hue`（色调）、`exposure`（曝光）、`saturation`（饱和度）、`contrast`（对比度）、`brightness`（亮度）、`shadow`（阴影）、`highlights`（高光）、`vignette`（暗角）。

### 5.5 动态特效（`RoomEffectFilter`）

```swift
let effect = FilterEngine.engine.filter.effect
effect.enable = true
effect.value = .SoulOut     // RoomEffectFilterCase

// 部分特效支持参数微调
effect.setParam(key: .threshold, value: 0.5)
let v = effect.getParam(key: .threshold)
```

`RoomEffectFilterCase` 可选：`none, Flip, SoulOut, Shake, Shine, Glitch, Scale, ZoomBlur, Water, Mosaic, Sketch, Toon, SmoothToon, EdgeDetection, Screen3, Screen4, Screen6, Screen9, Sepia`。

`RoomEffectParamKey` 可选：`flipFlag, threshold, toonThreshold, blurRadius`（各参数适用的特效见 `key.supportedCases`）。

### 5.6 风格滤镜 / 贴纸（按路径）

```swift
// 风格滤镜
FilterEngine.engine.filter.style.value = .path(path: "/path/to/lookup.png")
// 关闭
FilterEngine.engine.filter.style.value = .none

// 动态贴纸
FilterEngine.engine.filter.stiker.value = .path(path: "/path/to/sticker_dir")
FilterEngine.engine.filter.stiker.value = .none
```

### 5.7 虚拟背景（`RoomVirtualBackFilter`）

```swift
let vb = FilterEngine.engine.filter.virtualBackground
vb.enable = true
vb.value = .blur                       // 背景虚化
vb.value = .img(UIImage(named: "bg"))  // 自定义背景图
vb.value = .none                       // 关闭
```

---

## 六、云端素材下载

云端贴纸 / 滤镜素材通过 `MaterialRepository`（`FilterEngine.engine.materialRepository`）或风格特效的 `RoomStyleEffectFilter` 管理，支持分页拉取、下载进度、缓存。

```swift
guard let repo = FilterEngine.engine.materialRepository else { return }

Task {
    // 拉取贴纸 / 滤镜列表（分类：.paster / .filter）
    await repo.refresh(category: .paster)

    // 监听素材列表变化（Combine）
    let cancellable = repo.materialsPublisher.sink { materials in
        // materials: [CachedBeautyMaterial]
    }

    // 下载指定素材（带进度）
    do {
        let localPath = try await repo.download(id: "materialId") { progress in
            print("下载进度：\(progress)")
        }
        // 下载完成后按路径应用（如贴纸）
        FilterEngine.engine.filter.stiker.value = .path(path: localPath)
    } catch {
        print("下载失败：\(error)")
    }
}
```

风格特效素材（`RoomStyleEffectFilter`）提供更高层封装：

```swift
let styleEffect = FilterEngine.engine.filter.styleEffect
styleEffect.enable = true

// 可用云端风格
let styles = styleEffect.availableStyles  // [CachedBeautyMaterial]

Task {
    // 一键下载并应用
    let localPath = try await styleEffect.applyStyle(id: "styleId")
    // 查询状态
    let state = styleEffect.styleState(for: "styleId")  // UnifiedStickerState
}
```

`UnifiedStickerState`：`.notDownloaded` / `.downloading(progress:)` / `.downloaded(localPath:)` / `.failed(message:)`。

---

## 七、监听滤镜变更事件

每个子滤镜都暴露 `event: AnyPublisher<RoomFilterEvent, Never>`，可用于 UI 状态同步：

```swift
import Combine

var bag = Set<AnyCancellable>()

FilterEngine.engine.filter.face.event
    .sink { event in
        // RoomFilterEvent: .faceChange / .beautyChange / .styleChange / .effectChange ...
        print("滤镜变更：\(event)")
    }
    .store(in: &bag)
```

---

## 八、Objective-C 经典 API（`RTCBeautyFilter`）

若不使用 `FilterEngine`，也可直接使用底层的 `RTCBeautyFilter`。它以 delegate 回调返回处理后的 `CVPixelBuffer`。

```objc
#import <gpupixel/gpupixel.h>

@interface CameraController () <RTCBeautyFilterDelegate>
@property (nonatomic, strong) RTCBeautyFilter *beautyFilter;
@end

@implementation CameraController

- (void)setupBeauty {
    self.beautyFilter = [[RTCBeautyFilter alloc] initWithDelegate:self];

    // 美颜参数
    self.beautyFilter.beautyValue    = 0.6;  // 磨皮
    self.beautyFilter.whithValue     = 0.4;  // 美白
    self.beautyFilter.thinFaceValue  = 0.3;  // 瘦脸
    self.beautyFilter.eyeValue       = 0.2;  // 大眼
    self.beautyFilter.sharpenValue   = 0.2;  // 锐化

    // 画质
    self.beautyFilter.contrastValue   = 0.1;
    self.beautyFilter.saturationValue = 0.1;
    self.beautyFilter.temperature     = 0.0;

    // 美妆（type 选择样式，value 控制强度）
    self.beautyFilter.lipstickType  = 1;
    self.beautyFilter.lipstickValue = 0.8;
}

// 逐帧处理
- (void)onCameraFrame:(CVPixelBufferRef)pixelBuffer {
    [self.beautyFilter processVideoFrame:pixelBuffer rotation:0];
}

// 处理结果回调
- (void)didReceivePixelBuffer:(CVPixelBufferRef)pixelBuffer
                        width:(int)width
                       height:(int)height {
    // 渲染 / 推流
}

- (void)dealloc {
    [self.beautyFilter releaseInstance];
}

@end
```

### 常用属性一览

| 属性 | 含义 | 属性 | 含义 |
| --- | --- | --- | --- |
| `beautyValue` | 磨皮 | `thinNose` | 瘦鼻 |
| `whithValue` | 美白 | `thinChin` | 下巴 |
| `thinFaceValue` | 瘦脸 | `faceShave` | 削脸 |
| `eyeValue` | 大眼 | `adjustMouth` | 嘴型 |
| `sharpenValue` | 锐化 | `eyesDistance` | 眼距 |
| `ruddy` | 红润 | `nosePosition` | 鼻子位置 |
| `contrastValue` | 对比度 | `lipstickValue` / `lipstickType` | 口红强度 / 样式 |
| `exposureValue` | 曝光 | `blusherValue` / `blushType` | 腮红强度 / 样式 |
| `saturationValue` | 饱和度 | `lashValue` / `lashType` | 睫毛强度 / 样式 |
| `hueValue` | 色调 | `whitenTeethValue` | 美牙 |
| `temperature` / `tint` | 色温 / 色调 | `eyeShadowValue` / `eyeShadowType` | 眼影强度 / 样式 |
| `brightValue` | 亮度 | `eyeLinesValue` / `eyeLinesType` | 眼线强度 / 样式 |
| `shadowValue` / `highlightsValue` | 阴影 / 高光 | `browValue` / `browType` | 眉毛强度 / 样式 |
| `vignetValue` | 暗角 | `red` / `green` / `blue` | RGB 微调 |

### 其它方法

```objc
- (void)setSpecialEffectsSink:(NSInteger)index;   // 设置特效
- (void)setLookupPath:(NSString *)path level:(CGFloat)level;  // 加载 lookup 滤镜
- (void)setStikerPath:(NSString *)path;           // 加载贴纸
- (void)setStyleBeautyIndex:(NSInteger)index;     // 风格美颜
- (void)setStyleBeautyLevel:(CGFloat)level;
- (void)setStyleBeautyIntensity:(CGFloat)intensity;
```

---

## 九、注意事项

1. **真机运行**：当前仅提供 `arm64` 真机切片，模拟器无法编译运行。
2. **授权先行**：使用任何滤镜前请确保 `FilterEngine.create(key:appID:)` 已成功、`FilterEngine.auth == true`。
3. **裸数据生命周期**：`rgbaBuffer()` / `i420Buffer()` 返回的指针仅在下一帧处理前有效，切勿缓存或手动释放；iOS 上 RGBA 路径可能为空，实时数据请优先使用 CVPixelBuffer 回调或 I420。
4. **线程**：`process(buffer:)` 建议在相机采集回调线程调用；UI 相关的滤镜参数变更建议回到主线程处理。
5. **归一化取值**：`Room*Filter` 各 `Key` 的合法范围通过 `key.range` 获取，可用 `normalizedValue(from:)` / `rawValue(fromNormalized:)` 做 0~1 归一化与真实值互转，避免硬编码。
6. **资源与网络**：云端素材下载依赖 framework 内置的 `ca-bundle.crt`，请确保 framework 完整嵌入，未破坏 `res/` 目录结构。
7. **版本查询**：C++ 层可通过 `gpupixel::GPUPixel::GetVersion()` 获取版本号（当前 `1.0.0`）。

---

## 十、最简接入清单（Checklist）

- [ ] 通过 SPM / 手动方式引入 `gpupixel.xcframework` 并设为 Embed & Sign
- [ ] `Info.plist` 添加相机权限描述
- [ ] App 启动后调用 `FilterEngine.create(key:appID:)` 完成授权
- [ ] 设置 `filterHandle.handleBuffer` 回调并调用 `warmup()`
- [ ] 相机每帧调用 `filterHandle.process(buffer:rotaion:)`
- [ ] 通过 `engine.filter.xxx` 配置所需美颜 / 滤镜参数
- [ ] 将回调的 CVPixelBuffer 交给渲染 / 推流层
