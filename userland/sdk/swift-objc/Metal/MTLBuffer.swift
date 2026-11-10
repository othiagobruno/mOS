import Metal

@_silgen_name("mos_mtlbuffer_did_modify")
func mos_mtlbuffer_did_modify(_ buf: MTLBuffer, _ loc: Int, _ len: Int)

extension MTLBuffer {
    /// Overlay da Apple: `Range<Int>` vira `NSRange` e chama `-didModifyRange:`.
    public func didModifyRange(_ range: Range<Int>) {
        mos_mtlbuffer_did_modify(self, range.lowerBound, range.count)
    }
}
