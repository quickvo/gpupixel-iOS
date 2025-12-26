// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "gpupixel",
    platforms: [
        .iOS(.v15)
    ],
    products: [
        .library(
            name: "gpupixel",
            targets: ["gpupixel"]),
    ],
    targets: [
        
        .binaryTarget(name: "gpupixel", path: "gpupixel.xcframework"),
    ],
    swiftLanguageVersions: [.v5]
)

