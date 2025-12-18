// swift-tools-version: 5.8
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "GPUPixelLib",
    platforms: [
        .iOS(.v15)
    ],
    products: [
        .library(
            name: "GPUPixelLib",
            targets: ["gpupixel"]),
    ],
    targets: [
        
        .binaryTarget(name: "gpupixel", path: "gpupixel.xcframework"),
    ],
    swiftLanguageVersions: [.v5]
)

