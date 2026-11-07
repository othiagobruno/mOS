// mOS — overlay Swift do UIKit (Catalyst), segunda leva: os inits Swift de UIAction/UIMenu/UICommand/UIKeyCommand,
// UIBarButtonItem e UIButton, UIControl.enumerateEventHandlers, transformadores de configuração, os protocolos de
// configuração de conteúdo e alguns acessórios. Nomes e assinaturas da Apple.
import UIKit
import Foundation
import CoreGraphics

public typealias UIActionHandler = (UIAction) -> Void

/// String -> NSString (fora do módulo Foundation o importador do mOS não faz a ponte nos tipos importados)
@inline(__always) internal func _uiNS(_ s: String) -> NSString { s._bridgeToObjectiveC() }

extension UIAction {
    public convenience init(title: String = "", image: UIImage? = nil, identifier: UIActionIdentifier? = nil,
                            discoverabilityTitle: String? = nil, attributes: UIMenuElementAttributes = [],
                            state: UIMenuElementState = .off, handler: @escaping UIActionHandler) {
        self.init()
        self.title = _uiNS(title)
        self.image = image
        if let identifier { self.identifier = identifier }
        self.discoverabilityTitle = discoverabilityTitle.map { _uiNS($0) }
        self.attributes = attributes
        self.state = state
        self._mosSetHandler(handler)
    }

    public convenience init(title: String = "", subtitle: String? = nil, image: UIImage? = nil,
                            identifier: UIActionIdentifier? = nil, discoverabilityTitle: String? = nil,
                            attributes: UIMenuElementAttributes = [], state: UIMenuElementState = .off,
                            handler: @escaping UIActionHandler) {
        self.init(title: title, image: image, identifier: identifier, discoverabilityTitle: discoverabilityTitle,
                  attributes: attributes, state: state, handler: handler)
        self.subtitle = subtitle.map { _uiNS($0) }
    }
}

extension UIMenu {
    public convenience init(title: String = "", image: UIImage? = nil, identifier: UIMenuIdentifier? = nil,
                            options: UIMenuOptions = [], children: [UIMenuElement] = []) {
        self.init()
        self.title = _uiNS(title)
        self.image = image
        if let identifier { self.identifier = identifier }
        self.options = options
        self.children = children._bridgeToObjectiveC()
    }
}

extension UICommand {
    public convenience init(title: String = "", image: UIImage? = nil, action: Selector, propertyList: Any? = nil,
                            alternates: [UICommandAlternate] = [], discoverabilityTitle: String? = nil,
                            attributes: UIMenuElementAttributes = [], state: UIMenuElementState = .off) {
        self.init()
        self.title = _uiNS(title)
        self.image = image
        self.action = action
        self.propertyList = propertyList.map { $0 as AnyObject }
        self.alternates = alternates._bridgeToObjectiveC()
        self.discoverabilityTitle = discoverabilityTitle.map { _uiNS($0) }
        self.attributes = attributes
        self.state = state
    }
}

extension UIKeyCommand {
    public convenience init(title: String = "", image: UIImage? = nil, action: Selector, input: String,
                            modifierFlags: UIKeyModifierFlags = [], propertyList: Any? = nil,
                            alternates: [UICommandAlternate] = [], discoverabilityTitle: String? = nil,
                            attributes: UIMenuElementAttributes = [], state: UIMenuElementState = .off) {
        self.init(title: title, image: image, action: action, propertyList: propertyList, alternates: alternates,
                  discoverabilityTitle: discoverabilityTitle, attributes: attributes, state: state)
        self.input = _uiNS(input)
        self.modifierFlags = modifierFlags
    }
}

extension UIBarButtonItem {
    public convenience init(systemItem: UIBarButtonSystemItem, primaryAction: UIAction? = nil, menu: UIMenu? = nil) {
        self.init()
        self._mosSystemItem = systemItem.rawValue
        self.primaryAction = primaryAction
        self.menu = menu
    }

    public convenience init(title: String? = nil, image: UIImage? = nil, primaryAction: UIAction? = nil, menu: UIMenu? = nil) {
        self.init()
        self.title = title.map { _uiNS($0) }
        self.image = image
        self.primaryAction = primaryAction
        self.menu = menu
    }
}

extension UIButton {
    public convenience init(type buttonType: UIButtonType = .system, primaryAction: UIAction?) {
        self.init(type: buttonType)
        if let primaryAction { add(primaryAction, for: .primaryActionTriggered) }
    }
}

extension UIControl {
    public func enumerateEventHandlers(_ iterator: (_ actionHandler: UIAction?, _ targetAction: (Any?, Selector)?,
                                                    _ controlEvents: UIControlEvents, _ stop: inout Bool) -> Void) {
        var stop = false
        for i in 0..<_mosHandlerCount() {
            let a = _mosHandlerAction(i)
            let ta: (Any?, Selector)? = _mosHandlerSelector(i).map { (_mosHandlerTarget(i), $0) }
            iterator(a, ta, _mosHandlerEvents(i), &stop)
            if stop { break }
        }
    }
}

extension UISheetPresentationController {
    public var preferredCornerRadius: CGFloat? {
        get { _mosPreferredCornerRadius.isNaN ? nil : _mosPreferredCornerRadius }
        set { _mosPreferredCornerRadius = newValue ?? .nan }
    }
}

extension UIFocusEnvironment {
    /// o ambiente é este ou está contido nele (views: pela hierarquia)
    public func contains(_ environment: UIFocusEnvironment) -> Bool {
        if (environment as AnyObject) === (self as AnyObject) { return true }
        if let v = environment as? UIView, let me = self as? UIView { return v.isDescendant(of: me) }
        return false
    }
}

// MARK: - transformadores de configuração

public struct UIConfigurationColorTransformer {
    public let transform: (UIColor) -> UIColor
    public init(_ transform: @escaping (UIColor) -> UIColor) { self.transform = transform }
    public func callAsFunction(_ input: UIColor) -> UIColor { transform(input) }
    public static let grayscale = UIConfigurationColorTransformer { $0 }
    public static let preferredTint = UIConfigurationColorTransformer { $0 }
    public static let monochromeTint = UIConfigurationColorTransformer { $0 }
}

public struct UIConfigurationTextAttributesTransformer {
    public let transform: (AttributeContainer) -> AttributeContainer
    public init(_ transform: @escaping (AttributeContainer) -> AttributeContainer) { self.transform = transform }
    public func callAsFunction(_ input: AttributeContainer) -> AttributeContainer { transform(input) }
}

// MARK: - configuração de conteúdo (células)

public protocol UIConfigurationState {
    var traitCollection: UITraitCollection { get set }
}

public protocol UIContentConfiguration {
    func makeContentView() -> UIView & UIContentView
    func updated(for state: UIConfigurationState) -> Self
}

public protocol UIContentView: NSObjectProtocol {
    var configuration: UIContentConfiguration { get set }
}
