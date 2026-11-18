// mOS — Translation.framework (Swift, macOS 15+): tradução de texto no aparelho.
// Reimplementação a partir da documentação pública. No macOS a tradução usa modelos de idioma baixados pelo sistema;
// o mOS não tem esses modelos nem serviço de tradução, então nenhum par de idiomas está disponível:
// LanguageAvailability diz .unsupported e a sessão falha com TranslationError em vez de travar o app.

import Foundation

/// Erro de tradução. Os apps comparam com `case TranslationError.x` (operador ~=).
public struct TranslationError: Error, LocalizedError, Hashable, Sendable, CustomStringConvertible {
    let code: Int

    public static var internalError: TranslationError { TranslationError(code: 0) }
    public static var nothingToTranslate: TranslationError { TranslationError(code: 1) }
    public static var unsupportedSourceLanguage: TranslationError { TranslationError(code: 2) }
    public static var unsupportedTargetLanguage: TranslationError { TranslationError(code: 3) }
    public static var unsupportedLanguagePairing: TranslationError { TranslationError(code: 4) }
    public static var unableToIdentifyLanguage: TranslationError { TranslationError(code: 5) }

    public static func ~= (match: TranslationError, error: any Error) -> Bool {
        (error as? TranslationError)?.code == match.code
    }

    public var description: String {
        switch code {
        case 1: return "Translation: nada a traduzir"
        case 2: return "Translation: idioma de origem sem suporte"
        case 3: return "Translation: idioma de destino sem suporte"
        case 4: return "Translation: par de idiomas sem suporte (o mOS não tem modelos de tradução)"
        case 5: return "Translation: não foi possível identificar o idioma"
        default: return "Translation: erro interno"
        }
    }
    public var errorDescription: String? { description }
    public var failureReason: String? { description }
}

/// Quais idiomas o aparelho traduz.
public class LanguageAvailability: @unchecked Sendable {
    public enum Status: Sendable, Equatable, Hashable {
        case installed
        case supported
        case unsupported
    }

    public init() {}

    /// No mOS não há modelos de tradução: lista vazia.
    public var supportedLanguages: [Locale.Language] {
        get async { [] }
    }

    public func status(from source: Locale.Language, to target: Locale.Language?) async -> Status { .unsupported }

    public func status(for text: String, to target: Locale.Language?) async throws -> Status {
        if text.isEmpty { throw TranslationError.nothingToTranslate }
        return .unsupported
    }
}

/// Uma sessão de tradução entre dois idiomas.
public class TranslationSession: @unchecked Sendable {
    public let sourceLanguage: Locale.Language?
    public let targetLanguage: Locale.Language?

    public init(installedSource: Locale.Language, target: Locale.Language? = nil) {
        sourceLanguage = installedSource
        targetLanguage = target
    }

    /// O par de idiomas que a view `.translationTask` usa para criar a sessão.
    public struct Configuration: Equatable, Hashable, Sendable {
        public var source: Locale.Language?
        public var target: Locale.Language?
        public var version: Int = 0

        public init(source: Locale.Language? = nil, target: Locale.Language? = nil) {
            self.source = source
            self.target = target
        }

        /// pede uma nova tradução com a mesma configuração
        public mutating func invalidate() { version &+= 1 }
    }

    public struct Request: Sendable, Hashable {
        public var sourceText: String
        public var clientIdentifier: String?
        public init(sourceText: String, clientIdentifier: String? = nil) {
            self.sourceText = sourceText
            self.clientIdentifier = clientIdentifier
        }
    }

    public struct Response: Sendable, Hashable {
        public let sourceLanguage: Locale.Language
        public let targetLanguage: Locale.Language
        public let sourceText: String
        public let targetText: String
        public let clientIdentifier: String?
    }

    var canRequestDownloads: Bool { false }

    /// baixaria os modelos do par de idiomas; no mOS não há modelos
    public func prepareTranslation() async throws {
        throw TranslationError.unsupportedLanguagePairing
    }

    public func translate(_ string: String) async throws -> Response {
        if string.isEmpty { throw TranslationError.nothingToTranslate }
        throw TranslationError.unsupportedLanguagePairing
    }

    public func translations(from batch: [Request]) async throws -> [Response] {
        if batch.isEmpty || batch.allSatisfy({ $0.sourceText.isEmpty }) { throw TranslationError.nothingToTranslate }
        throw TranslationError.unsupportedLanguagePairing
    }
}
