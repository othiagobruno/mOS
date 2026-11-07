// mOS — teste do overlay Foundation da variante com interop ObjC (docs/swift-objc-runtime.md, fase 5, marco A): pontes
// Swift <-> ObjC de String, números, Bool, Array, Dictionary e Set sobre o Foundation do libSystem do mOS.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\foundation\Fnd -Module Fnd -Sources userland\tests-swiftobjc\foundation\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\foundation build\swiftobjctest\foundation\Fnd
// Imprime "foundation: N/N ok".
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("foundation: pontes Swift <-> ObjC")

// String
let s = "olá, mOS ✓"
let ns: NSString = s as NSString
check(ns.length == 10, "String -> NSString (length \(ns.length))")
check((ns as String) == s, "NSString -> String")
check(ns.isEqual(to: "olá, mOS ✓"), "NSString.isEqual(to:) com literal")
let lit: NSString = "literal"
check(lit.length == 7 && (lit as String) == "literal", "NSString(stringLiteral:)")

// números
let n: NSNumber = 42
check(n.intValue == 42 && (n as! Int) == 42, "NSNumber literal e NSNumber -> Int")
check((3.5 as NSNumber).doubleValue == 3.5, "Double -> NSNumber")
check((true as NSNumber).boolValue, "Bool -> NSNumber")
check((NSNumber(value: 7) as? Int) == 7 && (NSNumber(value: 2.5) as? Int) == nil, "cast condicional NSNumber -> Int")

// Array
let arr = ["um", "dois", "três"]
let na = arr as NSArray
check(na.count == 3, "[String] -> NSArray")
check(((na.object(at: 1) as! NSString) as String) == "dois", "elemento do NSArray é NSString")
check((na as! [String]) == arr, "NSArray -> [String]")
let mixed: NSArray = [1, "dois", 3.0]
check(mixed.count == 3 && (mixed as? [Int]) == nil, "NSArray misto não vira [Int]")

// Dictionary
let dict: [String: Int] = ["a": 1, "b": 2]
let nd = dict as NSDictionary
check(nd.count == 2 && ((nd.object(forKey: "b" as NSString) as! NSNumber).intValue == 2), "[String: Int] -> NSDictionary")
check((nd as! [String: Int]) == dict, "NSDictionary -> [String: Int]")
let lit2: NSDictionary = ["x": 10, "y": "z"]
check(lit2.count == 2, "NSDictionary(dictionaryLiteral:)")

// Set
let st: Set<String> = ["p", "q"]
let nset = st as NSSet
check(nset.count == 2 && (nset as! Set<String>) == st, "Set <-> NSSet")

// casts dinâmicos via Any/AnyObject
let anyArr: Any = na
check((anyArr as? [String]) == arr, "Any(NSArray) as? [String]")
let obj: AnyObject = s as AnyObject
check((obj as? String) == s, "String como AnyObject e de volta")
let anyNum: Any = n
check((anyNum as? Double) == 42, "Any(NSNumber) as? Double")

// iteração
var it = 0
for e in na { if e is NSString { it += 1 } }
check(it == 3, "for-in em NSArray")
var keys = 0
for (k, _) in nd { if k is NSString { keys += 1 } }
check(keys == 2, "for-in em NSDictionary")

check(NSNotFound == Int.max, "NSNotFound")
check(String(format: "%d-%@", 7, "x" as NSString) == "7-x", "String(format:)")

print("foundation: \(ok)/\(total) ok")
