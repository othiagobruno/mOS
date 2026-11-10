import Metal

@_silgen_name("mos_mtlbuffer_did_modify")
func mos_mtlbuffer_did_modify(_ buf: MTLBuffer, _ loc: Int, _ len: Int)

extension MTLBuffer {
    /// Quem compila contra este módulo chama o helper em C. O nome que o
    /// OpenBot importa de libswiftMetal.dylib é o de mos_mtl_modify.c;
    /// este silgen evita um segundo símbolo com o mesmo mangling.
    @_silgen_name("mos_mtl_did_modify_overlay")
    public func didModifyRange(_ range: Range<Int>) {
        mos_mtlbuffer_did_modify(self, range.lowerBound, range.count)
    }
}
