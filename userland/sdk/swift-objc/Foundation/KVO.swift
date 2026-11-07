// mOS — overlay Foundation (fase 5, marco B): o KVO do Swift (observe(_:options:changeHandler:) sobre KeyPath), com a
// API e os nomes da Apple, sobre o addObserver:forKeyPath:options:context: do Foundation do mOS. O caminho de chave vem
// do _kvcKeyPathString da stdlib (KeyPaths de propriedades @objc).

public protocol _KeyValueCodingAndObserving {}

extension NSObject: _KeyValueCodingAndObserving {}

public struct NSKeyValueObservedChange<Value> {
    public let newValue: Value?
    public let oldValue: Value?
    public let indexes: IndexSet?
    public let isPrior: Bool
}

/// o observador ObjC que o Foundation do mOS chama; repassa ao bloco Swift
internal final class _MOSKVOHelper: NSObject {
    let callback: (NSDictionary?) -> Void
    init(_ callback: @escaping (NSDictionary?) -> Void) { self.callback = callback; super.init() }
    override func observeValue(forKeyPath keyPath: String?, of object: Any?, change: [String: Any]?, context: UnsafeMutableRawPointer?) {
        callback(change.map { ($0 as [String: Any])._bridgeToObjectiveC() })
    }
}

public class NSKeyValueObservation: NSObject {
    internal weak var _object: NSObject?
    internal let _keyPath: String
    internal var _helper: _MOSKVOHelper?

    internal init(object: NSObject, keyPath: String, helper: _MOSKVOHelper) {
        _object = object
        _keyPath = keyPath
        _helper = helper
        super.init()
    }

    /// para de observar (também acontece ao soltar a observação)
    public func invalidate() {
        if let o = _object, let h = _helper { o.removeObserver(h, forKeyPath: _mosNSString(_keyPath) as String, context: nil) }
        _helper = nil
    }

    deinit { invalidate() }
}

extension _KeyValueCodingAndObserving {
    public func observe<Value>(_ keyPath: KeyPath<Self, Value>, options: NSKeyValueObservingOptions = [],
                               changeHandler: @escaping (Self, NSKeyValueObservedChange<Value>) -> Void) -> NSKeyValueObservation {
        let obj = self as AnyObject as! NSObject
        guard let path = keyPath._kvcKeyPathString else {
            fatalError("KVO: \(keyPath) não é um caminho @objc")
        }
        let me = self
        let helper = _MOSKVOHelper { change in
            func value(_ k: String) -> Value? {
                guard let c = change, let v = c.object(forKey: _mosNSString(k)) else { return nil }
                return v as? Value
            }
            let prior = (change?.object(forKey: _mosNSString("notificationIsPrior")) as? NSNumber)?.boolValue ?? false
            changeHandler(me, NSKeyValueObservedChange(newValue: value("new"), oldValue: value("old"), indexes: nil, isPrior: prior))
        }
        obj.addObserver(helper, forKeyPath: path, options: options, context: nil)
        return NSKeyValueObservation(object: obj, keyPath: path, helper: helper)
    }
}
