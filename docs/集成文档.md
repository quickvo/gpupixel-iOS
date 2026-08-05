# GPUPixel iOS SDK 接入文档

`gpupixel.xcframework` 是一套基于 GPU 的实时视频美颜 / 滤镜 / 特效 SDK，提供人脸美颜、美型、美妆、风格滤镜、虚拟背景、动态贴纸、画质调节等能力。SDK 以 `CVPixelBuffer` 为输入输出单元，可无缝对接摄像头采集、RTC 推流、本地渲染、离线视频导出等场景。

- **当前版本**：`1.0.0`（`gpupixel::GPUPixel::GetVersion()`）
- **最低系统**：iOS 15.0
- **架构支持**：`arm64`（仅真机）

---

## 目录

- [一、环境要求](#一环境要求)
- [二、架构总览](#二架构总览)
- [三、集成方式](#三集成方式)
- [四、鉴权（License 授权）](#四鉴权license-授权)
- [五、快速开始（Swift 推荐流程）](#五快速开始swift-推荐流程)
- [六、滤镜配置总览（`RoomVideoFilterConfig`）](#六滤镜配置总览roomvideofilterconfig)
- [七、云端素材下载](#七云端素材下载)
- [八、监听滤镜变更事件](#八监听滤镜变更事件)
- [九、细粒度授权门控（UI 灰置）](#九细粒度授权门控ui-灰置)
- [十、Objective-C 经典 API（`RTCBeautyFilter`）](#十objective-c-经典-apirtcbeautyfilter)
- [十一、线程与性能](#十一线程与性能)
- [十二、注意事项](#十二注意事项)
- [十三、常见问题（FAQ）](#十三常见问题faq)
- [十四、最简接入清单（Checklist）](#十四最简接入清单checklist)

---

## 一、环境要求

| 项目 | 要求 |
| --- | --- |
| 平台 | iOS 15.0 及以上 |
| 架构 | `arm64`（**仅真机**，当前未提供模拟器切片） |
| 语言 | Swift 5 / Objective-C 混编，Swift 工具链 ≥ 5.9 |
| 依赖框架 | `Foundation`、`CoreVideo`、`UIKit`、`Combine`、`Metal/OpenGLES`（系统自带） |
| 相机权限 | `Info.plist` 中需配置 `NSCameraUsageDescription` |
| 网络 | 云端素材下载 / 授权校验需要网络访问（HTTPS） |

> ⚠️ 由于当前 xcframework 只包含 `ios-arm64` 切片，无法在 iOS 模拟器上运行，请使用真机调试。

---

## 二、架构总览

SDK 提供**两层 API**，可按需选择：

| 层级 | 入口 | 语言 | 适用场景 |
| --- | --- | --- | --- |
| **现代层（推荐）** | `FilterEngine` | Swift | 新工程、需要 Combine 响应式、云端素材、细粒度授权门控 |
| **经典层（底层）** | `RTCBeautyFilter` | Objective-C | 已有 OC 工程、只需最基础美颜、想直接控制底层滤镜 |

现代层核心对象关系：

```
FilterEngine.engine (全局单例)
├── filterHandle : FiterHandle          // 逐帧处理 + 结果回调（CVPixelBuffer / I420）
├── filter       : RoomVideoFilterConfig // 所有滤镜参数配置入口
│   ├── face             (RoomFaceFilter)         基础美颜 / 美型
│   ├── beauty           (RoomBeautyFilter)       美妆（口红/腮红/眼线...）
│   ├── styleBeauty      (RoomStyleBeautyFilter)  一键风格美颜
│   ├── style            (RoomStyleFilter)        风格滤镜（按路径）
│   ├── styleEffect      (RoomStyleEffectFilter)  云端风格特效
│   ├── effect           (RoomEffectFilter)       动态特效
│   ├── quality          (RoomQulityFilter)       画质调节
│   ├── virtualBackground(RoomVirtualBackFilter)  虚拟背景
│   └── stiker           (RoomStickerFilter)      动态贴纸
└── materialRepository : MaterialRepository?      // 云端素材（贴纸/滤镜）分页 + 下载 + 缓存
```

设计要点：

- **单例引擎**：`FilterEngine.engine` 全局唯一，负责渲染管线与授权状态。
- **数据流**：摄像头帧 → `filterHandle.process(buffer:)` → GPU 管线 → `handleBuffer` 回调输出。
- **配置即状态**：所有滤镜通过 `engine.filter.xxx` 声明式配置，内部状态与渲染管线自动同步。
- **响应式**：每个子滤镜暴露 `event` 发布者，授权状态暴露 `authPublisher`，方便 UI 双向绑定。

---

## 三、集成方式

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
gpupixel::GPUPixel::ReleaseResource();  // 释放已加载资源（进程退出 / 页面销毁时可选调用）
```

---

## 四、鉴权（License 授权）

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

// 任意时刻同步查询授权状态
if FilterEngine.auth {
    // 已授权
}
```

### 响应式监听授权状态

授权是异步的，UI 应监听 `authPublisher` 而非轮询 `auth`：

```swift
import Combine

var bag = Set<AnyCancellable>()

FilterEngine.authPublisher
    .receive(on: DispatchQueue.main)
    .sink { authorized in
        // 根据授权结果刷新美颜面板可用状态
    }
    .store(in: &bag)

// authEpoch 每次验签成功会自增，可用于让上层授权快照失效
let epoch = FilterEngine.authEpoch
```

### 可选的全局开关

```swift
FilterEngine.logEnabled = true            // 打开 SDK 日志
FilterEngine.networkLoggingEnabled = true // 打开网络请求日志
FilterEngine.setAuthDebugLog(true)        // 打开授权调试日志（打印 Is*Authorized 结果）
```

> 授权是使用一切滤镜的前提。建议在**进入美颜页面前**确保授权完成；未授权时相关滤镜不会生效，细粒度门控见[第九节](#九细粒度授权门控ui-灰置)。

---

## 五、快速开始（Swift 推荐流程）

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

`FilterVideoRotation` 取值：`.rotation0` / `.rotation90` / `.rotation180` / `.rotation270`，并提供 `next()` 便捷轮转。

### 4. 离线视频导出（按帧时间驱动动效）

导出本地视频时，动态特效 / 动态贴纸需要按**帧时间**而非采集节奏播放动画。使用带 `mediaTime` 的 `process` 重载，传入帧的 PTS（秒）：

```swift
// mediaTime 传帧的显示时间戳（秒）；传 nil / 负值则使用墙钟（实时预览默认）
engine.filterHandle.process(buffer: pixelBuffer,
                            rotaion: .rotation0,
                            mediaTime: CMTimeGetSeconds(presentationTime))
```

### 5. 获取裸数据（可选）

若渲染层需要裸数据而非 `CVPixelBuffer`：

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

## 六、滤镜配置总览（`RoomVideoFilterConfig`）

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
filter.enable                   // 只读：是否有任意模块处于开启状态
filter.clear()                  // 清空全部滤镜
filter.reset()                  // 重置全部滤镜到初始值
```

### 6.1 基础美颜 / 美型（`RoomFaceFilter`）

以 `subscript(key:)` 读写，值为 `Float`，取值范围由 `key.range` 提供，可用 `normalizedValue(from:)` / `rawValue(fromNormalized:)` 在归一化值与原始值之间转换。

```swift
let face = FilterEngine.engine.filter.face
face.enable = true
face[.buffing] = 0.6   // 磨皮
face[.white]   = 0.4   // 美白
face[.thin]    = 0.3   // 瘦脸
face[.eye]     = 0.2   // 大眼
```

可用 `Key`（`RoomFaceFilter.Key`，遵循 `CaseIterable`，可通过 `allCases` 遍历）：

| Key | 含义 | Key | 含义 |
| --- | --- | --- | --- |
| `.buffing` | 磨皮 | `.thinNose` | 瘦鼻 |
| `.white` | 美白 | `.thinChin` | 下巴 |
| `.thin` | 瘦脸 | `.faceShave` | 削脸 |
| `.eye` | 大眼 | `.adjustMouth` | 嘴型 |
| `.sharpen` | 锐化 | `.nosePosition` | 鼻子位置 |
| `.ruddy` | 红润 | `.eyesDistance` | 眼距 |
| `.whitenTeeth` | 美牙 | | |

### 6.2 美妆（`RoomBeautyFilter`）

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

// 查询某部位当前风格的原始整型索引（用于状态回填 / 授权判断）
let idx = beauty.currentStyleInt(for: .lipstick)
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

### 6.3 一键风格美颜（`RoomStyleBeautyFilter`）

```swift
let styleBeauty = FilterEngine.engine.filter.styleBeauty
styleBeauty.enable = true
styleBeauty.value = .sweet          // RoomStyleBeautyCase
styleBeauty[.sweet, .value] = 0.7   // 调节该风格「强度」

// 每个风格有两个可调维度：.value（美妆强度）与 .filter（滤镜强度）
styleBeauty[.sweet, .filter] = 0.5
let initial = RoomStyleBeautyCase.sweet.getInitial(.value)  // 默认值
```

`RoomStyleBeautyCase` 可选：`none, sweet, temperament, fairSkinned, flicker, paleColored, smokyEyes, Charm`。
`RoomStyleBeautyValueKey` 可选：`.value`（美妆强度）、`.filter`（滤镜强度）。

### 6.4 画质调节（`RoomQulityFilter`）

```swift
let quality = FilterEngine.engine.filter.quality
quality.enable = true
quality[.temperature] = 0.5
quality[.contrast]    = 0.2
```

可用 `Key`：`temperature`（色温）、`tint`（色调）、`hue`（色相）、`exposure`（曝光）、`saturation`（饱和度）、`contrast`（对比度）、`brightness`（亮度）、`shadow`（阴影）、`highlights`（高光）、`vignette`（暗角）。

### 6.5 动态特效（`RoomEffectFilter`）

```swift
let effect = FilterEngine.engine.filter.effect
effect.enable = true
effect.value = .SoulOut     // RoomEffectFilterCase

// 部分特效支持参数微调
effect.setParam(key: .threshold, value: 0.5)
let v = effect.getParam(key: .threshold)
```

`RoomEffectFilterCase` 可选：`none, Flip, SoulOut, Shake, Shine, Glitch, Scale, ZoomBlur, Water, Mosaic, Sketch, Toon, SmoothToon, EdgeDetection, Screen3, Screen4, Screen6, Screen9, Sepia`。

`RoomEffectParamKey` 可选：`flipFlag, threshold, toonThreshold, blurRadius`。每个参数有各自的 `range`、`defaultValue`，并通过 `key.supportedCases` 声明它适用于哪些特效——设置前建议先判断当前 `effect.value` 是否在 `key.supportedCases` 中。

### 6.6 风格滤镜 / 贴纸（按路径）

```swift
// 风格滤镜
FilterEngine.engine.filter.style.value = .path(path: "/path/to/lookup.png")
// 关闭
FilterEngine.engine.filter.style.value = .none

// 动态贴纸
FilterEngine.engine.filter.stiker.value = .path(path: "/path/to/sticker_dir")
FilterEngine.engine.filter.stiker.value = .none
```

### 6.7 虚拟背景（`RoomVirtualBackFilter`）

```swift
let vb = FilterEngine.engine.filter.virtualBackground
vb.enable = true
vb.value = .blur                       // 背景虚化
vb.value = .img(UIImage(named: "bg"))  // 自定义背景图
vb.value = .none                       // 关闭
```

---

## 七、云端素材下载

云端贴纸 / 滤镜素材通过 `MaterialRepository`（`FilterEngine.engine.materialRepository`）或风格特效的 `RoomStyleEffectFilter` 管理，支持分页拉取、下载进度、缓存。

```swift
guard let repo = FilterEngine.engine.materialRepository else { return }

Task {
    // 拉取贴纸 / 滤镜列表（分类：.paster / .filter）
    await repo.refresh(category: .paster)
    // 或分页拉取
    await repo.refresh(category: .paster, page: 1, size: 20)

    // 监听素材列表变化（Combine）
    let cancellable = repo.materialsPublisher.sink { materials in
        // materials: [CachedBeautyMaterial]
    }

    // 监听所有素材下载状态
    let stateCancellable = repo.unifiedStatesPublisher.sink { states in
        // [String: UnifiedStickerState]
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

    // 取消下载
    await repo.cancelDownload(id: "materialId")
    await repo.cancelAllDownloads()
}
```

> `MaterialRepository` 同时是 `ObservableObject`，在 SwiftUI 中可直接 `@ObservedObject` 绑定 `materials` / `unifiedStates` / `currentCategory`。

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
    let state = styleEffect.styleState(for: "styleId")  // UnifiedStickerState?
}
```

`UnifiedStickerState`：`.notDownloaded` / `.downloading(progress:)` / `.downloaded(localPath:)` / `.failed(message:)`。

---

## 八、监听滤镜变更事件

每个子滤镜都暴露 `event: AnyPublisher<RoomFilterEvent, Never>`，可用于 UI 状态同步：

```swift
import Combine

var bag = Set<AnyCancellable>()

FilterEngine.engine.filter.face.event
    .sink { event in
        // RoomFilterEvent 取值见下表
        print("滤镜变更：\(event)")
    }
    .store(in: &bag)
```

`RoomFilterEvent` 取值：`.faceChange`、`.beautyChange`、`.styleChange`、`.styleBeautyChange`、`.styleEffectChange`、`.virtualChange`、`.effectChange`、`.effectParamChange(RoomEffectParamKey)`、`.qualityChange`、`.stikerChange`。

---

## 九、细粒度授权门控（UI 灰置）

License 可以按**模块 / 条目**授权，SDK 提供一组查询接口，便于在 UI 上把未授权的功能置灰或隐藏。整体未验签时一律返回 `false`。

```swift
// 模块（tab）级：BeautyAuthModule = face / makeup / style / filter / sticker / effects / color / virtualBackground
if FilterEngine.isModuleAuthorized(.makeup) {
    // 显示美妆入口
}

// 条目级
FilterEngine.isFaceTypeAuthorized(faceTypeInt)
FilterEngine.isStyleTypeAuthorized(styleTypeInt)
FilterEngine.isMakeupMainTypeAuthorized(mainTypeInt)
FilterEngine.isMakeupSubTypeAuthorized(mainType: mainInt, subType: subInt)
FilterEngine.isEffectsTypeAuthorized(effectTypeInt)
FilterEngine.isColorTypeAuthorized(colorTypeInt)
FilterEngine.isFilterAuthorized("filterId")     // 云端滤镜 id
FilterEngine.isStickerAuthorized("stickerId")   // 云端贴纸 id

// 虚拟背景
FilterEngine.isVirtualBlurAuthorized
FilterEngine.isVirtualCustomAuthorized
```

> 建议在 `authPublisher` 回调（或 `authEpoch` 变化）后刷新一次 UI 授权快照，避免使用过期状态。

---

## 十、Objective-C 经典 API（`RTCBeautyFilter`）

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

// 离线导出：按帧 PTS 驱动动效（负值 = 使用墙钟）
- (void)onExportFrame:(CVPixelBufferRef)pixelBuffer pts:(double)pts {
    [self.beautyFilter processVideoFrame:pixelBuffer rotation:0 mediaTime:pts];
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
| `hueValue` | 色相 | `whitenTeethValue` | 美牙 |
| `temperature` / `tint` | 色温 / 色调 | `eyeShadowValue` / `eyeShadowType` | 眼影强度 / 样式 |
| `brightValue` | 亮度 | `eyeLinesValue` / `eyeLinesType` | 眼线强度 / 样式 |
| `shadowValue` / `highlightsValue` | 阴影 / 高光 | `browValue` / `browType` | 眉毛强度 / 样式 |
| `vignetValue` | 暗角 | `red` / `green` / `blue` | RGB 微调 |

### 其它方法

```objc
- (void)setSpecialEffectsSink:(NSInteger)index;               // 设置特效
- (void)setLookupPath:(NSString *)path level:(CGFloat)level;  // 加载 lookup 滤镜
- (void)setStikerPath:(NSString *)path;                       // 加载贴纸
- (void)setStyleBeautyIndex:(NSInteger)index;                 // 风格美颜
- (void)setStyleBeautyLevel:(CGFloat)level;
- (void)setStyleBeautyIntensity:(CGFloat)intensity;

// 特效参数
- (void)setFlipXFlipFlag:(BOOL)flag;
- (void)setToonThreshold:(CGFloat)threshold;
- (void)setSmoothToonThreshold:(CGFloat)threshold;
- (void)setSmoothToonBlurRadius:(CGFloat)blurRadius;

// 授权（类方法，与现代层 FilterEngine.isXXX 对应）
+ (void)checkString:(NSString *)key completion:(void (^)(BOOL auth))completion;
+ (BOOL)isVerifySuccess;
+ (BOOL)isModuleEnabled:(NSInteger)moduleType;
// ... isFaceTypeAuthorized / isMakeupMainTypeAuthorized 等，详见 RTCBeautyFilter.h

// 裸数据（只读，指针仅在下一帧前有效）
- (const uint8_t *)i420Buffer;
- (const uint8_t *)rgbaBuffer;   // iOS 上可能为空
- (NSInteger)frameWidth;
- (NSInteger)frameHeight;
```

---

## 十一、线程与性能

- **处理线程**：`process(buffer:)` 建议在相机采集回调线程直接调用，避免额外拷贝与线程切换。
- **回调线程**：`handleBuffer` 在 SDK 内部处理线程回调；若要更新 UI，请自行切回主线程。
- **参数变更**：`engine.filter.xxx` 的读写与滤镜链更新是轻量操作，建议在主线程发起（配合 `event` 做 UI 双向绑定）。
- **首帧预热**：进入页面时调用 `filterHandle.warmup()`，提前编译 shader、加载模型，减少首帧卡顿。
- **单例复用**：`FilterEngine.engine` 全局唯一，切换页面时**复用**而非反复创建；经典层 `RTCBeautyFilter` 用完调用 `releaseInstance`。
- **裸数据零拷贝**：`i420Buffer()` / `rgbaBuffer()` 返回内部缓冲指针，避免拷贝，但生命周期仅到下一帧。

---

## 十二、注意事项

1. **真机运行**：当前仅提供 `arm64` 真机切片，模拟器无法编译运行。
2. **授权先行**：使用任何滤镜前请确保 `FilterEngine.create(key:appID:)` 已成功、`FilterEngine.auth == true`；细粒度功能可用性用[第九节](#九细粒度授权门控ui-灰置)的查询接口判断。
3. **裸数据生命周期**：`rgbaBuffer()` / `i420Buffer()` 返回的指针仅在下一帧处理前有效，切勿缓存或手动释放；iOS 上 RGBA 路径可能为空，实时数据请优先使用 CVPixelBuffer 回调或 I420。
4. **线程**：`process(buffer:)` 建议在相机采集回调线程调用；UI 相关的滤镜参数变更建议回到主线程处理。
5. **归一化取值**：`Room*Filter` 各 `Key` 的合法范围通过 `key.range` 获取，可用 `normalizedValue(from:)` / `rawValue(fromNormalized:)` 做 0~1 归一化与真实值互转，避免硬编码。
6. **资源与网络**：云端素材下载依赖 framework 内置的 `ca-bundle.crt`，请确保 framework 完整嵌入，未破坏 `res/` 目录结构。
7. **离线导出**：导出视频请使用带 `mediaTime` 的 `process` 重载，保证动效动画速度与内容一致，而非与导出吞吐一致。
8. **版本查询**：C++ 层可通过 `gpupixel::GPUPixel::GetVersion()` 获取版本号（当前 `1.0.0`）。

---

## 十三、常见问题（FAQ）

| 问题 | 排查方向 |
| --- | --- |
| 编译报 "building for iOS Simulator... but linking..." | 当前仅 `arm64` 真机切片，请切换到真机；模拟器暂不支持。 |
| 授权一直失败 | 检查 `key` / `appID` 是否正确、网络是否可用、`bundleId` 是否与 License 绑定；打开 `setAuthDebugLog(true)` 查看日志。 |
| 画面无美颜效果 | 确认 `FilterEngine.auth == true`、对应模块 `enable = true`、已设置 `handleBuffer` 且把回调 buffer 送去渲染。 |
| 画面方向 / 镜像不对 | 根据设备与摄像头方向设置 `currentRotation` / `updateRotation(_:)`。 |
| `rgbaBuffer()` 拿到全 0 数据 | iOS 已知限制，请改用 `i420Buffer()` 或 `CVPixelBuffer` 回调。 |
| 云端素材列表为空 | 确认已授权且网络可用，`refresh(category:)` 已完成；用 `networkLoggingEnabled` 查看请求。 |
| 动态贴纸 / 特效动画速度异常（导出） | 使用 `process(buffer:rotaion:mediaTime:)` 传入帧 PTS。 |

---

## 十四、最简接入清单（Checklist）

- [ ] 通过 SPM / 手动方式引入 `gpupixel.xcframework` 并设为 Embed & Sign
- [ ] `Info.plist` 添加相机权限描述 `NSCameraUsageDescription`
- [ ] App 启动后调用 `FilterEngine.create(key:appID:)` 完成授权
- [ ] 监听 `FilterEngine.authPublisher` 刷新 UI 授权状态
- [ ] 设置 `filterHandle.handleBuffer` 回调并调用 `warmup()`
- [ ] 相机每帧调用 `filterHandle.process(buffer:rotaion:)`（导出用 `mediaTime` 重载）
- [ ] 通过 `engine.filter.xxx` 配置所需美颜 / 滤镜参数
- [ ] 将回调的 `CVPixelBuffer` 交给渲染 / 推流层
