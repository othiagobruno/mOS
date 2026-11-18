// Teste do Translation: tools/test-frameworks-swift.sh Translation.
// O mOS não tem modelos de tradução: tudo responde "sem suporte" com TranslationError, sem travar.
import Foundation
import Translation

var ok = 0
var total = 0
func check(_ c: Bool, _ nome: String) {
    total += 1
    if c { ok += 1; print("  ok   \(nome)") } else { print("  FALHA \(nome)") }
}

let pt = Locale(identifier: "pt-BR").language
let en = Locale(identifier: "en").language

let av = LanguageAvailability()
check(await av.supportedLanguages.isEmpty, "supportedLanguages vazio")
check(await av.status(from: pt, to: en) == .unsupported, "status(from:to:) == .unsupported")

let s = TranslationSession(installedSource: pt, target: en)
check(s.targetLanguage == en, "targetLanguage guarda o destino")

do {
    _ = try await s.translate("Olá")
    check(false, "translate lança erro")
} catch TranslationError.unsupportedLanguagePairing {
    check(true, "translate lança TranslationError.unsupportedLanguagePairing (casa com ~=)")
} catch {
    check(false, "translate lançou outro erro: \(error)")
}

do {
    _ = try await s.translate("")
    check(false, "texto vazio lança erro")
} catch {
    check(TranslationError.nothingToTranslate ~= error, "texto vazio: nothingToTranslate")
    check(!(TranslationError.internalError ~= error), "~= não confunde erros diferentes")
}

do {
    try await s.prepareTranslation()
    check(false, "prepareTranslation lança erro")
} catch {
    check(error is TranslationError, "prepareTranslation lança TranslationError")
}

do {
    _ = try await s.translations(from: [TranslationSession.Request(sourceText: "um", clientIdentifier: "1")])
    check(false, "translations(from:) lança erro")
} catch {
    check(TranslationError.unsupportedLanguagePairing ~= error, "translations(from:) lança unsupportedLanguagePairing")
}

var cfg = TranslationSession.Configuration(source: pt, target: en)
let antes = cfg
check(cfg.source == pt && cfg.target == en, "Configuration guarda origem e destino")
cfg.invalidate()
check(cfg != antes, "invalidate() pede uma tradução nova")
check((TranslationError.unsupportedSourceLanguage as Error).localizedDescription.contains("origem"),
      "localizedDescription descreve o erro")

print("translation: \(ok)/\(total) ok")
