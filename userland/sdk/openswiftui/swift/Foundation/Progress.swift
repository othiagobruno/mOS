// mOS — Progress e FileHandle que o OpenSwiftUI nomeia no Foundation (o swift-foundation não os traz).

open class Progress: NSObject, @unchecked Sendable {
    open var totalUnitCount: Int64 = 0
    open var completedUnitCount: Int64 = 0
    open var localizedDescription: String = ""
    open var localizedAdditionalDescription: String = ""
    open var isIndeterminate: Bool { totalUnitCount <= 0 }
    open var fractionCompleted: Double {
        guard totalUnitCount > 0 else { return 0 }
        return min(1, Double(completedUnitCount) / Double(totalUnitCount))
    }
    public override init() { super.init() }
}

open class FileHandle: NSObject, @unchecked Sendable {
    public let fileDescriptor: Int32
    public init(fileDescriptor: Int32) { self.fileDescriptor = fileDescriptor }
    public convenience init?(forReadingAtPath path: String) { self.init(fileDescriptor: -1) }
}
