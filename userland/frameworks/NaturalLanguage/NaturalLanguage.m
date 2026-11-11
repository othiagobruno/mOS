/*
 * mOS — NaturalLanguage.framework (Mach-O, em /System/Library/Frameworks): NLLanguageRecognizer, a detecção de idioma.
 * O Setapp (e o WhatsApp) chamam -processString:, -dominantLanguage e o languageHypotheses(withMaximum:) do overlay
 * Swift (NaturalLanguage.swift, em /usr/lib/swift/libswiftNaturalLanguage.dylib).
 *
 * O modelo da Apple não existe aqui. O reconhecedor do mOS conta, no texto, o sistema de escrita de cada letra
 * (latino, cirílico, grego, árabe, hebraico, devanágari, tailandês, hangul, kana, han) e, no latino, as palavras
 * funcionais mais comuns de cada idioma (en, pt, es, fr, de, it, nl, sv, pl, tr). As hipóteses são essas contagens
 * normalizadas para somar 1. languageHints multiplica o peso de cada idioma; languageConstraints restringe a lista.
 * tools/build-frameworks-objc.sh
 */
#import <AppKit/AppKit.h>
#include <string.h>

#define NL_CONST(var, text) NSString *const var = @text;
NL_CONST(NLLanguageUndetermined, "und")
NL_CONST(NLLanguageEnglish, "en") NL_CONST(NLLanguagePortuguese, "pt") NL_CONST(NLLanguageSpanish, "es")
NL_CONST(NLLanguageFrench, "fr") NL_CONST(NLLanguageGerman, "de") NL_CONST(NLLanguageItalian, "it")
NL_CONST(NLLanguageDutch, "nl") NL_CONST(NLLanguageSwedish, "sv") NL_CONST(NLLanguagePolish, "pl")
NL_CONST(NLLanguageTurkish, "tr") NL_CONST(NLLanguageRussian, "ru") NL_CONST(NLLanguageUkrainian, "uk")
NL_CONST(NLLanguageGreek, "el") NL_CONST(NLLanguageArabic, "ar") NL_CONST(NLLanguageHebrew, "he")
NL_CONST(NLLanguageHindi, "hi") NL_CONST(NLLanguageThai, "th") NL_CONST(NLLanguageKorean, "ko")
NL_CONST(NLLanguageJapanese, "ja") NL_CONST(NLLanguageSimplifiedChinese, "zh-Hans")
NL_CONST(NLLanguageTraditionalChinese, "zh-Hant")

/* idiomas que o reconhecedor sabe distinguir, na ordem da tabela de pesos */
enum { L_EN, L_PT, L_ES, L_FR, L_DE, L_IT, L_NL, L_SV, L_PL, L_TR, L_RU, L_EL, L_AR, L_HE, L_HI, L_TH, L_KO, L_JA, L_ZH, NL_N };
static NSString *nl_code(int i) {
    static const char *const c[NL_N] = {"en", "pt", "es", "fr", "de", "it", "nl", "sv", "pl", "tr", "ru", "el", "ar", "he", "hi", "th", "ko", "ja", "zh-Hans"};
    return [NSString stringWithUTF8String:c[i]];
}

/* palavras funcionais (minúsculas, UTF-8); cada ocorrência vale um ponto para o idioma */
static const char *const nl_words[10][24] = {
    /* en */ {"the", "and", "is", "of", "to", "in", "that", "it", "you", "for", "with", "this", "are", "was", "have", "not", "on", "be", "what", "but", 0},
    /* pt */ {"o", "a", "os", "as", "de", "que", "e", "do", "da", "não", "um", "uma", "para", "com", "é", "no", "na", "em", "se", "mais", "por", "você", "isso", 0},
    /* es */ {"el", "la", "los", "las", "de", "que", "y", "en", "un", "una", "es", "no", "por", "con", "para", "lo", "del", "se", "pero", "más", "está", "muy", 0},
    /* fr */ {"le", "la", "les", "de", "des", "et", "est", "un", "une", "du", "que", "pas", "pour", "dans", "ce", "qui", "sur", "avec", "je", "vous", "il", "très", 0},
    /* de */ {"der", "die", "das", "und", "ist", "nicht", "ein", "eine", "zu", "den", "mit", "von", "ich", "sie", "es", "auf", "für", "auch", "sich", "dem", "wir", 0},
    /* it */ {"il", "la", "di", "che", "e", "è", "un", "una", "per", "non", "del", "della", "con", "sono", "lo", "gli", "le", "questo", "ma", "più", "anche", 0},
    /* nl */ {"de", "het", "een", "en", "van", "is", "dat", "niet", "ik", "je", "op", "te", "zijn", "met", "voor", "maar", "ook", "wat", "er", 0},
    /* sv */ {"och", "att", "det", "är", "som", "en", "på", "för", "med", "inte", "jag", "av", "till", "den", "har", "om", "ett", "vi", 0},
    /* pl */ {"i", "w", "nie", "się", "na", "że", "to", "jest", "z", "do", "jak", "co", "ale", "tak", "po", "od", "za", "jestem", 0},
    /* tr */ {"ve", "bir", "bu", "da", "de", "için", "ile", "çok", "ne", "daha", "gibi", "olan", "değil", "ama", "ben", "sen", 0},
};

@interface NLLanguageRecognizer : NSObject {
    double _score[NL_N];
    id _hints;          /* NSDictionary idioma -> NSNumber */
    id _constraints;    /* NSArray de idiomas */
}
+ (id)dominantLanguageForString:(id)string;
- (void)processString:(id)string;
- (void)reset;
- (id)dominantLanguage;
- (id)languageHypothesesWithMaximum:(NSUInteger)maxHypotheses;
@property (copy) id languageHints;
@property (copy) id languageConstraints;
@end

static int script_of(unsigned c) {
    if (c >= 0x0400 && c <= 0x052F) return L_RU;
    if (c >= 0x0370 && c <= 0x03FF) return L_EL;
    if (c >= 0x0600 && c <= 0x06FF) return L_AR;
    if (c >= 0x0590 && c <= 0x05FF) return L_HE;
    if (c >= 0x0900 && c <= 0x097F) return L_HI;
    if (c >= 0x0E00 && c <= 0x0E7F) return L_TH;
    if ((c >= 0xAC00 && c <= 0xD7AF) || (c >= 0x1100 && c <= 0x11FF)) return L_KO;
    if (c >= 0x3040 && c <= 0x30FF) return L_JA;
    if (c >= 0x4E00 && c <= 0x9FFF) return L_ZH;
    return -1;
}

@implementation NLLanguageRecognizer
+ (id)dominantLanguageForString:(id)string {
    NLLanguageRecognizer *r = [[[self alloc] init] autorelease];
    [r processString:string];
    return [r dominantLanguage];
}
- (void)dealloc { [_hints release]; [_constraints release]; [super dealloc]; }
- (id)languageHints { return _hints; }
- (void)setLanguageHints:(id)h { id n = [h copy]; [_hints release]; _hints = n; }
- (id)languageConstraints { return _constraints; }
- (void)setLanguageConstraints:(id)c { id n = [c copy]; [_constraints release]; _constraints = n; }
- (void)reset { memset(_score, 0, sizeof _score); }
- (void)processString:(id)string {
    const char *s = string ? [string UTF8String] : 0;
    if (!s) return;
    char word[64];
    int wn = 0, han = 0, kana = 0;
    for (const unsigned char *p = (const unsigned char *)s; ; ) {
        unsigned c = *p;
        int len = 1;
        if (c >= 0xF0 && p[1] && p[2] && p[3]) { c = (c & 7) << 18 | (p[1] & 63) << 12 | (p[2] & 63) << 6 | (p[3] & 63); len = 4; }
        else if (c >= 0xE0 && p[1] && p[2]) { c = (c & 15) << 12 | (p[1] & 63) << 6 | (p[2] & 63); len = 3; }
        else if (c >= 0xC0 && p[1]) { c = (c & 31) << 6 | (p[1] & 63); len = 2; }
        int sc = c ? script_of(c) : -1;
        if (sc == L_ZH) han++;
        if (sc == L_JA) kana++;
        if (sc >= 0 && sc != L_ZH) _score[sc] += 1;
        int letra = c && (sc < 0) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c < 0x250));
        if (letra && wn + len < (int)sizeof word - 1) {
            for (int i = 0; i < len; i++) {
                unsigned char b = p[i];
                word[wn++] = (char)(b >= 'A' && b <= 'Z' ? b + 32 : b);
            }
        } else if (wn) {
            word[wn] = 0;
            for (int l = 0; l < 10; l++)
                for (int i = 0; nl_words[l][i]; i++)
                    if (!strcmp(nl_words[l][i], word)) { _score[l] += 1; break; }
            wn = 0;
        }
        if (!c) break;
        p += len;
    }
    /* han sem kana é chinês; com kana, japonês */
    if (han) { if (kana) _score[L_JA] += han; else _score[L_ZH] += han; }
}
- (double)pesoDe:(int)i {
    NSString *code = nl_code(i);
    if (_constraints && [_constraints count] && ![_constraints containsObject:code]) return 0;
    double w = _score[i];
    id h = _hints ? [_hints objectForKey:code] : nil;
    if (h) w *= 1 + [h doubleValue];
    return w;
}
- (id)dominantLanguage {
    int best = -1;
    double bw = 0;
    for (int i = 0; i < NL_N; i++) { double w = [self pesoDe:i]; if (w > bw) { bw = w; best = i; } }
    return best < 0 ? nil : nl_code(best);
}
- (id)languageHypothesesWithMaximum:(NSUInteger)maxHypotheses {
    double tot = 0, w[NL_N];
    for (int i = 0; i < NL_N; i++) tot += (w[i] = [self pesoDe:i]);
    NSMutableDictionary *r = [NSMutableDictionary dictionary];
    if (tot <= 0) return r;
    for (NSUInteger k = 0; k < maxHypotheses; k++) {
        int best = -1;
        for (int i = 0; i < NL_N; i++) if (w[i] > 0 && (best < 0 || w[i] > w[best])) best = i;
        if (best < 0) break;
        [r setObject:[NSNumber numberWithDouble:w[best] / tot] forKey:nl_code(best)];
        w[best] = 0;
    }
    return r;
}
@end
