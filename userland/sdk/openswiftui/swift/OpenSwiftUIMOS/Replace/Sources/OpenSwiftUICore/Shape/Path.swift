//
//  Path.swift
//  OpenSwiftUICore
//
//  Audited for 6.5.4
//  Status: WIP
//  ID: 31FD92B70C320DDD253E93C7417D779A (SwiftUI)
//  ID: 3591905F51357E95FA93E39751507471 (SwiftUICore)
//
//  mOS: cópia do upstream com o caminho não-Darwin implementado. Sem ORBPath fora do Darwin, o Path
//  guarda os elementos no CGPath (Swift) do OpenCoreGraphics, no PathBox de tipo .cgPath; a geometria
//  fica em OpenSwiftUIMOS/Core/MOSPathGeometry.swift e o traço (strokedPath) no C do libSystem
//  (mos_path_stroke_outline). Trechos marcados com "mOS".

public import Foundation
package import OpenRenderBoxShims
import OpenSwiftUI_SPI
public import OpenCoreGraphicsShims

// MARK: - Path [WIP]

/// The outline of a 2D shape.
@available(OpenSwiftUI_v1_0, *)
@frozen
public struct Path: Equatable, LosslessStringConvertible, @unchecked Sendable {

    // MARK: - Path.PathBox

    @usableFromInline
    final package class PathBox: Equatable {
        private enum Kind: UInt8 {
            #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
            case cgPath
            #endif
            case rbPath
            case buffer
        }

        private var kind: Kind
        private var data: PathData

        #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
        @inline(__always)
        init(_ path: CGPath) {
            var data = PathData()
            kind = .cgPath
            data.cgPath = .passRetained(path)
            self.data = data
        }
        #endif

        package init(takingPath path: ORBPath) {
            var data = PathData()
            kind = .rbPath
            data.rbPath = path
            self.data = data
        }

        deinit {
            switch kind {
            #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
            case .cgPath:
                data.cgPath.release()
            #endif
            case .rbPath:
                data.rbPath.release()
            case .buffer:
                withUnsafeMutablePointer(to: &data) { pointer in
                    let storage = unsafeBitCast(pointer, to: ORBPath.Storage.self)
                    storage.destroy()
                }
            }
        }

        private func prepareBuffer() {
            let path: ORBPath
            switch kind {
            #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
            case .cgPath:
                path = ORBPath(cgPath: data.cgPath.takeRetainedValue())
            #endif
            case .rbPath:
                path = data.rbPath
            case .buffer:
                return
            }
            withUnsafeMutablePointer(to: &data) { pointer in
                let storage = unsafeBitCast(pointer, to: ORBPath.Storage.self)
                storage.initialize(capacity: 96, source: nil)
                storage.append(path: path)
                kind = .buffer
                path.release()
            }
        }

        private static let bufferCallbacks: UnsafePointer<ORBPath.Callbacks> = {
            let pointer = UnsafeMutablePointer<ORBPath.Callbacks>.allocate(capacity: 1)
            var callbacks = ORBPath.empty.callbacks.pointee
            callbacks.retain = { object in
                UnsafeRawPointer(
                    Unmanaged<PathBox>.fromOpaque(object)
                    .retain()
                    .toOpaque()
                )
            }
            callbacks.release = { object in
                Unmanaged<PathBox>.fromOpaque(object)
                    .release()
            }
            callbacks.apply = { object, info, callback in
                let box = object.assumingMemoryBound(to: PathBox.self).pointee
                let storage = withUnsafeMutablePointer(to: &box.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                return storage.apply(info: info, callback: callback)
            }
            callbacks.isEqual = { lhs, rhs in
                let lhsBox = lhs.assumingMemoryBound(to: PathBox.self).pointee
                let rhsBox = rhs.assumingMemoryBound(to: PathBox.self).pointee
                let lhsStorage = withUnsafeMutablePointer(to: &lhsBox.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                let rhsStorage = withUnsafeMutablePointer(to: &rhsBox.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                return lhsStorage.isEqual(to: rhsStorage)
            }
            callbacks.isEmpty = { object in
                let box = object.assumingMemoryBound(to: PathBox.self).pointee
                let storage = withUnsafeMutablePointer(to: &box.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                return storage.isEmpty
            }
            callbacks.isSingleElement = { object in
                let box = object.assumingMemoryBound(to: PathBox.self).pointee
                let storage = withUnsafeMutablePointer(to: &box.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                return storage.isEmpty
            }
            callbacks.bezierOrder = { object in
                let box = object.assumingMemoryBound(to: PathBox.self).pointee
                let storage = withUnsafeMutablePointer(to: &box.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                return storage.bezierOrder
            }
            callbacks.boundingRect = { object in
                let box = object.assumingMemoryBound(to: PathBox.self).pointee
                let storage = withUnsafeMutablePointer(to: &box.data) {
                    unsafeBitCast($0, to: ORBPath.Storage.self)
                }
                return storage.boundingRect
            }
            pointer.initialize(to: callbacks)
            return UnsafePointer(pointer)
        }()

        #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
        @inline(__always)
        fileprivate var cgPath: CGPath {
            let rbPath: ORBPath
            switch kind {
            case .cgPath:
                return data.cgPath.takeUnretainedValue()
            case .rbPath:
                rbPath = data.rbPath
            case .buffer:
                let storage = unsafeBitCast(self, to: ORBPath.Storage.self)
                rbPath = ORBPath(storage: storage, callbacks: Self.bufferCallbacks)
            }
            return rbPath.cgPath
        }
        #endif

        @inline(__always)
        fileprivate var rbPath: ORBPath {
            switch kind {
            case .cgPath:
                return ORBPath(cgPath: data.cgPath.takeUnretainedValue())
            case .rbPath:
                return data.rbPath
            case .buffer:
                let storage = unsafeBitCast(self, to: ORBPath.Storage.self)
                return ORBPath(storage: storage, callbacks: Self.bufferCallbacks)
            }
        }
        
        @inline(__always)
        fileprivate var boundingRect: CGRect {
            switch kind {
            case .cgPath:
                #if canImport(CoreGraphics)
                return data.cgPath.takeUnretainedValue().boundingBoxOfPath
                #else
                // mOS
                return MOSPathGeometry.boundingRect(MOSPathGeometry.elements(of: data.cgPath.takeUnretainedValue()))
                #endif
            case .rbPath:
                _openSwiftUIUnimplementedWarning()
                // return data.rbPath.boundingBox // FIXME: RB API
                return .zero
            case .buffer:
                let storage = unsafeBitCast(self, to: ORBPath.Storage.self)
                return storage.boundingRect
            }
        }

        @inline(__always)
        fileprivate func retainRBPath() -> ORBPath {
            let rbPath = rbPath
            rbPath.retain()
            return rbPath
        }

        @usableFromInline
        package static func == (lhs: PathBox, rhs: PathBox) -> Bool {
            #if !canImport(Darwin)
            // mOS: comparação pelos elementos do CGPath
            if lhs === rhs { return true }
            if lhs.kind == .cgPath, rhs.kind == .cgPath {
                return lhs.data.cgPath.takeUnretainedValue().elements == rhs.data.cgPath.takeUnretainedValue().elements
            }
            #endif
            return lhs.rbPath.isEqual(to: rhs.rbPath)
        }

        #if !canImport(Darwin)
        // MARK: mOS — elementos guardados no CGPath do OpenCoreGraphics

        /// Caixa a partir de elementos (sempre de tipo .cgPath, com um CGPath próprio).
        package convenience init(mosElements elements: [Path.Element]) {
            self.init(MOSPathGeometry.cgPath(elements))
        }

        package var mosElements: [Path.Element] {
            switch kind {
            case .cgPath: return MOSPathGeometry.elements(of: data.cgPath.takeUnretainedValue())
            case .rbPath, .buffer:
                // só nasce de ORBPath (OpenRenderBox), que não existe fora do Darwin
                _openSwiftUIPlatformUnimplementedWarning()
                return []
            }
        }

        package var mosIsEmpty: Bool {
            kind == .cgPath ? data.cgPath.takeUnretainedValue().elements.isEmpty : mosElements.isEmpty
        }

        /// Acrescenta no próprio CGPath; só deve ser chamado com a caixa referenciada uma única vez.
        package func mosAppendInPlace(_ new: [Path.Element]) -> Bool {
            guard kind == .cgPath else { return false }
            data.cgPath.takeUnretainedValue().elements.append(contentsOf: new.map(MOSPathGeometry.cgElement))
            return true
        }

        /// Ponto atual sem percorrer o caminho inteiro (de trás para frente).
        package var mosCurrentPoint: CGPoint? {
            guard kind == .cgPath else { return MOSPathGeometry.currentPoint(mosElements) }
            let els = data.cgPath.takeUnretainedValue().elements
            guard let last = els.last else { return nil }
            func end(_ e: CGPath.Element) -> CGPoint? {
                switch e {
                case let .moveToPoint(p), let .addLineToPoint(p), let .addQuadCurveToPoint(_, p), let .addCurveToPoint(_, _, p): return p
                case .closeSubpath: return nil
                }
            }
            if let p = end(last) { return p }
            // depois de closeSubpath: início do subcaminho (o último move, ou o primeiro ponto)
            for e in els.reversed() { if case let .moveToPoint(p) = e { return p } }
            return els.lazy.compactMap(end).first
        }
        #endif
    }

    @usableFromInline
    @frozen
    package enum Storage: Equatable {
        case empty
        case rect(CGRect)
        case ellipse(CGRect)
        indirect case roundedRect(FixedRoundedRect)
        @available(*, deprecated, message: "obsolete")
        indirect case stroked(StrokedPath)
        @available(*, deprecated, message: "obsolete")
        indirect case trimmed(TrimmedPath)
        case path(PathBox)
    }

    package var storage: Path.Storage

    package init(storage: Path.Storage) {
        self.storage = storage
    }

    package init(box: Path.PathBox) {
        self.storage = .path(box)
    }

    /// Creates an empty path.
    public init() {
        storage = .empty
    }

    #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
    /// Creates a path from an immutable shape path.
    ///
    /// - Parameter path: The immutable CoreGraphics path to initialize
    ///   the new path from.
    ///
    public init(_ path: CGPath) {
        guard !path.isEmpty else {
            storage = .empty
            return
        }
        #if canImport(Darwin)
        storage = .path(PathBox(path))
        #else
        // mOS: o CGPath do OpenCoreGraphics é mutável por referência; o Path (valor) guarda uma cópia
        storage = .path(PathBox(CGMutablePath(elements: path.elements)))
        #endif
    }

    /// Creates a path from a copy of a mutable shape path.
    ///
    /// - Parameter path: The CoreGraphics path to initialize the new
    ///   path from.
    ///
    public init(_ path: CGMutablePath) {
        guard !path.isEmpty else {
            storage = .empty
            return
        }
        #if canImport(Darwin)
        storage = .path(PathBox(path.mutableCopy()!))
        #else
        // mOS: mutableCopy() do OpenCoreGraphics devolve o próprio objeto; copia os elementos
        storage = .path(PathBox(CGMutablePath(elements: path.elements)))
        #endif
    }
    #endif

    /// Creates a path containing a rectangle.
    ///
    /// This is a convenience function that creates a path of a
    /// rectangle. Using this convenience function is more efficient
    /// than creating a path and adding a rectangle to it.
    ///
    /// Calling this function is equivalent to using `minX` and related
    /// properties to find the corners of the rectangle, then using the
    /// `move(to:)`, `addLine(to:)`, and `closeSubpath()` functions to
    /// add the rectangle.
    ///
    /// - Parameter rect: The rectangle to add.
    ///
    public init(_ rect: CGRect) {
        guard !rect.isNull else {
            storage = .empty
            return
        }
        storage = .rect(rect)
    }

    /// Creates a path containing a rounded rectangle.
    ///
    /// This is a convenience function that creates a path of a rounded
    /// rectangle. Using this convenience function is more efficient
    /// than creating a path and adding a rounded rectangle to it.
    ///
    /// - Parameters:
    ///   - rect: A rectangle, specified in user space coordinates.
    ///   - cornerSize: The size of the corners, specified in user space
    ///     coordinates.
    ///   - style: The corner style. Defaults to the `continous` style
    ///     if not specified.
    ///
    public init(roundedRect rect: CGRect, cornerSize: CGSize, style: RoundedCornerStyle = .continuous) {
        guard !rect.isNull else {
            storage = .empty
            return
        }
        guard cornerSize != .zero && !rect.isInfinite else {
            storage = .rect(rect)
            return
        }
        storage = .roundedRect(FixedRoundedRect(rect, cornerSize: cornerSize, style: style))
    }

    /// Creates a path containing a rounded rectangle.
    ///
    /// This is a convenience function that creates a path of a rounded
    /// rectangle. Using this convenience function is more efficient
    /// than creating a path and adding a rounded rectangle to it.
    ///
    /// - Parameters:
    ///   - rect: A rectangle, specified in user space coordinates.
    ///   - cornerRadius: The radius of all corners of the rectangle,
    ///     specified in user space coordinates.
    ///   - style: The corner style. Defaults to the `continous` style
    ///     if not specified.
    ///
    public init(roundedRect rect: CGRect, cornerRadius: CGFloat, style: RoundedCornerStyle = .continuous) {
        guard !rect.isNull else {
            storage = .empty
            return
        }
        guard cornerRadius != .zero && !rect.isInfinite else {
            storage = .rect(rect)
            return
        }
        storage = .roundedRect(FixedRoundedRect(rect, cornerRadius: cornerRadius, style: style))
    }

    /// Creates a path as the given rounded rectangle, which may have
    /// uneven corner radii.
    ///
    /// This is a convenience function that creates a path of a rounded
    /// rectangle. Using this function is more efficient than creating
    /// a path and adding a rounded rectangle to it.
    ///
    /// - Parameters:
    ///   - rect: A rectangle, specified in user space coordinates.
    ///   - cornerRadii: The radius of each corner of the rectangle,
    ///     specified in user space coordinates.
    ///   - style: The corner style. Defaults to the `continous` style
    ///     if not specified.
    ///
    public init(roundedRect rect: CGRect, cornerRadii: RectangleCornerRadii, style: RoundedCornerStyle = .continuous) {
        guard !rect.isNull else {
            storage = .empty
            return
        }
        // mOS: raios iguais viram .roundedRect; senão, elementos com um raio por canto
        let r = cornerRadii
        if r.topLeft == r.topRight, r.topLeft == r.bottomRight, r.topLeft == r.bottomLeft {
            self = Path(roundedRect: rect, cornerRadius: r.topLeft, style: style)
            return
        }
        var elements: [Path.Element] = []
        MOSPathGeometry.appendUnevenRoundedRect(
            rect,
            topLeft: CGSize(width: r.topLeft, height: r.topLeft), topRight: CGSize(width: r.topRight, height: r.topRight),
            bottomRight: CGSize(width: r.bottomRight, height: r.bottomRight), bottomLeft: CGSize(width: r.bottomLeft, height: r.bottomLeft),
            continuous: style == .continuous, to: &elements
        )
        self = Path(mosElements: elements)
    }

    /// mOS: Path a partir de elementos (vazio se não houver nenhum).
    package init(mosElements elements: [Path.Element]) {
        storage = elements.isEmpty ? .empty : .path(PathBox(mosElements: elements))
    }

    /// mOS: elementos de qualquer armazenamento (retângulos, elipses e retângulos arredondados expandidos).
    package var mosElements: [Path.Element] {
        var out: [Path.Element] = []
        switch storage {
        case .empty: break
        case let .rect(rect): MOSPathGeometry.appendRect(rect, to: &out)
        case let .ellipse(rect): MOSPathGeometry.appendEllipse(rect, to: &out)
        case let .roundedRect(rr):
            MOSPathGeometry.appendRoundedRect(rr.rect, cornerSize: rr.clampedCornerSize, continuous: rr.needsContinuousCorners, to: &out)
        case .stroked, .trimmed: break
        case let .path(box): out = box.mosElements
        }
        return out
    }

    /// Creates a path as an ellipse within the given rectangle.
    ///
    /// This is a convenience function that creates a path of an
    /// ellipse. Using this convenience function is more efficient than
    /// creating a path and adding an ellipse to it.
    ///
    /// The ellipse is approximated by a sequence of Bézier
    /// curves. Its center is the midpoint of the rectangle defined by
    /// the rect parameter. If the rectangle is square, then the
    /// ellipse is circular with a radius equal to one-half the width
    /// (or height) of the rectangle. If the rect parameter specifies a
    /// rectangular shape, then the major and minor axes of the ellipse
    /// are defined by the width and height of the rectangle.
    ///
    /// The ellipse forms a complete subpath of the path—that
    /// is, the ellipse drawing starts with a move-to operation and
    /// ends with a close-subpath operation, with all moves oriented in
    /// the clockwise direction. If you supply an affine transform,
    /// then the constructed Bézier curves that define the
    /// ellipse are transformed before they are added to the path.
    ///
    /// - Parameter rect: The rectangle that bounds the ellipse.
    ///
    public init(ellipseIn rect: CGRect) {
        guard !rect.isNull else {
            storage = .empty
            return
        }
        storage = rect.isInfinite ? .rect(rect) : .ellipse(rect)
    }

    /// Creates an empty path, then executes a closure to add its
    /// initial elements.
    ///
    /// - Parameter callback: The Swift function that will be called to
    ///   initialize the new path.
    ///
    public init(_ callback: (inout Path) -> ()) {
        storage = .empty
        callback(&self)
    }

    /// Initializes from the result of a previous call to
    /// `Path.stringRepresentation`. Fails if the `string` does not
    /// describe a valid path.
    public init?(_ string: String) {
        #if canImport(CoreGraphics)
        let mutablePath = CGMutablePath()
        let nsString = string as NSString
        guard let str = nsString.utf8String else {
            return nil
        }
        guard _CGPathParseString(mutablePath, str) else {
            return nil
        }
        storage = .path(PathBox(mutablePath))
        #else
        // mOS: mesmo formato textual do CGPath ("x y m x y l ... h")
        guard let elements = MOSPathGeometry.parse(string) else { return nil }
        storage = elements.isEmpty ? .empty : .path(PathBox(mosElements: elements))
        #endif
    }

    /// A description of the path that may be used to recreate the path
    /// via `init?(_:)`.
    public var description: String {
        #if canImport(Darwin)
        _CGPathCopyDescription(cgPath, 0.0)
        #else
        MOSPathGeometry.describe(mosElements)   // mOS
        #endif
    }

    #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES
    /// An immutable path representing the elements in the path.
    public var cgPath: CGPath {
        #if !canImport(Darwin)
        // mOS: cópia nova (o CGPath do OpenCoreGraphics é mutável por referência)
        return MOSPathGeometry.cgPath(mosElements)
        #endif
        #if canImport(Darwin)
        switch storage {
        case .empty:
            CGPath(rect: .null, transform: nil)
        case let .rect(rect):
            CGPath(rect: rect, transform: nil)
        case let .ellipse(rect):
            CGPath(ellipseIn: rect, transform: nil)
        case let .roundedRect(fixedRoundedRect):
            fixedRoundedRect.cgPath
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        case let .path(pathBox):
            pathBox.cgPath
        }
        #endif
    }
    #endif

    package func retainRBPath() -> ORBPath {
        switch storage {
        case .empty:
            ORBPath.empty
        case let .rect(rect):
            ORBPath(rect: rect, transform: nil)
        case let .ellipse(rect):
            ORBPath(ellipseIn: rect, transform: nil)
        case let .roundedRect(fixedRoundedRect):
            ORBPath(
                roundedRect: fixedRoundedRect.rect,
                cornerWidth: fixedRoundedRect.cornerSize.width,
                cornerHeight: fixedRoundedRect.cornerSize.height,
                style: fixedRoundedRect.style == .circular ? .circular : .continuous,
                transform: nil
            )
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        case let .path(pathBox):
            pathBox.retainRBPath()
        }
    }

    package mutating func withMutableBuffer(do body: (UnsafeMutableRawPointer) -> Void) {
        _openSwiftUIUnimplementedFailure()
    }

    /// A Boolean value indicating whether the path contains zero elements.
    public var isEmpty: Bool {
        switch storage {
        case .empty:
            true
        case let .rect(rect):
            rect.isNull
        case let .ellipse(rect):
            rect.isNull
        case let .roundedRect(fixedRoundedRect):
            fixedRoundedRect.rect.isNull
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        case let .path(pathBox):
            #if canImport(Darwin)
            pathBox.rbPath.isEmpty
            #else
            pathBox.mosIsEmpty   // mOS
            #endif
        }
    }

    /// A rectangle containing all path segments.
    ///
    /// This is the smallest rectangle completely enclosing all points
    /// in the path but not including control points for Bézier
    /// curves.
    public var boundingRect: CGRect {
        switch storage {
        case .empty:
            .null
        case let .rect(rect):
            rect
        case let .ellipse(rect):
            rect
        case let .roundedRect(fixedRoundedRect):
            fixedRoundedRect.rect
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        case let .path(pathBox):
            pathBox.boundingRect
        }
    }

    /// Returns true if the path contains a specified point.
    ///
    /// If `eoFill` is true, this method uses the even-odd rule to define which
    /// points are inside the path. Otherwise, it uses the non-zero rule.
    public func contains(_ p: CGPoint, eoFill: Bool = false) -> Bool {
        switch storage {
        case .empty:
            return false
        case let .rect(rect):
            return rect.contains(p)
        case let .ellipse(rect):
            return ellipseContains(in: rect, contains: p)
        case let .roundedRect(rect):
            #if canImport(Darwin)
            return rect.contains(p)
            #else
            return rect.contains(p)   // mOS: FixedRoundedRect.contains no overlay de RoundedCornerStyle
            #endif
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        case let .path(box):
            #if canImport(Darwin)
            let path = box.retainRBPath()
            // TBA
            defer { path.release() }
            return path.contains(point: p, eoFill: eoFill)
            #else
            return MOSPathGeometry.contains(box.mosElements, point: p, eoFill: eoFill)   // mOS
            #endif
        }
    }

    package func contains(
        points: [PlatformPoint],
        eoFill: Bool = false,
        origin: CGPoint = .zero
    ) -> BitVector64 {
        points.withUnsafeBufferPointer {
            contains(points: $0, eoFill: eoFill, origin: origin)
        }
    }

    package func contains(
        points: UnsafeBufferPointer<PlatformPoint>,
        eoFill: Bool = false,
        origin: CGPoint = .zero
    ) -> BitVector64 {
        // FIXME:
        // The RenderBox shim currently declares the batched mask result as Bool.
        // Use scalar containment to preserve every bit until that ABI is corrected.
        points.mapBool {
            contains(CGPoint(x: $0.x - origin.x, y: $0.y - origin.y), eoFill: eoFill)
        }
    }

    /// An element of a path.
    @frozen
    public enum Element: Equatable {
        /// A path element that terminates the current subpath (without closing
        /// it) and defines a new current point.
        case move(to: CGPoint)

        /// A line from the previous current point to the given point, which
        /// becomes the new current point.
        case line(to: CGPoint)

        /// A quadratic Bézier curve from the previous current point to the
        /// given end-point, using the single control point to define the curve.
        ///
        /// The end-point of the curve becomes the new current point.
        case quadCurve(to: CGPoint, control: CGPoint)

        /// A cubic Bézier curve from the previous current point to the given
        /// end-point, using the two control points to define the curve.
        ///
        /// The end-point of the curve becomes the new current point.
        case curve(to: CGPoint, control1: CGPoint, control2: CGPoint)

        /// A line from the start point of the current subpath (if any) to the
        /// current point, which terminates the subpath.
        ///
        /// After closing the subpath, the current point becomes undefined.
        case closeSubpath
    }

    /// Calls `body` with each element in the path.
    public func forEach(_ body: (Path.Element) -> Void) {
        #if canImport(CoreGraphics)
        // FIXME: Use RB API instead of CG API
        cgPath.applyWithBlock { element in
            let element = element.pointee
            switch element.type {
            case .moveToPoint:
                body(.move(to: element.points[0]))
            case .addLineToPoint:
                body(.line(to: element.points[0]))
            case .addQuadCurveToPoint:
                body(.quadCurve(to: element.points[1], control: element.points[0]))
            case .addCurveToPoint:
                body(.curve(to: element.points[2], control1: element.points[0], control2: element.points[1]))
            case .closeSubpath:
                body(.closeSubpath)
            @unknown default:
                break
            }
        }
        #else
        for element in mosElements { body(element) }   // mOS
        #endif
    }

    /// Returns a stroked copy of the path using `style` to define how the
    /// stroked outline is created.
    public func strokedPath(_ style: StrokeStyle) -> Path {
        #if canImport(Darwin)
        _openSwiftUIUnimplementedFailure()
        #else
        // mOS: contorno gerado em C (anéis na mesma orientação; preencher com não-zero)
        return Path(mosElements: MOSPathBridge.strokeOutline(self, style: style))
        #endif
    }

    /// Returns a partial copy of the path.
    ///
    /// The returned path contains the region between `from` and `to`, both of
    /// which must be fractions between zero and one defining points
    /// linearly-interpolated along the path.
    public func trimmedPath(from: CGFloat, to: CGFloat) -> Path {
        guard !isEmpty else {
            return Path()
        }
        guard from > 0 || to < 1 else {
            return self
        }
        guard to > from else {
            return Path()
        }
        #if canImport(Darwin)
        // let retainedPath = retainRBPath()
        _openSwiftUIUnimplementedWarning()
        return self
        #else
        return Path(mosElements: MOSPathGeometry.trimmed(mosElements, from: from, to: to))   // mOS
        #endif
    }

    package func rect() -> CGRect? {
        switch storage {
        case let .rect(rect):
            rect
        default:
            nil
        }
    }

    package func roundedRect() -> FixedRoundedRect? {
        switch storage {
        case let .rect(rect):
            FixedRoundedRect(rect)
        case let .ellipse(rect):
            if rect.width == rect.height {
                FixedRoundedRect(
                    rect,
                    cornerRadius: rect.width / 2,
                    style: .circular
                )
            } else {
                nil
            }
        case let .roundedRect(fixedRoundedRect):
            fixedRoundedRect
        default:
            nil
        }
    }
}

@available(*, unavailable)
extension Path.Storage: Sendable {}

@available(*, unavailable)
extension Path.PathBox: Sendable {}

// MARK: - Path + Shape

extension Path: Shape {
    nonisolated public func path(in _: CGRect) -> Path { self }

    public typealias AnimatableData = EmptyAnimatableData

    public typealias Body = _ShapeView<Path, ForegroundStyle>
}

// MARK: - Path + ProtobufMessage

extension Path: ProtobufMessage {
    package func encode(to encoder: inout ProtobufEncoder) throws {
        _openSwiftUIUnimplementedFailure()
    }

    package init(from decoder: inout ProtobufDecoder) throws {
        _openSwiftUIUnimplementedFailure()
    }
}

// MARK: - StrokedPath

@available(*, deprecated, message: "obsolete")
@usableFromInline
package struct StrokedPath: Equatable {
    public init(path: Path, style: StrokeStyle) {}

    @usableFromInline
    package static func == (a: StrokedPath, b: StrokedPath) -> Bool {
        true
    }
}

@available(*, unavailable)
extension StrokedPath: Sendable {}

// MARK: - TrimmedPath

@available(*, deprecated, message: "obsolete")
@usableFromInline
package struct TrimmedPath: Equatable {
    @usableFromInline
    package static func == (a: TrimmedPath, b: TrimmedPath) -> Swift.Bool {
        true
    }
}

@available(*, unavailable)
extension TrimmedPath: Sendable {}

// MARK: - Path + Extension [WIP]

@available(OpenSwiftUI_v1_0, *)
extension Path {
    // MARK: mOS — construção incremental
    //
    // Cada operação acrescenta elementos ao CGPath da caixa; se a caixa é exclusiva deste Path o
    // acréscimo é no próprio objeto (O(k)), senão (cópia de valor compartilhada, retângulo, elipse...)
    // os elementos são copiados para uma caixa nova.

    private mutating func mosAppend(_ body: (_ current: CGPoint?, _ out: inout [Path.Element]) -> Void) {
        if case .path(var box) = storage {
            storage = .empty
            if isKnownUniquelyReferenced(&box) {
                var new: [Path.Element] = []
                body(box.mosCurrentPoint, &new)
                if box.mosAppendInPlace(new) { storage = .path(box); return }
            }
            storage = .path(box)
        }
        var els = mosElements
        var new: [Path.Element] = []
        body(MOSPathGeometry.currentPoint(els), &new)
        els.append(contentsOf: new)
        self = Path(mosElements: els)
    }


    public mutating func move(to end: CGPoint) {
        mosAppend { _, out in out.append(.move(to: end)) }
    }

    public mutating func addLine(to end: CGPoint) {
        // como no CoreGraphics: sem ponto atual, não faz nada
        mosAppend { cur, out in if cur != nil { out.append(.line(to: end)) } }
    }

    public mutating func addQuadCurve(
        to end: CGPoint,
        control: CGPoint
    ) {
        mosAppend { cur, out in if cur != nil { out.append(.quadCurve(to: end, control: control)) } }
    }

    public mutating func addCurve(
        to end: CGPoint,
        control1: CGPoint,
        control2: CGPoint
    ) {
        mosAppend { cur, out in if cur != nil { out.append(.curve(to: end, control1: control1, control2: control2)) } }
    }

    public mutating func closeSubpath() {
        mosAppend { cur, out in if cur != nil { out.append(.closeSubpath) } }
    }

    public mutating func addRect(
        _ rect: CGRect,
        transform: CGAffineTransform = .identity
    ) {
        guard !rect.isNull else { return }
        if isEmpty, transform.isIdentity { self = Path(rect); return }
        mosAppend { _, out in MOSPathGeometry.appendRect(rect, transform: transform, to: &out) }
    }

    public mutating func addRoundedRect(
        in rect: CGRect,
        cornerSize: CGSize,
        style: RoundedCornerStyle = .continuous,
        transform: CGAffineTransform = .identity
    ) {
        guard !rect.isNull else { return }
        if isEmpty, transform.isIdentity { self = Path(roundedRect: rect, cornerSize: cornerSize, style: style); return }
        let rr = FixedRoundedRect(rect, cornerSize: cornerSize, style: style)
        mosAppend { _, out in
            MOSPathGeometry.appendRoundedRect(rect, cornerSize: rr.clampedCornerSize, continuous: rr.needsContinuousCorners, transform: transform, to: &out)
        }
    }

    @available(OpenSwiftUI_v4_0, *)
    public mutating func addRoundedRect(
        in rect: CGRect,
        cornerRadii: RectangleCornerRadii,
        style: RoundedCornerStyle = .continuous,
        transform: CGAffineTransform = .identity
    ) {
        guard !rect.isNull else { return }
        let r = cornerRadii
        mosAppend { _, out in
            MOSPathGeometry.appendUnevenRoundedRect(
                rect,
                topLeft: CGSize(width: r.topLeft, height: r.topLeft), topRight: CGSize(width: r.topRight, height: r.topRight),
                bottomRight: CGSize(width: r.bottomRight, height: r.bottomRight), bottomLeft: CGSize(width: r.bottomLeft, height: r.bottomLeft),
                continuous: style == .continuous, transform: transform, to: &out
            )
        }
    }

    public mutating func addEllipse(
        in rect: CGRect,
        transform: CGAffineTransform = .identity
    ) {
        guard !rect.isNull else { return }
        if isEmpty, transform.isIdentity { self = Path(ellipseIn: rect); return }
        mosAppend { _, out in MOSPathGeometry.appendEllipse(rect, transform: transform, to: &out) }
    }

    public mutating func addRects(
        _ rects: [CGRect],
        transform: CGAffineTransform = .identity
    ) {
        mosAppend { _, out in
            for rect in rects where !rect.isNull { MOSPathGeometry.appendRect(rect, transform: transform, to: &out) }
        }
    }

    public mutating func addLines(_ lines: [CGPoint]) {
        guard let first = lines.first else { return }
        mosAppend { _, out in
            out.append(.move(to: first))
            for p in lines.dropFirst() { out.append(.line(to: p)) }
        }
    }

    public mutating func addRelativeArc(
        center: CGPoint,
        radius: CGFloat,
        startAngle: Angle,
        delta: Angle,
        transform: CGAffineTransform = .identity
    ) {
        mosAppend { cur, out in
            var tmp: [Path.Element] = cur.map { [.move(to: $0)] } ?? []
            MOSPathGeometry.appendArc(center: center, radius: radius, start: CGFloat(startAngle.radians), delta: CGFloat(delta.radians), transform: transform, to: &tmp)
            out.append(contentsOf: cur == nil ? tmp : Array(tmp.dropFirst()))
        }
    }

    public mutating func addArc(
        center: CGPoint,
        radius: CGFloat,
        startAngle: Angle,
        endAngle: Angle,
        clockwise: Bool,
        transform: CGAffineTransform = .identity
    ) {
        mosAppend { cur, out in
            var tmp: [Path.Element] = cur.map { [.move(to: $0)] } ?? []
            MOSPathGeometry.appendArc(center: center, radius: radius, start: CGFloat(startAngle.radians), end: CGFloat(endAngle.radians),
                                      clockwise: clockwise, transform: transform, to: &tmp)
            out.append(contentsOf: cur == nil ? tmp : Array(tmp.dropFirst()))
        }
    }

    public mutating func addArc(
        tangent1End: CGPoint,
        tangent2End: CGPoint,
        radius: CGFloat,
        transform: CGAffineTransform = .identity
    ) {
        mosAppend { cur, out in
            var tmp: [Path.Element] = cur.map { [.move(to: $0)] } ?? []
            MOSPathGeometry.appendArc(tangent1End: tangent1End, tangent2End: tangent2End, radius: radius, transform: transform, to: &tmp)
            out.append(contentsOf: cur == nil ? tmp : Array(tmp.dropFirst()))
        }
    }

    public mutating func addPath(
        _ path: Path,
        transform: CGAffineTransform = .identity
    ) {
        // FIXME: Use RB instead of CG
        if case .empty = storage {
            self = path.applying(transform)
        } else if !path.isEmpty {
            #if canImport(CoreGraphics)
            let result = cgPath.mutableCopy()!
            result.addPath(path.cgPath, transform: transform)
            self = Path(result)
            #else
            let added = MOSPathGeometry.transformed(path.mosElements, transform)   // mOS
            mosAppend { _, out in out.append(contentsOf: added) }
            #endif
        }
    }

    public var currentPoint: CGPoint? {
        get {
            // mOS
            switch storage {
            case .empty: return nil
            case let .path(box): return box.mosCurrentPoint
            default: return MOSPathGeometry.currentPoint(mosElements)
            }
        }
    }

    @available(OpenSwiftUI_v5_0, *)
    public func normalized(eoFill: Bool = true) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    @available(OpenSwiftUI_v5_0, *)
    public func intersection(
        _ other: Path,
        eoFill: Bool = false
    ) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    @available(OpenSwiftUI_v5_0, *)
    public func union(
        _ other: Path,
        eoFill: Bool = false
    ) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    @available(OpenSwiftUI_v5_0, *)
    public func subtracting(
        _ other: Path,
        eoFill: Bool = false
    ) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    @available(OpenSwiftUI_v5_0, *)
    public func symmetricDifference(
        _ other: Path,
        eoFill: Bool = false
    ) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    @available(OpenSwiftUI_v5_0, *)
    public func lineIntersection(
        _ other: Path,
        eoFill: Bool = false
    ) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    @available(OpenSwiftUI_v5_0, *)
    public func lineSubtraction(
        _ other: Path,
        eoFill: Bool = false
    ) -> Path {
        _openSwiftUIUnimplementedFailure()
    }

    package mutating func formTrivialUnion(_ path: Path) {
        // TBA
        guard !path.isEmpty else {
            return
        }
        if let lhs = roundedRect(), let rhs = path.roundedRect() {
            if lhs.contains(rhs) {
                return
            }
            if rhs.contains(lhs) {
                self = path
                return
            }
        }
        addPath(path)
    }

    public func applying(_ transform: CGAffineTransform) -> Path {
        guard !transform.isIdentity else {
            return self
        }
        switch storage {
        case .empty:
            return self
        case let .rect(rect) where transform.isRectilinear:
            return Path(rect.applying(transform))
        case let .ellipse(rect) where transform.isRectilinear:
            return Path(ellipseIn: rect.applying(transform))
        case let .roundedRect(fixedRoundedRect) where transform.isRectilinear:
            return Path(storage: .roundedRect(fixedRoundedRect.applying(transform)))
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        default:
            // TBA
            #if canImport(CoreGraphics)
            var transform = transform
            return Path(cgPath.copy(using: &transform)!)
            #else
            return Path(mosElements: MOSPathGeometry.transformed(mosElements, transform))   // mOS
            #endif
        }
    }

    public func offsetBy(
        dx: CGFloat,
        dy: CGFloat
    ) -> Path {
        applying(CGAffineTransform(translationX: dx, y: dy))   // mOS
    }

    func mapPoints(_ body: (inout [CGPoint]) -> ()) -> Path {
        // TBA
        switch storage {
        case .empty:
            return self
        case let .rect(rect):
            var points = rect.cornerPoints
            body(&points)
            if let rect = CGRect(exactCornerPoints: points) {
                return Path(rect)
            }
            #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES   // mOS: CGMutablePath do OpenCoreGraphics
            let result = CGMutablePath()
            result.move(to: points[0])
            for point in points[1..<4] {
                result.addLine(to: point)
            }
            result.closeSubpath()
            return Path(result)
            #else
            _openSwiftUIPlatformUnimplementedFailure()
            #endif
        case let .ellipse(rect):
            var points = rect.cornerPoints
            body(&points)
            if let mapped = CGRect(exactCornerPoints: points),
               mapped.width == rect.width,
               mapped.height == rect.height {
                return Path(ellipseIn: mapped)
            }
        case let .roundedRect(rect):
            var points = rect.rect.cornerPoints
            body(&points)
            if let mapped = CGRect(exactCornerPoints: points),
               mapped.width == rect.rect.width,
               mapped.height == rect.rect.height {
                return Path(storage: .roundedRect(FixedRoundedRect(
                    mapped,
                    cornerSize: rect.cornerSize,
                    style: rect.style
                )))
            }
        case .stroked, .trimmed:
            _openSwiftUIUnreachableCode()
        case .path:
            break
        }
        #if canImport(CoreGraphics) || !OPENSWIFTUI_CF_CGTYPES   // mOS: CGMutablePath do OpenCoreGraphics
        var points: [CGPoint] = []
        forEach { element in
            switch element {
            case let .move(point), let .line(point):
                points.append(point)
            case let .quadCurve(point, control):
                points.append(contentsOf: [point, control])
            case let .curve(point, control1, control2):
                points.append(contentsOf: [point, control1, control2])
            case .closeSubpath:
                break
            }
        }
        body(&points)
        let result = CGMutablePath()
        var index = 0
        forEach { element in
            switch element {
            case .move:
                result.move(to: points[index])
                index += 1
            case .line:
                result.addLine(to: points[index])
                index += 1
            case .quadCurve:
                result.addQuadCurve(to: points[index], control: points[index + 1])
                index += 2
            case .curve:
                result.addCurve(to: points[index], control1: points[index + 1], control2: points[index + 2])
                index += 3
            case .closeSubpath:
                result.closeSubpath()
            }
        }
        return Path(result)
        #else
        _openSwiftUIPlatformUnimplementedFailure()
        #endif
    }
}

private func ellipseContains(in rect: CGRect, contains point: CGPoint) -> Bool {
    var rect = rect
    var point = point
    if rect.width != rect.height {
        let scale = rect.height / rect.width
        rect.origin.x *= scale
        rect.size.width *= scale
        point.x *= scale
    }
    let dx = point.x - (rect.origin.x + rect.width / 2)
    let dy = point.y - (rect.origin.y + rect.height / 2)
    let radius = rect.width / 2
    return dx * dx + dy * dy < radius * radius
}

// MARK: - RenderBox

private let temporaryPathCallbacks: UnsafePointer<ORBPath.Callbacks> = {
    let pointer = UnsafeMutablePointer<ORBPath.Callbacks>.allocate(capacity: 1)
    var callbacks = ORBPath.empty.callbacks.pointee
    callbacks.retain = { _ in
        _openSwiftUIUnreachableCode()
    }
    callbacks.release = { _ in
        _openSwiftUIUnreachableCode()
    }
    callbacks.apply = { object, info, callback in
        let storage = unsafeBitCast(object, to: ORBPath.Storage.self)
        return storage.apply(info: info, callback: callback)
    }
    callbacks.isEqual = { lhs, rhs in
        let lhs = unsafeBitCast(lhs, to: ORBPath.Storage.self)
        let rhs = unsafeBitCast(rhs, to: ORBPath.Storage.self)
        return lhs.isEqual(to: rhs)
    }
    callbacks.isEmpty = { object in
        let storage = unsafeBitCast(object, to: ORBPath.Storage.self)
        return storage.isEmpty
    }
    callbacks.isSingleElement = { object in
        let storage = unsafeBitCast(object, to: ORBPath.Storage.self)
        return storage.isSingleElement
    }
    callbacks.boundingRect = { object in
        let storage = unsafeBitCast(object, to: ORBPath.Storage.self)
        return storage.boundingRect
    }
    callbacks.cgPath = { object in
        let storage = unsafeBitCast(object, to: ORBPath.Storage.self)
        let cgPath = storage.cgPath
        return cgPath.map { .passUnretained($0) }
    }
    pointer.initialize(to: callbacks)
    return UnsafePointer(pointer)
}()

extension ORBPath {
    static func withTemporaryPath<R>(
        do body: (ORBPath) -> R,
        builder: (UnsafeMutableRawPointer) -> ()
    ) -> R {
        return withUnsafeTemporaryAllocation(
            of: UInt8.self,
            capacity: 128
        ) { bufferPointer in
            let pointer = UnsafeMutableRawBufferPointer(
                mutating: UnsafeRawBufferPointer(bufferPointer)
            ).baseAddress!
            let storage: ORBPath.Storage = unsafeBitCast(pointer, to: ORBPath.Storage.self)
            storage.initialize(capacity: 128, source: nil)
            builder(pointer)
            let path = ORBPath(
                storage: storage,
                callbacks: temporaryPathCallbacks
            )
            let result = body(path)
            storage.destroy()
            return result
        }
    }
}
