/*
 * mOS — UIKit.framework do Mac Catalyst: rolagem, tabelas, coleções, navegação e Auto Layout.
 *   UIScrollView: contentOffset = origem dos bounds (o desenho e o hitTest já a respeitam), roda do mouse e arrastar.
 *   UITableView: pede ao data source as seções/linhas/células e cria só as células visíveis (com as alturas do
 *     delegate); toque seleciona a linha (tableView:didSelectRowAtIndexPath:).
 *   UICollectionView: o mesmo, com grade (UICollectionViewFlowLayout) ou lista de largura cheia (layouts compostos).
 *   UINavigationController: pilha de view controllers, barra com título e "voltar".
 *   Auto Layout: as âncoras das UIViews são as do AppKit do mOS (NSLayoutConstraint do libSystem); este arquivo resolve
 *     as restrições ativas cujos itens são UIViews (translatesAutoresizingMaskIntoConstraints = NO), por propagação
 *     das igualdades, com tamanho intrínseco e frame atual como reserva, e limites (>=, <=) de largura/altura no fim.
 */
#import "UIKitMOS.h"
#import <objc/runtime.h>
#import <objc/message.h>

@interface NSIndexPath : NSObject
+ (instancetype)indexPathWithIndexes:(const NSUInteger *)indexes length:(NSUInteger)length;
- (NSUInteger)indexAtPosition:(NSUInteger)p;
- (NSUInteger)length;
@end

extern id cx_layout_anchor(id v, int attr);
extern void *UIGraphicsGetCurrentContext(void);
extern unsigned cx_layout_constraints(id **out);
extern int cx_layout_constraint_info(id c, id *a, int *aa, int *rel, id *b, int *ba, double *mult, double *k, float *prio);

static NSString *mos_key(NSInteger s, NSInteger r) { return [NSString stringWithFormat:@"%ld:%ld", (long)s, (long)r]; }
static NSNumber *num(double d) { return [NSNumber numberWithDouble:d]; }

/* ================================================================ NSIndexPath (UIKitAdditions) */
@implementation NSIndexPath (UIKitAdditions)
+ (instancetype)indexPathForRow:(NSInteger)row inSection:(NSInteger)section { NSUInteger i[2] = {(NSUInteger)section, (NSUInteger)row}; return [self indexPathWithIndexes:i length:2]; }
+ (instancetype)indexPathForItem:(NSInteger)item inSection:(NSInteger)section { return [self indexPathForRow:item inSection:section]; }
- (NSInteger)section { return [self length] > 0 ? (NSInteger)[self indexAtPosition:0] : 0; }
- (NSInteger)row { return [self length] > 1 ? (NSInteger)[self indexAtPosition:1] : 0; }
- (NSInteger)item { return [self row]; }
@end

/* ================================================================ UIScrollView */
@implementation UIScrollView
- (CGPoint)contentOffset { return _boundsOrigin; }
- (CGPoint)_mosClamp:(CGPoint)p {
    CGFloat maxX = _contentSize.width + _contentInset.right - _frame.size.width, maxY = _contentSize.height + _contentInset.bottom - _frame.size.height;
    if (p.x > maxX) p.x = maxX;
    if (p.y > maxY) p.y = maxY;
    if (p.x < -_contentInset.left) p.x = -_contentInset.left;
    if (p.y < -_contentInset.top) p.y = -_contentInset.top;
    return p;
}
- (void)setContentOffset:(CGPoint)p {
    _boundsOrigin = p;
    if ([_scrollDelegate respondsToSelector:@selector(scrollViewDidScroll:)]) [_scrollDelegate scrollViewDidScroll:self];
    [self layoutSubviews];
    [self setNeedsDisplay];
}
- (void)setContentOffset:(CGPoint)p animated:(BOOL)a { (void)a; [self setContentOffset:p]; }
- (CGSize)contentSize { return _contentSize; }
- (void)setContentSize:(CGSize)s { _contentSize = s; [self setNeedsDisplay]; }
- (UIEdgeInsets)contentInset { return _contentInset; }
- (void)setContentInset:(UIEdgeInsets)i { _contentInset = i; }
- (UIEdgeInsets)adjustedContentInset { return _contentInset; }
- (NSInteger)contentInsetAdjustmentBehavior { return 0; }
- (void)setContentInsetAdjustmentBehavior:(NSInteger)b { (void)b; }
- (id)delegate { return _scrollDelegate; }
- (void)setDelegate:(id)d { _scrollDelegate = d; }
- (BOOL)isScrollEnabled { return !_scrollDisabled; }
- (void)setScrollEnabled:(BOOL)e { _scrollDisabled = !e; }
- (BOOL)isDragging { return NO; }
- (BOOL)isDecelerating { return NO; }
- (BOOL)isTracking { return NO; }
- (void)setShowsVerticalScrollIndicator:(BOOL)s { (void)s; }
- (void)setShowsHorizontalScrollIndicator:(BOOL)s { (void)s; }
- (void)setAlwaysBounceVertical:(BOOL)b { (void)b; }
- (void)setAlwaysBounceHorizontal:(BOOL)b { (void)b; }
- (void)setBounces:(BOOL)b { (void)b; }
- (void)setKeyboardDismissMode:(NSInteger)m { (void)m; }
- (void)setDecelerationRate:(CGFloat)r { (void)r; }
- (void)flashScrollIndicators {}
- (void)scrollRectToVisible:(CGRect)r animated:(BOOL)a {
    (void)a;
    CGPoint o = _boundsOrigin;
    if (r.origin.y < o.y) o.y = r.origin.y; else if (r.origin.y + r.size.height > o.y + _frame.size.height) o.y = r.origin.y + r.size.height - _frame.size.height;
    [self setContentOffset:[self _mosClamp:o]];
}
/* rolagem pela roda (NSView hospedeira) e por arrastar */
- (void)_mosScrollBy:(CGFloat)dy dx:(CGFloat)dx {
    if (_scrollDisabled) return;
    CGPoint o = [self _mosClamp:CGPointMake(_boundsOrigin.x + dx, _boundsOrigin.y + dy)];
    if (o.x != _boundsOrigin.x || o.y != _boundsOrigin.y) [self setContentOffset:o];
}
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e { (void)e; UITouch *u = [t anyObject]; _dragLast = u ? u->_loc : CGPointMake(0, 0); }
- (void)touchesMoved:(NSSet *)t withEvent:(UIEvent *)e {
    (void)e;
    UITouch *u = [t anyObject];
    if (!u) return;
    [self _mosScrollBy:_dragLast.y - u->_loc.y dx:_contentSize.width > _frame.size.width ? _dragLast.x - u->_loc.x : 0];
    _dragLast = u->_loc;
}
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e { (void)t; (void)e; }
@end

/* ================================================================ UITableViewCell */
@implementation UITableViewCell
- (instancetype)initWithStyle:(NSInteger)style reuseIdentifier:(NSString *)rid {
    if ((self = [super initWithFrame:CGRectMake(0, 0, 320, 44)])) {
        _style = style; _reuseId = [rid copy];
        _contentView = [[UIView alloc] initWithFrame:[self bounds]];
        [self addSubview:_contentView];
        _backgroundColor = [[UIColor systemBackgroundColor] retain];
    }
    return self;
}
- (instancetype)initWithFrame:(CGRect)f { if ((self = [self initWithStyle:0 reuseIdentifier:nil])) _frame = f; return self; }
- (void)dealloc { [_contentView release]; [_textLabel release]; [_detailLabel release]; [_reuseId release]; [super dealloc]; }
- (UIView *)contentView { return _contentView; }
- (NSString *)reuseIdentifier { return _reuseId; }
- (UILabel *)textLabel { if (!_textLabel) { _textLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 0, 0)]; [_contentView addSubview:_textLabel]; [self setNeedsLayout]; } return _textLabel; }
- (UILabel *)detailTextLabel { if (!_detailLabel && _style != 0) { _detailLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 0, 0)]; [_contentView addSubview:_detailLabel]; } return _detailLabel; }
- (id)imageView { return nil; }
- (BOOL)isSelected { return _cellSelected; }
- (void)setSelected:(BOOL)s { _cellSelected = s; [self setNeedsDisplay]; }
- (void)setSelected:(BOOL)s animated:(BOOL)a { (void)a; [self setSelected:s]; }
- (void)setHighlighted:(BOOL)h animated:(BOOL)a { (void)h; (void)a; }
- (void)setAccessoryType:(NSInteger)t { (void)t; }
- (void)setSelectionStyle:(NSInteger)s { (void)s; }
- (void)prepareForReuse {}
- (void)setNeedsUpdateConfiguration {}
- (void)layoutSubviews {
    CGRect b = [self bounds];
    _contentView.frame = b;
    CGFloat w = b.size.width - 32, h = b.size.height;
    if (_textLabel && _detailLabel && _detailLabel->_text.length) {
        _textLabel.frame = CGRectMake(16, 2, w, h / 2);
        _detailLabel.frame = CGRectMake(16, h / 2, w, h / 2 - 2);
    } else if (_textLabel) _textLabel.frame = CGRectMake(16, 0, w, h);
}
- (void)drawRect:(CGRect)r {
    (void)r;
    void *c = UIGraphicsGetCurrentContext();
    if (!c) return;
    extern void CGContextFillRect(void *c, CGRect r);
    extern void CGContextSetFillColorWithColor(void *c, void *color);
    if (_cellSelected) { CGContextSetFillColorWithColor(c, [[UIColor systemGray5Color] CGColor]); CGContextFillRect(c, [self bounds]); }
    CGContextSetFillColorWithColor(c, [[UIColor separatorColor] CGColor]);
    CGContextFillRect(c, CGRectMake(16, _frame.size.height - 1, _frame.size.width - 16, 1));
}
@end

/* ================================================================ UITableView */
typedef id (*mos_cell_fn)(id, SEL, id, id);
@implementation UITableView
- (instancetype)initWithFrame:(CGRect)f style:(NSInteger)s { (void)s; return [self initWithFrame:f]; }
- (instancetype)initWithFrame:(CGRect)f {
    if ((self = [super initWithFrame:f])) { _rowHeight = 44; _registered = [[NSMutableDictionary alloc] init]; _cells = [[NSMutableDictionary alloc] init]; _rowY = [[NSMutableArray alloc] init]; _selSection = -1; }
    return self;
}
- (void)dealloc { [_registered release]; [_cells release]; [_rowY release]; [super dealloc]; }
- (id)dataSource { return _dataSource; }
- (void)setDataSource:(id)d { _dataSource = d; [self reloadData]; }
- (id)delegate { return _tvDelegate; }
- (void)setDelegate:(id)d { _tvDelegate = d; _scrollDelegate = d; }
- (CGFloat)rowHeight { return _rowHeight; }
- (void)setRowHeight:(CGFloat)h { _rowHeight = h > 0 ? h : 44; }
- (void)setEstimatedRowHeight:(CGFloat)h { (void)h; }
- (void)setSectionHeaderHeight:(CGFloat)h { _headerHeight = h; }
- (void)setSeparatorStyle:(NSInteger)s { (void)s; }
- (void)setSeparatorInset:(UIEdgeInsets)i { (void)i; }
- (void)setTableFooterView:(UIView *)v { (void)v; }
- (void)setTableHeaderView:(UIView *)v { (void)v; }
- (void)setAllowsSelection:(BOOL)a { (void)a; }
- (void)registerClass:(Class)c forCellReuseIdentifier:(NSString *)i { if (c && i) [_registered setObject:c forKey:i]; }
- (void)registerNib:(id)n forCellReuseIdentifier:(NSString *)i { (void)n; (void)i; }
- (void)registerClass:(Class)c forHeaderFooterViewReuseIdentifier:(NSString *)i { (void)c; (void)i; }
- (id)dequeueReusableCellWithIdentifier:(NSString *)i {
    Class c = [_registered objectForKey:i];
    return c ? [[[c alloc] initWithStyle:0 reuseIdentifier:i] autorelease] : nil;
}
- (id)dequeueReusableCellWithIdentifier:(NSString *)i forIndexPath:(NSIndexPath *)ip {
    (void)ip;
    id c = [self dequeueReusableCellWithIdentifier:i];
    return c ? c : [[[UITableViewCell alloc] initWithStyle:0 reuseIdentifier:i] autorelease];
}
- (id)dequeueReusableHeaderFooterViewWithIdentifier:(NSString *)i { (void)i; return nil; }
- (NSInteger)numberOfSections {
    return [_dataSource respondsToSelector:@selector(numberOfSectionsInTableView:)] ? ((NSInteger (*)(id, SEL, id))objc_msgSend)(_dataSource, @selector(numberOfSectionsInTableView:), self) : 1;
}
- (NSInteger)numberOfRowsInSection:(NSInteger)s {
    return [_dataSource respondsToSelector:@selector(tableView:numberOfRowsInSection:)] ?
        ((NSInteger (*)(id, SEL, id, NSInteger))objc_msgSend)(_dataSource, @selector(tableView:numberOfRowsInSection:), self, s) : 0;
}
- (CGFloat)_mosHeight:(NSInteger)s row:(NSInteger)r {
    if ([_tvDelegate respondsToSelector:@selector(tableView:heightForRowAtIndexPath:)]) {
        CGFloat h = ((CGFloat (*)(id, SEL, id, id))objc_msgSend)(_tvDelegate, @selector(tableView:heightForRowAtIndexPath:), self, [NSIndexPath indexPathForRow:r inSection:s]);
        if (h > 0) return h;
    }
    return _rowHeight;
}
/* recalcula as posições de todas as linhas e solta as células */
- (void)reloadData {
    for (UIView *c in [_cells allValues]) [c removeFromSuperview];
    [_cells removeAllObjects];
    [_rowY removeAllObjects];
    CGFloat y = 0;
    NSInteger ns = _dataSource ? [self numberOfSections] : 0;
    for (NSInteger s = 0; s < ns; s++) {
        NSMutableArray *ys = [NSMutableArray array];
        y += _headerHeight;
        NSInteger nr = [self numberOfRowsInSection:s];
        for (NSInteger r = 0; r <= nr; r++) { [ys addObject:num(y)]; if (r < nr) y += [self _mosHeight:s row:r]; }
        [_rowY addObject:ys];
    }
    _contentSize = CGSizeMake(_frame.size.width, y);
    [self layoutSubviews];
    [self setNeedsDisplay];
}
- (id)cellForRowAtIndexPath:(NSIndexPath *)ip { return [_cells objectForKey:mos_key([ip section], [ip row])]; }
/* cria as células que entram na área visível e solta as que saíram */
- (void)layoutSubviews {
    CGFloat top = _boundsOrigin.y - 100, bottom = _boundsOrigin.y + _frame.size.height + 100;
    NSMutableDictionary *keep = [NSMutableDictionary dictionary];
    for (NSInteger s = 0; s < (NSInteger)_rowY.count; s++) {
        NSArray *ys = [_rowY objectAtIndex:(NSUInteger)s];
        for (NSInteger r = 0; r + 1 < (NSInteger)ys.count; r++) {
            CGFloat y0 = [[ys objectAtIndex:(NSUInteger)r] doubleValue], y1 = [[ys objectAtIndex:(NSUInteger)r + 1] doubleValue];
            if (y1 < top || y0 > bottom) continue;
            NSString *k = mos_key(s, r);
            UITableViewCell *c = [_cells objectForKey:k];
            if (!c) {
                c = ((mos_cell_fn)objc_msgSend)(_dataSource, @selector(tableView:cellForRowAtIndexPath:), self, [NSIndexPath indexPathForRow:r inSection:s]);
                if (!c) continue;
                if ([c isKindOfClass:[UITableViewCell class]]) { c->_section = s; c->_row = r; c->_cellSelected = (s == _selSection && r == _selRow); }
                [self addSubview:c];
            }
            c.frame = CGRectMake(0, y0, _frame.size.width, y1 - y0);
            [c layoutIfNeeded];
            [keep setObject:c forKey:k];
        }
    }
    for (NSString *k in [_cells allKeys]) if (![keep objectForKey:k]) [[_cells objectForKey:k] removeFromSuperview];
    [_cells setDictionary:keep];
}
- (NSArray *)visibleCells { return [_cells allValues]; }
- (NSIndexPath *)indexPathForRowAtPoint:(CGPoint)p {
    for (NSInteger s = 0; s < (NSInteger)_rowY.count; s++) {
        NSArray *ys = [_rowY objectAtIndex:(NSUInteger)s];
        for (NSInteger r = 0; r + 1 < (NSInteger)ys.count; r++)
            if (p.y >= [[ys objectAtIndex:(NSUInteger)r] doubleValue] && p.y < [[ys objectAtIndex:(NSUInteger)r + 1] doubleValue]) return [NSIndexPath indexPathForRow:r inSection:s];
    }
    return nil;
}
- (NSIndexPath *)indexPathForCell:(UITableViewCell *)c { return [c isKindOfClass:[UITableViewCell class]] ? [NSIndexPath indexPathForRow:c->_row inSection:c->_section] : nil; }
- (NSIndexPath *)indexPathForSelectedRow { return _selSection >= 0 ? [NSIndexPath indexPathForRow:_selRow inSection:_selSection] : nil; }
- (void)selectRowAtIndexPath:(NSIndexPath *)ip animated:(BOOL)a scrollPosition:(NSInteger)p {
    (void)a; (void)p;
    for (UITableViewCell *c in [_cells allValues]) if ([c isKindOfClass:[UITableViewCell class]]) [c setSelected:(ip && c->_section == [ip section] && c->_row == [ip row])];
    _selSection = ip ? [ip section] : -1; _selRow = ip ? [ip row] : -1;
}
- (void)deselectRowAtIndexPath:(NSIndexPath *)ip animated:(BOOL)a { (void)ip; [self selectRowAtIndexPath:nil animated:a scrollPosition:0]; }
- (void)scrollToRowAtIndexPath:(NSIndexPath *)ip atScrollPosition:(NSInteger)p animated:(BOOL)a {
    (void)p;
    if ([ip section] >= (NSInteger)_rowY.count) return;
    NSArray *ys = [_rowY objectAtIndex:(NSUInteger)[ip section]];
    if ([ip row] + 1 >= (NSInteger)ys.count) return;
    CGFloat y0 = [[ys objectAtIndex:(NSUInteger)[ip row]] doubleValue], y1 = [[ys objectAtIndex:(NSUInteger)[ip row] + 1] doubleValue];
    [self scrollRectToVisible:CGRectMake(0, y0, _frame.size.width, y1 - y0) animated:a];
}
- (void)performBatchUpdates:(void (^)(void))u completion:(void (^)(BOOL))c { if (u) u(); [self reloadData]; if (c) c(YES); }
- (void)beginUpdates {}
- (void)endUpdates { [self reloadData]; }
- (void)insertRowsAtIndexPaths:(NSArray *)a withRowAnimation:(NSInteger)x { (void)a; (void)x; [self reloadData]; }
- (void)deleteRowsAtIndexPaths:(NSArray *)a withRowAnimation:(NSInteger)x { (void)a; (void)x; [self reloadData]; }
- (void)reloadRowsAtIndexPaths:(NSArray *)a withRowAnimation:(NSInteger)x { (void)a; (void)x; [self reloadData]; }
- (void)reloadSections:(id)s withRowAnimation:(NSInteger)x { (void)s; (void)x; [self reloadData]; }
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e { [super touchesBegan:t withEvent:e]; UITouch *u = [t anyObject]; _touchStart = u ? u->_loc : CGPointMake(0, 0); }
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e {
    (void)e;
    UITouch *u = [t anyObject];
    if (!u || fabs(u->_loc.y - _touchStart.y) > 8) return;           /* arrastou: rolagem, não seleção */
    NSIndexPath *ip = [self indexPathForRowAtPoint:[u locationInView:self]];
    if (!ip) return;
    [self selectRowAtIndexPath:ip animated:NO scrollPosition:0];
    if ([_tvDelegate respondsToSelector:@selector(tableView:didSelectRowAtIndexPath:)]) [_tvDelegate tableView:self didSelectRowAtIndexPath:ip];
}
@end

/* ================================================================ coleções */
@implementation UICollectionReusableView
- (void)dealloc { [_reuseId release]; [super dealloc]; }
- (NSString *)reuseIdentifier { return _reuseId; }
- (void)prepareForReuse {}
@end

@implementation UICollectionViewCell
- (instancetype)initWithFrame:(CGRect)f {
    if ((self = [super initWithFrame:f])) { _contentView = [[UIView alloc] initWithFrame:[self bounds]]; [self addSubview:_contentView]; }
    return self;
}
- (void)dealloc { [_contentView release]; [_mosLabel release]; [super dealloc]; }
- (UIView *)contentView { return _contentView; }
- (UILabel *)_mosTextLabel { if (!_mosLabel) { _mosLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 0, 0)]; [_contentView addSubview:_mosLabel]; } return _mosLabel; }
- (BOOL)isSelected { return _cellSelected; }
- (void)setSelected:(BOOL)s { _cellSelected = s; [self setNeedsDisplay]; }
- (BOOL)isHighlighted { return NO; }
- (void)setHighlighted:(BOOL)h { (void)h; }
- (void)setNeedsUpdateConfiguration {}
- (void)layoutSubviews { _contentView.frame = [self bounds]; if (_mosLabel) _mosLabel.frame = CGRectMake(16, 0, _frame.size.width - 32, _frame.size.height); }
@end
@implementation UICollectionViewListCell
- (void)drawRect:(CGRect)r {
    (void)r;
    void *c = UIGraphicsGetCurrentContext();
    if (!c) return;
    extern void CGContextFillRect(void *c, CGRect r);
    extern void CGContextSetFillColorWithColor(void *c, void *color);
    if (_cellSelected) { CGContextSetFillColorWithColor(c, [[UIColor systemGray5Color] CGColor]); CGContextFillRect(c, [self bounds]); }
    CGContextSetFillColorWithColor(c, [[UIColor separatorColor] CGColor]);
    CGContextFillRect(c, CGRectMake(16, _frame.size.height - 1, _frame.size.width - 16, 1));
}
@end

@implementation UICollectionViewLayout
- (id)collectionView { return _collectionView; }
- (void)invalidateLayout { [_collectionView reloadData]; }
- (CGSize)collectionViewContentSize { return [(UICollectionView *)_collectionView contentSize]; }
- (void)prepareLayout {}
- (void)registerClass:(Class)c forDecorationViewOfKind:(NSString *)k { (void)c; (void)k; }
@end
@implementation UICollectionViewFlowLayout
- (instancetype)init { if ((self = [super init])) { _itemSize = CGSizeMake(50, 50); _lineSpacing = 10; _itemSpacing = 10; } return self; }
- (CGSize)itemSize { return _itemSize; }
- (void)setItemSize:(CGSize)s { _itemSize = s; }
- (void)setEstimatedItemSize:(CGSize)s { (void)s; }
- (CGFloat)minimumLineSpacing { return _lineSpacing; }
- (void)setMinimumLineSpacing:(CGFloat)s { _lineSpacing = s; }
- (CGFloat)minimumInteritemSpacing { return _itemSpacing; }
- (void)setMinimumInteritemSpacing:(CGFloat)s { _itemSpacing = s; }
- (NSInteger)scrollDirection { return _direction; }
- (void)setScrollDirection:(NSInteger)d { _direction = d; }
- (UIEdgeInsets)sectionInset { return _sectionInset; }
- (void)setSectionInset:(UIEdgeInsets)i { _sectionInset = i; }
- (void)setHeaderReferenceSize:(CGSize)s { (void)s; }
- (void)setFooterReferenceSize:(CGSize)s { (void)s; }
@end
@implementation UICollectionViewCompositionalLayout
- (instancetype)initWithSection:(id)s { (void)s; return [self init]; }
- (instancetype)initWithSection:(id)s configuration:(id)c { (void)s; (void)c; return [self init]; }
- (instancetype)initWithSectionProvider:(id)p { (void)p; return [self init]; }
- (instancetype)initWithSectionProvider:(id)p configuration:(id)c { (void)p; (void)c; return [self init]; }
@end

@implementation UICollectionView
- (instancetype)initWithFrame:(CGRect)f collectionViewLayout:(UICollectionViewLayout *)l {
    if ((self = [super initWithFrame:f])) {
        _registered = [[NSMutableDictionary alloc] init]; _cells = [[NSMutableDictionary alloc] init]; _frames = [[NSMutableArray alloc] init];
        [self setCollectionViewLayout:l];
    }
    return self;
}
- (instancetype)initWithFrame:(CGRect)f { return [self initWithFrame:f collectionViewLayout:[[[UICollectionViewFlowLayout alloc] init] autorelease]]; }
- (void)dealloc { [_registered release]; [_cells release]; [_frames release]; [_layout release]; [super dealloc]; }
- (UICollectionViewLayout *)collectionViewLayout { return _layout; }
- (void)setCollectionViewLayout:(UICollectionViewLayout *)l { [_layout release]; _layout = [l retain]; if (l) l->_collectionView = self; [self reloadData]; }
- (void)setCollectionViewLayout:(UICollectionViewLayout *)l animated:(BOOL)a { (void)a; [self setCollectionViewLayout:l]; }
- (id)dataSource { return _dataSource; }
- (void)setDataSource:(id)d { _dataSource = d; [self reloadData]; }
- (id)delegate { return _cvDelegate; }
- (void)setDelegate:(id)d { _cvDelegate = d; _scrollDelegate = d; }
- (void)setAllowsSelection:(BOOL)a { (void)a; }
- (void)setAllowsMultipleSelection:(BOOL)a { (void)a; }
- (void)registerClass:(Class)c forCellWithReuseIdentifier:(NSString *)i { if (c && i) [_registered setObject:c forKey:i]; }
- (void)registerClass:(Class)c forSupplementaryViewOfKind:(NSString *)k withReuseIdentifier:(NSString *)i { if (c && i) [_registered setObject:c forKey:[k stringByAppendingString:i]]; }
- (void)registerNib:(id)n forCellWithReuseIdentifier:(NSString *)i { (void)n; (void)i; }
- (id)dequeueReusableCellWithReuseIdentifier:(NSString *)i forIndexPath:(NSIndexPath *)ip {
    (void)ip;
    Class c = [_registered objectForKey:i];
    return [[[(c ? c : [UICollectionViewCell class]) alloc] initWithFrame:CGRectMake(0, 0, 50, 50)] autorelease];
}
- (id)dequeueReusableSupplementaryViewOfKind:(NSString *)k withReuseIdentifier:(NSString *)i forIndexPath:(NSIndexPath *)ip {
    (void)ip;
    Class c = [_registered objectForKey:[k stringByAppendingString:i]];
    return [[[(c ? c : [UICollectionReusableView class]) alloc] initWithFrame:CGRectMake(0, 0, 0, 0)] autorelease];
}
- (NSInteger)numberOfSections {
    return [_dataSource respondsToSelector:@selector(numberOfSectionsInCollectionView:)] ? ((NSInteger (*)(id, SEL, id))objc_msgSend)(_dataSource, @selector(numberOfSectionsInCollectionView:), self) : 1;
}
- (NSInteger)numberOfItemsInSection:(NSInteger)s {
    return [_dataSource respondsToSelector:@selector(collectionView:numberOfItemsInSection:)] ?
        ((NSInteger (*)(id, SEL, id, NSInteger))objc_msgSend)(_dataSource, @selector(collectionView:numberOfItemsInSection:), self, s) : 0;
}
- (CGSize)_mosItemSize:(NSInteger)s item:(NSInteger)i flow:(UICollectionViewFlowLayout *)fl {
    if ([_cvDelegate respondsToSelector:@selector(collectionView:layout:sizeForItemAtIndexPath:)])
        return ((CGSize (*)(id, SEL, id, id, id))objc_msgSend)(_cvDelegate, @selector(collectionView:layout:sizeForItemAtIndexPath:), self, _layout, [NSIndexPath indexPathForItem:i inSection:s]);
    return fl->_itemSize;
}
/* posições dos itens: grade (flow) ou lista de largura cheia (layout composto/lista) */
- (void)reloadData {
    for (UIView *c in [_cells allValues]) [c removeFromSuperview];
    [_cells removeAllObjects];
    [_frames removeAllObjects];
    if (!_dataSource) { _contentSize = CGSizeMake(0, 0); return; }
    BOOL flow = [_layout isKindOfClass:[UICollectionViewFlowLayout class]];
    UICollectionViewFlowLayout *fl = flow ? (UICollectionViewFlowLayout *)_layout : nil;
    CGFloat W = _frame.size.width, y = 0;
    NSInteger ns = [self numberOfSections];
    for (NSInteger s = 0; s < ns; s++) {
        NSMutableArray *fs = [NSMutableArray array];
        NSInteger n = [self numberOfItemsInSection:s];
        CGFloat x = flow ? fl->_sectionInset.left : 0, rowH = 0;
        if (flow) y += fl->_sectionInset.top;
        for (NSInteger i = 0; i < n; i++) {
            CGRect r;
            if (flow) {
                CGSize sz = [self _mosItemSize:s item:i flow:fl];
                if (x > fl->_sectionInset.left && x + sz.width > W - fl->_sectionInset.right) { x = fl->_sectionInset.left; y += rowH + fl->_lineSpacing; rowH = 0; }
                r = CGRectMake(x, y, sz.width, sz.height);
                x += sz.width + fl->_itemSpacing;
                if (sz.height > rowH) rowH = sz.height;
            } else {
                r = CGRectMake(0, y, W, 44);
                y += 44;
            }
            [fs addObject:[NSArray arrayWithObjects:num(r.origin.x), num(r.origin.y), num(r.size.width), num(r.size.height), nil]];
        }
        if (flow) y += rowH + fl->_sectionInset.bottom;
        [_frames addObject:fs];
    }
    _contentSize = CGSizeMake(W, y);
    [self layoutSubviews];
    [self setNeedsDisplay];
}
static CGRect fr(NSArray *a) { return CGRectMake([[a objectAtIndex:0] doubleValue], [[a objectAtIndex:1] doubleValue], [[a objectAtIndex:2] doubleValue], [[a objectAtIndex:3] doubleValue]); }
- (void)layoutSubviews {
    CGFloat top = _boundsOrigin.y - 100, bottom = _boundsOrigin.y + _frame.size.height + 100;
    NSMutableDictionary *keep = [NSMutableDictionary dictionary];
    for (NSInteger s = 0; s < (NSInteger)_frames.count; s++) {
        NSArray *fs = [_frames objectAtIndex:(NSUInteger)s];
        for (NSInteger i = 0; i < (NSInteger)fs.count; i++) {
            CGRect r = fr([fs objectAtIndex:(NSUInteger)i]);
            if (r.origin.y + r.size.height < top || r.origin.y > bottom) continue;
            NSString *k = mos_key(s, i);
            UIView *c = [_cells objectForKey:k];
            if (!c) {
                c = ((mos_cell_fn)objc_msgSend)(_dataSource, @selector(collectionView:cellForItemAtIndexPath:), self, [NSIndexPath indexPathForItem:i inSection:s]);
                if (!c) continue;
                [self addSubview:c];
            }
            c.frame = r;
            [c layoutIfNeeded];
            [keep setObject:c forKey:k];
        }
    }
    for (NSString *k in [_cells allKeys]) if (![keep objectForKey:k]) [[_cells objectForKey:k] removeFromSuperview];
    [_cells setDictionary:keep];
}
- (id)cellForItemAtIndexPath:(NSIndexPath *)ip { return [_cells objectForKey:mos_key([ip section], [ip item])]; }
- (NSArray *)visibleCells { return [_cells allValues]; }
- (NSIndexPath *)indexPathForItemAtPoint:(CGPoint)p {
    for (NSInteger s = 0; s < (NSInteger)_frames.count; s++) {
        NSArray *fs = [_frames objectAtIndex:(NSUInteger)s];
        for (NSInteger i = 0; i < (NSInteger)fs.count; i++) {
            CGRect r = fr([fs objectAtIndex:(NSUInteger)i]);
            if (p.x >= r.origin.x && p.y >= r.origin.y && p.x < r.origin.x + r.size.width && p.y < r.origin.y + r.size.height) return [NSIndexPath indexPathForItem:i inSection:s];
        }
    }
    return nil;
}
- (NSIndexPath *)indexPathForCell:(UICollectionViewCell *)c { for (NSString *k in _cells) if ([_cells objectForKey:k] == c) { NSArray *p = [k componentsSeparatedByString:@":"]; return [NSIndexPath indexPathForItem:[[p objectAtIndex:1] integerValue] inSection:[[p objectAtIndex:0] integerValue]]; } return nil; }
- (NSArray *)indexPathsForVisibleItems { NSMutableArray *a = [NSMutableArray array]; for (UIView *c in [_cells allValues]) { id ip = [self indexPathForCell:(id)c]; if (ip) [a addObject:ip]; } return a; }
- (void)selectItemAtIndexPath:(NSIndexPath *)ip animated:(BOOL)a scrollPosition:(NSUInteger)p { (void)a; (void)p; for (UIView *c in [_cells allValues]) if ([c isKindOfClass:[UICollectionViewCell class]]) [(UICollectionViewCell *)c setSelected:[self cellForItemAtIndexPath:ip] == c]; }
- (void)deselectItemAtIndexPath:(NSIndexPath *)ip animated:(BOOL)a { (void)a; UICollectionViewCell *c = [self cellForItemAtIndexPath:ip]; if ([c isKindOfClass:[UICollectionViewCell class]]) [c setSelected:NO]; }
- (void)performBatchUpdates:(void (^)(void))u completion:(void (^)(BOOL))c { if (u) u(); [self reloadData]; if (c) c(YES); }
- (void)insertItemsAtIndexPaths:(NSArray *)a { (void)a; [self reloadData]; }
- (void)deleteItemsAtIndexPaths:(NSArray *)a { (void)a; [self reloadData]; }
- (void)reloadItemsAtIndexPaths:(NSArray *)a { (void)a; [self reloadData]; }
- (void)reloadSections:(id)s { (void)s; [self reloadData]; }
- (void)scrollToItemAtIndexPath:(NSIndexPath *)ip atScrollPosition:(NSUInteger)p animated:(BOOL)a {
    (void)p;
    if ([ip section] >= (NSInteger)_frames.count) return;
    NSArray *fs = [_frames objectAtIndex:(NSUInteger)[ip section]];
    if ([ip item] >= (NSInteger)fs.count) return;
    [self scrollRectToVisible:fr([fs objectAtIndex:(NSUInteger)[ip item]]) animated:a];
}
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e { [super touchesBegan:t withEvent:e]; UITouch *u = [t anyObject]; _touchStart = u ? u->_loc : CGPointMake(0, 0); }
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e {
    (void)e;
    UITouch *u = [t anyObject];
    if (!u || fabs(u->_loc.y - _touchStart.y) > 8) return;
    NSIndexPath *ip = [self indexPathForItemAtPoint:[u locationInView:self]];
    if (!ip) return;
    [self selectItemAtIndexPath:ip animated:NO scrollPosition:0];
    if ([_cvDelegate respondsToSelector:@selector(collectionView:didSelectItemAtIndexPath:)]) [_cvDelegate collectionView:self didSelectItemAtIndexPath:ip];
}
@end

/* ================================================================ navegação */
static char k_navitem, k_navctl;
@implementation UINavigationItem
- (instancetype)initWithTitle:(NSString *)t { if ((self = [super init])) _title = [t copy]; return self; }
- (void)dealloc { [_title release]; [_rightItems release]; [_leftItems release]; [_titleView release]; [super dealloc]; }
- (NSString *)title { return _title; }
- (void)setTitle:(NSString *)t { [_title release]; _title = [t copy]; }
- (id)rightBarButtonItem { return [_rightItems lastObject]; }
- (void)setRightBarButtonItem:(id)i { [_rightItems release]; _rightItems = i ? [[NSArray alloc] initWithObjects:i, nil] : nil; }
- (NSArray *)rightBarButtonItems { return _rightItems; }
- (void)setRightBarButtonItems:(NSArray *)a { [_rightItems release]; _rightItems = [a copy]; }
- (id)leftBarButtonItem { return [_leftItems lastObject]; }
- (void)setLeftBarButtonItem:(id)i { [_leftItems release]; _leftItems = i ? [[NSArray alloc] initWithObjects:i, nil] : nil; }
- (NSArray *)leftBarButtonItems { return _leftItems; }
- (void)setLeftBarButtonItems:(NSArray *)a { [_leftItems release]; _leftItems = [a copy]; }
- (id)titleView { return _titleView; }
- (void)setTitleView:(id)v { [_titleView release]; _titleView = [v retain]; }
- (void)setLargeTitleDisplayMode:(NSInteger)m { (void)m; }
- (void)setHidesBackButton:(BOOL)h { (void)h; }
- (void)setBackButtonTitle:(NSString *)t { (void)t; }
- (void)setSearchController:(id)s { (void)s; }
- (void)setStandardAppearance:(id)a { (void)a; }
- (void)setScrollEdgeAppearance:(id)a { (void)a; }
@end

@implementation UIViewController (MOSNavigation)
- (UINavigationItem *)navigationItem {
    UINavigationItem *i = objc_getAssociatedObject(self, &k_navitem);
    if (!i) { i = [[[UINavigationItem alloc] initWithTitle:_title] autorelease]; objc_setAssociatedObject(self, &k_navitem, i, OBJC_ASSOCIATION_RETAIN); }
    return i;
}
- (UINavigationController *)navigationController {
    for (UIViewController *v = self; v; v = v->_parent) { id n = objc_getAssociatedObject(v, &k_navctl); if (n) return n; if ([v isKindOfClass:[UINavigationController class]]) return (UINavigationController *)v; }
    return nil;
}
@end

@implementation UINavigationBar
- (UINavigationController *)_nav { return _navController; }
- (void)drawRect:(CGRect)r {
    (void)r;
    void *c = UIGraphicsGetCurrentContext();
    if (!c) return;
    extern void CGContextFillRect(void *c, CGRect r);
    extern void CGContextSetFillColorWithColor(void *c, void *color);
    CGContextSetFillColorWithColor(c, [[UIColor secondarySystemBackgroundColor] CGColor]);
    CGContextFillRect(c, [self bounds]);
    CGContextSetFillColorWithColor(c, [[UIColor separatorColor] CGColor]);
    CGContextFillRect(c, CGRectMake(0, _frame.size.height - 1, _frame.size.width, 1));
    UINavigationController *n = _navController;
    UIViewController *top = [n->_vcs lastObject];
    NSString *t = top ? (top.navigationItem.title ? top.navigationItem.title : top->_title) : nil;
    NSDictionary *at = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont boldSystemFontOfSize:13], NSFontAttributeName, nil];
    if (t.length) [t drawAtPoint:NSMakePoint(_frame.size.width / 2 - t.length * 3.6, 13) withAttributes:at];
    if (n->_vcs.count > 1) {
        NSColor *tint = [NSColor colorWithRed:0 green:0.48 blue:1 alpha:1];
        [@"‹ Voltar" drawAtPoint:NSMakePoint(10, 13) withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:tint, NSForegroundColorAttributeName, nil]];
    }
}
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e {
    (void)e;
    UITouch *u = [t anyObject];
    UINavigationController *n = _navController;
    if (u && [u locationInView:self].x < 90 && n->_vcs.count > 1) [n popViewControllerAnimated:YES];
}
- (void)setPrefersLargeTitles:(BOOL)p { (void)p; }
- (void)setStandardAppearance:(id)a { (void)a; }
- (void)setScrollEdgeAppearance:(id)a { (void)a; }
- (void)setTranslucent:(BOOL)t { (void)t; }
@end

@implementation UINavigationController
- (instancetype)initWithRootViewController:(UIViewController *)root {
    if ((self = [self init])) { if (root) [self pushViewController:root animated:NO]; }
    return self;
}
- (instancetype)init { if ((self = [super init])) _vcs = [[NSMutableArray alloc] init]; return self; }
- (void)dealloc { [_vcs release]; [_bar release]; [super dealloc]; }
- (void)loadView {
    [super loadView];
    _bar = [[UINavigationBar alloc] initWithFrame:CGRectMake(0, 0, 0, 44)];
    _bar->_navController = self;
    [_view addSubview:_bar];
    [self _mosShowTop];
}
- (UINavigationBar *)navigationBar { (void)[self view]; return _bar; }
- (BOOL)isNavigationBarHidden { return _barHidden; }
- (void)setNavigationBarHidden:(BOOL)h { _barHidden = h; [_bar setHidden:h]; [_view setNeedsLayout]; [self _mosLayout]; }
- (void)setNavigationBarHidden:(BOOL)h animated:(BOOL)a { (void)a; [self setNavigationBarHidden:h]; }
- (NSArray *)viewControllers { return [[_vcs copy] autorelease]; }
- (void)setViewControllers:(NSArray *)a { [self setViewControllers:a animated:NO]; }
- (void)setViewControllers:(NSArray *)a animated:(BOOL)an {
    (void)an;
    UIViewController *old = [_vcs lastObject];
    if (old && old->_view) [old->_view removeFromSuperview];
    for (UIViewController *v in _vcs) [v removeFromParentViewController];
    [_vcs removeAllObjects];
    for (UIViewController *v in a) { [self addChildViewController:v]; objc_setAssociatedObject(v, &k_navctl, self, OBJC_ASSOCIATION_ASSIGN); [_vcs addObject:v]; }
    if (_view) [self _mosShowTop];
}
- (UIViewController *)topViewController { return [_vcs lastObject]; }
- (UIViewController *)visibleViewController { return [_vcs lastObject]; }
- (void)_mosLayout {
    if (!_view) return;
    CGRect b = [_view bounds];
    CGFloat barH = _barHidden ? 0 : 44;
    _bar.frame = CGRectMake(0, 0, b.size.width, 44);
    UIViewController *top = [_vcs lastObject];
    if (top) [top view].frame = CGRectMake(0, barH, b.size.width, b.size.height - barH);
}
- (void)viewDidLayoutSubviews { [self _mosLayout]; }
- (void)_mosShowTop {
    UIViewController *top = [_vcs lastObject];
    if (!top || !_view) return;
    UIView *v = [top view];
    [top viewWillAppear:NO];
    [_view insertSubview:v atIndex:0];
    [self _mosLayout];
    [top viewDidAppear:NO];
    [_bar setNeedsDisplay];
}
- (void)pushViewController:(UIViewController *)vc animated:(BOOL)a {
    (void)a;
    if (!vc) return;
    UIViewController *old = [_vcs lastObject];
    if (old && old->_view) { [old viewWillDisappear:NO]; [old->_view removeFromSuperview]; [old viewDidDisappear:NO]; }
    [self addChildViewController:vc];
    objc_setAssociatedObject(vc, &k_navctl, self, OBJC_ASSOCIATION_ASSIGN);
    [_vcs addObject:vc];
    if (_view) [self _mosShowTop];
}
- (UIViewController *)popViewControllerAnimated:(BOOL)a {
    (void)a;
    if (_vcs.count < 2) return nil;
    UIViewController *top = [[[_vcs lastObject] retain] autorelease];
    [top viewWillDisappear:NO];
    if (top->_view) [top->_view removeFromSuperview];
    [top viewDidDisappear:NO];
    [top removeFromParentViewController];
    [_vcs removeLastObject];
    [self _mosShowTop];
    return top;
}
- (NSArray *)popToRootViewControllerAnimated:(BOOL)a { NSMutableArray *r = [NSMutableArray array]; while (_vcs.count > 1) [r addObject:[self popViewControllerAnimated:a]]; return r; }
- (NSArray *)popToViewController:(UIViewController *)vc animated:(BOOL)a { NSMutableArray *r = [NSMutableArray array]; while (_vcs.count > 1 && [_vcs lastObject] != vc) [r addObject:[self popViewControllerAnimated:a]]; return r; }
- (id)interactivePopGestureRecognizer { return nil; }
- (void)setDelegate:(id)d { (void)d; }
@end

/* ================================================================ Auto Layout */
@implementation UILayoutGuide
- (UIView *)owningView { return _owningView; }
- (CGRect)layoutFrame { return _owningView ? [_owningView bounds] : CGRectMake(0, 0, 0, 0); }
#define ANC(name, attr) - (id)name { return cx_layout_anchor(_owningView, attr); }
ANC(leadingAnchor, 5) ANC(trailingAnchor, 6) ANC(leftAnchor, 1) ANC(rightAnchor, 2) ANC(topAnchor, 3) ANC(bottomAnchor, 4)
ANC(widthAnchor, 7) ANC(heightAnchor, 8) ANC(centerXAnchor, 9) ANC(centerYAnchor, 10)
@end

static char k_safe, k_margins;
@implementation UIView (MOSAutoLayout)
#undef ANC
#define ANC(name, attr) - (id)name { return cx_layout_anchor(self, attr); }
ANC(leadingAnchor, 5) ANC(trailingAnchor, 6) ANC(leftAnchor, 1) ANC(rightAnchor, 2) ANC(topAnchor, 3) ANC(bottomAnchor, 4)
ANC(widthAnchor, 7) ANC(heightAnchor, 8) ANC(centerXAnchor, 9) ANC(centerYAnchor, 10) ANC(lastBaselineAnchor, 11)
ANC(firstBaselineAnchor, 12)
- (UILayoutGuide *)_mosGuide:(void *)key {
    UILayoutGuide *g = objc_getAssociatedObject(self, key);
    if (!g) { g = [[[UILayoutGuide alloc] init] autorelease]; g->_owningView = self; objc_setAssociatedObject(self, key, g, OBJC_ASSOCIATION_RETAIN); }
    return g;
}
- (UILayoutGuide *)safeAreaLayoutGuide { return [self _mosGuide:&k_safe]; }
- (UILayoutGuide *)layoutMarginsGuide { return [self _mosGuide:&k_margins]; }
- (UILayoutGuide *)readableContentGuide { return [self _mosGuide:&k_margins]; }
- (UILayoutGuide *)keyboardLayoutGuide { return [self _mosGuide:&k_safe]; }
- (UIEdgeInsets)safeAreaInsets { UIEdgeInsets z = {0, 0, 0, 0}; return z; }
- (UIEdgeInsets)layoutMargins { UIEdgeInsets m = {8, 8, 8, 8}; return m; }
- (void)setLayoutMargins:(UIEdgeInsets)m { (void)m; }
- (void)setDirectionalLayoutMargins:(UIEdgeInsets)m { (void)m; }
- (void)setPreservesSuperviewLayoutMargins:(BOOL)p { (void)p; }
- (void)setInsetsLayoutMarginsFromSafeArea:(BOOL)p { (void)p; }
- (void)addLayoutGuide:(UILayoutGuide *)g { if (g) g->_owningView = self; }
- (void)removeLayoutGuide:(UILayoutGuide *)g { (void)g; }
- (void)addConstraint:(id)c { [c setActive:YES]; }
- (void)addConstraints:(NSArray *)a { for (id c in a) [c setActive:YES]; }
- (void)removeConstraint:(id)c { [c setActive:NO]; }
- (void)removeConstraints:(NSArray *)a { for (id c in a) [c setActive:NO]; }
- (NSArray *)constraints {
    id *cs; unsigned n = cx_layout_constraints(&cs);
    NSMutableArray *r = [NSMutableArray array];
    for (unsigned i = 0; i < n; i++) { id a, b; int aa, rel, ba; double m, k; float p; if (cx_layout_constraint_info(cs[i], &a, &aa, &rel, &b, &ba, &m, &k, &p) && (a == self || b == self)) [r addObject:cs[i]]; }
    return r;
}
- (void)setContentHuggingPriority:(float)p forAxis:(NSInteger)ax { (void)p; (void)ax; }
- (void)setContentCompressionResistancePriority:(float)p forAxis:(NSInteger)ax { (void)p; (void)ax; }
- (float)contentHuggingPriorityForAxis:(NSInteger)ax { (void)ax; return 250; }
- (float)contentCompressionResistancePriorityForAxis:(NSInteger)ax { (void)ax; return 750; }
- (void)setNeedsUpdateConstraints {}
- (void)updateConstraintsIfNeeded {}
- (void)updateConstraints {}
- (void)invalidateIntrinsicContentSize { [self setNeedsLayout]; }
- (CGSize)systemLayoutSizeFittingSize:(CGSize)s { CGSize i = [self intrinsicContentSize]; return (i.width >= 0 && i.height >= 0) ? i : s; }
- (CGSize)systemLayoutSizeFittingSize:(CGSize)s withHorizontalFittingPriority:(float)h verticalFittingPriority:(float)v { (void)h; (void)v; return [self systemLayoutSizeFittingSize:s]; }

/* ---- resolvedor: atributos por view (coordenadas do superview), propagando as igualdades */
typedef struct { double v[13]; float pr[13]; unsigned known; } mos_lv_t;
static int norm_attr(int a) { return a == 5 ? 1 : a == 6 ? 2 : a == 11 ? 4 : a == 12 ? 3 : a; }
static double frame_attr(CGRect f, int a) {
    switch (norm_attr(a)) {
    case 1: return f.origin.x; case 2: return f.origin.x + f.size.width; case 3: return f.origin.y;
    case 4: return f.origin.y + f.size.height; case 7: return f.size.width; case 8: return f.size.height;
    case 9: return f.origin.x + f.size.width / 2; case 10: return f.origin.y + f.size.height / 2;
    }
    return 0;
}
static BOOL is_uiview(id o) { return o && [o isKindOfClass:[UIView class]]; }
static NSUInteger mos_index(NSArray *a, id o) { NSUInteger n = a.count; for (NSUInteger i = 0; i < n; i++) if ([a objectAtIndex:i] == o) return i; return NSNotFound; }

- (void)_mosSolveLayout {
    id *cs; unsigned n = cx_layout_constraints(&cs);
    if (!n) return;
    /* as views (da árvore desta janela) com restrições e translatesAutoresizingMaskIntoConstraints = NO */
    NSMutableArray *views = [NSMutableArray array];
    for (unsigned i = 0; i < n; i++) {
        id a, b; int aa, rel, ba; double m, k; float p;
        if (!cx_layout_constraint_info(cs[i], &a, &aa, &rel, &b, &ba, &m, &k, &p)) continue;
        id v2[2] = {a, b};
        for (int j = 0; j < 2; j++) {
            UIView *v = v2[j];
            if (is_uiview(v) && !v->_tamic && [v isDescendantOfView:self] && ![views containsObject:v]) [views addObject:v];
        }
    }
    NSUInteger nv = views.count;
    if (!nv) return;
    mos_lv_t *lv = calloc(nv, sizeof *lv);
    /* prioridades: as maiores primeiro (uma igualdade obrigatória fixa o atributo antes das opcionais) */
    unsigned *ord = malloc(n * sizeof *ord);
    float *prio = malloc(n * sizeof *prio);
    for (unsigned i = 0; i < n; i++) {
        id a, b; int aa, rel, ba; double m, k; float p = 1000;
        if (!cx_layout_constraint_info(cs[i], &a, &aa, &rel, &b, &ba, &m, &k, &p)) p = 0;
        ord[i] = i; prio[i] = p > 0 ? p : 1000;
    }
    for (unsigned i = 1; i < n; i++) {                                     /* inserção: estável */
        unsigned x = ord[i]; unsigned j = i;
        while (j > 0 && prio[ord[j - 1]] < prio[x]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = x;
    }
    for (int round = 0; round < 2; round++) {
        for (NSUInteger i = 0; i < nv; i++) lv[i].known = 0;
        for (int pass = 0; pass < 6; pass++) {
            for (unsigned oi = 0; oi < n; oi++) {
                unsigned i = ord[oi];
                id a, b; int aa, rel, ba; double m, k; float p;
                if (!cx_layout_constraint_info(cs[i], &a, &aa, &rel, &b, &ba, &m, &k, &p) || rel != 0) continue;
                NSUInteger ia = is_uiview(a) ? mos_index(views, a) : NSNotFound;
                NSUInteger ib = is_uiview(b) ? mos_index(views, b) : NSNotFound;
                UIView *t; int tat; id s; int sat; BOOL inverse = NO;
                if (ia != NSNotFound) { t = a; tat = aa; s = b; sat = ba; }
                else if (ib != NSNotFound) { t = b; tat = ba; s = a; sat = aa; inverse = YES; }
                else continue;
                NSUInteger it = mos_index(views, t);
                int nt = norm_attr(tat);
                if ((lv[it].known & (1u << nt)) && lv[it].pr[nt] >= prio[i]) continue;   /* já fixado por uma de prioridade maior ou igual */
                double sv;
                if (!s) { if (inverse) continue; sv = 0; }
                else {
                    UIView *sview = s;
                    if (!is_uiview(sview)) continue;
                    NSUInteger is = mos_index(views, sview);
                    int ns = norm_attr(sat);
                    if (sview == t->_superview) sv = frame_attr(CGRectMake(0, 0, sview->_frame.size.width, sview->_frame.size.height), ns);
                    else if (is != NSNotFound && (lv[is].known & (1u << ns)) && sview->_superview == t->_superview) sv = lv[is].v[ns];
                    else if (is != NSNotFound) continue;                            /* ainda não resolvida: próxima passada */
                    else if (t->_superview) sv = frame_attr([sview convertRect:[sview bounds] toView:t->_superview], ns);
                    else sv = frame_attr(sview->_frame, ns);
                }
                double val = !s ? k : inverse ? (m != 0 ? (sv - k) / m : sv) : m * sv + k;
                lv[it].v[nt] = val;
                lv[it].known |= 1u << nt;
                lv[it].pr[nt] = prio[i];
            }
        }
        /* frames */
        for (NSUInteger i = 0; i < nv; i++) {
            UIView *v = [views objectAtIndex:i];
            mos_lv_t *L = &lv[i];
            unsigned K = L->known;
            CGSize in = [v intrinsicContentSize];
            CGRect f = v->_frame;
            double w = (K & 1u << 7) ? L->v[7] : ((K & 1u << 1) && (K & 1u << 2)) ? L->v[2] - L->v[1]
                     : ((K & 1u << 1) && (K & 1u << 9)) ? 2 * (L->v[9] - L->v[1]) : in.width >= 0 ? in.width : f.size.width;
            double h = (K & 1u << 8) ? L->v[8] : ((K & 1u << 3) && (K & 1u << 4)) ? L->v[4] - L->v[3]
                     : ((K & 1u << 3) && (K & 1u << 10)) ? 2 * (L->v[10] - L->v[3]) : in.height >= 0 ? in.height : f.size.height;
            double x = (K & 1u << 1) ? L->v[1] : (K & 1u << 2) ? L->v[2] - w : (K & 1u << 9) ? L->v[9] - w / 2 : f.origin.x;
            double y = (K & 1u << 3) ? L->v[3] : (K & 1u << 4) ? L->v[4] - h : (K & 1u << 10) ? L->v[10] - h / 2 : f.origin.y;
            /* limites (>=, <=), constantes ou relativos a outra view; da menor para a maior prioridade (a maior vence),
               e só sobre atributos fixados com prioridade menor que a do limite */
            for (int oi = (int)n - 1; oi >= 0; oi--) {
                unsigned c = ord[oi];
                id a, b; int aa, rel, ba; double m, k; float p;
                if (!cx_layout_constraint_info(cs[c], &a, &aa, &rel, &b, &ba, &m, &k, &p) || a != v || rel == 0) continue;
                double lim = k;
                if (b) {
                    if (!is_uiview(b)) continue;
                    UIView *bv = b;
                    CGRect br = bv == v->_superview ? CGRectMake(0, 0, bv->_frame.size.width, bv->_frame.size.height)
                              : v->_superview ? [bv convertRect:[bv bounds] toView:v->_superview] : bv->_frame;
                    lim = m * frame_attr(br, ba) + k;
                }
                int na = norm_attr(aa);
                CGRect cur = CGRectMake(x, y, w, h);
                double val = frame_attr(cur, aa);
                if (!(rel > 0 ? val < lim : val > lim)) continue;              /* já satisfeito */
                /* o atributo fixado por igualdade de prioridade maior ou igual não cede */
                if ((L->known & (1u << na)) && L->pr[na] >= prio[c]) continue;
                switch (na) {
                case 7: w = lim; break;
                case 8: h = lim; break;
                case 1: if (K & 1u << 2) w = x + w - lim; x = lim; break;      /* borda esquerda: a direita fica, se fixada */
                case 2: if (K & 1u << 1) w = lim - x; else x = lim - w; break;
                case 3: if (K & 1u << 4) h = y + h - lim; y = lim; break;
                case 4: if (K & 1u << 3) h = lim - y; else y = lim - h; break;
                case 9: x = lim - w / 2; break;
                case 10: y = lim - h / 2; break;
                }
            }
            v->_frame = CGRectMake(x, y, w < 0 ? 0 : w, h < 0 ? 0 : h);
        }
    }
    free(lv); free(ord); free(prio);
}
@end
