#!/usr/bin/env python3
"""Testes do leitor de imports de WebKit (sem VM e sem os apps reais).

Monta Mach-O sintéticos com a mesma disposição que tools/machocheck.c lê:
binds do dyld_info, imports encadeados e um fat com o slice fora do início.
"""
import os
import struct
import tempfile
import unittest

from webkit_api_scan import (
    framework_surface,
    parse_binds_raw,
    parse_macho,
    scan_file,
    selector_from_method_line,
    summarize,
)

HERE = os.path.dirname(os.path.abspath(__file__))
WEBKIT_M = os.path.normpath(os.path.join(HERE, "..", "userland", "frameworks", "WebKit", "WebKit.m"))


def p32(v):
    return struct.pack("<I", v)


def p64(v):
    return struct.pack("<Q", v)


def cstr(s):
    return s.encode("ascii") + b"\0"


def build_thin(cpu, dylib, imports, meth_names, bind_bytes=None):
    """Mach-O 64 mínimo: um LC_LOAD_DYLIB, binds ou chained fixups, e __objc_methname."""
    meth = b"\0".join(n.encode("ascii") for n in meth_names)
    if meth_names:
        meth += b"\0"
    else:
        meth = b"\0"
    dylib_name = cstr(dylib)
    dylib_cmd_size = (24 + len(dylib_name) + 7) & ~7
    dylib_cmd = bytearray(dylib_cmd_size)
    struct.pack_into("<IIIIII", dylib_cmd, 0, 0x0C, dylib_cmd_size, 24, 0, 0x10000, 0x10000)
    dylib_cmd[24:24 + len(dylib_name)] = dylib_name

    if bind_bytes is not None:
        extra = bytes(bind_bytes)
        info = bytearray(48)
        struct.pack_into("<II", info, 0, 0x80000022, 48)
        extra_kind = "bind"
    else:
        names_blob = bytearray()
        imp_bytes = bytearray()
        for name, ordinal, weak in imports:
            no = len(names_blob)
            names_blob += name.encode("ascii") + b"\0"
            word = (ordinal & 0xFF) | ((1 if weak else 0) << 8) | (no << 9)
            imp_bytes += p32(word)
        header = bytearray(28)
        struct.pack_into("<IIIIIII", header, 0, 0, 0, 28, 28 + len(imp_bytes), len(imports), 1, 0)
        extra = bytes(header) + bytes(imp_bytes) + bytes(names_blob)
        info = bytearray(16)
        struct.pack_into("<II", info, 0, 0x80000034, 16)
        extra_kind = "chained"

    seg_size = 72 + 80
    header_size = 32
    cmds_len = seg_size + len(dylib_cmd) + len(info)
    extra_off = header_size + cmds_len
    meth_off = extra_off + len(extra)

    seg = bytearray(seg_size)
    struct.pack_into("<II", seg, 0, 0x19, seg_size)
    seg[8:24] = b"__TEXT" + b"\0" * 10
    struct.pack_into("<I", seg, 64, 1)
    seg[72:72 + 16] = b"__objc_methname\0"
    seg[72 + 16:72 + 32] = b"__TEXT" + b"\0" * 10
    struct.pack_into("<Q", seg, 72 + 40, len(meth))
    struct.pack_into("<I", seg, 72 + 48, meth_off)

    if extra_kind == "bind":
        struct.pack_into("<II", info, 16, extra_off, len(extra))
    else:
        struct.pack_into("<II", info, 8, extra_off, len(extra))

    cmds = bytes(seg) + bytes(dylib_cmd) + bytes(info)
    if len(cmds) != cmds_len:
        raise AssertionError((len(cmds), cmds_len))
    mh = bytearray(32)
    struct.pack_into("<IIIIIII", mh, 0, 0xFEEDFACF, cpu, 0, 2, 3, len(cmds), 0)
    return bytes(mh) + cmds + extra + meth


class ParserTests(unittest.TestCase):
    def test_bind_opcodes_match_machocheck_layout(self):
        blob = bytearray()
        blob += bytes([0x40]) + b"_OBJC_CLASS_$_WKWebView\0"
        blob += bytes([0x11])  # ordinal 1
        blob += bytes([0x90])  # DO_BIND
        blob += bytes([0x40 | 1]) + b"_OBJC_CLASS_$_WKDownload\0"  # weak flag
        blob += bytes([0x90])
        blob += bytes([0x00])
        got = parse_binds_raw(bytes(blob), 0, len(blob), False)
        self.assertEqual(got, [
            ("_OBJC_CLASS_$_WKWebView", 1, 0),
            ("_OBJC_CLASS_$_WKDownload", 1, 1),
        ])

    def test_chained_import_in_thin_macho(self):
        imports = [("_OBJC_CLASS_$_WKWebView", 1, 0), ("_OBJC_CLASS_$_WKDownload", 1, 1)]
        blob = build_thin(0x01000007, "/System/Library/Frameworks/WebKit.framework/WebKit", imports, ["loadRequest:", "nota"])
        slices = parse_macho(blob)
        self.assertEqual(len(slices), 1)
        self.assertEqual(slices[0]["cpu"], "x86_64")
        self.assertIn("WebKit.framework", slices[0]["dylibs"][0]["path"])
        names = {(n, w) for n, _o, w in slices[0]["imports"]}
        self.assertIn(("_OBJC_CLASS_$_WKWebView", 0), names)
        self.assertIn(("_OBJC_CLASS_$_WKDownload", 1), names)
        self.assertIn("loadRequest:", slices[0]["meth"])

    def test_fat_slice_not_at_zero(self):
        imports = [("_OBJC_CLASS_$_WKWebView", 1, 0)]
        thin = build_thin(0x0100000C, "/System/Library/Frameworks/WebKit.framework/Versions/A/WebKit", imports, ["evaluateJavaScript:completionHandler:"])
        # fat 32: magic, nfat, one arch. slice começa em 4096 para não coincidir com offset 0
        pad = 4096
        fat = bytearray(pad) + thin
        struct.pack_into(">II", fat, 0, 0xCAFEBABE, 1)
        struct.pack_into(">IIIII", fat, 8, 0x0100000C, 0, pad, len(thin), 12)
        slices = parse_macho(bytes(fat))
        self.assertEqual([s["cpu"] for s in slices], ["arm64"])
        self.assertEqual(slices[0]["imports"][0][0], "_OBJC_CLASS_$_WKWebView")
        self.assertIn("evaluateJavaScript:completionHandler:", slices[0]["meth"])

    def test_scan_file_reports_missing_class_against_framework_source(self):
        imports = [("_OBJC_CLASS_$_WKWebView", 1, 0), ("_OBJC_CLASS_$_WKDownload", 1, 0)]
        blob = build_thin(0x01000007, "/System/Library/Frameworks/WebKit.framework/WebKit", imports,
                          ["loadRequest:", "startDownloadUsingRequest:completionHandler:", "webView:navigationResponse:didBecomeDownload:"])
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "Fake.app", "Contents", "MacOS")
            os.makedirs(path)
            with open(os.path.join(path, "Fake"), "wb") as fh:
                fh.write(blob)
            surface = framework_surface(WEBKIT_M)
            report = summarize(d, surface)
        classes_missing = [s["class"] for s in report["classes_missing_from_framework"]]
        classes_ok = [s["class"] for s in report["classes_present_in_framework"]]
        self.assertIn("WKDownload", classes_missing)
        self.assertIn("WKWebView", classes_ok)
        sels_missing = [s["selector"] for s in report["selectors_not_in_framework_source"]]
        sels_ok = [s["selector"] for s in report["selectors_implemented"]]
        self.assertIn("loadRequest:", sels_ok)
        self.assertNotIn("startDownloadUsingRequest:completionHandler:", sels_missing)
        self.assertIn("webView:navigationResponse:didBecomeDownload:", sels_missing)

    def test_selector_parser_and_real_framework_surface(self):
        self.assertEqual(
            selector_from_method_line("- (void)evaluateJavaScript:(NSString *)js completionHandler:(void (^)(id, NSError *))handler {"),
            "evaluateJavaScript:completionHandler:",
        )
        self.assertEqual(selector_from_method_line("- (void)reload;"), "reload")
        surface = framework_surface(WEBKIT_M)
        self.assertIn("WKWebView", surface["classes"])
        self.assertIn("WKWebViewConfiguration", surface["classes"])
        self.assertNotIn("WKDownload", surface["classes"])
        self.assertIn("evaluateJavaScript:completionHandler:", surface["selectors"])
        self.assertIn("loadRequest:", surface["selectors"])
        self.assertIn("setURLSchemeHandler:forURLScheme:", surface["selectors"])
        self.assertIn("webView:shouldGoToBackForwardListItem:willUseInstantBack:completionHandler:", surface["selectors"])
        self.assertIn("goToBackForwardListItem:", surface["selectors"])
        self.assertIn("WebArchive", surface["classes"])
        self.assertIn("WebResource", surface["classes"])
        with open(WEBKIT_M, encoding="utf-8") as fh:
            src = fh.read()
        for key in (
            "WebMainResource", "WebSubresources", "WebSubframeArchives",
            "WebResourceData", "WebResourceURL", "WebResourceMIMEType",
            "WebResourceTextEncodingName", "WebResourceFrameName",
        ):
            self.assertIn(f'@"{key}"', src)
        self.assertIn("shouldGoToBackForwardListItem:willUseInstantBack:completionHandler:", src)
        self.assertIn("textEncodingName:(NSString *)e baseURL:(NSURL *)u completionHandler:", src)

    def test_scan_file_on_synthetic(self):
        imports = [("_OBJC_CLASS_$_JSContext", 1, 0)]
        blob = build_thin(0x01000007, "/System/Library/Frameworks/JavaScriptCore.framework/JavaScriptCore", imports, [])
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, "bin"), "wb") as fh:
                fh.write(blob)
            hits = scan_file(os.path.join(d, "bin"))
        self.assertEqual(hits[0]["symbols"][0]["name"], "_OBJC_CLASS_$_JSContext")
        self.assertIn("JavaScriptCore", hits[0]["symbols"][0]["provider"])


if __name__ == "__main__":
    unittest.main()
