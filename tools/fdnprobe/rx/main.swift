// mOS — teste do _StringProcessing (Regex) na VM, separado do fdnprobe: imprime RX-OK ok/total.
var ok = 0, total = 0
func t(_ nome: String, _ cond: @autoclosure () -> Bool) {
    total += 1
    if cond() { ok += 1 } else { print("RX-FAIL \(nome)") }
}
print("RX-INICIO")
let re = try? Regex<Substring>("[0-9]+")
print("RX-CRIOU \(re != nil)")
if let re { t("contains", "abc123".contains(re)) } else { t("cria", false) }
if let re2 = try? Regex("([a-z]+)-([0-9]+)"), let m = try? re2.firstMatch(in: "id: abc-42") {
    t("captura", m.output.count == 3)
} else { t("captura", false) }
print("RX-OK \(ok)/\(total)")
