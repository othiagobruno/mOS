// mOS — o TLS da transação de observação para o OpenSwiftUICore. O OpenObservation exportava `_ThreadLocal`; o
// Observation do swift-src (a ABI da Apple, tools/build-swift-foundation-objc.sh) o deixa interno. O valor é o mesmo
// ponteiro que o ObservationRegistrar.access lê: as duas funções vêm de userland/sdk/swift-objc/Observation/ThreadLocal.c,
// que o libswiftObservation do mOS exporta para isto.
@_silgen_name("_swift_observation_tls_get")
func _mosObservationTLSGet() -> UnsafeMutableRawPointer?

@_silgen_name("_swift_observation_tls_set")
func _mosObservationTLSSet(_ value: UnsafeMutableRawPointer?)

struct _ThreadLocal {
    static var value: UnsafeMutableRawPointer? {
        get { _mosObservationTLSGet() }
        set { _mosObservationTLSSet(newValue) }
    }
}
