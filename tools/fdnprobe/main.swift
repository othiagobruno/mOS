// mOS — teste do Foundation Swift real na VM (tools/fdnprobe.sh): imprime FDN-OK ok/total, e FDN-FAIL <nome> por item.
import Foundation

var ok = 0, total = 0
func t(_ nome: String, _ cond: @autoclosure () -> Bool) {
    total += 1
    if cond() { ok += 1 } else { print("FDN-FAIL \(nome)") }
}

let u = URL(string: "https://example.com/a/b?x=1#f")!
t("url.host", u.host == "example.com")
t("url.path", u.path == "/a/b")
t("url.query", u.query == "x=1")
t("url.append", u.appendingPathComponent("c").path == "/a/b/c")
t("url.file", URL(fileURLWithPath: "/tmp/x.txt").pathExtension == "txt")
var comp = URLComponents(string: "https://h.com/p")!
comp.queryItems = [URLQueryItem(name: "q", value: "a b")]
t("urlcomponents", comp.url?.absoluteString == "https://h.com/p?q=a%20b")

let d = Data([1, 2, 3, 4])
t("data.count", d.count == 4)
t("data.range", d.range(of: Data([3, 4])) == 2..<4)
t("data.base64", Data(base64Encoded: d.base64EncodedString()) == d)

struct P: Codable, Equatable { var n: String; var v: Int; var l: [Double] }
let p = P(n: "ação", v: 2, l: [1.5, -3])
if let enc = try? JSONEncoder().encode(p), let dec = try? JSONDecoder().decode(P.self, from: enc) {
    t("json", dec == p)
} else { t("json", false) }
if let pl = try? PropertyListEncoder().encode(p), let dec = try? PropertyListDecoder().decode(P.self, from: pl) {
    t("plist", dec == p)
} else { t("plist", false) }

t("uuid", UUID(uuidString: "E621E1F8-C36C-495A-93FC-0C247A3E6E5F")?.uuidString == "E621E1F8-C36C-495A-93FC-0C247A3E6E5F")
t("date", Date(timeIntervalSince1970: 86400).timeIntervalSince1970 == 86400)
var cal = Calendar(identifier: .gregorian)
cal.timeZone = TimeZone(secondsFromGMT: 0)!
let dc = cal.dateComponents([.year, .month, .day], from: Date(timeIntervalSince1970: 1_000_000_000))
t("calendar", dc.year == 2001 && dc.month == 9 && dc.day == 9)
t("calendar.symbols", cal.standaloneWeekdaySymbols.count == 7)

t("trim", "  x \n".trimmingCharacters(in: .whitespacesAndNewlines) == "x")
t("split", "a,b;c".components(separatedBy: CharacterSet(charactersIn: ",;")) == ["a", "b", "c"])
t("percent", "a b".addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) == "a%20b")

var s = IndexSet([1, 2, 3, 7])
s.shift(startingAt: 3, by: 2)
t("indexset", Array(s) == [1, 2, 5, 9] && s.rangeView.count == 3)
t("nsrange", NSRange(location: 1, length: 2).intersection(NSRange(location: 2, length: 5)) == NSRange(location: 2, length: 1))
let str = "olá mundo"
if let r = str.range(of: "mundo") { t("nsrange.string", Range(NSRange(r, in: str), in: str) == r) } else { t("nsrange.string", false) }

let fm = FileManager.default
let tmp = URL(fileURLWithPath: NSTemporaryDirectory() as String).appendingPathComponent("fdnprobe.txt")
let escreveu = (try? Data("mOS".utf8).write(to: tmp)) != nil
t("file.write", escreveu)
t("file.read", (try? Data(contentsOf: tmp)) == Data("mOS".utf8))
t("file.exists", fm.fileExists(atPath: tmp.path))
try? fm.removeItem(at: tmp)
t("file.remove", !fm.fileExists(atPath: tmp.path))

let sc = Scanner(string: "  abc 42 3.5 x")
t("scanner.chars", sc.scanCharacters(from: .letters) == "abc")
t("scanner.int", sc.scanInt() == 42)
t("scanner.double", sc.scanDouble() == 3.5)
t("scanner.upto", sc.scanUpToString("zz") == "x")

let qi = URLQueryItem(name: "q", value: "1")
t("bridge.queryitem", URLQueryItem._unconditionallyBridgeFromObjectiveC(qi._bridgeToObjectiveC()) == qi)
let uc = URLComponents(string: "https://h.com/p?a=b")!
t("bridge.urlcomponents", URLComponents._unconditionallyBridgeFromObjectiveC(uc._bridgeToObjectiveC()).host == "h.com")
let nsc = CharacterSet.decimalDigits._bridgeToObjectiveC()
t("bridge.charset.objc", nsc.characterIsMember(0x35) && !nsc.characterIsMember(0x41))
t("bridge.charset.volta", CharacterSet._unconditionallyBridgeFromObjectiveC(nsc).contains("7"))
t("bridge.calendar", Calendar._unconditionallyBridgeFromObjectiveC(Calendar(identifier: .gregorian)._bridgeToObjectiveC()).identifier == .gregorian)

print("FDN-OK \(ok)/\(total)")
