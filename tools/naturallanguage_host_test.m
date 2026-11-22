/*
 * mOS — teste de host de userland/frameworks/NaturalLanguage/NaturalLanguage.m contra o Foundation do macOS
 * (tools/naturallanguage_host_test.sh). Tokenização por palavra/frase/parágrafo, NLTagger TokenType e o idioma.
 */
#import <Foundation/Foundation.h>
@interface NLTokenizer : NSObject
- (instancetype)initWithUnit:(NSInteger)u;
- (void)setString:(NSString *)s;
- (NSArray *)tokensForRange:(NSRange)r;
- (NSRange)tokenRangeAtIndex:(NSUInteger)i;
@end
@interface NLTagger : NSObject
- (instancetype)initWithTagSchemes:(NSArray *)s;
- (void)setString:(NSString *)s;
- (NSString *)dominantLanguage;
- (void)enumerateTagsInRange:(NSRange)r unit:(NSInteger)u scheme:(NSString *)s options:(NSUInteger)o usingBlock:(void (^)(NSString *, NSRange, BOOL *))b;
@end
static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)
static NSArray *words(NSString *s, NSInteger unit) {
    NLTokenizer *t = [[NLTokenizer alloc] initWithUnit:unit];
    [t setString:s];
    NSMutableArray *out = [NSMutableArray array];
    for (NSValue *v in [t tokensForRange:NSMakeRange(0, s.length)]) [out addObject:[s substringWithRange:v.rangeValue]];
    return out;
}
int main(void) {
    @autoreleasepool {
        NSArray *w = words(@"Olá, mundo! Don't stop: 3.14 é π.", 0);
        CK([w isEqual:(@[@"Olá", @"mundo", @"Don't", @"stop", @"3.14", @"é", @"π"])], "palavras com acento, contração e número");
        NSArray *cjk = words(@"日本語abc", 0);
        CK(cjk.count == 4 && [cjk[3] isEqual:@"abc"], "ideogramas um a um");
        NSArray *sent = words(@"Primeira frase. Segunda?! Terceira", 1);
        CK(sent.count == 3 && [sent[0] isEqual:@"Primeira frase. "] && [sent[1] isEqual:@"Segunda?! "] && [sent[2] isEqual:@"Terceira"], "frases");
        NSArray *par = words(@"um\ndois\n\ntrês", 2);
        CK(par.count == 3 && [par[2] isEqual:@"três"], "parágrafos");
        NLTokenizer *t = [[NLTokenizer alloc] initWithUnit:0];
        [t setString:@"ab cd"];
        NSRange r = [t tokenRangeAtIndex:4];
        CK(r.location == 3 && r.length == 2, "tokenRangeAtIndex");
        NLTagger *g = [[NLTagger alloc] initWithTagSchemes:@[@"TokenType"]];
        [g setString:@"oi, tudo"];
        NSMutableArray *tags = [NSMutableArray array];
        [g enumerateTagsInRange:NSMakeRange(0, 8) unit:0 scheme:@"TokenType" options:4 usingBlock:^(NSString *tag, NSRange tr, BOOL *stop) {
            [tags addObject:[NSString stringWithFormat:@"%@:%@", tag, [@"oi, tudo" substringWithRange:tr]]];
        }];
        CK([tags isEqual:(@[@"Word:oi", @"Punctuation:,", @"Word:tudo"])], "NLTagger TokenType sem espaços");
        [g setString:@"o gato e a casa não são de ninguém, você sabe que isso é para mim"];
        CK([[g dominantLanguage] isEqual:@"pt"], "idioma dominante");
    }
    printf("naturallanguage: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
