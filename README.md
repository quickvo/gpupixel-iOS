# GPUPixel iOS

<p>
  <img src="https://img.shields.io/badge/platform-iOS%2015.0%2B-blue.svg" alt="Platform iOS 15.0+" />
  <img src="https://img.shields.io/badge/arch-arm64-lightgrey.svg" alt="arm64" />
  <img src="https://img.shields.io/badge/Swift-5.9%2B-orange.svg" alt="Swift 5.9+" />
  <img src="https://img.shields.io/badge/SPM-supported-brightgreen.svg" alt="SPM supported" />
  <img src="https://img.shields.io/badge/version-1.0.0-informational.svg" alt="Version 1.0.0" />
</p>

基于 GPU 的实时视频美颜 / 滤镜 / 特效 iOS SDK。提供**人脸美颜、美型、美妆、风格滤镜、虚拟背景、动态贴纸、画质调节**等能力，以 `CVPixelBuffer` 为输入输出单元，可无缝对接摄像头采集、RTC 推流、本地渲染与离线视频导出。

> `gpupixel.xcframework` 以本地二进制形式随仓库分发，支持 Swift Package Manager 一键集成。

## ✨ 特性

- **实时美颜美型** — 磨皮、美白、瘦脸、大眼、瘦鼻、削脸等 13 项可调参数
- **美妆** — 口红 / 腮红 / 眼线 / 眼影 / 睫毛 / 眉毛，多种风格预设
- **一键风格美颜** — 甜美、气质、白皙、烟熏等整套风格
- **滤镜与特效** — 风格滤镜、动态特效（灵魂出窍、故障、卡通等）、画质调节
- **虚拟背景** — 背景虚化 / 自定义背景图
- **动态贴纸** — 本地与云端素材，支持分页、下载进度与缓存
- **云端素材管理** — `MaterialRepository` 提供拉取 / 下载 / 缓存能力
- **响应式 API** — 基于 Combine 的事件流与授权状态发布者，便于 UI 双向绑定
- **两层 API** — 现代层 `FilterEngine`（Swift）与经典层 `RTCBeautyFilter`（Objective-C）按需选择

## 📋 环境要求

| 项目 | 要求 |
| --- | --- |
| 平台 | iOS 15.0 及以上 |
| 架构 | `arm64`（**仅真机**，暂不提供模拟器切片） |
| 语言 | Swift 5 / Objective-C，Swift 工具链 ≥ 5.9 |
| 权限 | `Info.plist` 需配置 `NSCameraUsageDescription` |
| 网络 | 云端素材下载 / 授权校验需 HTTPS 网络访问 |

> ⚠️ 当前 xcframework 仅包含 `ios-arm64` 切片，无法在模拟器上运行，请使用真机调试。

## 📦 安装

### Swift Package Manager（推荐）

在 Xcode 中 `File → Add Package Dependencies… → Add Local…`，选择本仓库根目录（包含 `Package.swift`）即可；或在你的 `Package.swift` 中通过本地路径引用：

```swift
.package(path: "path/to/gpupixel-iOS")
```

### 手动集成

1. 将 `gpupixel.xcframework` 拖入 Xcode 工程；
2. 在 `Target → General → Frameworks, Libraries, and Embedded Content` 中设为 **Embed & Sign**；
3. 在 `Build Settings` 中确认 `Enable Bitcode = No`。

> framework 内已内置人脸模型（`models/`）与素材、根证书（`res/`），无需额外拷贝资源。

## 🚀 快速开始

```swift
import gpupixel
import CoreVideo

// 1. 授权（App 启动或进入美颜页前调用一次）
Task {
    let ok = await FilterEngine.create(key: "你的License Key", appID: "你的AppID")
    print(ok ? "GPUPixel 授权成功" : "授权失败，请检查 key / appID / 网络")
}

let engine = FilterEngine.engine

// 2. 设置处理结果回调（已美颜的 CVPixelBuffer）
engine.filterHandle.handleBuffer = { buffer, rotation in
    // 交给渲染 / 推流层
}
engine.filterHandle.warmup() // 可选：预热，减少首帧卡顿

// 3. 配置滤镜
engine.filter.face.enable = true
engine.filter.face[.buffing] = 0.6   // 磨皮
engine.filter.face[.white]   = 0.4   // 美白
engine.filter.face[.thin]    = 0.3   // 瘦脸

// 4. 相机每帧送入 SDK（在 AVCaptureVideoDataOutputSampleBufferDelegate 中）
func captureOutput(_ output: AVCaptureOutput,
                   didOutput sampleBuffer: CMSampleBuffer,
                   from connection: AVCaptureConnection) {
    guard let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
    engine.filterHandle.process(buffer: pixelBuffer, rotaion: .rotation0)
}
```

## 🎛️ 滤镜模块

所有滤镜通过 `FilterEngine.engine.filter` 声明式配置：

| 属性 | 类型 | 作用 |
| --- | --- | --- |
| `face` | `RoomFaceFilter` | 基础美颜与美型（磨皮、美白、瘦脸、大眼、瘦鼻等） |
| `beauty` | `RoomBeautyFilter` | 美妆（口红、腮红、眼线、眼影、睫毛、眉毛）及风格 |
| `styleBeauty` | `RoomStyleBeautyFilter` | 一键风格美颜（甜美、气质、白皙、烟熏等） |
| `style` | `RoomStyleFilter` | 风格滤镜（按素材路径加载） |
| `styleEffect` | `RoomStyleEffectFilter` | 云端风格特效（异步下载与应用） |
| `effect` | `RoomEffectFilter` | 动态特效（灵魂出窍、抖动、故障、马赛克、卡通等） |
| `quality` | `RoomQulityFilter` | 画质调节（色温、色调、曝光、饱和度、对比度等） |
| `virtualBackground` | `RoomVirtualBackFilter` | 虚拟背景（图片 / 模糊 / 关闭） |
| `stiker` | `RoomStickerFilter` | 动态贴纸（按素材路径加载） |

## 📚 文档

完整的接入指南、API 说明、云端素材下载、细粒度授权门控、Objective-C 经典 API、线程与性能、FAQ 等内容见：

**➡️ [完整集成文档](docs/集成文档.md)**

## 🧵 性能建议

- 在相机采集回调线程直接调用 `process(buffer:)`，避免额外拷贝与线程切换。
- 进入页面时调用 `filterHandle.warmup()` 预编译 shader、加载模型。
- `FilterEngine.engine` 为全局单例，切换页面时**复用**而非反复创建。
- 离线导出请使用带 `mediaTime` 的 `process` 重载，保证动效速度与内容一致。

## 📄 许可

请参考仓库授权说明，SDK 使用需完成 License 授权（`FilterEngine.create(key:appID:)`）。
