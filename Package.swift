// swift-tools-version: 5.9
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
