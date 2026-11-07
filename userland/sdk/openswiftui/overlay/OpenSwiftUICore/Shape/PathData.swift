//
//  PathData.swift
//  OpenSwiftUICore
//
//  mOS: a união C (PathData.h) não tem cgPath fora do Darwin e some do importador Swift.
//  O Path do mOS guarda o CGPath do OpenCoreGraphics nesta struct.

package import OpenCoreGraphicsShims
package import OpenRenderBoxShims

package struct PathData {
    package typealias Buffer = (
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
        UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8
    )

    private var storage: Buffer = (
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    )

    package init() {}

    package init(cgPath: Unmanaged<CGPath>) {
        self.cgPath = cgPath
    }

    package var cgPath: Unmanaged<CGPath> {
        get {
            withUnsafeBytes(of: storage) { buffer in
                buffer.load(as: Unmanaged<CGPath>.self)
            }
        }
        set {
            withUnsafeMutableBytes(of: &storage) { buffer in
                buffer.storeBytes(of: newValue, as: Unmanaged<CGPath>.self)
            }
        }
    }

    package init(rbPath: ORBPath) {
        self.rbPath = rbPath
    }

    package var rbPath: ORBPath {
        get {
            withUnsafeBytes(of: storage) { buffer in
                buffer.load(as: ORBPath.self)
            }
        }
        set {
            withUnsafeMutableBytes(of: &storage) { buffer in
                buffer.storeBytes(of: newValue, as: ORBPath.self)
            }
        }
    }

    package init(buffer: Buffer) {
        self.storage = buffer
    }

    package var buffer: Buffer {
        get { storage }
        set { storage = newValue }
    }
}
