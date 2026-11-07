// mOS — overlay Swift do CoreData.framework (no SDK da Apple ele fica dentro do próprio framework): o fetch genérico
// que binários Swift da Apple importam. Usa o -executeFetchRequest:error: do CoreData.m do mOS.
// tools/build-frameworks-objc.ps1

@_exported import CoreData

extension NSManagedObjectContext {
    public func fetch<T>(_ request: NSFetchRequest<T>) throws -> [T] where T: NSFetchRequestResult {
        guard let list = __executeFetchRequestMOS(request) else { return [] }
        var out: [T] = []
        let n = Int(list.count())
        out.reserveCapacity(n)
        for i in 0..<n { out.append(list.object(at: UInt(i)) as! T) }
        return out
    }

    private func __executeFetchRequestMOS(_ request: NSFetchRequest<some NSFetchRequestResult>) -> NSArray? {
        execute(unsafeBitCast(request, to: NSFetchRequest<any NSFetchRequestResult>.self), error: nil)
    }
}
