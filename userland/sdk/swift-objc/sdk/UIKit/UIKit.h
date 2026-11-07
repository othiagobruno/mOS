/*
 * mOS SDK (variante com interop ObjC) — UIKit do Mac Catalyst, o subconjunto que o overlay Swift
 * (userland/sdk/swift-objc/UIKit, tools/build-swift-uikit-objc.ps1) usa, no formato do SDK da Apple. As classes e
 * as constantes estão no UIKit.framework do mOS (userland/frameworks/UIKit). Os typedefs com wrapper Swift
 * (UIWindowLevel, UIContentSizeCategory, UIAccessibilityTraits) entram nos nomes mangled pelo nome C.
 */
#pragma once
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#define UIKIT_EXTERN extern __attribute__((visibility("default")))
#define NS_TYPED_ENUM __attribute__((swift_wrapper(struct)))

typedef CGFloat UIWindowLevel NS_TYPED_EXTENSIBLE_ENUM;
UIKIT_EXTERN const UIWindowLevel UIWindowLevelNormal NS_SWIFT_NAME(UIWindowLevel.normal);
UIKIT_EXTERN const UIWindowLevel UIWindowLevelAlert NS_SWIFT_NAME(UIWindowLevel.alert);
UIKIT_EXTERN const UIWindowLevel UIWindowLevelStatusBar NS_SWIFT_NAME(UIWindowLevel.statusBar);

typedef NSString *UIContentSizeCategory NS_TYPED_ENUM;
UIKIT_EXTERN const UIContentSizeCategory UIContentSizeCategoryUnspecified NS_SWIFT_NAME(UIContentSizeCategory.unspecified);
UIKIT_EXTERN const UIContentSizeCategory UIContentSizeCategoryExtraSmall NS_SWIFT_NAME(UIContentSizeCategory.extraSmall);
UIKIT_EXTERN const UIContentSizeCategory UIContentSizeCategorySmall NS_SWIFT_NAME(UIContentSizeCategory.small);
UIKIT_EXTERN const UIContentSizeCategory UIContentSizeCategoryMedium NS_SWIFT_NAME(UIContentSizeCategory.medium);
UIKIT_EXTERN const UIContentSizeCategory UIContentSizeCategoryLarge NS_SWIFT_NAME(UIContentSizeCategory.large);
UIKIT_EXTERN const UIContentSizeCategory UIContentSizeCategoryExtraLarge NS_SWIFT_NAME(UIContentSizeCategory.extraLarge);
UIKIT_EXTERN BOOL UIContentSizeCategoryIsAccessibilityCategory(UIContentSizeCategory category) NS_REFINED_FOR_SWIFT;
UIKIT_EXTERN NSComparisonResult UIContentSizeCategoryCompareToCategory(UIContentSizeCategory lhs, UIContentSizeCategory rhs) NS_REFINED_FOR_SWIFT;

typedef uint64_t UIAccessibilityTraits NS_TYPED_ENUM;
UIKIT_EXTERN UIAccessibilityTraits UIAccessibilityTraitNone;
UIKIT_EXTERN UIAccessibilityTraits UIAccessibilityTraitButton;


/* ---- classes base */
@interface UIResponder : NSObject
@end
@interface UIColor : NSObject
@end
@interface UIImage : NSObject
@end
@interface UITraitCollection : NSObject
@end
@protocol UIFocusEnvironment <NSObject>
@end
@interface UIView : UIResponder <UIFocusEnvironment>
- (instancetype)initWithFrame:(CGRect)frame __attribute__((objc_designated_initializer));
- (nullable instancetype)initWithCoder:(NSCoder *)coder __attribute__((objc_designated_initializer));
- (BOOL)isDescendantOfView:(UIView *)view;
@property (nonatomic) NSInteger tag;
@end

/* ---- menus e ações */
typedef NSString *UIMenuIdentifier NS_TYPED_EXTENSIBLE_ENUM;
typedef NSString *UIActionIdentifier NS_TYPED_EXTENSIBLE_ENUM;
typedef NS_OPTIONS(NSUInteger, UIMenuElementAttributes) {
    UIMenuElementAttributesDisabled = 1 << 0, UIMenuElementAttributesDestructive = 1 << 1,
    UIMenuElementAttributesHidden = 1 << 2, UIMenuElementAttributesKeepsMenuPresented = 1 << 3,
};
typedef NS_ENUM(NSInteger, UIMenuElementState) { UIMenuElementStateOff, UIMenuElementStateOn, UIMenuElementStateMixed };
typedef NS_OPTIONS(NSUInteger, UIMenuOptions) {
    UIMenuOptionsDisplayInline = 1 << 0, UIMenuOptionsDestructive = 1 << 1, UIMenuOptionsSingleSelection = 1 << 5,
    UIMenuOptionsDisplayAsPalette = 1 << 7,
};
typedef NS_OPTIONS(NSInteger, UIKeyModifierFlags) {
    UIKeyModifierAlphaShift = 1 << 16, UIKeyModifierShift = 1 << 17, UIKeyModifierControl = 1 << 18,
    UIKeyModifierAlternate = 1 << 19, UIKeyModifierCommand = 1 << 20, UIKeyModifierNumericPad = 1 << 21,
};

@interface UIMenuElement : NSObject
@property (nonatomic, copy) NSString *title;
@property (nullable, nonatomic, copy) NSString *subtitle;
@property (nullable, nonatomic, strong) UIImage *image;
@end

@interface UIAction : UIMenuElement
@property (nullable, nonatomic, copy) NSString *discoverabilityTitle;
@property (nonatomic) UIMenuElementAttributes attributes;
@property (nonatomic) UIMenuElementState state;
@property (nonatomic, copy) UIActionIdentifier identifier;
- (void)_mosSetHandler:(void (^)(UIAction *action))handler;
- (void)_mosPerform;
@end

@interface UIMenu : UIMenuElement
@property (nonatomic, copy) UIMenuIdentifier identifier;
@property (nonatomic) UIMenuOptions options;
@property (nonatomic, copy) NSArray<UIMenuElement *> *children;
@end

@interface UICommandAlternate : NSObject
@end
@interface UICommand : UIMenuElement
@property (nonatomic) SEL action;
@property (nullable, nonatomic, strong) id propertyList;
@property (nonatomic, copy) NSArray<UICommandAlternate *> *alternates;
@property (nullable, nonatomic, copy) NSString *discoverabilityTitle;
@property (nonatomic) UIMenuElementAttributes attributes;
@property (nonatomic) UIMenuElementState state;
@end
@interface UIKeyCommand : UICommand
@property (nullable, nonatomic, copy) NSString *input;
@property (nonatomic) UIKeyModifierFlags modifierFlags;
@end

/* ---- barras e controles */
typedef NS_ENUM(NSInteger, UIBarButtonSystemItem) {
    UIBarButtonSystemItemDone, UIBarButtonSystemItemCancel, UIBarButtonSystemItemEdit, UIBarButtonSystemItemSave,
    UIBarButtonSystemItemAdd, UIBarButtonSystemItemFlexibleSpace, UIBarButtonSystemItemFixedSpace,
    UIBarButtonSystemItemCompose, UIBarButtonSystemItemReply, UIBarButtonSystemItemAction, UIBarButtonSystemItemOrganize,
    UIBarButtonSystemItemBookmarks, UIBarButtonSystemItemSearch, UIBarButtonSystemItemRefresh, UIBarButtonSystemItemStop,
    UIBarButtonSystemItemCamera, UIBarButtonSystemItemTrash, UIBarButtonSystemItemPlay, UIBarButtonSystemItemPause,
    UIBarButtonSystemItemRewind, UIBarButtonSystemItemFastForward, UIBarButtonSystemItemUndo, UIBarButtonSystemItemRedo,
    UIBarButtonSystemItemPageCurl, UIBarButtonSystemItemClose,
};
@interface UIBarItem : NSObject
@property (nullable, nonatomic, copy) NSString *title;
@property (nullable, nonatomic, strong) UIImage *image;
@end
@interface UIBarButtonItem : UIBarItem
@property (nullable, nonatomic, strong) UIAction *primaryAction;
@property (nullable, nonatomic, strong) UIMenu *menu;
@property (nonatomic) NSInteger _mosSystemItem;
@end

typedef NS_ENUM(NSInteger, UIButtonType) {
    UIButtonTypeCustom = 0, UIButtonTypeSystem, UIButtonTypeDetailDisclosure, UIButtonTypeInfoLight, UIButtonTypeInfoDark,
    UIButtonTypeContactAdd, UIButtonTypePlain, UIButtonTypeClose,
};
typedef NS_OPTIONS(NSUInteger, UIControlEvents) {
    UIControlEventTouchDown = 1 << 0, UIControlEventTouchDownRepeat = 1 << 1, UIControlEventTouchDragInside = 1 << 2,
    UIControlEventTouchDragOutside = 1 << 3, UIControlEventTouchDragEnter = 1 << 4, UIControlEventTouchDragExit = 1 << 5,
    UIControlEventTouchUpInside = 1 << 6, UIControlEventTouchUpOutside = 1 << 7, UIControlEventTouchCancel = 1 << 8,
    UIControlEventValueChanged = 1 << 12, UIControlEventPrimaryActionTriggered = 1 << 13,
    UIControlEventMenuActionTriggered = 1 << 14, UIControlEventEditingDidBegin = 1 << 16,
    UIControlEventEditingChanged = 1 << 17, UIControlEventEditingDidEnd = 1 << 18,
    UIControlEventEditingDidEndOnExit = 1 << 19, UIControlEventAllTouchEvents = 0x00000FFF,
    UIControlEventAllEditingEvents = 0x000F0000, UIControlEventAllEvents = 0xFFFFFFFF,
};
@interface UIControl : UIView
- (void)addAction:(UIAction *)action forControlEvents:(UIControlEvents)controlEvents;
- (NSUInteger)_mosHandlerCount;
- (nullable id)_mosHandlerTarget:(NSUInteger)i;
- (nullable SEL)_mosHandlerSelector:(NSUInteger)i;
- (nullable UIAction *)_mosHandlerAction:(NSUInteger)i;
- (UIControlEvents)_mosHandlerEvents:(NSUInteger)i;
- (void)sendActionsForControlEvents:(UIControlEvents)controlEvents;
@end
@interface UIButton : UIControl
+ (instancetype)buttonWithType:(UIButtonType)type;
@property (nonatomic, readonly) UIButtonType buttonType;
- (void)setTitle:(nullable NSString *)title forState:(NSUInteger)state;
- (void)setImage:(nullable UIImage *)image forState:(NSUInteger)state;
@end

/* ---- apresentação */
@interface UIPresentationController : NSObject
@end
@interface UISheetPresentationController : UIPresentationController
@property (nonatomic) CGFloat _mosPreferredCornerRadius;
@end


/* ---- terceira leva: tipos que as configurações, células, listas e ponteiro usam */
typedef struct NSDirectionalEdgeInsets { CGFloat top, leading, bottom, trailing; } NSDirectionalEdgeInsets;
typedef NS_OPTIONS(NSUInteger, NSDirectionalRectEdge) {
    NSDirectionalRectEdgeNone = 0, NSDirectionalRectEdgeTop = 1 << 0, NSDirectionalRectEdgeLeading = 1 << 1,
    NSDirectionalRectEdgeBottom = 1 << 2, NSDirectionalRectEdgeTrailing = 1 << 3, NSDirectionalRectEdgeAll = 15,
};
typedef NS_ENUM(NSInteger, NSLineBreakMode) {
    NSLineBreakByWordWrapping = 0, NSLineBreakByCharWrapping, NSLineBreakByClipping, NSLineBreakByTruncatingHead,
    NSLineBreakByTruncatingTail, NSLineBreakByTruncatingMiddle,
};
typedef NS_OPTIONS(NSUInteger, UIAxis) { UIAxisNeither = 0, UIAxisHorizontal = 1 << 0, UIAxisVertical = 1 << 1, UIAxisBoth = 3 };
typedef float UILayoutPriority NS_TYPED_EXTENSIBLE_ENUM;
typedef NS_ENUM(NSInteger, UITextLayoutDirection) {
    UITextLayoutDirectionRight = 2, UITextLayoutDirectionLeft, UITextLayoutDirectionUp, UITextLayoutDirectionDown,
};
typedef NSInteger UITextDirection NS_TYPED_ENUM;
typedef NS_ENUM(NSInteger, UITableViewRowAnimation) {
    UITableViewRowAnimationFade, UITableViewRowAnimationRight, UITableViewRowAnimationLeft, UITableViewRowAnimationTop,
    UITableViewRowAnimationBottom, UITableViewRowAnimationNone, UITableViewRowAnimationMiddle,
    UITableViewRowAnimationAutomatic = 100,
};

@interface NSAttributedString : NSObject
@end
@interface UIFont : NSObject
+ (UIFont *)systemFontOfSize:(CGFloat)size;
@end
@interface UIImageConfiguration : NSObject
@end
@interface UIImageSymbolConfiguration : UIImageConfiguration
@end
@interface UIBezierPath : NSObject
@end
@interface UITargetedPreview : NSObject
@end
@interface UIPointerStyle : NSObject
@end
@interface UIFocusSystem : NSObject
@end
@interface UIScrollView : UIView
@end
@protocol UITableViewDataSource <NSObject>
@end
@protocol UICollectionViewDataSource <NSObject>
@end
@interface UITableView : UIScrollView
- (nullable id)dequeueReusableCellWithIdentifier:(NSString *)identifier;
- (void)reloadData;
@property (nonatomic, weak, nullable) id dataSource;
@end
@interface UICollectionViewLayout : NSObject
@end
@interface UICollectionView : UIScrollView
- (instancetype)initWithFrame:(CGRect)frame collectionViewLayout:(UICollectionViewLayout *)layout;
- (void)registerClass:(nullable Class)cellClass forCellWithReuseIdentifier:(NSString *)identifier;
- (id)dequeueReusableCellWithReuseIdentifier:(NSString *)identifier forIndexPath:(id)indexPath;
- (void)registerClass:(nullable Class)viewClass forSupplementaryViewOfKind:(NSString *)kind withReuseIdentifier:(NSString *)identifier;
- (id)dequeueReusableSupplementaryViewOfKind:(NSString *)kind withReuseIdentifier:(NSString *)identifier forIndexPath:(id)indexPath;
- (void)reloadData;
@property (nonatomic, weak, nullable) id dataSource;
@end
@interface UILabel : UIView
@property (nullable, nonatomic, copy) NSString *text;
@end
@interface UITableViewCell : UIView
@property (nonatomic, readonly, nullable) UILabel *textLabel;
@property (nonatomic, readonly, nullable) UILabel *detailTextLabel;
@end
@interface UITableViewHeaderFooterView : UIView
@end
@interface UICollectionReusableView : UIView
@end
@interface UICollectionViewCell : UICollectionReusableView
- (UILabel *)_mosTextLabel;
@end
@interface UICollectionViewListCell : UICollectionViewCell
@end
__attribute__((swift_name("__UICellConfigurationState")))
@interface UICellConfigurationState : NSObject
@end
@protocol NSCollectionLayoutEnvironment <NSObject>
@end
@interface NSCollectionLayoutSection : NSObject
@end

NS_ASSUME_NONNULL_END
