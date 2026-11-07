/*
 * mOS — UIKit.framework do Mac Catalyst: as classes que os apps referenciam (OBJC_CLASS_$_...), com a hierarquia do SDK
 * da Apple, para subclasses e casts dos apps funcionarem. Ainda sem comportamento próprio além do herdado (UIView,
 * UIViewController, UIControl... em UIKitCore.m); vão ganhando implementação aos poucos
 * (NSShadow: no Catalyst é do UIKit; o libSystem só tem a chave NSShadowAttributeName).
 */
#import "UIKitMOS.h"

#define K(N, S) @interface N : S @end @implementation N @end

/* ---- views */
K(UIImageView, UIView) 
K(UITextView, UIScrollView) K(UISearchTextField, UITextField)
K(UISearchBar, UIView) K(UIToolbar, UIView) K(UITabBar, UIView) K(UIStackView, UIView)
K(UIVisualEffectView, UIView) K(UIActivityIndicatorView, UIView) K(UIProgressView, UIView) K(UIPickerView, UIView)
K(UIDatePicker, UIControl) K(UISlider, UIControl) K(UIStepper, UIControl)
K(UISegmentedControl, UIControl) K(UIPageControl, UIControl) K(UIRefreshControl, UIControl) K(UIInputView, UIView)
K(UITableViewHeaderFooterView, UIView)
/* UIScrollView, UITableView(Cell), UICollectionView(+células, layouts), UINavigation*, UILayoutGuide: UIKitViews.m */
K(UIListContentView, UIView)

/* ---- view controllers */
K(UITabBarController, UIViewController)
K(UISplitViewController, UIViewController) K(UITableViewController, UIViewController)
K(UICollectionViewController, UIViewController) K(UIAlertController, UIViewController)
K(UISearchController, UIViewController) K(UIPageViewController, UIViewController)
K(UIActivityViewController, UIViewController) K(UIDocumentPickerViewController, UIViewController)
K(UIImagePickerController, UINavigationController)

/* ---- gestos */
/* UIGestureRecognizer, UITapGestureRecognizer, UIPanGestureRecognizer, UILongPressGestureRecognizer, UITouch, UIEvent: UIKitEvents.m */
K(UISwipeGestureRecognizer, UIGestureRecognizer)
K(UIPinchGestureRecognizer, UIGestureRecognizer) K(UIRotationGestureRecognizer, UIGestureRecognizer)
K(UIHoverGestureRecognizer, UIGestureRecognizer) K(UIScreenEdgePanGestureRecognizer, UIPanGestureRecognizer)

/* ---- barras, menus, ações */
K(UITabBarItem, UIBarItem) K(UIBarButtonItemGroup, NSObject)
K(UIBarAppearance, NSObject) K(UINavigationBarAppearance, UIBarAppearance)
K(UIToolbarAppearance, UIBarAppearance) K(UITabBarAppearance, UIBarAppearance)
/* UIMenuElement, UIMenu, UIAction, UICommand, UIKeyCommand, UIBarItem, UIBarButtonItem, UIButton: UIKitMenus.m */
K(UIDeferredMenuElement, UIMenuElement) K(UIMenuSystem, NSObject)
K(UIMenuController, NSObject) K(UIMenuItem, NSObject) K(UIContextMenuConfiguration, NSObject)
K(UIContextMenuInteraction, NSObject) K(UIAlertAction, NSObject) K(UIContextualAction, NSObject)
K(UISwipeActionsConfiguration, NSObject) K(UIActivity, NSObject) K(UIApplicationShortcutItem, NSObject)
K(UIApplicationShortcutIcon, NSObject)

/* ---- efeitos, animação, feedback, dinâmica */
K(UIVisualEffect, NSObject) K(UIBlurEffect, UIVisualEffect) K(UIVibrancyEffect, UIVisualEffect)
K(UIMotionEffect, NSObject) K(UIInterpolatingMotionEffect, UIMotionEffect) K(UIMotionEffectGroup, UIMotionEffect)
K(UIFeedbackGenerator, NSObject) K(UIImpactFeedbackGenerator, UIFeedbackGenerator)
K(UISelectionFeedbackGenerator, UIFeedbackGenerator) K(UINotificationFeedbackGenerator, UIFeedbackGenerator)
K(UIViewPropertyAnimator, NSObject) K(UICubicTimingParameters, NSObject) K(UISpringTimingParameters, NSObject)
K(UIPercentDrivenInteractiveTransition, NSObject) K(UIDynamicAnimator, NSObject) K(UIDynamicBehavior, NSObject)
K(UIDynamicItemBehavior, UIDynamicBehavior) K(UIFieldBehavior, UIDynamicBehavior) K(UIGravityBehavior, UIDynamicBehavior)
K(UICollisionBehavior, UIDynamicBehavior) K(UIFocusSystem, NSObject) K(UIFocusEffect, NSObject)
K(UIFocusHaloEffect, UIFocusEffect)

/* ---- interações, arrastar e soltar, prévias, ponteiro */
K(UIPreviewParameters, NSObject) K(UITargetedPreview, NSObject) K(UIPreviewTarget, NSObject)
K(UIPreviewInteraction, NSObject) K(UIDragItem, NSObject) K(UIDragInteraction, NSObject) K(UIDropInteraction, NSObject)
K(UIDropProposal, NSObject) K(UITableViewDropProposal, UIDropProposal) K(UIPointerInteraction, NSObject)
K(UIPointerStyle, NSObject) K(UIToolTipInteraction, NSObject) K(UILargeContentViewerInteraction, NSObject)
K(UITextInteraction, NSObject) K(UIPress, NSObject)

/* ---- coleções e tabelas */

K(UICollectionViewCompositionalLayoutConfiguration, NSObject)
K(UICollectionViewLayoutAttributes, NSObject) K(UICollectionViewLayoutInvalidationContext, NSObject)
K(UICollectionViewFlowLayoutInvalidationContext, UICollectionViewLayoutInvalidationContext)
K(UICollectionViewUpdateItem, NSObject) K(UICollectionViewDiffableDataSource, NSObject)
K(NSDiffableDataSourceSnapshot, NSObject) K(NSCollectionLayoutDimension, NSObject) K(NSCollectionLayoutSize, NSObject)
K(NSCollectionLayoutItem, NSObject) K(NSCollectionLayoutGroup, NSCollectionLayoutItem) K(NSCollectionLayoutSection, NSObject)
K(NSCollectionLayoutEdgeSpacing, NSObject) K(NSCollectionLayoutSpacing, NSObject)
K(NSCollectionLayoutSupplementaryItem, NSCollectionLayoutItem)
K(NSCollectionLayoutBoundarySupplementaryItem, NSCollectionLayoutSupplementaryItem)
K(NSCollectionLayoutDecorationItem, NSCollectionLayoutItem)
K(UIListContentConfiguration, NSObject) K(UIBackgroundConfiguration, NSObject) K(UIButtonConfiguration, NSObject)
K(UICellAccessoryCustomView, NSObject) K(UILocalizedIndexedCollation, NSObject)

/* ---- texto */
K(NSTextStorage, NSMutableAttributedString) K(NSLayoutManager, NSObject) K(NSTextContainer, NSObject)
K(NSTextContentManager, NSObject) K(NSTextContentStorage, NSTextContentManager) K(NSTextTab, NSObject)
K(NSStringDrawingContext, NSObject) K(UIFontDescriptor, NSObject) K(UIFontMetrics, NSObject) K(UITextPosition, NSObject)
K(UITextRange, NSObject) K(UITextSelectionRect, NSObject) K(UITextInputMode, NSObject)
K(UITextInputStringTokenizer, NSObject) K(UISearchToken, NSObject)

/* ---- imagens, gráficos, impressão, recursos */
K(UIImageConfiguration, NSObject) K(UIImageSymbolConfiguration, UIImageConfiguration) K(UIImageAsset, NSObject)
K(UIGraphicsRendererFormat, NSObject) K(UIGraphicsImageRendererFormat, UIGraphicsRendererFormat)
K(UIGraphicsRenderer, NSObject) K(UIGraphicsImageRenderer, UIGraphicsRenderer) K(UIGraphicsPDFRenderer, UIGraphicsRenderer)
K(UIBezierPath, NSObject) K(UIPrintFormatter, NSObject) K(UISimpleTextPrintFormatter, UIPrintFormatter)
K(UIPrintInteractionController, NSObject) K(UIPrintInfo, NSObject) K(UIPrintPageRenderer, NSObject)
K(NSDataAsset, NSObject) K(UINib, NSObject) K(UIStoryboard, NSObject)

/* ---- apresentação, cenas, sistema */
K(UIPresentationController, NSObject) K(UISheetPresentationController, UIPresentationController)
K(UISheetPresentationControllerDetent, NSObject) K(UISceneOpenURLOptions, NSObject) K(UIOpenURLContext, NSObject)
K(UIWindowSceneActivationRequestOptions, NSObject) K(UIPasteboard, NSObject) K(UIDocumentInteractionController, NSObject)
K(UIAccessibilityElement, NSObject) K(UIAccessibilityCustomAction, NSObject) K(UIAccessibilityCustomRotor, NSObject)
K(UIAccessibilityCustomRotorItemResult, NSObject) K(NSShadow, NSObject) K(UICellConfigurationState, NSObject)
