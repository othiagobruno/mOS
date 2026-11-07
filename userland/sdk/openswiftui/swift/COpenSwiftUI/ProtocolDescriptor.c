/* Endereço dos descritores de protocolo que o grafo de views percorre.
   O módulo Swift chama _mos_pd. O ABI público é SwiftUI, como nos binários da Apple. O OpenCombine fica no nome dele. */
extern const char $s11OpenCombine16ObservableObjectMp;
extern const char $s7SwiftUI8CommandsMp;
extern const char $s7SwiftUI5SceneMp;
extern const char $s7SwiftUI15PreviewProviderMp;

const void *_mos_pd(int which) {
    switch (which) {
    case 0: return &$s11OpenCombine16ObservableObjectMp;
    case 1: return &$s7SwiftUI8CommandsMp;
    case 2: return &$s7SwiftUI5SceneMp;
    default: return &$s7SwiftUI15PreviewProviderMp;
    }
}
