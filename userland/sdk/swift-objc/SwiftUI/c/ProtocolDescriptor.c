/* mOS — endereços dos descritores de protocolo que o grafo de views percorre (SwiftUI.framework, variante ObjC).
   Como em userland/sdk/openswiftui/swift/COpenSwiftUI/ProtocolDescriptor.c, mas o ObservableObject é o do
   Combine.framework (nome da Apple). */
extern const char $s7Combine16ObservableObjectMp;
extern const char $s7SwiftUI8CommandsMp;
extern const char $s7SwiftUI5SceneMp;
extern const char $s7SwiftUI15PreviewProviderMp;

const void *_mos_pd(int which) {
    switch (which) {
    case 0: return &$s7Combine16ObservableObjectMp;
    case 1: return &$s7SwiftUI8CommandsMp;
    case 2: return &$s7SwiftUI5SceneMp;
    default: return &$s7SwiftUI15PreviewProviderMp;
    }
}
