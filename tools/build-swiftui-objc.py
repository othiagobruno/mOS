#!/usr/bin/env python3
# mOS — porte para Linux de tools/build-swiftui-objc.ps1 (fase 6 de docs/swift-objc-runtime.md) e da parte C/C++ de
# tools/build-openswiftui-deps.ps1 que ele liga: o SwiftUI.framework para binários Swift da Apple.
# O OpenSwiftUI e as dependências (github.com/OpenSwiftUIProject, MIT) compilados sobre a stdlib COM interop ObjC
# (build/swift/objc), com library evolution e -module-abi-name SwiftUI, num dylib só:
#   /System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI   (em build/swift/objc/dylib)
# Fontes fixados por commit e SHA-256 (o .ps1 baixava o main sem fixar); baixados para .tools/osui-src se faltarem.
# Antes: tools/build-libswift-real.sh, build-swift-combine.sh e build-swift-foundation-objc.sh (Foundation, Observation).
#   tools/build-swiftui-objc.py                    tudo (Src, Native, Deps, Core, UI, Link)
#   tools/build-swiftui-objc.py --from Core        a partir de uma etapa
#   tools/build-swiftui-objc.py --only UI --typecheck
#   MOS_ARCH=arm64 tools/build-swiftui-objc.py     fatia arm64 (build/swift/objc-arm64, como build-swift-combine.sh)
import argparse, hashlib, os, re, shutil, subprocess, sys, tarfile, time, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARCH = os.environ.get('MOS_ARCH', 'x86_64')
if ARCH not in ('x86_64', 'arm64'):
    sys.exit('arquitetura desconhecida: %s (x86_64 ou arm64)' % ARCH)
TRIPLE = '%s-apple-macos26' % ARCH
ZIGT = 'aarch64-macos' if ARCH == 'arm64' else 'x86_64-macos'
SWB = os.path.join(ROOT, 'build', 'swift')
SFX = '-arm64' if ARCH == 'arm64' else ''

# commits do main de cada repositório em 7 nov. 2026 (o porte do Windows de 8 nov. usou o main da época)
REPOS = {
    'OpenSwiftUI':        ('808ef19d5658a05ebbdfd60fe431d7559e8bc9cf', '8f497af009019427828fdecfb5582a9de5f6ecb6d98adfff4871601a4847fb6a'),
    'Compute':            ('41ecaef85f8bd3f73c3ee42a253cc603fd8df0fe', '6e02d83fd99911dc167120f72e4094f8df183bc12efca90ef753b0e7898997da'),
    'OpenRenderBox':      ('f8b4f1f1ef3cd32f3395cc99ae5a5b6d83b0b5cc', 'fae543d0c5c795882a38207afe1d3980b320d9f1fe6e457673ac28ed10b9a1ed'),
    'OpenAttributeGraph': ('46f2e1b832fa9626bd88f62ba34ba0b799b5206c', '2436a04afa9808f70b7feb8a58992f3bdae725558c1e00a4fd946a34f29a7fd3'),
    'OpenCoreGraphics':   ('de4866d03ab727b5204633a2c29cb1412dd90ee6', '7490fb808f9b4ab9c8b053cdc54724372b69129953f6cf76dd9c76ab3b5988b3'),
}
SRC = os.environ.get('MOS_OSUI_SRC', os.path.join(ROOT, '.tools', 'osui-src'))
DEPS = os.path.join(SRC, 'deps')
OSUI = os.path.join(SRC, 'main', 'OpenSwiftUI-main', 'Sources')


def find_tool(env, cands):
    v = os.environ.get(env)
    if v:
        return v
    for c in cands:
        if c and os.path.exists(c):
            return c
    return None


ZIG = find_tool('MOS_ZIG', [shutil.which('zig')])
FE = find_tool('SWIFTC', [os.path.join(ROOT, '.tools/swift-6.4.2/usr/bin/swift-frontend')])
SWSRC = os.environ.get('SWIFT_SRC', os.path.join(ROOT, '.tools', 'swift-src'))
RES = '/tmp/mos-swift-resdir'
OBJC = os.path.join(SWB, 'objc' + SFX)
OV = os.path.join(OBJC, 'overlays')
INC = os.path.join(OBJC, 'sdk', 'include')
SUI = os.path.join(OBJC, 'swiftui')
NAT = os.path.join(SWB, 'openswiftui' + SFX)  # objetos C/C++ da variante nativa
NSDK = os.path.join(ROOT, 'userland', 'sdk', 'openswiftui')
MINE = os.path.join(ROOT, 'userland', 'sdk', 'swift-objc', 'SwiftUI')
SHIM = os.path.join(NSDK, 'include')
# módulo clang por baixo do overlay DeveloperToolsSupport (userland/frameworks/DeveloperToolsSupport, da Beatriz)
DTS_INC = os.path.join(ROOT, 'userland', 'frameworks', 'DeveloperToolsSupport', 'include')
CTINC = os.path.join(ROOT, 'userland', 'sdk', 'swift-objc', 'SwiftUI', 'CoreText', 'include')
SDKSWIFT = os.path.join(NSDK, 'swift')
IMPORTER = os.path.join(NSDK, 'importer')
STAGES = ['Src', 'Native', 'Deps', 'Core', 'UI', 'Link']

fails = 0


def log(*a):
    print(*a, flush=True)


def run(cmd, out=None):
    """roda; devolve (código, texto)"""
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    if out:
        with open(out, 'w') as f:
            f.write(p.stdout)
    return p.returncode, p.stdout


# ---------------------------------------------------------------- fontes
def stage_src():
    dl = os.path.join(SRC, 'dl')
    os.makedirs(dl, exist_ok=True)
    for repo, (rev, sha) in REPOS.items():
        dest = os.path.join(SRC, 'main', 'OpenSwiftUI-main') if repo == 'OpenSwiftUI' else os.path.join(DEPS, repo + '-main')
        stamp = os.path.join(dest, '.mos-rev')
        if os.path.exists(stamp) and open(stamp).read().strip() == rev:
            continue
        tgz = os.path.join(dl, '%s-%s.tar.gz' % (repo, rev))
        if not os.path.exists(tgz):
            log('baixando %s %s...' % (repo, rev[:12]))
            urllib.request.urlretrieve('https://codeload.github.com/OpenSwiftUIProject/%s/tar.gz/%s' % (repo, rev), tgz + '.part')
            os.replace(tgz + '.part', tgz)
        h = hashlib.sha256(open(tgz, 'rb').read()).hexdigest()
        if h != sha:
            sys.exit('%s: SHA-256 não bate (%s)' % (tgz, h))
        if os.path.exists(dest):
            shutil.rmtree(dest)
        tmp = dest + '.tmp'
        if os.path.exists(tmp):
            shutil.rmtree(tmp)
        os.makedirs(tmp)
        with tarfile.open(tgz) as t:
            t.extractall(tmp, filter='data')
        top = os.listdir(tmp)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        os.replace(os.path.join(tmp, top[0]), dest)
        os.rmdir(tmp)
        open(stamp, 'w').write(rev + '\n')
        log('%s: %s' % (repo, rev[:12]))


# ---------------------------------------------------------------- C/C++ da variante nativa
COMMON = ['-target', ZIGT, '-O2', '-fno-sanitize=all', '-ffunction-sections', '-DNDEBUG', '-w', '-c',
          '-isystem' + os.path.join(SWSRC, 'include'), '-isystem' + os.path.join(SWSRC, 'stdlib', 'include'),
          '-isystem' + os.path.join(SWSRC, 'stdlib', 'public', 'SwiftShims')]
CXX = ['-std=c++20', '-fno-exceptions', '-fno-rtti']


def walk(dirs, exts, skip=()):
    out = []
    for d in dirs:
        for r, _, fs in os.walk(d):
            for f in sorted(fs):
                if os.path.splitext(f)[1] in exts and f not in skip:
                    out.append(os.path.join(r, f))
    return out


def build_files(name, files, cflags, cxxflags):
    global fails
    out = os.path.join(NAT, name)
    os.makedirs(out, exist_ok=True)
    ok, bad = 0, []
    for f in files:
        o = os.path.join(out, os.path.splitext(os.path.basename(f))[0] + '.o')
        is_cxx = f.endswith(('.cpp', '.mm'))
        # .mm: só a parte C++ (o trecho ObjC fica atrás de __OBJC__ e não é usado no mOS)
        src = ['-x', 'c++', f] if f.endswith('.mm') else [f]
        code, txt = run([ZIG, 'c++' if is_cxx else 'cc'] + COMMON + (cxxflags if is_cxx else cflags) + src + ['-o', o])
        if code:
            bad.append(os.path.basename(f))
            for l in [l for l in txt.splitlines() if 'error:' in l][:4]:
                log('  ' + l[:240])
        else:
            ok += 1
    log('%s (C/C++): %d ok, %d com erro %s' % (name, ok, len(bad), '(' + ', '.join(bad) + ')' if bad else ''))
    fails += len(bad)


def stage_native():
    global fails
    oag = os.path.join(DEPS, 'OpenAttributeGraph-main', 'Sources')
    # Compute: motor de grafo (o OpenAttributeGraphShims o usa com OPENATTRIBUTEGRAPH_COMPUTE)
    s = os.path.join(DEPS, 'Compute-main', 'Sources')
    inc = ['-I' + SHIM, '-include', os.path.join(SHIM, 'mos_openswiftui_prelude.h'), '-I' + s + '/Platform/include',
           '-I' + s + '/Utilities/include', '-I' + s + '/ComputeCxx', '-I' + s + '/ComputeCxx/internalInclude',
           '-I' + s + '/ComputeCxx/include', '-I' + s + '/SwiftCorelibsCoreFoundation/include', '-isystem' + oag + '/SwiftCorelibs/include',
           '-DCOMPILED_WITH_SWIFT', '-DPURE_BRIDGING_MODE', '-DCOMPUTE_MOS', '-DSWIFT_INLINE_NAMESPACE=__runtime']
    files = walk([s + '/Platform', s + '/Utilities', s + '/ComputeCxx'], ('.c', '.cpp'), ('DebugServer.cpp', 'Connection.cpp'))
    files.append(os.path.join(NSDK, 'src', 'compute_mos.cpp'))
    build_files('Compute', files, inc + ['-std=c11'], inc + CXX)
    # OpenRenderBox (C/C++): paths, cores, animação
    s = os.path.join(DEPS, 'OpenRenderBox-main', 'Sources')
    inc = ['-I' + SHIM, '-include', os.path.join(SHIM, 'mos_openswiftui_prelude.h'), '-I' + s + '/OpenRenderBoxCxx',
           '-I' + s + '/OpenRenderBoxCxx/include', '-I' + s + '/OpenRenderBoxShimsCxx/include', '-I' + oag,
           '-isystem' + oag + '/SwiftCorelibs/include', '-isystem' + os.path.join(ROOT, 'userland', 'sdk', 'include'), '-fblocks',
           '-DOPENRENDERBOX', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENRENDERBOX_MOS']
    build_files('OpenRenderBox', walk([s + '/OpenRenderBoxCxx', s + '/OpenRenderBoxShimsCxx'], ('.c', '.cpp', '.mm')), inc + ['-std=c11'], inc + CXX)
    # OpenSwiftUI_SPI (C do OpenSwiftUI): include próprio com o módulo principal e os Shims (menos o CoreText_Private,
    # que no mOS é módulo Swift); o mOS é plataforma não-Darwin (OPENSWIFTUI_MOS no OpenSwiftUITargetConditionals.h)
    spi = os.path.join(OSUI, 'OpenSwiftUI_SPI')
    spi_inc = os.path.join(NAT, 'OpenSwiftUI_SPI', 'include')
    if os.path.exists(spi_inc):
        shutil.rmtree(spi_inc)
    os.makedirs(os.path.join(spi_inc, 'OpenRenderBoxShims'))
    shim_mods = [('QuartzCore_Private', 'QuartzCore'), ('CoreFoundation_Private', 'CoreFoundation'), ('CoreGraphics_Private', 'CoreGraphics'),
                 ('CoreUI_Private', 'CoreUI'), ('OpenSwiftUICSymbols', 'CSymbols'), ('dyldPrivate', 'dyld'), ('kdebug_Private', 'kdebug'),
                 ('GraphicsServices_Private', 'GraphicsServices'), ('MobileGestaltPrivate', 'MobileGestalt'),
                 ('libAccessibilityPrivate', 'Accessibility'), ('UIFoundation_Private', 'UIFoundation')]
    mm = ('// mOS: OpenSwiftUI_SPI + os Shims (gerado por tools/build-swiftui-objc.py)\nmodule OpenSwiftUI_SPI {\n    umbrella "%s/Overlay"\n'
          '    export *\n\n    module Util {\n        umbrella "%s/Util"\n        export *\n    }\n}\n') % (spi, spi)
    cgp = os.path.join(spi_inc, 'Shims', 'CoreGraphics')
    os.makedirs(cgp)
    h = open(os.path.join(spi, 'Shims/CoreGraphics/CoreGraphics_Private.h')).read().replace('float cg_nullable *headroom', 'float * cg_nullable headroom')
    open(os.path.join(cgp, 'CoreGraphics_Private.h'), 'w').write(h)
    dyp = os.path.join(spi_inc, 'Shims', 'dyld')
    os.makedirs(dyp)
    h = open(os.path.join(spi, 'Shims/dyld/dyld_Private.h')).read().replace(
        '#if OPENSWIFTUI_TARGET_OS_DARWIN\n#include <mach-o/dyld.h>', '#if OPENSWIFTUI_TARGET_OS_DARWIN || defined(OPENSWIFTUI_MOS)\n#include <mach-o/dyld.h>')
    open(os.path.join(dyp, 'dyld_Private.h'), 'w').write(h)
    patched = {'CoreGraphics_Private': cgp, 'dyldPrivate': dyp}
    for k, d in shim_mods:
        mm += '\nmodule %s {\n    umbrella "%s"\n    export *\n}\n' % (k, patched.get(k, os.path.join(spi, 'Shims', d)))
    open(os.path.join(spi_inc, 'module.modulemap'), 'w').write(mm)
    shutil.copy(os.path.join(spi, 'OpenSwiftUIBase.h'), spi_inc)
    t = open(os.path.join(spi, 'OpenSwiftUITargetConditionals.h')).read()
    branch = ('#if defined(OPENSWIFTUI_MOS)   /* mOS: Mach-O do macOS, mas sem o SDK/ObjC da Apple -> caminho não-Darwin */\n'
              '#define OPENSWIFTUI_TARGET_OS_DARWIN       0\n#define OPENSWIFTUI_TARGET_OS_LINUX        0\n#define OPENSWIFTUI_TARGET_OS_WINDOWS      0\n'
              '#define OPENSWIFTUI_TARGET_OS_BSD          0\n#define OPENSWIFTUI_TARGET_OS_ANDROID      0\n#define OPENSWIFTUI_TARGET_OS_CYGWIN       0\n'
              '#define OPENSWIFTUI_TARGET_OS_WASI         0\n#elif __APPLE__')
    t = re.sub(r'(?m)^#if __APPLE__\s*$', lambda m: branch, t, count=1)
    open(os.path.join(spi_inc, 'OpenSwiftUITargetConditionals.h'), 'w').write(t)
    for f in os.listdir(os.path.join(s, 'OpenRenderBoxShimsCxx/include/OpenRenderBox/OpenRenderBoxShims')):
        if f.endswith('.h'):
            shutil.copy(os.path.join(s, 'OpenRenderBoxShimsCxx/include/OpenRenderBox/OpenRenderBoxShims', f), os.path.join(spi_inc, 'OpenRenderBoxShims'))
    defs = ['-DOPENSWIFTUI', '-DOPENSWIFTUI_MOS', '-DOPENSWIFTUI_CF_CGTYPES', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENSWIFTUI_LINK_COREUI=0',
            '-DOPENSWIFTUI_OPENCOMBINE', '-DOPENSWIFTUI_RELEASE_2024']
    cinc = ['-I' + spi_inc, '-I' + s + '/OpenRenderBoxCxx/include', '-I' + SHIM, '-include', os.path.join(SHIM, 'mos_openswiftui_prelude.h'),
            '-isystem' + oag + '/SwiftCorelibs/include', '-isystem' + os.path.join(ROOT, 'userland', 'sdk', 'include')] + defs
    # o C do SPI escreve à mão nomes Swift com o módulo OpenSwiftUI/OpenSwiftUICore; o framework usa -module-abi-name SwiftUI
    spi_src = os.path.join(NAT, 'OpenSwiftUI_SPI', 'src')
    if os.path.exists(spi_src):
        shutil.rmtree(spi_src)
    os.makedirs(spi_src)
    files = []
    # Shims/CSymbols: getSymbolPathName (dladdr). O dyld_Private.c não entra: o mOS tem o dyld de verdade no libSystem
    for f in walk([spi + '/Overlay', spi + '/Util', spi + '/Shims/CSymbols'], ('.c',)):
        x = open(f, encoding='utf-8').read()
        n = re.sub(r'\$s15OpenSwiftUICore', '$s7SwiftUI', re.sub(r'\$s11OpenSwiftUI(?=\d)', '$s7SwiftUI', x))
        if n != x:
            f = os.path.join(spi_src, os.path.basename(f))
            open(f, 'w').write(n)
        files.append(f)
    cinc += ['-iquote', spi + '/Util', '-iquote', spi + '/Overlay']
    build_files('OpenSwiftUI_SPI', files, cinc + ['-std=gnu11'], cinc + CXX)
    # CoreText: nesta variante é o do libSystem (o ctfont.c da variante nativa cria CF no formato Swift e não entra)


# ---------------------------------------------------------------- SDK desta variante e Swift
SINC = os.path.join(SUI, 'sdk-include')
MCACHE = os.path.join(SUI, 'modcache')


def prepare_sdk():
    if os.path.exists(SINC):
        shutil.rmtree(SINC)
    shutil.copytree(INC, SINC, symlinks=True)
    shutil.copytree(os.path.join(MINE, 'sdk', 'AppKit'), os.path.join(SINC, 'AppKit'), dirs_exist_ok=True)
    mmf = os.path.join(SINC, 'module.modulemap')
    mm = open(mmf).read()
    mm += '\n// AppKit do mOS (userland/libsystem/appkit.c): o subconjunto que o SwiftUI.framework usa (NSHostingView)\nmodule AppKit [system] [extern_c] {\n    header "AppKit/AppKit.h"\n    export *\n}\n'
    open(mmf, 'w').write(mm)
    nso = os.path.join(SINC, 'objc', 'NSObject.h')
    if os.path.exists(nso):
        t = open(nso).read()
        open(nso, 'w').write(t.replace('- (instancetype)init\n', '- (instancetype _Nonnull)init /* mOS: não-nulo, como no SDK da Apple */\n'))
    # ponte automática NSString -> String etc. (no SDK da Apple vem das API notes do Foundation)
    fh = os.path.join(SINC, 'Foundation', 'Foundation.h')
    t = open(fh).read()
    for b in ['NSString=Swift.String', 'NSArray=Swift.Array', 'NSDictionary=Swift.Dictionary', 'NSSet=Swift.Set', 'NSData=Foundation.Data',
              'NSURL=Foundation.URL', 'NSDate=Foundation.Date', 'NSUUID=Foundation.UUID', 'NSLocale=Foundation.Locale',
              'NSTimeZone=Foundation.TimeZone', 'NSDateComponents=Foundation.DateComponents', 'NSIndexSet=Foundation.IndexSet',
              'NSNotification=Foundation.Notification', 'NSURLRequest=Foundation.URLRequest', 'NSIndexPath=Foundation.IndexPath']:
        k, v = b.split('=')
        t = re.sub(r'(?m)^@interface %s([ <])' % k, lambda m: '__attribute__((swift_bridge("%s"))) @interface %s%s' % (v, k, m.group(1)), t)
    open(fh, 'w').write(t)
    shutil.copy(os.path.join(MINE, 'sdk', 'Foundation', 'NSFoundationSwiftUI.h'), os.path.join(SINC, 'Foundation'))
    mm = open(mmf).read()
    mm += '\n// classes do Foundation do libSystem que o SwiftUI.framework usa\nmodule _MOSFoundationSwiftUI [system] [extern_c] {\n    header "Foundation/NSFoundationSwiftUI.h"\n    export *\n}\n'
    shutil.copy(os.path.join(MINE, 'sdk', 'CoreGraphics', 'CGSwiftUI.h'), os.path.join(SINC, 'CoreGraphics'))
    mm += '\n// SwiftUI.framework\nmodule _MOSCoreGraphicsSwiftUI [system] [extern_c] {\n    header "CoreGraphics/CGSwiftUI.h"\n    export *\n}\n'
    if 'module CommonCrypto' not in mm:
        mm += 'module CommonCrypto [system] [extern_c] {\n    header "CommonCrypto/CommonDigest.h"\n    export *\n}\n'
    open(mmf, 'w').write(mm)
    pth = os.path.join(SINC, 'pthread.h')
    if os.path.exists(pth):
        t = open(pth).read()
        open(pth, 'w').write(t.replace('int pthread_key_create(pthread_key_t *, void (* _Nullable)(void *));',
                                       'int pthread_key_create(pthread_key_t *, void (* _Nullable)(void * _Nullable));'))
    if os.path.exists(MCACHE):
        shutil.rmtree(MCACHE)


TYPECHECK = False


def build_swift(name, files, extra, lang='5', evolution=True):
    global fails
    out = os.path.join(SUI, name)
    os.makedirs(out, exist_ok=True)
    a = ['-target', TRIPLE, '-resource-dir', RES, '-module-name', name, '-parse-as-library', '-swift-version', lang, '-O',
         '-I', OBJC, '-I', OV, '-Xcc', '-I' + DTS_INC, '-module-cache-path', MCACHE, '-I', os.path.join(SWSRC, 'stdlib/public/SwiftShims/swift/shims'),
         '-disable-implicit-concurrency-module-import', '-enable-experimental-feature', 'Extern',
         '-Xcc', '-isystem' + SINC, '-Xcc', '-nostdlibinc', '-Xcc', '-DMOS_SWIFT_OBJC=1', '-Xcc', '-Wno-nullability-completeness']
    if TYPECHECK:
        a = ['-typecheck'] + a
    else:
        a = ['-c'] + a + ['-emit-module-path', os.path.join(out, name + '.swiftmodule'), '-o', os.path.join(out, name + '.o')]
    for d in sorted(os.listdir(SUI)):
        if d != name and os.path.exists(os.path.join(SUI, d, d + '.swiftmodule')):
            a += ['-I', os.path.join(SUI, d)]
    if evolution:
        a.append('-enable-library-evolution')
    a += extra + files
    rsp = os.path.join(out, name + '.rsp')
    with open(rsp, 'w') as f:
        for x in a:
            if re.search(r'[\s"]', x):
                x = '"' + x.replace('\\', '\\\\').replace('"', '\\"') + '"'
            f.write(x + '\n')
    t0 = time.time()
    code, txt = run([FE, '-frontend', '@' + rsp], os.path.join(out, 'build.log'))
    errs = [l for l in txt.splitlines() if re.search(r':\d+:\d+: error: |<unknown>:0: error: |^error: ', l)]
    dt = int(time.time() - t0)
    if code or errs:
        for l in errs[:30]:
            log('  ' + l)
        log('%s: FALHOU (%d erros, %ds; log em %s/build.log)' % (name, len(errs), dt, out))
        fails += 1
        return False
    log('%s: ok (%d fontes, %ds)' % (name, len(files), dt))
    return True


def build_c(name, src, flags=()):
    global fails
    out = os.path.join(SUI, name)
    os.makedirs(out, exist_ok=True)
    o = os.path.join(out, os.path.splitext(os.path.basename(src))[0] + '.o')
    code, txt = run([ZIG, 'cc', '-target', ZIGT, '-O2', '-c', '-w'] + list(flags) + [src, '-o', o])
    if code:
        log(txt[-2000:])
        log('%s: FALHOU' % name)
        fails += 1


NOIMPORT = ('Darwin|ObjectiveC|Combine|AppKit|UIKit|QuartzCore|CoreGraphics|os|OSLog|Observation|UniformTypeIdentifiers|Metal|simd|XPC|'
            'IOKit|CoreImage|CoreAudio|CoreMIDI|Spatial|QuickLookUI|CryptoKit|Accessibility|DeveloperToolsSupport|SwiftUI')
EXTRAS = False


def convert_swift(txt, mode='up'):
    lines = re.split(r'\r?\n', txt)
    for i, l in enumerate(lines):
        if re.match(r'^\s*#\s*(if|elseif)\b', l):
            if mode == 'up':
                l = re.sub(r'\bos\(\s*macOS\s*\)', 'false', l)
                l = re.sub(r'\bcanImport\(\s*(%s)\s*\)' % NOIMPORT, 'false', l)
            else:
                l = re.sub(r'\bcanImport\(\s*(%s)\s*\)' % NOIMPORT.replace('CoreGraphics|', ''), 'false', l)
            if mode == 'up' and re.search(r'canImport\(\s*_Concurrency\s*\)', l):
                l = re.sub(r'^(\s*#\s*(?:if|elseif)).*$', r'\1 false // mOS: sem _Concurrency', l)
            elif mode != 'up':
                l = re.sub(r'\bcanImport\(\s*_Concurrency\s*\)', 'false', l)
        elif not re.match(r'^\s*#', l):
            l = re.sub(r'@MainActor(\s*\(unsafe\))?\s?', '', l)
            l = re.sub(r'@isolated\(any\)\s?', '', l)
            l = re.sub(r'\{\s+in\b', '{', l)
            l = re.sub(r'^(\s*)(@_spi\(OpenSwiftUI\)\s+)?((?:public|package|internal)\s+)?import OpenObservation\b',
                       lambda m: m.group(1) + '@_spi(OpenSwiftUI) ' + (m.group(3) or '') + 'import Observation', l)
            l = re.sub(r'\bOpenObservation\.', 'Observation.', l)
            l = re.sub(r'\bimport class Foundation\.NSAttributedString\b', 'import class MOSFoundationExtras.NSAttributedString', l)
            l = re.sub(r'\bimport class Foundation\.(Thread|Bundle|UserDefaults|NSLock|Formatter|NumberFormatter|DateFormatter)\b',
                       r'import class _MOSFoundationSwiftUI.\1', l)
            l = re.sub(r'\bFoundation\.(Progress|Thread|Bundle|UserDefaults)\b', r'\1', l)
            l = re.sub(r'\bMOSCGMutablePathRef\b', 'CGMutablePath', l)
            l = re.sub(r'\bMOSCGPathRef\b', 'CGPath', l)
            l = re.sub(r'\bMOSCGPathElement\b', 'CGPathElement', l)
            if EXTRAS:
                m = re.match(r'^(\s*)((?:@_spi\([^)]*\)\s*)*)((?:public|package|internal|@_exported)\s+)?import Foundation\s*$', l)
                if m:
                    l = l + '\n' + m.group(1) + (m.group(3) or '') + 'import MOSFoundationExtras'
        lines[i] = l
    return '\n'.join(lines)


def prepare_src(name, src_dir, overlay_dirs, ov_mode='ov'):
    dst = os.path.join(SUI, 'src', name)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    os.makedirs(dst)
    if src_dir:
        for r, _, fs in os.walk(src_dir):
            for f in fs:
                if not f.endswith('.swift'):
                    continue
                p = os.path.join(r, f)
                out = os.path.join(dst, os.path.relpath(p, src_dir))
                os.makedirs(os.path.dirname(out), exist_ok=True)
                open(out, 'w').write(convert_swift(open(p, encoding='utf-8').read()))
    for od in overlay_dirs:
        if not od or not os.path.isdir(od):
            continue
        for r, _, fs in os.walk(od):
            for f in fs:
                p = os.path.join(r, f)
                out = os.path.join(dst, os.path.relpath(p, od))
                os.makedirs(os.path.dirname(out), exist_ok=True)
                if f.endswith('.swift'):
                    open(out, 'w').write(convert_swift(open(p, encoding='utf-8').read(), ov_mode))
                elif f.endswith('.remove'):
                    try:
                        os.remove(out[:-7])
                    except FileNotFoundError:
                        pass
                else:
                    shutil.copy(p, out)
    return dst


def apply_patches(d, lst):
    for rel, old, new in lst:
        f = os.path.join(d, rel)
        if not os.path.exists(f):
            log('  remendo: falta ' + rel)
            continue
        x = open(f).read()
        if old not in x:
            log('  remendo não casou em %s: %s' % (rel, old))
            continue
        open(f, 'w').write(x.replace(old, new))


CORE_PATCHES = [
    # o NSLocalizedString do MOSFoundationExtras (-module-abi-name Foundation) não é achado pelo núcleo: o Bundle da
    # assinatura casa com o `package struct Bundle` do swift-foundation e a declaração cai. É o mesmo que ele faz:
    ('View/Text/Text/Text.swift', """        NSLocalizedString(
            key,
            tableName: tableName,
            bundle: .kit,
            value: "",
            comment: comment
        )""", """        Bundle.kit.localizedString(forKey: key, value: "", table: tableName)"""),
    ('Shim/CoreFoundationNonDarwinShims.swift', 'equal: { lhs, rhs in (lhs as? NSObject)?.isEqual(rhs) ?? false },',
     'equal: { lhs, rhs in DarwinBoolean((lhs as? NSObject)?.isEqual(rhs) ?? false) },'),
    ('View/Text/Typesetting/TypesettingLanguage.swift', ').map(String.init) {', ').map({ $0 as String }) {'),
    ('Util/AccessibilityShims.swift', 'coder.decodeObject(of: NSString.self, forKey: "label") as String?', 'coder.decodeObject(of: NSString.self, forKey: "label") as? String'),
    ('Util/AccessibilityShims.swift', 'coder.decodeObject(of: NSString.self, forKey: "value") as String?', 'coder.decodeObject(of: NSString.self, forKey: "value") as? String'),
    ('Util/AccessibilityShims.swift', 'coder.decodeObject(of: NSAttributedString.self, forKey: "attributedLabel")?.copy() as? NSAttributedString', 'nil'),
    ('Util/AccessibilityShims.swift', 'coder.decodeObject(of: NSAttributedString.self, forKey: "attributedValue")?.copy() as? NSAttributedString', 'nil'),
    ('Util/AccessibilityShims.swift', 'coder.encode(attributedLabel, forKey: "attributedLabel")', '_ = attributedLabel'),
    ('Util/AccessibilityShims.swift', 'coder.encode(attributedValue, forKey: "attributedValue")', '_ = attributedValue'),
    ('Accessibility/AccessibilityValue.swift', ').map { $0 as NSNumber }', ').map { NSNumber(value: Double("\\($0)") ?? 0) }'),
    ('View/Image/GraphicsImage.swift', 'case ioSurface(IOSurfaceRef)', 'case ioSurface(IOSurface)'),
    ('Log/Logging.swift', '@usableFromInline\npackage enum Log {', 'public enum Log {'),
    ('Log/Logging.swift', 'runtimeIssuesLog', '_mosRuntimeIssuesLogger'),
    ('Data/Environment/Environment.swift', 'Log.runtimeIssuesLog', 'Log._mosRuntimeIssuesLogger'),
]


def swift_files(d):
    out = []
    for r, _, fs in os.walk(d):
        out += [os.path.join(r, f) for f in sorted(fs) if f.endswith('.swift')]
    return sorted(out)


CMP = os.path.join(DEPS, 'Compute-main', 'Sources')
ORB = os.path.join(DEPS, 'OpenRenderBox-main', 'Sources')
ORBINC = os.path.join(SUI, 'orb-include')
SPIINC = os.path.join(NAT, 'OpenSwiftUI_SPI', 'include')
OSUI_DEFS = ['OPENSWIFTUI', 'OPENSWIFTUI_RELEASE_2024', 'OPENSWIFTUI_SUPPORT_2021_API', 'OPENSWIFTUI_SUPPORT_2022_API', 'OPENSWIFTUI_SUPPORT_2023_API',
             'OPENSWIFTUI_SUPPORT_2024_API', 'OPENSWIFTUI_CF_CGTYPES', 'OPENRENDERBOX_CF_CGTYPES', 'OPENSWIFTUI_SWIFT_LOG',
             'OPENSWIFTUI_COMPUTE', 'OPENSWIFTUI_MOS', 'OPENSWIFTUI_MOS_OBJC', 'OPENSWIFTUI_XCFRAMEWORK_BUILD']


def availability_names():
    # todos os OpenSwiftUI_<versão> que os fontes citam (o Package.swift do OpenSwiftUI os define por versão do SDK)
    names = set()
    for d in (OSUI, SDKSWIFT, MINE):
        for p in swift_files(d):
            names.update(re.findall(r'\bOpenSwiftUI_((?:[A-Za-z]+_)?v\d+(?:_\d+)?)\b', open(p, encoding='utf-8', errors='replace').read()))
    return sorted(names)


def osui_flags():
    f = ['-I', os.path.join(SDKSWIFT, 'COpenSwiftUI'), '-I', SPIINC, '-I', CMP + '/ComputeCxx/include', '-I', ORBINC,
         '-I', CTINC, '-I', os.path.join(ROOT, 'userland', 'sdk', 'swift', 'MOSBridge')]
    for a in ['-I' + SPIINC, '-I' + ORBINC, '-I' + CMP + '/Platform/include', '-I' + CMP + '/ComputeCxx/include',
              '-I' + CTINC, '-DOPENSWIFTUI', '-DOPENSWIFTUI_MOS', '-DOPENSWIFTUI_CF_CGTYPES',
              '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENRENDERBOX_MOS', '-DOPENSWIFTUI_LINK_COREUI=0']:
        f += ['-Xcc', a]
    for d in OSUI_DEFS:
        f += ['-D', d]
    for ft in ['BareSlashRegexLiterals', 'InternalImportsByDefault', 'InferSendableFromCaptures']:
        f += ['-enable-upcoming-feature', ft]
    f += ['-package-name', 'OpenSwiftUI', '-module-abi-name', 'SwiftUI', '-experimental-spi-only-imports', '-unavailable-decl-optimization=stub']
    minv = 'macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, visionOS 1.0'
    for v in availability_names():
        f += ['-define-availability', 'OpenSwiftUI_%s:%s' % (v, minv)]
    f += ['-define-availability', '_distantFuture:macOS 99.0, iOS 99.0, tvOS 99.0, watchOS 99.0, visionOS 99.0']
    return f


def stage_deps():
    build_c('c', os.path.join(MINE, 'c', 'cf_objc_runtime.c'))
    build_c('c', os.path.join(MINE, 'c', 'ProtocolDescriptor.c'))
    build_swift('Glibc', [os.path.join(MINE, 'Glibc', 'Glibc.swift')], [])
    build_swift('Logging', swift_files(os.path.join(SDKSWIFT, 'Logging')), [])
    build_swift('MOSFoundationExtras', swift_files(os.path.join(MINE, 'Extras')), ['-module-abi-name', 'Foundation'])
    # CoreText desta variante: cabeçalho com a API da Apple sobre o CoreText do libSystem (SwiftUI/CoreText)
    ct = ['-I', CTINC, '-Xcc', '-I' + CTINC]
    build_swift('CoreText', swift_files(os.path.join(MINE, 'CoreText')), ct)
    build_swift('CoreText_Private', swift_files(os.path.join(SDKSWIFT, 'CoreText_Private')), ct)
    ov2 = os.path.join(NSDK, 'overlay')
    sup = prepare_src('ComputeCxxSwiftSupport', CMP + '/ComputeCxxSwiftSupport', [ov2 + '/Compute/ComputeCxxSwiftSupport'])
    build_swift('ComputeCxxSwiftSupport', swift_files(sup), [])
    cflags = ['-I', CMP + '/ComputeCxx/include', '-Xcc', '-I' + CMP + '/Platform/include', '-Xcc', '-I' + CMP + '/ComputeCxx/include']
    cmp_src = prepare_src('Compute', CMP + '/Compute', [ov2 + '/Compute/Compute', os.path.join(MINE, 'Replace', 'Compute')])
    build_swift('Compute', swift_files(cmp_src), cflags, '6')
    sh_src = prepare_src('OpenAttributeGraphShims', os.path.join(DEPS, 'OpenAttributeGraph-main/Sources/OpenAttributeGraphShims'),
                         [ov2 + '/OpenAttributeGraph/OpenAttributeGraphShims'])
    build_swift('OpenAttributeGraphShims', swift_files(sh_src), cflags + ['-D', 'OPENATTRIBUTEGRAPH_COMPUTE'])
    ocg = os.path.join(DEPS, 'OpenCoreGraphics-main', 'Sources')
    build_swift('OpenCoreGraphicsShims', swift_files(os.path.join(MINE, 'OpenCoreGraphicsShims')), [], '6')
    src = prepare_src('OpenQuartzCore', ocg + '/OpenQuartzCore', [])
    for p in swift_files(src):
        t = open(p).read()
        open(p, 'w').write(re.sub(r'\bOpenCoreGraphics\.', 'CoreGraphics.', re.sub(r'import OpenCoreGraphics\b', 'import CoreGraphics', t)))
    build_swift('OpenQuartzCore', swift_files(src), ['-enable-upcoming-feature', 'InternalImportsByDefault'], '6')
    src = prepare_src('OpenQuartzCoreShims', ocg + '/OpenQuartzCoreShims', [])
    build_swift('OpenQuartzCoreShims', swift_files(src), [], '6')
    if os.path.exists(ORBINC):
        shutil.rmtree(ORBINC)
    shutil.copytree(ORB + '/OpenRenderBoxCxx/include', ORBINC)
    mmf = os.path.join(ORBINC, 'module.modulemap')
    # as classes ObjC do OpenRenderBox (ORBAnimation, ORBLayer...) não existem no lado nativo, compilado sem ObjC
    bh = os.path.join(ORBINC, 'OpenRenderBox', 'ORBBase.h')
    b = open(bh).read()
    if '#if ORB_TARGET_OS_DARWIN && __OBJC__\n' not in b:
        sys.exit('ORBBase.h: remendo do ORB_OBJC_FOUNDATION não casou')
    open(bh, 'w').write(b.replace('#if ORB_TARGET_OS_DARWIN && __OBJC__\n', '#if ORB_TARGET_OS_DARWIN && __OBJC__ && !defined(OPENRENDERBOX_MOS)\n', 1))
    mm = open(mmf).read()   # lido antes de abrir para escrita (abrir com 'w' antes truncava o arquivo)
    open(mmf, 'w').write(re.sub(r'(?s)\r?\n(?:explicit )?module OpenRenderBoxCxx\.ObjC \{.*?\n\}\r?\n?', '\n', mm))
    xcc = []
    for a in ['-I' + ORBINC, '-DOPENRENDERBOX', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENRENDERBOX_MOS']:
        xcc += ['-Xcc', a]
    # SWIFT_PACKAGE: o Export.swift do OpenRenderBox só reexporta o módulo C (OpenRenderBoxCxx.C) dentro dele
    orb_flags = ['-I', ORBINC, '-D', 'SWIFT_PACKAGE', '-D', 'OPENRENDERBOX', '-D', 'OPENRENDERBOX_CF_CGTYPES', '-enable-upcoming-feature', 'InternalImportsByDefault',
                 '-package-name', 'OpenRenderBox'] + xcc
    orb_src = prepare_src('OpenRenderBox', ORB + '/OpenRenderBox', [ov2 + '/OpenRenderBox/OpenRenderBox'])
    build_swift('OpenRenderBox', swift_files(orb_src), orb_flags)
    build_swift('OpenRenderBoxShims', swift_files(ORB + '/OpenRenderBoxShims'), orb_flags)


def stage_core():
    mos = os.path.join(SDKSWIFT, 'OpenSwiftUIMOS')
    core = prepare_src('OpenSwiftUICore', OSUI + '/OpenSwiftUICore',
                       [mos + '/Replace/Sources/OpenSwiftUICore', os.path.join(NSDK, 'overlay', 'OpenSwiftUICore'), os.path.join(MINE, 'Replace', 'OpenSwiftUICore')])
    apply_patches(core, CORE_PATCHES)
    for p in swift_files(core):
        t = open(p).read()
        if '#isolation' in t:
            open(p, 'w').write(t.replace('#isolation', 'nil'))
    rt = os.path.join(core, 'View/Text/Resolve/ResolvedText.swift')
    if os.path.exists(rt):
        t = open(rt).read()
        n = t.replace('#if false\n        var adaptiveImageGlyph', '#if canImport(CoreText)\n        var adaptiveImageGlyph')
        if n != t:
            open(rt, 'w').write(n)
    files = swift_files(core) + swift_files(prepare_src('OpenSwiftUICore-mos', None, [mos + '/Core', os.path.join(MINE, 'Core')]))
    log('OpenSwiftUICore: %d fontes...' % len(files))
    build_swift('OpenSwiftUICore', files, osui_flags(), '5', True)


def stage_ui():
    mos = os.path.join(SDKSWIFT, 'OpenSwiftUIMOS')
    ui = prepare_src('OpenSwiftUI', OSUI + '/OpenSwiftUI', [mos + '/Replace/Sources/OpenSwiftUI', os.path.join(MINE, 'Replace', 'OpenSwiftUI')], 'up')
    for p in swift_files(ui):
        txt = open(p).read()
        o = txt.replace('#isolation', 'nil')
        lines = o.split('\n')
        for i, l in enumerate(lines):
            if re.match(r'^\s*#', l):
                continue
            n = re.sub(r'\)\s+async\b', ')', l)
            n = re.sub(r'\basync\s+throws\b', 'throws', n)
            n = re.sub(r'\basync\s+->', '->', n)
            lines[i] = re.sub(r'(?<!\.)\bawait\b', '', n)
        o = '\n'.join(lines)
        if o != txt:
            open(p, 'w').write(o)
    files = swift_files(ui) + swift_files(prepare_src('OpenSwiftUI-mos', None, [mos + '/App', os.path.join(MINE, 'UI')]))
    log('OpenSwiftUI: %d fontes...' % len(files))
    build_swift('OpenSwiftUI', files, osui_flags(), '5', True)


def witness_bridge(objs):
    """Tabelas de testemunha (WP) que os binários da Apple usam direto e o framework não emite.

    No SwiftUI da Apple os tipos e os protocolos de base (View, ViewModifier...) estão no mesmo módulo; no OpenSwiftUI
    estão em OpenSwiftUICore e OpenSwiftUI, dois módulos resilientes, e uma conformidade entre módulos resilientes não
    tem tabela estática, só o descritor (Mc). Para cada WP de userland/sdk/swift-objc/SwiftUI/c/apple-witness-tables.txt
    cujo descritor e cujos metadados existem, gera um símbolo com o nome da Apple e um construtor que o preenche com a
    tabela que o runtime monta (swift_getWitnessTable) ao carregar o framework."""
    lst = os.path.join(MINE, 'c', 'apple-witness-tables.txt')
    nm = find_tool('MOS_NM', [os.path.join(os.path.dirname(FE), 'llvm-nm'), shutil.which('llvm-nm')])
    if not os.path.exists(lst) or not nm:
        return None
    defs = set()
    libs = [os.path.join(OBJC, 'dylib', 'usr/lib/swift', f) for f in ('libswiftCore.dylib', 'libswiftFoundation.dylib')]
    for o in objs + [l for l in libs if os.path.exists(l)]:
        _, txt = run([nm, '-g', '--defined-only', '-j', o])
        defs.update(s[1:] if s.startswith('_') else s for s in txt.split())
    out, n = [], 0
    for wp in [l.strip() for l in open(lst) if l.strip() and not l.startswith('#')]:
        if wp in defs or not wp.endswith('WP'):
            continue
        mc = wp[:-2] + 'Mc'
        if mc not in defs:
            continue
        # o tipo é o prefixo mais longo com acessor de metadados (Ma) ou metadados diretos (N)
        body = wp[:-2]
        typ = None
        for i in range(len(body), 2, -1):
            if body[:i] + 'Ma' in defs:
                typ = ('Ma', body[:i] + 'Ma'); break
            if body[:i] + 'N' in defs:
                typ = ('N', body[:i] + 'N'); break
        if not typ:
            continue
        k = 'w%d' % n
        n += 1
        out.append('extern const char %s_mc[] __asm__("_%s");' % (k, mc))
        if typ[0] == 'Ma':
            out.append('extern MR %s_ma(long) __asm__("_%s");' % (k, typ[1]))
            md = '%s_ma(0).md' % k
        else:
            out.append('extern const char %s_md[] __asm__("_%s");' % (k, typ[1]))
            md = '(const void *)%s_md' % k
        out.append('__attribute__((visibility("default"))) const void *%s[MAXW] __asm__("_%s");' % (k, wp))
        out.append('static void %s_fill(void) { fill(%s, %s_mc, %s); }' % (k, k, k, md))
    if not n:
        return None
    src = os.path.join(SUI, 'c', 'witness_bridge.c')
    os.makedirs(os.path.dirname(src), exist_ok=True)
    calls = ''.join('    w%d_fill();\n' % i for i in range(n))
    open(src, 'w').write("""/* gerado por tools/build-swiftui-objc.py (witness_bridge): %d tabelas de testemunha com o nome da Apple */
#include <stdint.h>
#define MAXW 64
typedef struct { const void *md; long state; } MR;
extern const void **swift_getWitnessTable(const void *conformance, const void *type, const void *const *args);
static const void *rel(const char *p) {           /* ponteiro relativo (indireto quando o bit 0 está ligado) */
    int32_t o = *(const int32_t *)p;
    const char *a = p + (o & ~1);
    return (o & 1) ? *(const void *const *)a : (const void *)a;
}
static void fill(const void **dst, const char *conf, const void *type) {
    const char *proto = rel(conf);
    uint32_t nreq = *(const uint32_t *)(proto + 16);   /* ProtocolDescriptor.NumRequirements */
    const void **t = swift_getWitnessTable(conf, type, 0);
    if (!t || nreq + 1 > MAXW) return;
    for (uint32_t i = 0; i <= nreq; i++) dst[i] = t[i];
}
%s
__attribute__((constructor)) static void mos_witness_bridge(void) {
%s}
""" % (n, '\n'.join(out), calls))
    o = src[:-2] + '.o'
    code, txt = run([ZIG, 'cc', '-target', ZIGT, '-O2', '-c', '-w', src, '-o', o])
    if code:
        log(txt[-1500:]); log('witness_bridge: FALHOU'); sys.exit(1)
    log('witness_bridge: %d tabelas WP' % n)
    return o


def stage_link():
    objs = []
    for r, ds, fs in os.walk(SUI):
        if os.path.relpath(r, SUI).split(os.sep)[0] == 'src':
            continue
        objs += [os.path.join(r, f) for f in fs if f.endswith('.o') and f != 'witness_bridge.o']
    for d in ['Compute', 'OpenRenderBox', 'OpenSwiftUI_SPI']:
        dd = os.path.join(NAT, d)
        objs += [os.path.join(dd, f) for f in sorted(os.listdir(dd)) if f.endswith('.o') and not f.endswith('_swift.o') and os.path.getsize(os.path.join(dd, f)) > 0]
    wt = witness_bridge(objs)
    if wt:
        objs.append(wt)
    dy = os.path.join(OBJC, 'dylib')
    install = '/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI'
    out = dy + install
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cmb = os.path.join(SUI, 'c', 'Combine.dylib')
    if os.path.lexists(cmb):
        os.remove(cmb)
    os.symlink(dy + '/System/Library/Frameworks/Combine.framework/Versions/A/Combine', cmb)
    libs = [cmb]
    # DeveloperToolsSupport (ImageResource/ColorResource; userland/frameworks/DeveloperToolsSupport): o SwiftUI da Apple liga nele
    dts = dy + '/System/Library/Frameworks/DeveloperToolsSupport.framework/Versions/A/DeveloperToolsSupport'
    if os.path.exists(dts):
        lnk = os.path.join(SUI, 'c', 'DeveloperToolsSupport.dylib')   # o zig só aceita dylib com extensão (como o Combine)
        if os.path.lexists(lnk):
            os.remove(lnk)
        os.symlink(dts, lnk)
        libs.append(lnk)
    for m in ['Core', 'Foundation', 'Dispatch', 'ObjectiveC', 'Darwin', 'CoreFoundation', 'CoreGraphics', 'os', 'Observation',
              '_Concurrency', '_StringProcessing']:
        p = os.path.join(dy, 'usr/lib/swift/libswift%s.dylib' % m)
        if os.path.exists(p):
            libs.append(p)
    code, txt = run([ZIG, 'c++', '-target', ZIGT, '-shared', '-o', out, '-Wl,-install_name,' + install, '-Wl,-undefined,dynamic_lookup'] + objs + libs)
    if code and sys.platform == 'darwin' and shutil.which('xcrun'):
        # o linker Mach-O do zig 0.17.1 cai com SIGSEGV no macOS (tools/zig-link.sh): o mesmo link pelo clang do Xcode
        log('SwiftUI.framework: o zig falhou (%d); repetindo com o clang++ do Xcode' % code)
        arch = 'arm64' if ZIGT.startswith(('aarch64', 'arm64')) else 'x86_64'
        code, txt = run(['xcrun', 'clang++', '-arch', arch, '-mmacosx-version-min=11', '-w', '-Wl,-not_for_dyld_shared_cache',
                         '-shared', '-o', out, '-Wl,-install_name,' + install, '-Wl,-undefined,dynamic_lookup'] + objs + libs)
        if code and os.path.exists(out):
            os.remove(out)
    if code:
        log('\n'.join(txt.splitlines()[:15]))
        log('SwiftUI.framework: link FALHOU')
        sys.exit(1)
    log('SwiftUI.framework: ok (%d KB, %d objetos)' % (os.path.getsize(out) // 1024, len(objs)))
    # Mac Catalyst: o app liga /System/iOSSupport/System/Library/Frameworks/SwiftUI.framework (o WhatsApp). O FAT da
    # imagem não tem links simbólicos, então vai uma cópia; o loader procura a imagem já carregada por todos os nomes.
    cat = dy + '/System/iOSSupport' + install
    os.makedirs(os.path.dirname(cat), exist_ok=True)
    shutil.copyfile(out, cat)


def main():
    global TYPECHECK, EXTRAS
    ap = argparse.ArgumentParser()
    ap.add_argument('--from', dest='frm', default='Src')
    ap.add_argument('--only', default='')
    ap.add_argument('--typecheck', action='store_true')
    a = ap.parse_args()
    TYPECHECK = a.typecheck
    stages = a.only.split(',') if a.only else STAGES[STAGES.index(a.frm):]
    if not ZIG or not FE or not os.path.exists(FE):
        sys.exit('build-swiftui-objc: falta zig ou swift-frontend 6.4.2')
    if not os.path.isdir(os.path.join(RES, 'shims')):
        tc = os.path.realpath(os.path.join(os.path.dirname(FE), '..', 'lib', 'swift'))
        os.makedirs(os.path.join(RES, 'shims'), exist_ok=True)
        if os.path.isdir(os.path.join(tc, 'clang')) and not os.path.exists(os.path.join(RES, 'clang')):
            os.symlink(os.path.join(tc, 'clang'), os.path.join(RES, 'clang'))
        for f in os.listdir(os.path.join(tc, 'shims')):
            if f != 'module.modulemap':
                shutil.copy(os.path.join(tc, 'shims', f), os.path.join(RES, 'shims'))
    if 'Src' in stages:
        stage_src()
    if 'Native' in stages:
        stage_native()
    if any(s in stages for s in ('Deps', 'Core', 'UI')):
        for m in ['Foundation', 'Combine', 'Observation', 'Dispatch', 'CoreGraphics']:
            if not os.path.exists(os.path.join(OV, m + '.swiftmodule')):
                sys.exit('falta o overlay %s em %s' % (m, OV))
        os.makedirs(SUI, exist_ok=True)
        prepare_sdk()
    if 'Deps' in stages:
        stage_deps()
    EXTRAS = True
    if 'Core' in stages:
        stage_core()
    if 'UI' in stages:
        stage_ui()
    if 'Link' in stages and not TYPECHECK:
        if fails:
            sys.exit('SwiftUI.framework: %d falhas antes do link' % fails)
        stage_link()
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
