// mOS — nomes da ABI do System.framework do macOS que o swift-system novo mudou. Compilado junto do swift-system
// (tools/build-swift-foundation-objc.sh), no mesmo módulo System.
//
// O System do macOS 11-14 tinha FileDescriptor._read(into:retryOnInterrupt:) com `throws`; o código inlinado de
// FileDescriptor.read(into:) dos apps chama esse nome. O swift-system atual tirou o `throws` (mesmo comportamento:
// o erro vem no Result). O nome antigo sai aqui e chama o atual.

extension FileDescriptor {
    @_silgen_name("$s6System14FileDescriptorV5_read4into16retryOnInterrupts6ResultOySiAA5ErrnoVGSw_SbtKF")
    @usableFromInline
    internal func _mosReadThrowingABI(
        into buffer: UnsafeMutableRawBufferPointer, retryOnInterrupt: Bool
    ) throws -> Result<Int, Errno> {
        _read(into: buffer, retryOnInterrupt: retryOnInterrupt)
    }
}
