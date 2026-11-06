// mOS SDK — overlay Swift do módulo clang MOSDarwin (a libc do macOS; veja userland\sdk\openswiftui\darwin).
// Faz o papel do overlay Darwin do Swift no SDK da Apple: o que o importador não traz sozinho (errno é macro, funções
// variádicas como open/fcntl/ioctl não são importadas, constantes de modo como mode_t, DarwinBoolean).
// Compilado com -import-underlying-module.

@_exported import MOSDarwin

// MARK: - errno

public var errno: Int32 {
    get { __error().pointee }
    set { __error().pointee = newValue }
}

// MARK: - variádicas (via as funções inline de mos_darwin.h)

public func open(_ path: UnsafePointer<CChar>, _ oflag: Int32) -> Int32 { _mos_open(path, oflag, 0) }
public func open(_ path: UnsafePointer<CChar>, _ oflag: Int32, _ mode: mode_t) -> Int32 { _mos_open(path, oflag, mode) }
public func openat(_ fd: Int32, _ path: UnsafePointer<CChar>, _ oflag: Int32) -> Int32 { _mos_openat(fd, path, oflag, 0) }
public func openat(_ fd: Int32, _ path: UnsafePointer<CChar>, _ oflag: Int32, _ mode: mode_t) -> Int32 { _mos_openat(fd, path, oflag, mode) }
public func fcntl(_ fd: Int32, _ cmd: Int32) -> Int32 { _mos_fcntl(fd, cmd, 0) }
public func fcntl(_ fd: Int32, _ cmd: Int32, _ value: Int32) -> Int32 { _mos_fcntl(fd, cmd, Int(value)) }
public func fcntl(_ fd: Int32, _ cmd: Int32, _ ptr: UnsafeMutableRawPointer) -> Int32 { _mos_fcntl_ptr(fd, cmd, ptr) }
public func ioctl(_ fd: Int32, _ request: UInt, _ value: Int32) -> Int32 { _mos_ioctl(fd, request, Int(value)) }
public func ioctl(_ fd: Int32, _ request: UInt, _ ptr: UnsafeMutableRawPointer) -> Int32 { _mos_ioctl_ptr(fd, request, ptr) }
public func ioctl(_ fd: Int32, _ request: UInt) -> Int32 { _mos_ioctl(fd, request, 0) }

// MARK: - modos de arquivo como mode_t (no C são int)

public var S_IFMT: mode_t { mode_t(0o170000) }
public var S_IFIFO: mode_t { mode_t(0o010000) }
public var S_IFCHR: mode_t { mode_t(0o020000) }
public var S_IFDIR: mode_t { mode_t(0o040000) }
public var S_IFBLK: mode_t { mode_t(0o060000) }
public var S_IFREG: mode_t { mode_t(0o100000) }
public var S_IFLNK: mode_t { mode_t(0o120000) }
public var S_IFSOCK: mode_t { mode_t(0o140000) }
public var S_IFWHT: mode_t { mode_t(0o160000) }
public var S_IRWXU: mode_t { mode_t(0o000700) }
public var S_IRUSR: mode_t { mode_t(0o000400) }
public var S_IWUSR: mode_t { mode_t(0o000200) }
public var S_IXUSR: mode_t { mode_t(0o000100) }
public var S_IRWXG: mode_t { mode_t(0o000070) }
public var S_IRGRP: mode_t { mode_t(0o000040) }
public var S_IWGRP: mode_t { mode_t(0o000020) }
public var S_IXGRP: mode_t { mode_t(0o000010) }
public var S_IRWXO: mode_t { mode_t(0o000007) }
public var S_IROTH: mode_t { mode_t(0o000004) }
public var S_IWOTH: mode_t { mode_t(0o000002) }
public var S_IXOTH: mode_t { mode_t(0o000001) }
public var S_ISUID: mode_t { mode_t(0o004000) }
public var S_ISGID: mode_t { mode_t(0o002000) }
public var S_ISVTX: mode_t { mode_t(0o001000) }
public var S_ISTXT: mode_t { S_ISVTX }
public var S_IREAD: mode_t { S_IRUSR }
public var S_IWRITE: mode_t { S_IWUSR }
public var S_IEXEC: mode_t { S_IXUSR }

// MARK: - DarwinBoolean (o Boolean do C)

@frozen
public struct DarwinBoolean: ExpressibleByBooleanLiteral, Sendable {
    @usableFromInline var _value: UInt8
    @_transparent public init(_ value: Bool) { _value = value ? 1 : 0 }
    @_transparent public init(booleanLiteral value: Bool) { self.init(value) }
    @_transparent public var boolValue: Bool { _value != 0 }
}

extension DarwinBoolean: CustomReflectable, CustomStringConvertible, Equatable {
    public var customMirror: Mirror { Mirror(reflecting: boolValue) }
    public var description: String { boolValue.description }
    public static func == (l: DarwinBoolean, r: DarwinBoolean) -> Bool { l.boolValue == r.boolValue }
}

@_transparent public func _convertBoolToDarwinBoolean(_ x: Bool) -> DarwinBoolean { DarwinBoolean(x) }
@_transparent public func _convertDarwinBooleanToBool(_ x: DarwinBoolean) -> Bool { x.boolValue }

// MARK: - constantes que o importador não traz (macros com conversões)

public var MAP_FAILED: UnsafeMutableRawPointer! { UnsafeMutableRawPointer(bitPattern: -1) }
public var SIG_DFL: sig_t? { nil }
public var SIG_IGN: sig_t { unsafeBitCast(1, to: sig_t.self) }
public var SIG_ERR: sig_t { unsafeBitCast(-1, to: sig_t.self) }

// MARK: - POSIXErrorCode (gerado de sys/errno.h do macOS; valores do Darwin)

@frozen public enum POSIXErrorCode: Int32, Sendable {
    case EPERM = 1
    case ENOENT = 2
    case ESRCH = 3
    case EINTR = 4
    case EIO = 5
    case ENXIO = 6
    case E2BIG = 7
    case ENOEXEC = 8
    case EBADF = 9
    case ECHILD = 10
    case EDEADLK = 11
    case ENOMEM = 12
    case EACCES = 13
    case EFAULT = 14
    case ENOTBLK = 15
    case EBUSY = 16
    case EEXIST = 17
    case EXDEV = 18
    case ENODEV = 19
    case ENOTDIR = 20
    case EISDIR = 21
    case EINVAL = 22
    case ENFILE = 23
    case EMFILE = 24
    case ENOTTY = 25
    case ETXTBSY = 26
    case EFBIG = 27
    case ENOSPC = 28
    case ESPIPE = 29
    case EROFS = 30
    case EMLINK = 31
    case EPIPE = 32
    case EDOM = 33
    case ERANGE = 34
    case EAGAIN = 35
    case EINPROGRESS = 36
    case EALREADY = 37
    case ENOTSOCK = 38
    case EDESTADDRREQ = 39
    case EMSGSIZE = 40
    case EPROTOTYPE = 41
    case ENOPROTOOPT = 42
    case EPROTONOSUPPORT = 43
    case ESOCKTNOSUPPORT = 44
    case ENOTSUP = 45
    case EPFNOSUPPORT = 46
    case EAFNOSUPPORT = 47
    case EADDRINUSE = 48
    case EADDRNOTAVAIL = 49
    case ENETDOWN = 50
    case ENETUNREACH = 51
    case ENETRESET = 52
    case ECONNABORTED = 53
    case ECONNRESET = 54
    case ENOBUFS = 55
    case EISCONN = 56
    case ENOTCONN = 57
    case ESHUTDOWN = 58
    case ETOOMANYREFS = 59
    case ETIMEDOUT = 60
    case ECONNREFUSED = 61
    case ELOOP = 62
    case ENAMETOOLONG = 63
    case EHOSTDOWN = 64
    case EHOSTUNREACH = 65
    case ENOTEMPTY = 66
    case EPROCLIM = 67
    case EUSERS = 68
    case EDQUOT = 69
    case ESTALE = 70
    case EREMOTE = 71
    case EBADRPC = 72
    case ERPCMISMATCH = 73
    case EPROGUNAVAIL = 74
    case EPROGMISMATCH = 75
    case EPROCUNAVAIL = 76
    case ENOLCK = 77
    case ENOSYS = 78
    case EFTYPE = 79
    case EAUTH = 80
    case ENEEDAUTH = 81
    case EPWROFF = 82
    case EDEVERR = 83
    case EOVERFLOW = 84
    case EBADEXEC = 85
    case EBADARCH = 86
    case ESHLIBVERS = 87
    case EBADMACHO = 88
    case ECANCELED = 89
    case EIDRM = 90
    case ENOMSG = 91
    case EILSEQ = 92
    case ENOATTR = 93
    case EBADMSG = 94
    case EMULTIHOP = 95
    case ENODATA = 96
    case ENOLINK = 97
    case ENOSR = 98
    case ENOSTR = 99
    case EPROTO = 100
    case ETIME = 101
    case EOPNOTSUPP = 102
    case ENOPOLICY = 103
    case ENOTRECOVERABLE = 104
    case EOWNERDEAD = 105
    case EQFULL = 106
    case ENOTCAPABLE = 107
}

extension POSIXErrorCode {
    public static var EWOULDBLOCK: POSIXErrorCode { .EAGAIN }
}

// MARK: - relógios (no C, CLOCK_* são macros para constantes de um enum anônimo: o Swift as vê como Int32)

public var CLOCK_REALTIME: clockid_t { clockid_t(0) }
public var CLOCK_MONOTONIC_RAW: clockid_t { clockid_t(4) }
public var CLOCK_MONOTONIC_RAW_APPROX: clockid_t { clockid_t(5) }
public var CLOCK_MONOTONIC: clockid_t { clockid_t(6) }
public var CLOCK_UPTIME_RAW: clockid_t { clockid_t(8) }
public var CLOCK_UPTIME_RAW_APPROX: clockid_t { clockid_t(9) }
public var CLOCK_PROCESS_CPUTIME_ID: clockid_t { clockid_t(12) }
public var CLOCK_THREAD_CPUTIME_ID: clockid_t { clockid_t(16) }

// MARK: - math com mais de um resultado (como o overlay tgmath da Apple)

@_transparent public func modf(_ x: Double) -> (Double, Double) {
    var i = 0.0
    let f = MOSDarwin.modf(x, &i)
    return (i, f)
}
@_transparent public func modf(_ x: Float) -> (Float, Float) {
    var i: Float = 0
    let f = modff(x, &i)
    return (i, f)
}
@_transparent public func frexp(_ x: Double) -> (Double, Int) {
    var e: Int32 = 0
    let m = MOSDarwin.frexp(x, &e)
    return (m, Int(e))
}
@_transparent public func remquo(_ x: Double, _ y: Double) -> (Double, Int) {
    var q: Int32 = 0
    let r = MOSDarwin.remquo(x, y, &q)
    return (r, Int(q))
}
// MARK: - sobrecargas Float (como o tgmath.swift do overlay Darwin da Apple)

@_transparent public func sqrt(_ x: Float) -> Float { sqrtf(x) }
@_transparent public func cbrt(_ x: Float) -> Float { cbrtf(x) }
@_transparent public func exp(_ x: Float) -> Float { expf(x) }
@_transparent public func exp2(_ x: Float) -> Float { exp2f(x) }
@_transparent public func expm1(_ x: Float) -> Float { expm1f(x) }
@_transparent public func log(_ x: Float) -> Float { logf(x) }
@_transparent public func log2(_ x: Float) -> Float { log2f(x) }
@_transparent public func log10(_ x: Float) -> Float { log10f(x) }
@_transparent public func log1p(_ x: Float) -> Float { log1pf(x) }
@_transparent public func sin(_ x: Float) -> Float { sinf(x) }
@_transparent public func cos(_ x: Float) -> Float { cosf(x) }
@_transparent public func tan(_ x: Float) -> Float { tanf(x) }
@_transparent public func asin(_ x: Float) -> Float { asinf(x) }
@_transparent public func acos(_ x: Float) -> Float { acosf(x) }
@_transparent public func atan(_ x: Float) -> Float { atanf(x) }
@_transparent public func sinh(_ x: Float) -> Float { sinhf(x) }
@_transparent public func cosh(_ x: Float) -> Float { coshf(x) }
@_transparent public func tanh(_ x: Float) -> Float { tanhf(x) }
@_transparent public func asinh(_ x: Float) -> Float { asinhf(x) }
@_transparent public func acosh(_ x: Float) -> Float { acoshf(x) }
@_transparent public func atanh(_ x: Float) -> Float { atanhf(x) }
@_transparent public func erf(_ x: Float) -> Float { erff(x) }
@_transparent public func erfc(_ x: Float) -> Float { erfcf(x) }
@_transparent public func tgamma(_ x: Float) -> Float { tgammaf(x) }
@_transparent public func lgamma(_ x: Float) -> Float { lgammaf(x) }
@_transparent public func trunc(_ x: Float) -> Float { truncf(x) }
@_transparent public func round(_ x: Float) -> Float { roundf(x) }
@_transparent public func floor(_ x: Float) -> Float { floorf(x) }
@_transparent public func ceil(_ x: Float) -> Float { ceilf(x) }
@_transparent public func fabs(_ x: Float) -> Float { fabsf(x) }
@_transparent public func rint(_ x: Float) -> Float { rintf(x) }
@_transparent public func nearbyint(_ x: Float) -> Float { nearbyintf(x) }
@_transparent public func pow(_ x: Float, _ y: Float) -> Float { powf(x, y) }
@_transparent public func atan2(_ x: Float, _ y: Float) -> Float { atan2f(x, y) }
@_transparent public func hypot(_ x: Float, _ y: Float) -> Float { hypotf(x, y) }
@_transparent public func fmod(_ x: Float, _ y: Float) -> Float { fmodf(x, y) }
@_transparent public func remainder(_ x: Float, _ y: Float) -> Float { remainderf(x, y) }
@_transparent public func fdim(_ x: Float, _ y: Float) -> Float { fdimf(x, y) }
@_transparent public func fmax(_ x: Float, _ y: Float) -> Float { fmaxf(x, y) }
@_transparent public func fmin(_ x: Float, _ y: Float) -> Float { fminf(x, y) }
@_transparent public func copysign(_ x: Float, _ y: Float) -> Float { copysignf(x, y) }
@_transparent public func nextafter(_ x: Float, _ y: Float) -> Float { nextafterf(x, y) }

// MARK: - ioctl (no C são macros com _IOR/_IOW, que o importador não avalia)

public var TIOCGWINSZ: UInt { 0x4008_7468 }
public var TIOCSWINSZ: UInt { 0x8008_7467 }
public var FIONREAD: UInt { 0x4004_667F }
public var FIONBIO: UInt { 0x8004_667E }
