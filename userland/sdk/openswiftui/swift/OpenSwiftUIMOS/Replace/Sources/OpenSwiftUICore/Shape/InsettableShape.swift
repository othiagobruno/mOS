//
//  InsettableShape.swift
//  OpenSwiftUICore
//
//  Audited for 6.0.87
//  Status: WIP

public import Foundation

// MARK: - InsettableShape

/// A shape type that is able to inset itself to produce another shape.
public protocol InsettableShape: Shape {
    
    /// The type of the inset shape.
    associatedtype InsetShape: InsettableShape
    
    /// Returns `self` inset by `amount`.
    func inset(by amount: CGFloat) -> InsetShape
}

// MARK: - InsettableShape + Extension (disfavoredOverload)

extension InsettableShape {
    /// Returns a view that is the result of insetting `self` by
    /// `style.lineWidth / 2`, stroking the resulting shape with
    /// `style`, and then filling with `content`.
    @inlinable
    @_disfavoredOverload
    public func strokeBorder<S>(_ content: S, style: StrokeStyle, antialiased: Bool = true) -> some View where S: ShapeStyle {
        inset(by: style.lineWidth * 0.5)
             .stroke(style: style)
             .fill(content, style: FillStyle(antialiased: antialiased))
    }

    /// Returns a view that is the result of insetting `self` by
    /// `style.lineWidth / 2`, stroking the resulting shape with
    /// `style`, and then filling with the foreground color.
    @inlinable
    @_disfavoredOverload
    public func strokeBorder(style: StrokeStyle, antialiased: Bool = true) -> some View {
        inset(by: style.lineWidth * 0.5)
             .stroke(style: style)
             .fill(style: FillStyle(antialiased: antialiased))
    }

    /// Returns a view that is the result of filling the `lineWidth`-sized
    /// border (aka inner stroke) of `self` with `content`. This is
    /// equivalent to insetting `self` by `lineWidth / 2` and stroking the
    /// resulting shape with `lineWidth` as the line-width.
    @inlinable
    @_disfavoredOverload
    public func strokeBorder<S>(_ content: S, lineWidth: CGFloat = 1, antialiased: Bool = true) -> some View where S: ShapeStyle {
        strokeBorder(
            content,
            style: StrokeStyle(lineWidth: lineWidth),
            antialiased: antialiased
        )
    }

    /// Returns a view that is the result of filling the `lineWidth`-sized
    /// border (aka inner stroke) of `self` with the foreground color.
    /// This is equivalent to insetting `self` by `lineWidth / 2` and
    /// stroking the resulting shape with `lineWidth` as the line-width.
    @inlinable
    @_disfavoredOverload
    public func strokeBorder(lineWidth: CGFloat = 1, antialiased: Bool = true) -> some View {
        strokeBorder(
            style: StrokeStyle(lineWidth: lineWidth),
            antialiased: antialiased
        )
    }
}

// MARK: - Retangle + InsettableShape

extension Rectangle: InsettableShape {
    @inlinable
    public func inset(by amount: CGFloat) -> some InsettableShape {
        _Inset(amount: amount)
    }

    @usableFromInline
    @frozen
    struct _Inset: InsettableShape {
        @usableFromInline
        var amount: CGFloat
    
        @inlinable
        init(amount: CGFloat) {
            self.amount = amount
        }

        @usableFromInline
        nonisolated func path(in rect: CGRect) -> Path {
            Path(rect.insetBy(dx: amount, dy: amount))
        }

        @usableFromInline
        nonisolated var layoutDirectionBehavior: LayoutDirectionBehavior {
            .fixed
        }
        
        @usableFromInline
        var animatableData: CGFloat {
            get { amount }
            set { amount = newValue }
        }
    
        @inlinable
        func inset(by amount: CGFloat) -> Self {
            var copy = self
            copy.amount += amount
            return copy
        }
    }
}

// MARK: - RoundedRectangle + InsettableShape

extension RoundedRectangle: InsettableShape {
    @inlinable
    public func inset(by amount: CGFloat) -> some InsettableShape {
        _Inset(base: self, amount: amount)
    }

    @usableFromInline
    @frozen
    struct _Inset: InsettableShape {
        @usableFromInline
        var base: RoundedRectangle
        
        @usableFromInline
        var amount: CGFloat
    
        @inlinable
        init(base: RoundedRectangle, amount: CGFloat) {
            (self.base, self.amount) = (base, amount)
        }

        // mOS: o canto diminui junto com o retângulo (borda interna paralela à externa)
        @usableFromInline
        nonisolated func path(in rect: CGRect) -> Path {
            let r = rect.insetBy(dx: amount, dy: amount)
            guard !r.isNull, !r.isEmpty else { return Path() }
            let size = CGSize(width: max(base.cornerSize.width - amount, 0), height: max(base.cornerSize.height - amount, 0))
            return Path(roundedRect: r, cornerSize: size, style: base.style)
        }

        @usableFromInline
        nonisolated var layoutDirectionBehavior: LayoutDirectionBehavior {
            .fixed
        }
        
        @usableFromInline
        var animatableData: AnimatablePair<RoundedRectangle.AnimatableData, CGFloat> {
            get { AnimatablePair(base.animatableData, amount) }
            set { (base.animatableData, amount) = (newValue.first, newValue.second) }
        }
    
        @inlinable
        func inset(by amount: CGFloat) -> Self {
            var copy = self
            copy.amount += amount
            return copy
        }
    }
}

// MARK: - UnevenRoundedRectangle + InsettableShape

extension UnevenRoundedRectangle: InsettableShape {
    @inlinable
    public func inset(by amount: CGFloat) -> some InsettableShape {
        _Inset(base: self, amount: amount)
    }

    @usableFromInline
    @frozen
    struct _Inset: InsettableShape {
        @usableFromInline
        var base: UnevenRoundedRectangle
        
        @usableFromInline
        var amount: CGFloat
    
        @inlinable
        init(base: UnevenRoundedRectangle, amount: CGFloat) {
            (self.base, self.amount) = (base, amount)
        }

        // mOS: cada raio diminui de `amount`
        @usableFromInline
        nonisolated func path(in rect: CGRect) -> Path {
            let r = rect.insetBy(dx: amount, dy: amount)
            guard !r.isNull, !r.isEmpty else { return Path() }
            let c = base.cornerRadii
            let radii = RectangleCornerRadii(
                topLeft: max(c.topLeft - amount, 0), topRight: max(c.topRight - amount, 0),
                bottomRight: max(c.bottomRight - amount, 0), bottomLeft: max(c.bottomLeft - amount, 0)
            )
            return Path(roundedRect: r, cornerRadii: radii, style: base.style)
        }
        
        @usableFromInline
        var animatableData: AnimatablePair<UnevenRoundedRectangle.AnimatableData, CGFloat> {
            get { AnimatablePair(base.animatableData, amount) }
            set { (base.animatableData, amount) = (newValue.first, newValue.second) }
        }
    
        @inlinable
        func inset(by amount: CGFloat) -> Self {
            var copy = self
            copy.amount += amount
            return copy
        }
    }
}

// MARK: - Capsule + InsettableShape

extension Capsule: InsettableShape {
    @inlinable
    public func inset(by amount: CGFloat) -> some InsettableShape {
        _Inset(amount: _Inset._makeInset(amount, style: style))
    }

    @usableFromInline
    @frozen
    struct _Inset: InsettableShape {
        @usableFromInline
        var amount: CGFloat
    
        @inlinable
        init(amount: CGFloat) {
            self.amount = amount
        }

        // mOS: o estilo do canto vem no bit baixo de `amount` (_makeInset)
        @usableFromInline
        nonisolated func path(in rect: CGRect) -> Path {
            let (inset, style) = Self._extractInset(amount)
            let r = rect.insetBy(dx: inset, dy: inset)
            guard !r.isNull, !r.isEmpty else { return Path() }
            return Capsule(style: style).path(in: r)
        }

        @usableFromInline
        nonisolated var layoutDirectionBehavior: LayoutDirectionBehavior {
            .fixed
        }

        @usableFromInline
        var animatableData: CGFloat {
            get { amount }
            set { amount = newValue }
        }

        @inlinable
        func inset(by amount: CGFloat) -> Self {
            let (inset, style) = Self._extractInset(self.amount)
            return Self(amount: Self._makeInset(inset + amount, style: style))
        }
        
        @_alwaysEmitIntoClient
        static func _makeInset(_ inset: CGFloat, style: RoundedCornerStyle) -> CGFloat {
            var u = unsafeBitCast(inset, to: UInt.self)
            u = (u & ~1) | (style == .circular ? 0 : 1)
            return unsafeBitCast(u, to: CGFloat.self)
        }
        
        @_alwaysEmitIntoClient
        static func _extractInset(_ inset: CGFloat) -> (CGFloat, RoundedCornerStyle) {
            let u = unsafeBitCast(inset, to: UInt.self)
            return (
                unsafeBitCast(u & ~1, to: CGFloat.self),
                (u & 1) == 0 ? .circular : .continuous
            )
        }
    }
}

// MARK: - Ellipse + InsettableShape

extension Ellipse: InsettableShape {
    @inlinable
    public func inset(by amount: CGFloat) -> some InsettableShape {
        _Inset(amount: amount)
    }

    @usableFromInline
    @frozen
    struct _Inset: InsettableShape {
        @usableFromInline
        var amount: CGFloat
    
        @inlinable
        init(amount: CGFloat) {
            self.amount = amount
        }

        @usableFromInline
        nonisolated func path(in rect: CGRect) -> Path {
            let r = rect.insetBy(dx: amount, dy: amount)   // mOS
            guard !r.isNull, !r.isEmpty else { return Path() }
            return Path(ellipseIn: r)
        }

        @usableFromInline
        nonisolated var layoutDirectionBehavior: LayoutDirectionBehavior {
            .fixed
        }

        @usableFromInline
        var animatableData: CGFloat {
            get { amount }
            set { amount = newValue }
        }

        @inlinable
        func inset(by amount: CGFloat) -> Self {
            var copy = self
            copy.amount += amount
            return copy
        }
    }
}

// MARK: - Circle + InsettableShape

extension Circle: InsettableShape {
    @inlinable
    public func inset(by amount: CGFloat) -> some InsettableShape {
        _Inset(amount: amount)
    }

    @usableFromInline
    @frozen
    struct _Inset: InsettableShape {
        @usableFromInline
        var amount: CGFloat
    
        @inlinable
        init(amount: CGFloat) {
            self.amount = amount
        }

        @usableFromInline
        nonisolated func path(in rect: CGRect) -> Path {
            let r = rect.insetBy(dx: amount, dy: amount)   // mOS
            guard !r.isNull, !r.isEmpty else { return Path() }
            return Circle().path(in: r)
        }

        @usableFromInline
        nonisolated var layoutDirectionBehavior: LayoutDirectionBehavior {
            .fixed
        }
        
        @usableFromInline
        var animatableData: CGFloat {
            get { amount }
            set { amount = newValue }
        }
    
        @inlinable
        func inset(by amount: CGFloat) -> InsetShape {
            var copy = self
            copy.amount += amount
            return copy
        }

        @usableFromInline
        typealias InsetShape = Self
    }
}

extension Rectangle {
    struct AsymmetricalInset: Shape {
        let rectangle: Rectangle
        let insets: EdgeInsets

        func path(in rect: CGRect) -> Path {
            rectangle.path(in: rect.inset(by: insets))
        }
    }

    package func outset(by insets: EdgeInsets) -> some Shape {
        AsymmetricalInset(rectangle: self, insets: -insets)
    }
}
