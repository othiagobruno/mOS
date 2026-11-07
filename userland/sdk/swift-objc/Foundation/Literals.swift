// mOS — overlay Foundation (fase 5, marco A): literais e iteração das classes ObjC, com as assinaturas da Apple
// (os binários importam NSString.init(stringLiteral:), NSNumber.init(integerLiteral:), NSDictionary.init(dictionaryLiteral:),
// NSArray.makeIterator(), NSEnumerator.makeIterator(), NSFastEnumerationIterator.next()...).

// MARK: - NSString

extension NSString: ExpressibleByStringLiteral {
    public required convenience init(unicodeScalarLiteral value: StaticString) { self.init(stringLiteral: value) }
    public required convenience init(extendedGraphemeClusterLiteral value: StaticString) { self.init(stringLiteral: value) }
    public required convenience init(stringLiteral value: StaticString) {
        let u = Array(value.description.utf16)
        self.init(characters: u, length: u.count)
    }
}

extension NSString {
    public convenience init(string aString: String) {
        let u = Array(aString.utf16)
        self.init(characters: u, length: u.count)
    }

    public convenience init(format: __shared NSString, _ args: CVarArg...) {
        self.init(format: format as String, arguments: args)
    }

    public convenience init(format: String, _ args: CVarArg...) {
        self.init(format: format, arguments: args)
    }

    public convenience init(format: String, arguments: [CVarArg]) {
        let f = _mosNSString(format)
        let s = withVaList(arguments) { NSString(format: f, arguments: $0) }
        self.init(string: _mosString(s))
    }
}

// MARK: - NSNumber

extension NSNumber: ExpressibleByIntegerLiteral, ExpressibleByFloatLiteral, ExpressibleByBooleanLiteral {
    public required convenience init(integerLiteral value: Int) { self.init(integer: value) }
    public required convenience init(floatLiteral value: Double) { self.init(double: value) }
    public required convenience init(booleanLiteral value: Bool) { self.init(integer: value ? 1 : 0) }
}

// MARK: - NSArray / NSDictionary / NSSet

extension NSArray: ExpressibleByArrayLiteral {
    public required convenience init(arrayLiteral elements: Any...) {
        let objs = elements.map { _mosObject($0) }
        self.init(array: objs)
    }

    public convenience init(array: [AnyObject]) {
        self.init(objects: array, count: array.count)
    }
}

extension NSDictionary: ExpressibleByDictionaryLiteral {
    public required convenience init(dictionaryLiteral elements: (Any, Any)...) {
        let ks = elements.map { unsafeBitCast(_mosObject($0.0), to: NSCopying.self) }
        let vs = elements.map { _mosObject($0.1) }
        self.init(objects: vs, forKeys: ks, count: vs.count)
    }
}

// MARK: - iteração (NSFastEnumeration)

public struct NSFastEnumerationIterator: IteratorProtocol {
    let enumerable: NSFastEnumeration
    var state = [NSFastEnumerationState(state: 0, itemsPtr: nil, mutationsPtr: nil,
                                        extra: (0, 0, 0, 0, 0))]
    var objects: [Unmanaged<AnyObject>?] = Array(repeating: nil, count: 16)
    var n = 0
    var count = 0

    public init(_ enumerable: NSFastEnumeration) { self.enumerable = enumerable }

    mutating func refresh() {
        n = 0
        count = objects.withUnsafeMutableBufferPointer { b in
            b.baseAddress!.withMemoryRebound(to: AnyObject?.self, capacity: 16) { p in
                enumerable.countByEnumerating(with: &state[0], objects: AutoreleasingUnsafeMutablePointer(p), count: 16)
            }
        }
    }

    public mutating func next() -> Any? {
        if n >= count { refresh(); if count == 0 { return nil } }
        let items = state[0].itemsPtr!
        let v: AnyObject? = items[n]
        n += 1
        return v
    }
}

extension NSArray: Sequence {
    public func makeIterator() -> NSFastEnumerationIterator { NSFastEnumerationIterator(self) }
}

extension NSDictionary: Sequence {
    public struct Iterator: IteratorProtocol {
        var it: NSFastEnumerationIterator
        let dict: NSDictionary
        public mutating func next() -> (key: Any, value: Any)? {
            guard let k = it.next() else { return nil }
            return (key: k, value: dict.object(forKey: k as AnyObject) as Any)
        }
    }
    public func makeIterator() -> Iterator { Iterator(it: NSFastEnumerationIterator(self), dict: self) }
}

extension NSSet: Sequence {
    public func makeIterator() -> NSFastEnumerationIterator { NSFastEnumerationIterator(self) }
}

extension NSEnumerator: Sequence {
    // como na Apple: NSEnumerator também é NSFastEnumeration
    public func makeIterator() -> NSFastEnumerationIterator { NSFastEnumerationIterator(self) }
}
