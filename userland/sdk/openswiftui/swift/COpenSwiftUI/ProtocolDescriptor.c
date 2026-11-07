/* Endereço dos descritores de protocolo que o grafo de views percorre.
   O módulo Swift chama _mos_pd. Os símbolos são os do OpenSwiftUI (abi) e do OpenCombine. */
extern const char $s11OpenCombine16ObservableObjectMp;
extern const char $s11OpenSwiftUI8CommandsMp;
extern const char $s11OpenSwiftUI5SceneMp;
extern const char $s11OpenSwiftUI15PreviewProviderMp;

const void *_mos_pd(int which) {
    switch (which) {
    case 0: return &$s11OpenCombine16ObservableObjectMp;
    case 1: return &$s11OpenSwiftUI8CommandsMp;
    case 2: return &$s11OpenSwiftUI5SceneMp;
    default: return &$s11OpenSwiftUI15PreviewProviderMp;
    }
}
