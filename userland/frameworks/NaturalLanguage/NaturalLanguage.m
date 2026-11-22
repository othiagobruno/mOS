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
/* os demais idiomas do NLLanguage (o reconhecedor não os distingue, mas os apps usam as constantes) */
NL_CONST(NLLanguageVietnamese, "vi") NL_CONST(NLLanguageSlovak, "sk") NL_CONST(NLLanguageRomanian, "ro")
NL_CONST(NLLanguageNorwegian, "nb") NL_CONST(NLLanguageMalay, "ms") NL_CONST(NLLanguageIndonesian, "id")
NL_CONST(NLLanguageHungarian, "hu") NL_CONST(NLLanguageFinnish, "fi") NL_CONST(NLLanguageDanish, "da")
NL_CONST(NLLanguageCzech, "cs") NL_CONST(NLLanguageCroatian, "hr") NL_CONST(NLLanguageCatalan, "ca")
NL_CONST(NLLanguageBulgarian, "bg") NL_CONST(NLLanguageAmharic, "am") NL_CONST(NLLanguageArmenian, "hy")
NL_CONST(NLLanguageBengali, "bn") NL_CONST(NLLanguageBurmese, "my") NL_CONST(NLLanguageCherokee, "chr")
NL_CONST(NLLanguageGeorgian, "ka") NL_CONST(NLLanguageGujarati, "gu") NL_CONST(NLLanguageIcelandic, "is")
NL_CONST(NLLanguageKannada, "kn") NL_CONST(NLLanguageKazakh, "kk") NL_CONST(NLLanguageKhmer, "km")
NL_CONST(NLLanguageLao, "lo") NL_CONST(NLLanguageMalayalam, "ml") NL_CONST(NLLanguageMarathi, "mr")
NL_CONST(NLLanguageMongolian, "mn") NL_CONST(NLLanguageOriya, "or") NL_CONST(NLLanguagePersian, "fa")
NL_CONST(NLLanguagePunjabi, "pa") NL_CONST(NLLanguageSinhalese, "si") NL_CONST(NLLanguageTamil, "ta")
NL_CONST(NLLanguageTelugu, "te") NL_CONST(NLLanguageTibetan, "bo") NL_CONST(NLLanguageUrdu, "ur")

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

/* ---------------------------------------------------------------- NLTokenizer, NLTagger, NLEmbedding
 * Tokenização de verdade pela classe de cada caractere UTF-16: palavra = sequência de letras/dígitos (com ' e ’
 * no meio), cada ideograma CJK sozinho; frase termina em . ! ? … seguido de espaço/fim; parágrafo em \n.
 * NLTagger dá o esquema TokenType (Word, Punctuation, Whitespace, Other) e o Language pelo reconhecedor acima;
 * classe gramatical e nomes próprios precisam do modelo da Apple, que não existe aqui (tag nil, como sem modelo). */
NL_CONST(NLTagSchemeTokenType, "TokenType") NL_CONST(NLTagSchemeLexicalClass, "LexicalClass") NL_CONST(NLTagSchemeNameType, "NameType")
NL_CONST(NLTagSchemeNameTypeOrLexicalClass, "NameTypeOrLexicalClass") NL_CONST(NLTagSchemeLemma, "Lemma")
NL_CONST(NLTagSchemeLanguage, "Language") NL_CONST(NLTagSchemeScript, "Script") NL_CONST(NLTagSchemeSentimentScore, "SentimentScore")
NL_CONST(NLTagWord, "Word") NL_CONST(NLTagPunctuation, "Punctuation") NL_CONST(NLTagWhitespace, "Whitespace") NL_CONST(NLTagOther, "Other")
NL_CONST(NLTagNoun, "Noun") NL_CONST(NLTagVerb, "Verb") NL_CONST(NLTagAdjective, "Adjective") NL_CONST(NLTagAdverb, "Adverb")
NL_CONST(NLTagPronoun, "Pronoun") NL_CONST(NLTagDeterminer, "Determiner") NL_CONST(NLTagParticle, "Particle")
NL_CONST(NLTagPreposition, "Preposition") NL_CONST(NLTagNumber, "Number") NL_CONST(NLTagConjunction, "Conjunction")
NL_CONST(NLTagInterjection, "Interjection") NL_CONST(NLTagClassifier, "Classifier") NL_CONST(NLTagIdiom, "Idiom")
NL_CONST(NLTagOtherWord, "OtherWord") NL_CONST(NLTagSentenceTerminator, "SentenceTerminator") NL_CONST(NLTagOpenQuote, "OpenQuote")
NL_CONST(NLTagCloseQuote, "CloseQuote") NL_CONST(NLTagOpenParenthesis, "OpenParenthesis") NL_CONST(NLTagCloseParenthesis, "CloseParenthesis")
NL_CONST(NLTagWordJoiner, "WordJoiner") NL_CONST(NLTagDash, "Dash") NL_CONST(NLTagOtherPunctuation, "OtherPunctuation")
NL_CONST(NLTagParagraphBreak, "ParagraphBreak") NL_CONST(NLTagOtherWhitespace, "OtherWhitespace")
NL_CONST(NLTagPersonalName, "PersonalName") NL_CONST(NLTagPlaceName, "PlaceName") NL_CONST(NLTagOrganizationName, "OrganizationName")

enum { NLTokenUnitWord = 0, NLTokenUnitSentence = 1, NLTokenUnitParagraph = 2, NLTokenUnitDocument = 3 };
static int nl_is_space(unsigned c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0 || c == 0x2028 || c == 0x2029 || c == 0x3000 || (c >= 0x2000 && c <= 0x200A); }
static int nl_is_cjk(unsigned c) { return (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x3040 && c <= 0x30FF) || (c >= 0xF900 && c <= 0xFAFF); }
static int nl_is_word(unsigned c) {
    if (nl_is_space(c)) return 0;
    if (c < 128) return (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'z') || c == '_';
    if (c >= 0x2010 && c <= 0x205E) return 0;        /* pontuação geral */
    if (c >= 0x3000 && c <= 0x303F) return 0;        /* pontuação CJK */
    if (c == 0xAB || c == 0xBB || c == 0xBF || c == 0xA1 || c == 0xB7) return 0;
    return 1;                                         /* letras acentuadas, cirílico, grego, árabe, CJK... */
}
id objc_msgSend(id, SEL, ...);
/* NSValue com um NSRange (o SDK do mOS não declara +valueWithRange:) */
static id nl_range_value(NSRange r) { return ((id (*)(id, SEL, NSRange))objc_msgSend)((id)NSClassFromString(@"NSValue"), @selector(valueWithRange:), r); }
/* chama f para cada token da unidade dentro de [from, to) */
static void nl_tokens(NSString *s, int unit, NSUInteger from, NSUInteger to, void (^f)(NSRange, BOOL *)) {
    NSUInteger n = s.length;
    if (to > n) to = n;
    BOOL stop = NO;
    if (unit == NLTokenUnitDocument) { if (to > from) f(NSMakeRange(from, to - from), &stop); return; }
    NSUInteger i = from;
    while (i < to && !stop) {
        unsigned c = [s characterAtIndex:i];
        if (unit == NLTokenUnitWord) {
            if (!nl_is_word(c)) { i++; continue; }
            NSUInteger st = i;
            if (nl_is_cjk(c)) { f(NSMakeRange(st, 1), &stop); i++; continue; }
            while (i < to) {
                unsigned d = [s characterAtIndex:i];
                if (nl_is_word(d) && !nl_is_cjk(d)) { i++; continue; }
                if ((d == '\'' || d == 0x2019 || d == '.' || d == ',') && i + 1 < to && nl_is_word([s characterAtIndex:i + 1]) &&
                    !nl_is_cjk([s characterAtIndex:i + 1]) && (d == '\'' || d == 0x2019 || ((d == '.' || d == ',') && i > st &&
                    [s characterAtIndex:i - 1] >= '0' && [s characterAtIndex:i - 1] <= '9'))) { i++; continue; }   /* don't, 3.14 */
                break;
            }
            f(NSMakeRange(st, i - st), &stop);
            continue;
        }
        while (i < to && nl_is_space([s characterAtIndex:i])) i++;
        if (i >= to) break;
        NSUInteger st = i;
        if (unit == NLTokenUnitParagraph) {
            while (i < to && [s characterAtIndex:i] != '\n' && [s characterAtIndex:i] != 0x2029) i++;
            NSUInteger end = i;
            if (i < to) i++;
            f(NSMakeRange(st, end - st + (end < to ? 1 : 0)), &stop);
            continue;
        }
        while (i < to) {                               /* frase */
            unsigned d = [s characterAtIndex:i++];
            if (d == '\n' || d == 0x2029) break;
            if (d == '.' || d == '!' || d == '?' || d == 0x2026 || d == 0x3002 || d == 0xFF01 || d == 0xFF1F) {
                while (i < to && ([s characterAtIndex:i] == '.' || [s characterAtIndex:i] == '!' || [s characterAtIndex:i] == '?' ||
                                  [s characterAtIndex:i] == '"' || [s characterAtIndex:i] == 0x201D || [s characterAtIndex:i] == ')')) i++;
                if (i >= to || nl_is_space([s characterAtIndex:i]) || d >= 0x3000) break;
            }
        }
        NSUInteger end = i;
        while (end < to && nl_is_space([s characterAtIndex:end]) && [s characterAtIndex:end] != '\n') end++;   /* a frase leva o espaço */
        f(NSMakeRange(st, end - st), &stop);
        i = end;
    }
}

@interface NLTokenizer : NSObject {
    NSString *_string;
    NSInteger _unit;
    NSString *_lang;
}
@end
@implementation NLTokenizer
- (instancetype)initWithUnit:(NSInteger)unit { if ((self = [super init])) _unit = unit; return self; }
- (instancetype)init { return [self initWithUnit:NLTokenUnitWord]; }
- (void)dealloc { [_string release]; [_lang release]; [super dealloc]; }
- (NSInteger)unit { return _unit; }
- (NSString *)string { return _string; }
- (void)setString:(NSString *)s { NSString *o = _string; _string = [s copy]; [o release]; }
- (void)setLanguage:(NSString *)l { NSString *o = _lang; _lang = [l copy]; [o release]; }
- (NSArray *)tokensForRange:(NSRange)r {
    NSMutableArray *a = [NSMutableArray array];
    if (!_string) return a;
    nl_tokens(_string, (int)_unit, r.location, r.location + r.length, ^(NSRange t, BOOL *stop) {
        (void)stop;
        [a addObject:nl_range_value(t)];
    });
    return a;
}
- (NSRange)tokenRangeAtIndex:(NSUInteger)idx {
    __block NSRange found = NSMakeRange(0x7fffffffffffffffUL, 0);
    if (_string)
        nl_tokens(_string, (int)_unit, 0, _string.length, ^(NSRange t, BOOL *stop) {
            if (idx >= t.location && idx < t.location + t.length) { found = t; *stop = YES; }
        });
    return found;
}
- (NSRange)tokenRangeForRange:(NSRange)r {
    NSRange a = [self tokenRangeAtIndex:r.location], b = [self tokenRangeAtIndex:r.location + (r.length ? r.length - 1 : 0)];
    if (a.location == 0x7fffffffffffffffUL) return a;
    NSUInteger end = b.location == 0x7fffffffffffffffUL ? a.location + a.length : b.location + b.length;
    return NSMakeRange(a.location, end - a.location);
}
/* bloco (NSRange, NLTokenizerAttributes, BOOL *stop): atributo 1 = numérico, 2 = símbolo, 4 = emoji */
- (void)enumerateTokensInRange:(NSRange)r usingBlock:(void (^)(NSRange, NSUInteger, BOOL *))blk {
    if (!_string || !blk) return;
    NSString *s = _string;
    nl_tokens(s, (int)_unit, r.location, r.location + r.length, ^(NSRange t, BOOL *stop) {
        NSUInteger attrs = 1;
        for (NSUInteger k = t.location; k < t.location + t.length; k++) {
            unsigned c = [s characterAtIndex:k];
            if (!(c >= '0' && c <= '9') && c != '.' && c != ',') { attrs = 0; break; }
        }
        blk(t, attrs, stop);
    });
}
@end

@interface NLTagger : NSObject {
    NSArray *_schemes;
    NSString *_string;
}
@end
@implementation NLTagger
+ (NSArray *)availableTagSchemesForUnit:(NSInteger)u language:(NSString *)l {
    (void)u; (void)l;
    return [NSArray arrayWithObjects:NLTagSchemeTokenType, NLTagSchemeLanguage, nil];
}
- (instancetype)initWithTagSchemes:(NSArray *)s { if ((self = [super init])) _schemes = [s copy]; return self; }
- (void)dealloc { [_schemes release]; [_string release]; [super dealloc]; }
- (NSArray *)tagSchemes { return _schemes; }
- (NSString *)string { return _string; }
- (void)setString:(NSString *)s { NSString *o = _string; _string = [s copy]; [o release]; }
- (void)setLanguage:(NSString *)l range:(NSRange)r { (void)l; (void)r; }
- (void)setOrthography:(id)o range:(NSRange)r { (void)o; (void)r; }
- (NSString *)dominantLanguage {
    if (!_string) return nil;
    NLLanguageRecognizer *rec = [[NLLanguageRecognizer alloc] init];
    [rec processString:_string];
    NSString *l = [[[rec dominantLanguage] retain] autorelease];
    [rec release];
    return l;
}
static NSString *nl_token_type(NSString *s, NSRange t) {
    unsigned c = [s characterAtIndex:t.location];
    if (nl_is_space(c)) return c == '\n' || c == 0x2029 ? NLTagWhitespace : NLTagWhitespace;
    if (nl_is_word(c)) return NLTagWord;
    if (c < 128 && !(c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '"' || c == '\'' || c == '(' ||
                     c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '-')) return NLTagOther;
    return NLTagPunctuation;
}
- (NSString *)tagAtIndex:(NSUInteger)idx unit:(NSInteger)unit scheme:(NSString *)scheme tokenRange:(NSRange *)out {
    if (!_string || idx >= _string.length) { if (out) *out = NSMakeRange(0x7fffffffffffffffUL, 0); return nil; }
    NLTokenizer *tk = [[NLTokenizer alloc] initWithUnit:unit];
    [tk setString:_string];
    NSRange t = [tk tokenRangeAtIndex:idx];
    [tk release];
    if (t.location == 0x7fffffffffffffffUL) t = NSMakeRange(idx, 1);
    if (out) *out = t;
    if ([scheme isEqual:NLTagSchemeTokenType]) return nl_token_type(_string, t);
    if ([scheme isEqual:NLTagSchemeLanguage]) return [self dominantLanguage];
    return nil;
}
/* opções: 1 omitir palavras, 2 pontuação, 4 espaço, 8 outros, 16 juntar contrações, 32 juntar nomes */
- (void)enumerateTagsInRange:(NSRange)r unit:(NSInteger)unit scheme:(NSString *)scheme options:(NSUInteger)opts
                  usingBlock:(void (^)(NSString *, NSRange, BOOL *))blk {
    if (!_string || !blk) return;
    NSString *s = _string;
    NSString *lang = [scheme isEqual:NLTagSchemeLanguage] ? [self dominantLanguage] : nil;
    BOOL tt = [scheme isEqual:NLTagSchemeTokenType];
    NSUInteger from = r.location, to = r.location + r.length;
    if (to > s.length) to = s.length;
    if (unit != NLTokenUnitWord) {
        nl_tokens(s, (int)unit, from, to, ^(NSRange t, BOOL *stop) { blk(lang, t, stop); });
        return;
    }
    /* palavra: os tokens e também a pontuação/espaço entre eles, como o NLTagger da Apple */
    __block NSUInteger pos = from;
    __block BOOL halted = NO;
    void (^gap)(NSUInteger) = ^(NSUInteger until) {
        while (pos < until && !halted) {
            NSRange g = NSMakeRange(pos, 1);
            NSString *ty = nl_token_type(s, g);
            BOOL skip = (ty == NLTagPunctuation && (opts & 2)) || (ty == NLTagWhitespace && (opts & 4)) || (ty == NLTagOther && (opts & 8));
            if (!skip) blk(tt ? ty : lang, g, &halted);
            pos++;
        }
    };
    nl_tokens(s, NLTokenUnitWord, from, to, ^(NSRange t, BOOL *stop) {
        gap(t.location);
        if (halted) { *stop = YES; return; }
        if (!(opts & 1)) blk(tt ? NLTagWord : lang, t, &halted);
        pos = t.location + t.length;
        if (halted) *stop = YES;
    });
    gap(to);
}
- (NSArray *)tagsInRange:(NSRange)r unit:(NSInteger)unit scheme:(NSString *)scheme options:(NSUInteger)opts tokenRanges:(NSArray **)ranges {
    NSMutableArray *tags = [NSMutableArray array], *rs = [NSMutableArray array];
    [self enumerateTagsInRange:r unit:unit scheme:scheme options:opts usingBlock:^(NSString *tag, NSRange t, BOOL *stop) {
        (void)stop;
        [tags addObject:tag ? (id)tag : [(id)NSClassFromString(@"NSNull") performSelector:@selector(null)]];
        [rs addObject:nl_range_value(t)];
    }];
    if (ranges) *ranges = rs;
    return tags;
}
@end

/* NLEmbedding: os vetores da Apple não existem aqui; as fábricas devolvem nil, como num idioma sem modelo */
@interface NLEmbedding : NSObject
@end
@implementation NLEmbedding
+ (instancetype)wordEmbeddingForLanguage:(NSString *)l { (void)l; return nil; }
+ (instancetype)wordEmbeddingForLanguage:(NSString *)l revision:(NSUInteger)r { (void)l; (void)r; return nil; }
+ (instancetype)sentenceEmbeddingForLanguage:(NSString *)l { (void)l; return nil; }
+ (instancetype)sentenceEmbeddingForLanguage:(NSString *)l revision:(NSUInteger)r { (void)l; (void)r; return nil; }
+ (instancetype)embeddingWithContentsOfURL:(id)u error:(id *)e { (void)u; if (e) *e = nil; return nil; }
+ (NSUInteger)currentRevisionForLanguage:(NSString *)l { (void)l; return 0; }
@end
