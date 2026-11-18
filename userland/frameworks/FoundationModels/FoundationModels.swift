// mOS — FoundationModels.framework (Swift, macOS 26): o modelo de linguagem do sistema.
// Reimplementação a partir da documentação pública. No macOS o modelo vem do Apple Intelligence; o mOS não tem modelo
// de linguagem no aparelho, então SystemLanguageModel.default diz que está indisponível (.deviceNotEligible) e as
// sessões falham com LanguageModelSession.GenerationError.assetsUnavailable em vez de travar o app.

// MARK: - Prompt

/// Um valor que vira texto de prompt.
public protocol PromptRepresentable {
    var promptRepresentation: Prompt { get }
}

/// O texto enviado ao modelo.
public struct Prompt: Sendable, PromptRepresentable, CustomStringConvertible {
    var parts: [String]

    public init<P: PromptRepresentable>(_ content: P) {
        parts = content.promptRepresentation.parts
    }

    public init<each P: PromptRepresentable>(_ components: repeat each P) {
        var out: [String] = []
        for c in repeat each components { out += c.promptRepresentation.parts }
        parts = out
    }

    init(parts: [String]) { self.parts = parts }

    public var promptRepresentation: Prompt { self }
    public var description: String { parts.joined(separator: "\n") }
}

extension String: PromptRepresentable {
    public var promptRepresentation: Prompt { Prompt(parts: [self]) }
}

extension Array: PromptRepresentable where Element: PromptRepresentable {
    public var promptRepresentation: Prompt { Prompt(parts: flatMap { $0.promptRepresentation.parts }) }
}

/// Monta um Prompt a partir das linhas de um bloco (`respond { "..." }`).
@resultBuilder
public struct PromptBuilder {
    public static func buildBlock<each P: PromptRepresentable>(_ components: repeat each P) -> Prompt {
        Prompt(repeat each components)
    }
    public static func buildExpression<P: PromptRepresentable>(_ e: P) -> P { e }
    public static func buildOptional(_ p: Prompt?) -> Prompt { p ?? Prompt(parts: []) }
    public static func buildEither(first p: Prompt) -> Prompt { p }
    public static func buildEither(second p: Prompt) -> Prompt { p }
    public static func buildArray(_ ps: [Prompt]) -> Prompt { Prompt(parts: ps.flatMap { $0.parts }) }
}

// MARK: - Conteúdo gerado

/// Um tipo que o modelo sabe gerar; PartiallyGenerated é a forma parcial entregue durante o streaming.
public protocol Generable {
    associatedtype PartiallyGenerated = Self
}

extension String: Generable {
    public typealias PartiallyGenerated = String
}

/// Uma ferramenta que o modelo pode chamar durante a resposta.
public protocol Tool: Sendable {
    var name: String { get }
    var description: String { get }
}

// MARK: - Opções

public struct GenerationOptions: Sendable, Equatable {
    /// Como o próximo token é escolhido.
    public struct SamplingMode: Sendable, Equatable {
        enum Kind: Sendable, Equatable {
            case greedy
            case top(Int, UInt64?)
            case threshold(Double, UInt64?)
        }
        let kind: Kind

        public static var greedy: SamplingMode { SamplingMode(kind: .greedy) }
        public static func random(top k: Int, seed: UInt64? = nil) -> SamplingMode {
            SamplingMode(kind: .top(k, seed))
        }
        public static func random(probabilityThreshold: Double, seed: UInt64? = nil) -> SamplingMode {
            SamplingMode(kind: .threshold(probabilityThreshold, seed))
        }
    }

    public var sampling: SamplingMode?
    public var temperature: Double?
    public var maximumResponseTokens: Int?

    public init(sampling: SamplingMode? = nil, temperature: Double? = nil, maximumResponseTokens: Int? = nil) {
        self.sampling = sampling
        self.temperature = temperature
        self.maximumResponseTokens = maximumResponseTokens
    }
}

// MARK: - Modelo

public final class SystemLanguageModel: @unchecked Sendable {
    public enum Availability: Sendable, Equatable {
        public enum UnavailableReason: Sendable, Equatable {
            case deviceNotEligible
            case appleIntelligenceNotEnabled
            case modelNotReady
        }
        case available
        case unavailable(UnavailableReason)
    }

    public enum UseCase: Sendable, Equatable { case general, contentTagging }

    public let useCase: UseCase

    public init(useCase: UseCase = .general) { self.useCase = useCase }

    public static let `default` = SystemLanguageModel()

    /// O mOS não tem modelo de linguagem no aparelho.
    public var availability: Availability { .unavailable(.deviceNotEligible) }
    public var isAvailable: Bool { false }
    public var supportedLanguages: Set<String> { [] }
}

// MARK: - Sessão

public final class LanguageModelSession: @unchecked Sendable {
    public let model: SystemLanguageModel
    public let tools: [any Tool]
    public let instructions: String?

    public init(model: SystemLanguageModel = .default, tools: [any Tool] = [], instructions: String? = nil) {
        self.model = model
        self.tools = tools
        self.instructions = instructions
    }

    /// Nunca há resposta em curso: toda geração falha antes de começar.
    public var isResponding: Bool { false }

    public enum GenerationError: Error, Sendable, CustomStringConvertible {
        public struct Context: Sendable {
            public let debugDescription: String
            public init(debugDescription: String) { self.debugDescription = debugDescription }
        }
        case exceededContextWindowSize(Context)
        case assetsUnavailable(Context)
        case guardrailViolation(Context)
        case unsupportedGuide(Context)
        case unsupportedLanguageOrLocale(Context)
        case decodingFailure(Context)
        case rateLimited(Context)
        case concurrentRequests(Context)
        case refusal(Context)

        public var description: String {
            switch self {
            case .exceededContextWindowSize(let c), .assetsUnavailable(let c), .guardrailViolation(let c),
                 .unsupportedGuide(let c), .unsupportedLanguageOrLocale(let c), .decodingFailure(let c),
                 .rateLimited(let c), .concurrentRequests(let c), .refusal(let c):
                return c.debugDescription
            }
        }
    }

    static var unavailable: GenerationError {
        .assetsUnavailable(.init(debugDescription:
            "FoundationModels: o mOS não tem modelo de linguagem no aparelho (SystemLanguageModel indisponível)"))
    }

    public struct Response<Content> {
        public let content: Content
        public let rawContent: String
    }

    public struct ResponseStream<Content: Generable>: AsyncSequence {
        public typealias Element = Snapshot

        public struct Snapshot {
            public var content: Content.PartiallyGenerated
            public var rawContent: String
        }

        public struct AsyncIterator: AsyncIteratorProtocol {
            public mutating func next(isolation actor: isolated (any Actor)?) async throws -> Snapshot? {
                throw LanguageModelSession.unavailable
            }
            public mutating func next() async throws -> Snapshot? {
                throw LanguageModelSession.unavailable
            }
        }

        public func makeAsyncIterator() -> AsyncIterator { AsyncIterator() }

        public func collect() async throws -> Response<Content> {
            throw LanguageModelSession.unavailable
        }
    }

    public func respond(options: GenerationOptions = GenerationOptions(),
                        @PromptBuilder prompt: () throws -> Prompt) async throws -> Response<String> {
        _ = try prompt()
        throw LanguageModelSession.unavailable
    }

    public func respond(to prompt: String, options: GenerationOptions = GenerationOptions()) async throws
        -> Response<String> {
        throw LanguageModelSession.unavailable
    }

    public func respond(to prompt: Prompt, options: GenerationOptions = GenerationOptions()) async throws
        -> Response<String> {
        throw LanguageModelSession.unavailable
    }

    public func streamResponse(options: GenerationOptions = GenerationOptions(),
                               @PromptBuilder prompt: () throws -> Prompt) throws -> ResponseStream<String> {
        _ = try prompt()
        return ResponseStream()
    }

    public func streamResponse(to prompt: String, options: GenerationOptions = GenerationOptions())
        -> ResponseStream<String> {
        ResponseStream()
    }

    /// carregaria o modelo antes do primeiro pedido; sem modelo, não faz nada
    public func prewarm() {}
}
