// mOS — stdin/stdout/stderr do overlay Darwin da Apple (variáveis Swift sobre __stdinp/__stdoutp/__stderrp do libSystem):
// o bare-modifier-monitor do Codex desktop escreve em `stdout` (nome mangled $s6Darwin6stdoutSpySo7__sFILEVGvg).
public var stdin: UnsafeMutablePointer<FILE> {
    get { return __stdinp }
    set { __stdinp = newValue }
}
public var stdout: UnsafeMutablePointer<FILE> {
    get { return __stdoutp }
    set { __stdoutp = newValue }
}
public var stderr: UnsafeMutablePointer<FILE> {
    get { return __stderrp }
    set { __stderrp = newValue }
}
