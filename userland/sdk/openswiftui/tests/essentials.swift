// mOS — teste de execução do FoundationEssentials (swift-foundation) + _StringProcessing no harness machotest.
// tools\test-openswiftui-deps.ps1 -Essentials
import FoundationEssentials

var falhas = 0
func check(_ nome: String, _ ok: Bool, _ detalhe: @autoclosure () -> String = "") {
    if ok { print("  ok: \(nome)") } else { falhas += 1; print("  FALHOU: \(nome) \(detalhe())") }
}

// Date / Calendar / TimeZone
let d = Date(timeIntervalSince1970: 1_700_000_000)          // 2023-11-14 22:13:20 UTC
var cal = Calendar(identifier: .gregorian)
cal.timeZone = TimeZone(identifier: "GMT")!
let c = cal.dateComponents([.year, .month, .day, .hour, .minute, .second], from: d)
check("Calendar.dateComponents", c.year == 2023 && c.month == 11 && c.day == 14 && c.hour == 22 && c.minute == 13 && c.second == 20, "\(c)")
let amanha = cal.date(byAdding: .day, value: 1, to: d)!
check("Calendar.date(byAdding:)", amanha.timeIntervalSince(d) == 86_400)
check("Date() avança", Date().timeIntervalSince1970 > 1_600_000_000, "\(Date().timeIntervalSince1970)")
check("ISO8601", d.formatted(.iso8601) == "2023-11-14T22:13:20Z", d.formatted(.iso8601))

// JSON
struct Ponto: Codable, Equatable { var x: Int; var nome: String; var tags: [String] }
let p = Ponto(x: 42, nome: "olá", tags: ["a", "b"])
let enc = JSONEncoder(); enc.outputFormatting = .sortedKeys
let json = try! enc.encode(p)
check("JSONEncoder", String(decoding: json, as: UTF8.self) == #"{"nome":"olá","tags":["a","b"],"x":42}"#, String(decoding: json, as: UTF8.self))
check("JSONDecoder", (try? JSONDecoder().decode(Ponto.self, from: json)) == p)
let plist = try! PropertyListEncoder().encode(p)
check("PropertyList ida e volta", (try? PropertyListDecoder().decode(Ponto.self, from: plist)) == p)

// AttributedString
enum Destaque: AttributedStringKey { typealias Value = Int; static let name = "destaque" }
var a = AttributedString("Olá mundo")
let r = a.range(of: "mundo")!
a[r][Destaque.self] = 7
check("AttributedString.runs", a.runs.count == 2, "\(a.runs.count)")
check("AttributedString caracteres", String(a.characters) == "Olá mundo")

// URL / UUID / Data / CharacterSet
let u = URL(string: "https://exemplo.com:8080/a/b?q=1#f")!
check("URL componentes", u.host() == "exemplo.com" && u.port == 8080 && u.path() == "/a/b" && u.query() == "q=1", "\(u)")
check("URL relativa", URL(string: "../c", relativeTo: u)!.absoluteString == "https://exemplo.com:8080/c")
let id = UUID(uuidString: "E621E1F8-C36C-495A-93FC-0C247A3E6E5F")!
check("UUID", id.uuidString == "E621E1F8-C36C-495A-93FC-0C247A3E6E5F")
let dados = Data("mOS".utf8)
check("Data.base64", dados.base64EncodedString() == "bU9T" && Data(base64Encoded: "bU9T") == dados)
check("CharacterSet", CharacterSet.whitespacesAndNewlines.contains(" ") && !CharacterSet.whitespaces.contains("a") && CharacterSet.decimalDigits.contains("7"))

// _StringProcessing (algoritmos e Regex)
check("contains/replacing", "abcabc".contains("ca") && "abcabc".replacing("b", with: "-") == "a-ca-c")
let re = try! Regex("[0-9]+")
check("Regex", "x12y345".matches(of: re).map { String($0.output[0].substring!) } == ["12", "345"])

print(falhas == 0 ? "TUDO OK" : "\(falhas) FALHAS")
