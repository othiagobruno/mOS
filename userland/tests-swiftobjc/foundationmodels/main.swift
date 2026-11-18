// Teste do FoundationModels: tools/test-frameworks-swift.sh FoundationModels.
// O mOS não tem modelo de linguagem: o modelo diz que está indisponível e as sessões lançam erro, sem travar.
import FoundationModels

var ok = 0
var total = 0
func check(_ c: Bool, _ nome: String) {
    total += 1
    if c { ok += 1; print("  ok   \(nome)") } else { print("  FALHA \(nome)") }
}

let modelo = SystemLanguageModel.default
check(!modelo.isAvailable, "isAvailable == false")
check(modelo.availability == .unavailable(.deviceNotEligible), "availability: .unavailable(.deviceNotEligible)")
switch modelo.availability {
case .available: check(false, "switch em availability")
case .unavailable(let r): check(r == .deviceNotEligible, "switch em availability devolve o motivo")
}

let p = Prompt("linha um")
check(p.description == "linha um", "Prompt(String)")
let p2 = Prompt("a", ["b", "c"], p)
check(p2.description == "a\nb\nc\nlinha um", "Prompt(repeat each) junta String, [String] e Prompt")

let opts = GenerationOptions(sampling: .random(top: 5, seed: 7), temperature: 0.5)
check(opts.sampling == .random(top: 5, seed: 7) && opts.sampling != .greedy, "GenerationOptions.SamplingMode")
check(GenerationOptions.SamplingMode.random(probabilityThreshold: 0.9) != .greedy, "random(probabilityThreshold:)")

let s = LanguageModelSession(model: modelo, tools: [], instructions: "Seja breve")
check(!s.isResponding, "isResponding == false")

do {
    _ = try await s.respond(options: opts) { "Resuma"; "o texto" }
    check(false, "respond lança erro")
} catch let e as LanguageModelSession.GenerationError {
    if case .assetsUnavailable = e { check(true, "respond lança GenerationError.assetsUnavailable") }
    else { check(false, "respond lançou \(e)") }
} catch {
    check(false, "respond lançou outro erro: \(error)")
}

do {
    let stream = try s.streamResponse(options: GenerationOptions()) { "Olá" }
    var n = 0
    for try await snap in stream { _ = snap.content; n += 1 }
    check(false, "streamResponse: o iterador lança erro (recebeu \(n))")
} catch {
    check(error is LanguageModelSession.GenerationError, "streamResponse: o iterador lança GenerationError")
}

do {
    _ = try await s.streamResponse { "Olá" }.collect()
    check(false, "collect lança erro")
} catch {
    check(error is LanguageModelSession.GenerationError, "collect lança GenerationError")
}

print("foundationmodels: \(ok)/\(total) ok")
