# Stubs restantes da frente Foundation/CoreFoundation

Gerado por `tools/fdn-inventario.py` a partir de `sys_zerados.c`. Total: **949**.

| Grupo | Quantidade |
|---|---:|
| extensões | 144 |
| PredicateExpressions | 98 |
| Data | 71 |
| Calendar | 61 |
| JSONEncoder | 40 |
| IndexSet | 32 |
| JSONDecoder | 30 |
| URLResourceValues | 30 |
| DateComponents | 29 |
| URL | 26 |
| URLRequest | 25 |
| _BridgedStoredNSError | 18 |
| NSURLSession | 17 |
| Date | 16 |
| NSNotificationCenter | 13 |
| IndexPath | 12 |
| CustomNSError | 11 |
| URLComponents | 11 |
| AttributeScopes | 11 |
| Locale | 11 |
| _BridgedNSError | 10 |
| NSScanner | 10 |
| FloatingPointFormatStyle | 9 |
| NSFileHandle | 9 |
| NSRunLoop | 8 |
| _NSRange | 7 |
| AttributedString | 6 |
| UUID | 6 |
| NSURLSessionWebSocketTask | 6 |
| __DataStorage | 5 |
| AsyncLineSequence | 5 |
| KeyPathComparator | 5 |
| NumberFormatStyleConfiguration | 5 |
| NSProgress | 5 |
| NSCoder | 5 |
| NSTimer | 5 |
| CharacterSet | 4 |
| _ErrorCodeProtocol | 4 |
| URLError | 4 |
| Predicate | 4 |
| NSDictionary | 4 |
| UICollectionView | 4 |
| NSKeyedUnarchiver | 4 |
| NSSet | 4 |
| NSArray | 4 |
| NSObject | 4 |
| CocoaError | 3 |
| DateInterval | 3 |
| URLQueryItem | 3 |
| ParseStrategy | 3 |
| PredicateError | 3 |
| IntegerFormatStyle | 3 |
| PredicateExpression | 3 |
| PropertyListDecoder | 3 |
| PropertyListEncoder | 3 |
| NSKeyValueObservation | 3 |
| AttributeDynamicLookup | 3 |
| NSKeyValueObservedChange | 3 |
| NSFastEnumerationIterator | 3 |
| _ObjectiveCBridgeableError | 3 |
| _KeyValueCodingAndObserving | 3 |
| NSError | 3 |
| NSNumber | 3 |
| NSString | 3 |
| FormatStyle | 2 |
| DataProtocol | 2 |
| Notification | 2 |
| ListFormatStyle | 2 |
| PredicateBindings | 2 |
| _AsyncBytesBuffer | 2 |
| TimeZone | 2 |
| NSExpression | 2 |
| NSOrderedSet | 2 |
| NSFileManager | 2 |
| NSData | 2 |
| CGPoint | 2 |
| UIButton | 2 |
| POSIXError | 1 |
| StringStyle | 1 |
| ContiguousBytes | 1 |
| NSLocalizedString | 1 |
| StandardPredicateExpression | 1 |
| _KeyValueCodingAndObservingPublishing | 1 |
| NSLog | 1 |
| SortOrder | 1 |
| CFErrorRef | 1 |
| NSIndexSet | 1 |
| NSPredicate | 1 |
| NSEnumerator | 1 |
| NSUndoManager | 1 |
| NSKeyedArchiver | 1 |
| CKFetchRecordZoneChangesOperation | 1 |
| CGRect | 1 |
| CGSize | 1 |

## extensões (144)

- `$s10_ErrorType10Foundation01_A12CodeProtocolPTl`  
  associated type descriptor for Foundation._ErrorCodeProtocol._ErrorType
- `$s11Compression12OutputFilterC5writeyyxSgK10Foundation12DataProtocolRzlFTj`  
  dispatch thunk of Compression.OutputFilter.write<A where A: Foundation.DataProtocol>(A?) throws -> ()
- `$s11Compression12OutputFilterC_5using14bufferCapacity9writingToAcA0C9OperationO_AA9AlgorithmOSiy10Foundation4DataVSgKctKcfc`  
  Compression.OutputFilter.init(_: Compression.FilterOperation, using: Compression.Algorithm, bufferCapacity: Swift.Int, writingTo: (Foundation.Data?) throws -> ()) throws -> Compression.OutputFilter
- `$s12CoreGraphics7CGFloatV10FoundationE10truncatingACSo8NSNumberCh_tcfC`  
  (extension in Foundation):CoreGraphics.CGFloat.init(truncating: __shared __C.NSNumber) -> CoreGraphics.CGFloat
- `$s12CoreGraphics7CGFloatV10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):CoreGraphics.CGFloat._bridgeToObjectiveC() -> __C.NSNumber
- `$s12CoreGraphics7CGFloatV10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_ACSgztFZ`  
  static (extension in Foundation):CoreGraphics.CGFloat._forceBridgeFromObjectiveC(_: __C.NSNumber, result: inout CoreGraphics.CGFloat?) -> ()
- `$s12CoreGraphics7CGFloatV10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_ACSgztFZ`  
  static (extension in Foundation):CoreGraphics.CGFloat._conditionallyBridgeFromObjectiveC(_: __C.NSNumber, result: inout CoreGraphics.CGFloat?) -> Swift.Bool
- `$s12CoreGraphics7CGFloatV10FoundationE36_unconditionallyBridgeFromObjectiveCyACSo8NSNumberCSgFZ`  
  static (extension in Foundation):CoreGraphics.CGFloat._unconditionallyBridgeFromObjectiveC(__C.NSNumber?) -> CoreGraphics.CGFloat
- `$s12CoreGraphics7CGFloatVs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for CoreGraphics.CGFloat : Swift._ObjectiveCBridgeable in Foundation
- `$s4Code10Foundation21_BridgedStoredNSErrorPTl`  
  associated type descriptor for Foundation._BridgedStoredNSError.Code
- `$s5UIKit29UITableViewDiffableDataSourceC05tableC012cellProviderACyxq_GSo0bC0C_So0bC4CellCSgAH_10Foundation9IndexPathVq_tctcfC`  
  UIKit.UITableViewDiffableDataSource.__allocating_init(tableView: __C.UITableView, cellProvider: (__C.UITableView, Foundation.IndexPath, B) -> __C.UITableViewCell?) -> UIKit.UITableViewDiffableDataSource<A, B>
- `$s5UIKit29UITableViewDiffableDataSourceC05tableC012cellProviderACyxq_GSo0bC0C_So0bC4CellCSgAH_10Foundation9IndexPathVq_tctcfCTq`  
  method descriptor for UIKit.UITableViewDiffableDataSource.__allocating_init(tableView: __C.UITableView, cellProvider: (__C.UITableView, Foundation.IndexPath, B) -> __C.UITableViewCell?) -> UIKit.UITableViewDiffableDataSource<A, B>
- `$s5UIKit29UITableViewDiffableDataSourceC05tableC012cellProviderACyxq_GSo0bC0C_So0bC4CellCSgAH_10Foundation9IndexPathVq_tctcfc`  
  UIKit.UITableViewDiffableDataSource.init(tableView: __C.UITableView, cellProvider: (__C.UITableView, Foundation.IndexPath, B) -> __C.UITableViewCell?) -> UIKit.UITableViewDiffableDataSource<A, B>
- `$s5UIKit29UITableViewDiffableDataSourceC05tableC0_12canEditRowAtSbSo0bC0C_10Foundation9IndexPathVtFTq`  
  method descriptor for UIKit.UITableViewDiffableDataSource.tableView(_: __C.UITableView, canEditRowAt: Foundation.IndexPath) -> Swift.Bool
- `$s5UIKit29UITableViewDiffableDataSourceC05tableC0_12cellForRowAtSo0bC4CellCSo0bC0C_10Foundation9IndexPathVtFTq`  
  method descriptor for UIKit.UITableViewDiffableDataSource.tableView(_: __C.UITableView, cellForRowAt: Foundation.IndexPath) -> __C.UITableViewCell
- `$s5UIKit29UITableViewDiffableDataSourceC14itemIdentifier3forq_Sg10Foundation9IndexPathV_tFTj`  
  dispatch thunk of UIKit.UITableViewDiffableDataSource.itemIdentifier(for: Foundation.IndexPath) -> B?
- `$s5UIKit29UITableViewDiffableDataSourceC9indexPath3for10Foundation05IndexH0VSgq__tFTj`  
  dispatch thunk of UIKit.UITableViewDiffableDataSource.indexPath(for: B) -> Foundation.IndexPath?
- `$s5UIKit34UICollectionViewDiffableDataSourceC010collectionC012cellProviderACyxq_GSo0bC0C_So0bC4CellCSgAH_10Foundation9IndexPathVq_tctcfC`  
  UIKit.UICollectionViewDiffableDataSource.__allocating_init(collectionView: __C.UICollectionView, cellProvider: (__C.UICollectionView, Foundation.IndexPath, B) -> __C.UICollectionViewCell?) -> UIKit.UICollectionViewDiffableDataSource<A, B>
- `$s5UIKit34UICollectionViewDiffableDataSourceC010collectionC012cellProviderACyxq_GSo0bC0C_So0bC4CellCSgAH_10Foundation9IndexPathVq_tctcfCTq`  
  method descriptor for UIKit.UICollectionViewDiffableDataSource.__allocating_init(collectionView: __C.UICollectionView, cellProvider: (__C.UICollectionView, Foundation.IndexPath, B) -> __C.UICollectionViewCell?) -> UIKit.UICollectionViewDiffableDataSource<A, B>
- `$s5UIKit34UICollectionViewDiffableDataSourceC010collectionC012cellProviderACyxq_GSo0bC0C_So0bC4CellCSgAH_10Foundation9IndexPathVq_tctcfc`  
  UIKit.UICollectionViewDiffableDataSource.init(collectionView: __C.UICollectionView, cellProvider: (__C.UICollectionView, Foundation.IndexPath, B) -> __C.UICollectionViewCell?) -> UIKit.UICollectionViewDiffableDataSource<A, B>
- `$s5UIKit34UICollectionViewDiffableDataSourceC013supplementaryC8ProviderSo0b8ReusableC0CSgSo0bC0C_SS10Foundation9IndexPathVtcSgvsTj`  
  dispatch thunk of UIKit.UICollectionViewDiffableDataSource.supplementaryViewProvider.setter : ((__C.UICollectionView, Swift.String, Foundation.IndexPath) -> __C.UICollectionReusableView?)?
- `$s5UIKit34UICollectionViewDiffableDataSourceC14itemIdentifier3forq_Sg10Foundation9IndexPathV_tFTj`  
  dispatch thunk of UIKit.UICollectionViewDiffableDataSource.itemIdentifier(for: Foundation.IndexPath) -> B?
- `$s5UIKit34UICollectionViewDiffableDataSourceC9indexPath3for10Foundation05IndexH0VSgq__tFTj`  
  dispatch thunk of UIKit.UICollectionViewDiffableDataSource.indexPath(for: B) -> Foundation.IndexPath?
- `$s5UIKit35UICollectionLayoutListConfigurationV20itemSeparatorHandlerAA06UIListgE0V10Foundation9IndexPathV_AFtcSgvs`  
  UIKit.UICollectionLayoutListConfiguration.itemSeparatorHandler.setter : ((Foundation.IndexPath, UIKit.UIListSeparatorConfiguration) -> UIKit.UIListSeparatorConfiguration)?
- `$s5UIKit40UIConfigurationTextAttributesTransformerVyAC10Foundation18AttributeContainerVAFccfC`  
  UIKit.UIConfigurationTextAttributesTransformer.init((Foundation.AttributeContainer) -> Foundation.AttributeContainer) -> UIKit.UIConfigurationTextAttributesTransformer
- `$s6Output10Foundation19PredicateExpressionPTl`  
  associated type descriptor for Foundation.PredicateExpression.Output
- `$s7Network10NWListenerC7ServiceV4name4type6domain9txtRecordAESSSg_SSAJ10Foundation4DataVSgtcfC`  
  Network.NWListener.Service.init(name: Swift.String?, type: Swift.String, domain: Swift.String?, txtRecord: Foundation.Data?) -> Network.NWListener.Service
- `$s7Network12NWConnectionC4send7content0D7Context10isComplete10completiony10Foundation4DataVSg_AC07ContentE0CSbAC14SendCompletionOtF`  
  Network.NWConnection.send(content: Foundation.Data?, contentContext: Network.NWConnection.ContentContext, isComplete: Swift.Bool, completion: Network.NWConnection.SendCompletion) -> ()
- `$s7Network12NWConnectionC7receive23minimumIncompleteLength07maximumF010completionySi_Siy10Foundation4DataVSg_AC14ContentContextCSgSbAA7NWErrorOSgtctF`  
  Network.NWConnection.receive(minimumIncompleteLength: Swift.Int, maximumLength: Swift.Int, completion: (Foundation.Data?, Network.NWConnection.ContentContext?, Swift.Bool, Network.NWError?) -> ()) -> ()
- `$s8Dispatch0A4DataV10Foundation0B8ProtocolADMc`  
  protocol conformance descriptor for Dispatch.DispatchData : Foundation.DataProtocol in Foundation
- `$sSD10FoundationE26_forceBridgeFromObjectiveC_6resultySo12NSDictionaryC_SDyxq_GSgztFZ`  
  static (extension in Foundation):Swift.Dictionary._forceBridgeFromObjectiveC(_: __C.NSDictionary, result: inout [A : B]?) -> ()
- `$sSD10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo12NSDictionaryC_SDyxq_GSgztFZ`  
  static (extension in Foundation):Swift.Dictionary._conditionallyBridgeFromObjectiveC(_: __C.NSDictionary, result: inout [A : B]?) -> Swift.Bool
- `$sSDyxq_Gs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for [A : B] : Swift._ObjectiveCBridgeable in Foundation
- `$sSS10FoundationE10contentsOf12usedEncodingSSAA3URLVh_SSAAE0E0VztKcfC`  
  (extension in Foundation):Swift.String.init(contentsOf: __shared Foundation.URL, usedEncoding: inout (extension in Foundation):Swift.String.Encoding) throws -> Swift.String
- `$sSS10FoundationE14contentsOfFile12usedEncodingS2Sh_SSAAE0F0VztKcfC`  
  (extension in Foundation):Swift.String.init(contentsOfFile: __shared Swift.String, usedEncoding: inout (extension in Foundation):Swift.String.Encoding) throws -> Swift.String
- `$sSS10FoundationE17LocalizationValueV13stringLiteralACSS_tcfC`  
  (extension in Foundation):Swift.String.LocalizationValue.init(stringLiteral: Swift.String) -> (extension in Foundation):Swift.String.LocalizationValue
- `$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0_9specifieryx_SStAA18_FormatSpecifiableRzlF`  
  (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation.appendInterpolation<A where A: Foundation._FormatSpecifiable>(_: A, specifier: Swift.String) -> ()
- `$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0yySSF`  
  (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation.appendInterpolation(Swift.String) -> ()
- `$sSS10FoundationE17LocalizationValueV19StringInterpolationV13appendLiteralyySSF`  
  (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation.appendLiteral(Swift.String) -> ()
- `$sSS10FoundationE17LocalizationValueV19StringInterpolationV15literalCapacity18interpolationCountAESi_SitcfC`  
  (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation.init(literalCapacity: Swift.Int, interpolationCount: Swift.Int) -> (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation
- `$sSS10FoundationE17LocalizationValueV19StringInterpolationVMa`  
  type metadata accessor for (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation
- `$sSS10FoundationE17LocalizationValueV19stringInterpolationA2C06StringE0V_tcfC`  
  (extension in Foundation):Swift.String.LocalizationValue.init(stringInterpolation: (extension in Foundation):Swift.String.LocalizationValue.StringInterpolation) -> (extension in Foundation):Swift.String.LocalizationValue
- `$sSS10FoundationE17LocalizationValueVMa`  
  type metadata accessor for (extension in Foundation):Swift.String.LocalizationValue
- `$sSS10FoundationE5bytes8encodingSSSgxh_SSAAE8EncodingVtcSTRzs5UInt8V7ElementRtzlufC`  
  (extension in Foundation):Swift.String.init<A where A: Swift.Sequence, A.Element == Swift.UInt8>(bytes: __shared A, encoding: (extension in Foundation):Swift.String.Encoding) -> Swift.String?
- `$sSS10FoundationE8EncodingV10macOSRomanACvgZ`  
  static (extension in Foundation):Swift.String.Encoding.macOSRoman.getter : (extension in Foundation):Swift.String.Encoding
- `$sSS10FoundationE8EncodingV2eeoiySbAC_ACtFZ`  
  static (extension in Foundation):Swift.String.Encoding.== infix((extension in Foundation):Swift.String.Encoding, (extension in Foundation):Swift.String.Encoding) -> Swift.Bool
- `$sSS10FoundationE8EncodingVMn`  
  nominal type descriptor for (extension in Foundation):Swift.String.Encoding
- `$sSS10FoundationE8EncodingVSQAAMc`  
  protocol conformance descriptor for (extension in Foundation):Swift.String.Encoding : Swift.Equatable in Foundation
- `$sSS10FoundationE8EncodingVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for (extension in Foundation):Swift.String.Encoding : Swift.CustomStringConvertible in Foundation
- `$sSS10FoundationEySSSo8NSStringCcfC`  
  (extension in Foundation):Swift.String.init(__C.NSString) -> Swift.String
- `$sSSs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.String : Swift._ObjectiveCBridgeable in Foundation
- `$sSSs7CVarArg10FoundationMc`  
  protocol conformance descriptor for Swift.String : Swift.CVarArg in Foundation
- `$sSW10Foundation15ContiguousBytesAAWP`  
  protocol witness table for Swift.UnsafeRawBufferPointer : Foundation.ContiguousBytes in Foundation
- `$sSa10FoundationE26_forceBridgeFromObjectiveC_6resultySo7NSArrayC_SayxGSgztFZ`  
  static (extension in Foundation):Swift.Array._forceBridgeFromObjectiveC(_: __C.NSArray, result: inout [A]?) -> ()
- `$sSayxG10Foundation12DataProtocolABs5UInt8VRszlMc`  
  protocol conformance descriptor for <A where A == Swift.UInt8> [A] : Foundation.DataProtocol in Foundation
- `$sSayxG10Foundation15ContiguousBytesABs5UInt8VRszlMc`  
  protocol conformance descriptor for <A where A == Swift.UInt8> [A] : Foundation.ContiguousBytes in Foundation
- `$sSayxGs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for [A] : Swift._ObjectiveCBridgeable in Foundation
- `$sSayxGs7CVarArg10FoundationMc`  
  protocol conformance descriptor for [A] : Swift.CVarArg in Foundation
- `$sSb10FoundationE10truncatingSbSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.Bool.init(truncating: __shared __C.NSNumber) -> Swift.Bool
- `$sSb10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Bool._bridgeToObjectiveC() -> __C.NSNumber
- `$sSb10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SbSgztFZ`  
  static (extension in Foundation):Swift.Bool._conditionallyBridgeFromObjectiveC(_: __C.NSNumber, result: inout Swift.Bool?) -> Swift.Bool
- `$sSci10Foundations5UInt8V7ElementRtzrlE5linesAA17AsyncLineSequenceVyxGvg`  
  (extension in Foundation):Swift.AsyncSequence< where A.Element == Swift.UInt8>.lines.getter : Foundation.AsyncLineSequence<A>
- `$sSd10FoundationE10truncatingSdSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.Double.init(truncating: __shared __C.NSNumber) -> Swift.Double
- `$sSd10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Double._bridgeToObjectiveC() -> __C.NSNumber
- `$sSds21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.Double : Swift._ObjectiveCBridgeable in Foundation
- `$sSf10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Float._bridgeToObjectiveC() -> __C.NSNumber
- `$sSfs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.Float : Swift._ObjectiveCBridgeable in Foundation
- `$sSh10FoundationE19_bridgeToObjectiveCSo5NSSetCyF`  
  (extension in Foundation):Swift.Set._bridgeToObjectiveC() -> __C.NSSet
- `$sSh10FoundationE36_unconditionallyBridgeFromObjectiveCyShyxGSo5NSSetCSgFZ`  
  static (extension in Foundation):Swift.Set._unconditionallyBridgeFromObjectiveC(__C.NSSet?) -> Swift.Set<A>
- `$sShyxGs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.Set<A> : Swift._ObjectiveCBridgeable in Foundation
- `$sShyxGs7CVarArg10FoundationMc`  
  protocol conformance descriptor for Swift.Set<A> : Swift.CVarArg in Foundation
- `$sSi10Foundation18_FormatSpecifiableAAWP`  
  protocol witness table for Swift.Int : Foundation._FormatSpecifiable in Foundation
- `$sSi10FoundationE10truncatingSiSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.Int.init(truncating: __shared __C.NSNumber) -> Swift.Int
- `$sSi10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Int._bridgeToObjectiveC() -> __C.NSNumber
- `$sSi10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SiSgztFZ`  
  static (extension in Foundation):Swift.Int._forceBridgeFromObjectiveC(_: __C.NSNumber, result: inout Swift.Int?) -> ()
- `$sSi10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SiSgztFZ`  
  static (extension in Foundation):Swift.Int._conditionallyBridgeFromObjectiveC(_: __C.NSNumber, result: inout Swift.Int?) -> Swift.Bool
- `$sSi10FoundationE36_unconditionallyBridgeFromObjectiveCySiSo8NSNumberCSgFZ`  
  static (extension in Foundation):Swift.Int._unconditionallyBridgeFromObjectiveC(__C.NSNumber?) -> Swift.Int
- `$sSis21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.Int : Swift._ObjectiveCBridgeable in Foundation
- `$sSn10FoundationSS5IndexVRszrlE_2inSnyACGSgSo8_NSRangeV_SShtcfC`  
  (extension in Foundation):Swift.Range< where A == Swift.String.Index>.init(_: __C._NSRange, in: __shared Swift.String) -> Swift.Range<Swift.String.Index>?
- `$sSu10FoundationE10truncatingSuSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.UInt.init(truncating: __shared __C.NSNumber) -> Swift.UInt
- `$sSu10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.UInt._bridgeToObjectiveC() -> __C.NSNumber
- `$sSus21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.UInt : Swift._ObjectiveCBridgeable in Foundation
- `$sSy10FoundationE10components11separatedBySaySSGAA12CharacterSetV_tF`  
  (extension in Foundation):Swift.StringProtocol.components(separatedBy: Foundation.CharacterSet) -> [Swift.String]
- `$sSy10FoundationE10components11separatedBySaySSGqd___tSyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.components<A where A1: Swift.StringProtocol>(separatedBy: A1) -> [Swift.String]
- `$sSy10FoundationE10lowercased4withSSAA6LocaleVSg_tF`  
  (extension in Foundation):Swift.StringProtocol.lowercased(with: Foundation.Locale?) -> Swift.String
- `$sSy10FoundationE10uppercased4withSSAA6LocaleVSg_tF`  
  (extension in Foundation):Swift.StringProtocol.uppercased(with: Foundation.Locale?) -> Swift.String
- `$sSy10FoundationE11capitalized4withSSAA6LocaleVSg_tF`  
  (extension in Foundation):Swift.StringProtocol.capitalized(with: Foundation.Locale?) -> Swift.String
- `$sSy10FoundationE11capitalizedSSvg`  
  (extension in Foundation):Swift.StringProtocol.capitalized.getter : Swift.String
- `$sSy10FoundationE12commonPrefix4with7optionsSSqd___So22NSStringCompareOptionsVtSyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.commonPrefix<A where A1: Swift.StringProtocol>(with: A1, options: __C.NSStringCompareOptions) -> Swift.String
- `$sSy10FoundationE13lengthOfBytes5usingSiSSAAE8EncodingV_tF`  
  (extension in Foundation):Swift.StringProtocol.lengthOfBytes(using: (extension in Foundation):Swift.String.Encoding) -> Swift.Int
- `$sSy10FoundationE16rangeOfCharacter4from7options0B0SnySS5IndexVGSgAA0D3SetV_So22NSStringCompareOptionsVAItF`  
  (extension in Foundation):Swift.StringProtocol.rangeOfCharacter(from: Foundation.CharacterSet, options: __C.NSStringCompareOptions, range: Swift.Range<Swift.String.Index>?) -> Swift.Range<Swift.String.Index>?
- `$sSy10FoundationE17applyingTransform_7reverseSSSgSo08NSStringC0a_SbtF`  
  (extension in Foundation):Swift.StringProtocol.applyingTransform(_: __C.NSStringTransform, reverse: Swift.Bool) -> Swift.String?
- `$sSy10FoundationE18localizedLowercaseSSvg`  
  (extension in Foundation):Swift.StringProtocol.localizedLowercase.getter : Swift.String
- `$sSy10FoundationE18localizedUppercaseSSvg`  
  (extension in Foundation):Swift.StringProtocol.localizedUppercase.getter : Swift.String
- `$sSy10FoundationE18trimmingCharacters2inSSAA12CharacterSetV_tF`  
  (extension in Foundation):Swift.StringProtocol.trimmingCharacters(in: Foundation.CharacterSet) -> Swift.String
- `$sSy10FoundationE19enumerateSubstrings2in7options_yqd___So26NSStringEnumerationOptionsVySSSg_SnySS5IndexVGAJSbztctSXRd__AI5BoundRtd__lF`  
  (extension in Foundation):Swift.StringProtocol.enumerateSubstrings<A where A1: Swift.RangeExpression, A1.Bound == Swift.String.Index>(in: A1, options: __C.NSStringEnumerationOptions, _: (Swift.String?, Swift.Range<Swift.String.Index>, Swift.Range<Swift.String.Index>, inout Swift.Bool) -> ()) -> ()
- `$sSy10FoundationE19replacingCharacters2in4withSSqd_0__qd__tSyRd__SXRd_0_SS5IndexV5BoundRtd_0_r0_lF`  
  (extension in Foundation):Swift.StringProtocol.replacingCharacters<A, B where A1: Swift.StringProtocol, B1: Swift.RangeExpression, B1.Bound == Swift.String.Index>(in: B1, with: A1) -> Swift.String
- `$sSy10FoundationE20localizedCapitalizedSSvg`  
  (extension in Foundation):Swift.StringProtocol.localizedCapitalized.getter : Swift.String
- `$sSy10FoundationE20localizedCapitalizedSSvpMV`  
  property descriptor for (extension in Foundation):Swift.StringProtocol.localizedCapitalized : Swift.String
- `$sSy10FoundationE20replacingOccurrences2of4with7options5rangeSSqd___qd_0_So22NSStringCompareOptionsVSnySS5IndexVGSgtSyRd__SyRd_0_r0_lF`  
  (extension in Foundation):Swift.StringProtocol.replacingOccurrences<A, B where A1: Swift.StringProtocol, B1: Swift.StringProtocol>(of: A1, with: B1, options: __C.NSStringCompareOptions, range: Swift.Range<Swift.String.Index>?) -> Swift.String
- `$sSy10FoundationE21addingPercentEncoding21withAllowedCharactersSSSgAA12CharacterSetV_tF`  
  (extension in Foundation):Swift.StringProtocol.addingPercentEncoding(withAllowedCharacters: Foundation.CharacterSet) -> Swift.String?
- `$sSy10FoundationE22caseInsensitiveCompareySo18NSComparisonResultVqd__SyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.caseInsensitiveCompare<A where A1: Swift.StringProtocol>(A1) -> __C.NSComparisonResult
- `$sSy10FoundationE23removingPercentEncodingSSSgvg`  
  (extension in Foundation):Swift.StringProtocol.removingPercentEncoding.getter : Swift.String?
- `$sSy10FoundationE32localizedCaseInsensitiveContainsySbqd__SyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.localizedCaseInsensitiveContains<A where A1: Swift.StringProtocol>(A1) -> Swift.Bool
- `$sSy10FoundationE32rangeOfComposedCharacterSequence2atSnySS5IndexVGAE_tF`  
  (extension in Foundation):Swift.StringProtocol.rangeOfComposedCharacterSequence(at: Swift.String.Index) -> Swift.Range<Swift.String.Index>
- `$sSy10FoundationE33rangeOfComposedCharacterSequences3forSnySS5IndexVGqd___tSXRd__AE5BoundRtd__lF`  
  (extension in Foundation):Swift.StringProtocol.rangeOfComposedCharacterSequences<A where A1: Swift.RangeExpression, A1.Bound == Swift.String.Index>(for: A1) -> Swift.Range<Swift.String.Index>
- `$sSy10FoundationE36decomposedStringWithCanonicalMappingSSvg`  
  (extension in Foundation):Swift.StringProtocol.decomposedStringWithCanonicalMapping.getter : Swift.String
- `$sSy10FoundationE37precomposedStringWithCanonicalMappingSSvg`  
  (extension in Foundation):Swift.StringProtocol.precomposedStringWithCanonicalMapping.getter : Swift.String
- `$sSy10FoundationE41precomposedStringWithCompatibilityMappingSSvg`  
  (extension in Foundation):Swift.StringProtocol.precomposedStringWithCompatibilityMapping.getter : Swift.String
- `$sSy10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF`  
  (extension in Foundation):Swift.StringProtocol.data(using: (extension in Foundation):Swift.String.Encoding, allowLossyConversion: Swift.Bool) -> Foundation.Data?
- `$sSy10FoundationE4hashSivg`  
  (extension in Foundation):Swift.StringProtocol.hash.getter : Swift.Int
- `$sSy10FoundationE5range2of7optionsAB6localeSnySS5IndexVGSgqd___So22NSStringCompareOptionsVAiA6LocaleVSgtSyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.range<A where A1: Swift.StringProtocol>(of: A1, options: __C.NSStringCompareOptions, range: Swift.Range<Swift.String.Index>?, locale: Foundation.Locale?) -> Swift.Range<Swift.String.Index>?
- `$sSy10FoundationE5write2to10atomically8encodingyAA3URLV_SbSSAAE8EncodingVtKF`  
  (extension in Foundation):Swift.StringProtocol.write(to: Foundation.URL, atomically: Swift.Bool, encoding: (extension in Foundation):Swift.String.Encoding) throws -> ()
- `$sSy10FoundationE7compare_7options5range6localeSo18NSComparisonResultVqd___So22NSStringCompareOptionsVSnySS5IndexVGSgAA6LocaleVSgtSyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.compare<A where A1: Swift.StringProtocol>(_: A1, options: __C.NSStringCompareOptions, range: Swift.Range<Swift.String.Index>?, locale: Foundation.Locale?) -> __C.NSComparisonResult
- `$sSy10FoundationE7folding7options6localeSSSo22NSStringCompareOptionsV_AA6LocaleVSgtF`  
  (extension in Foundation):Swift.StringProtocol.folding(options: __C.NSStringCompareOptions, locale: Foundation.Locale?) -> Swift.String
- `$sSy10FoundationE7padding8toLength7withPad10startingAtSSSi_qd__SitSyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.padding<A where A1: Swift.StringProtocol>(toLength: Swift.Int, withPad: A1, startingAt: Swift.Int) -> Swift.String
- `$sSy10FoundationE8containsySbqd__SyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.contains<A where A1: Swift.StringProtocol>(A1) -> Swift.Bool
- `$sSy10FoundationE9appendingySSqd__SyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.appending<A where A1: Swift.StringProtocol>(A1) -> Swift.String
- `$ss10ArraySliceVyxG10Foundation15ContiguousBytesADs5UInt8VRszlMc`  
  protocol conformance descriptor for <A where A == Swift.UInt8> Swift.ArraySlice<A> : Foundation.ContiguousBytes in Foundation
- `$ss11AnyHashableV10FoundationE19_bridgeToObjectiveCSo8NSObjectCyF`  
  (extension in Foundation):Swift.AnyHashable._bridgeToObjectiveC() -> __C.NSObject
- `$ss15CollectionOfOneVyxG10Foundation15ContiguousBytesADs5UInt8VRszlMc`  
  protocol conformance descriptor for <A where A == Swift.UInt8> Swift.CollectionOfOne<A> : Foundation.ContiguousBytes in Foundation
- `$ss5ErrorP10FoundationAC13CustomNSErrorRzSYRzs17FixedWidthInteger8RawValueSYRpzrlE5_codeSivg`  
  (extension in Foundation):Swift.Error< where A: Foundation.CustomNSError, A: Swift.RawRepresentable, A.Swift.RawRepresentable.RawValue: Swift.FixedWidthInteger>._code.getter : Swift.Int
- `$ss5ErrorP10FoundationAC13CustomNSErrorRzrlE5_codeSivg`  
  (extension in Foundation):Swift.Error< where A: Foundation.CustomNSError>._code.getter : Swift.Int
- `$ss5ErrorP10FoundationAC13CustomNSErrorRzrlE7_domainSSvg`  
  (extension in Foundation):Swift.Error< where A: Foundation.CustomNSError>._domain.getter : Swift.String
- `$ss5Int16V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Int16._bridgeToObjectiveC() -> __C.NSNumber
- `$ss5Int32V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Int32._bridgeToObjectiveC() -> __C.NSNumber
- `$ss5Int32V10FoundationE7exactlyABSgSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.Int32.init(exactly: __shared __C.NSNumber) -> Swift.Int32?
- `$ss5Int32Vs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.Int32 : Swift._ObjectiveCBridgeable in Foundation
- `$ss5Int64V10FoundationE10truncatingABSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.Int64.init(truncating: __shared __C.NSNumber) -> Swift.Int64
- `$ss5Int64V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.Int64._bridgeToObjectiveC() -> __C.NSNumber
- `$ss5SliceV10FoundationAC15ContiguousBytesRzrlE010withUnsafeD0yqd__qd__SWKXEKlF`  
  (extension in Foundation):Swift.Slice< where A: Foundation.ContiguousBytes>.withUnsafeBytes<A>((Swift.UnsafeRawBufferPointer) throws -> A1) throws -> A1
- `$ss5SliceVyxG10Foundation15ContiguousBytesA2dERzrlMc`  
  protocol conformance descriptor for < where A: Foundation.ContiguousBytes> Swift.Slice<A> : Foundation.ContiguousBytes in Foundation
- `$ss6UInt16V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.UInt16._bridgeToObjectiveC() -> __C.NSNumber
- `$ss6UInt32V10FoundationE10truncatingABSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.UInt32.init(truncating: __shared __C.NSNumber) -> Swift.UInt32
- `$ss6UInt32V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.UInt32._bridgeToObjectiveC() -> __C.NSNumber
- `$ss6UInt32Vs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.UInt32 : Swift._ObjectiveCBridgeable in Foundation
- `$ss6UInt64V10FoundationE10truncatingABSo8NSNumberCh_tcfC`  
  (extension in Foundation):Swift.UInt64.init(truncating: __shared __C.NSNumber) -> Swift.UInt64
- `$ss6UInt64V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF`  
  (extension in Foundation):Swift.UInt64._bridgeToObjectiveC() -> __C.NSNumber
- `$ss6UInt64Vs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for Swift.UInt64 : Swift._ObjectiveCBridgeable in Foundation
- `$ss8DurationV10FoundationE15TimeFormatStyleV7PatternV12minuteSecond17padMinuteToLength017fractionalSecondsL0015roundFractionalN0AGSi_Sis25FloatingPointRoundingRuleOtFZ`  
  static (extension in Foundation):Swift.Duration.TimeFormatStyle.Pattern.minuteSecond(padMinuteToLength: Swift.Int, fractionalSecondsLength: Swift.Int, roundFractionalSeconds: Swift.FloatingPointRoundingRule) -> (extension in Foundation):Swift.Duration.TimeFormatStyle.Pattern
- `$ss8DurationV10FoundationE15TimeFormatStyleV7PatternVMa`  
  type metadata accessor for (extension in Foundation):Swift.Duration.TimeFormatStyle.Pattern
- `$ss8DurationV10FoundationE15TimeFormatStyleVAC0dE0ACMc`  
  protocol conformance descriptor for (extension in Foundation):Swift.Duration.TimeFormatStyle : Foundation.FormatStyle in Foundation
- `$ss8DurationV10FoundationE15TimeFormatStyleVMa`  
  type metadata accessor for (extension in Foundation):Swift.Duration.TimeFormatStyle
- `$ss8DurationV10FoundationE9formattedy12FormatOutputQzxAC0D5StyleRzAB0D5InputRtzlF`  
  (extension in Foundation):Swift.Duration.formatted<A where A: Foundation.FormatStyle, A.FormatInput == Swift.Duration>(A) -> A.FormatOutput

## PredicateExpressions (98)

- `$s10Foundation20PredicateExpressionsO018CollectionContainsD0V4basexvg`  
  Foundation.PredicateExpressions.CollectionContainsCollection.base.getter : A
- `$s10Foundation20PredicateExpressionsO018CollectionContainsD0V5otherq_vg`  
  Foundation.PredicateExpressions.CollectionContainsCollection.other.getter : B
- `$s10Foundation20PredicateExpressionsO018CollectionContainsD0VMn`  
  nominal type descriptor for Foundation.PredicateExpressions.CollectionContainsCollection
- `$s10Foundation20PredicateExpressionsO10ArithmeticV2opAC0D8OperatorOvg`  
  Foundation.PredicateExpressions.Arithmetic.op.getter : Foundation.PredicateExpressions.ArithmeticOperator
- `$s10Foundation20PredicateExpressionsO10ArithmeticV3lhsxvg`  
  Foundation.PredicateExpressions.Arithmetic.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO10ArithmeticV3rhsq_vg`  
  Foundation.PredicateExpressions.Arithmetic.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO10ArithmeticVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Arithmetic
- `$s10Foundation20PredicateExpressionsO10ComparisonV2opAC0D8OperatorOvg`  
  Foundation.PredicateExpressions.Comparison.op.getter : Foundation.PredicateExpressions.ComparisonOperator
- `$s10Foundation20PredicateExpressionsO10ComparisonV3lhsxvg`  
  Foundation.PredicateExpressions.Comparison.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO10ComparisonV3rhsq_vg`  
  Foundation.PredicateExpressions.Comparison.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO10ComparisonVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Comparison
- `$s10Foundation20PredicateExpressionsO10NilLiteralVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.NilLiteral
- `$s10Foundation20PredicateExpressionsO10UnaryMinusV7wrappedxvg`  
  Foundation.PredicateExpressions.UnaryMinus.wrapped.getter : A
- `$s10Foundation20PredicateExpressionsO10UnaryMinusVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.UnaryMinus
- `$s10Foundation20PredicateExpressionsO11ConditionalV10trueBranchq_vg`  
  Foundation.PredicateExpressions.Conditional.trueBranch.getter : B
- `$s10Foundation20PredicateExpressionsO11ConditionalV11falseBranchq0_vg`  
  Foundation.PredicateExpressions.Conditional.falseBranch.getter : C
- `$s10Foundation20PredicateExpressionsO11ConditionalV4testxvg`  
  Foundation.PredicateExpressions.Conditional.test.getter : A
- `$s10Foundation20PredicateExpressionsO11ConditionalVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Conditional
- `$s10Foundation20PredicateExpressionsO11ConjunctionV3lhsxvg`  
  Foundation.PredicateExpressions.Conjunction.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO11ConjunctionV3rhsq_vg`  
  Foundation.PredicateExpressions.Conjunction.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO11ConjunctionVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Conjunction
- `$s10Foundation20PredicateExpressionsO11ConjunctionVy_xq_GAA08StandardB10ExpressionA2aGRzAaGR_rlMc`  
  protocol conformance descriptor for < where A: Foundation.StandardPredicateExpression, B: Foundation.StandardPredicateExpression> Foundation.PredicateExpressions.Conjunction<A, B> : Foundation.StandardPredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO11DisjunctionV3lhsxvg`  
  Foundation.PredicateExpressions.Disjunction.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO11DisjunctionV3rhsq_vg`  
  Foundation.PredicateExpressions.Disjunction.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO11DisjunctionVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Disjunction
- `$s10Foundation20PredicateExpressionsO11IntDivisionV3lhsxvg`  
  Foundation.PredicateExpressions.IntDivision.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO11IntDivisionV3rhsq_vg`  
  Foundation.PredicateExpressions.IntDivision.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO11IntDivisionVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.IntDivision
- `$s10Foundation20PredicateExpressionsO11NilCoalesceV3lhsxvg`  
  Foundation.PredicateExpressions.NilCoalesce.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO11NilCoalesceV3rhsq_vg`  
  Foundation.PredicateExpressions.NilCoalesce.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO11NilCoalesceVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.NilCoalesce
- `$s10Foundation20PredicateExpressionsO11build_Equal3lhs3rhsAC0E0Vy_xq_Gx_q_tAA0B10ExpressionRzAaJR_SQ6OutputRpzAKQy_ALRSr0_lFZ`  
  static Foundation.PredicateExpressions.build_Equal<A, B where A: Foundation.PredicateExpression, B: Foundation.PredicateExpression, A.Output: Swift.Equatable, A.Output == B.Output>(lhs: A, rhs: B) -> Foundation.PredicateExpressions.Equal<A, B>
- `$s10Foundation20PredicateExpressionsO12ForcedUnwrapV5innerxvg`  
  Foundation.PredicateExpressions.ForcedUnwrap.inner.getter : A
- `$s10Foundation20PredicateExpressionsO12ForcedUnwrapVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.ForcedUnwrap
- `$s10Foundation20PredicateExpressionsO12IntRemainderV3lhsxvg`  
  Foundation.PredicateExpressions.IntRemainder.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO12IntRemainderV3rhsq_vg`  
  Foundation.PredicateExpressions.IntRemainder.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO12IntRemainderVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.IntRemainder
- `$s10Foundation20PredicateExpressionsO13FloatDivisionV3lhsxvg`  
  Foundation.PredicateExpressions.FloatDivision.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO13FloatDivisionV3rhsq_vg`  
  Foundation.PredicateExpressions.FloatDivision.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO13FloatDivisionVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.FloatDivision
- `$s10Foundation20PredicateExpressionsO13build_KeyPath4root03keyF0AC0eF0Vy_xq_Gx_sAGCy6OutputQzq_GtAA0B10ExpressionRzr0_lFZ`  
  static Foundation.PredicateExpressions.build_KeyPath<A, B where A: Foundation.PredicateExpression>(root: A, keyPath: Swift.KeyPath<A.Output, B>) -> Foundation.PredicateExpressions.KeyPath<A, B>
- `$s10Foundation20PredicateExpressionsO14build_containsyAC16SequenceContainsVy_xq_Gx_q_tAA0B10ExpressionRzAaHR_ST6OutputRpzSQAIRp_AI_7ElementQZAKRSr0_lFZ`  
  static Foundation.PredicateExpressions.build_contains<A, B where A: Foundation.PredicateExpression, B: Foundation.PredicateExpression, A.Output: Swift.Sequence, B.Output: Swift.Equatable, B.Output == A.Output.Element>(A, B) -> Foundation.PredicateExpressions.SequenceContains<A, B>
- `$s10Foundation20PredicateExpressionsO15ConditionalCastV5inputxvg`  
  Foundation.PredicateExpressions.ConditionalCast.input.getter : A
- `$s10Foundation20PredicateExpressionsO15ConditionalCastVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.ConditionalCast
- `$s10Foundation20PredicateExpressionsO16SequenceContainsV7elementq_vg`  
  Foundation.PredicateExpressions.SequenceContains.element.getter : B
- `$s10Foundation20PredicateExpressionsO16SequenceContainsV8sequencexvg`  
  Foundation.PredicateExpressions.SequenceContains.sequence.getter : A
- `$s10Foundation20PredicateExpressionsO16SequenceContainsVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.SequenceContains
- `$s10Foundation20PredicateExpressionsO16SequenceContainsVy_xq_GAA08StandardB10ExpressionA2aGRzAaGR_rlMc`  
  protocol conformance descriptor for < where A: Foundation.StandardPredicateExpression, B: Foundation.StandardPredicateExpression> Foundation.PredicateExpressions.SequenceContains<A, B> : Foundation.StandardPredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO17build_Conjunction3lhs3rhsAC0E0Vy_xq_Gx_q_tAA0B10ExpressionRzAaJR_Sb6OutputRtzSbAKRt_r0_lFZ`  
  static Foundation.PredicateExpressions.build_Conjunction<A, B where A: Foundation.PredicateExpression, B: Foundation.PredicateExpression, A.Output == Swift.Bool, B.Output == Swift.Bool>(lhs: A, rhs: B) -> Foundation.PredicateExpressions.Conjunction<A, B>
- `$s10Foundation20PredicateExpressionsO18ArithmeticOperatorO3addyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ArithmeticOperator.add(Foundation.PredicateExpressions.ArithmeticOperator.Type) -> Foundation.PredicateExpressions.ArithmeticOperator
- `$s10Foundation20PredicateExpressionsO18ArithmeticOperatorO8multiplyyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ArithmeticOperator.multiply(Foundation.PredicateExpressions.ArithmeticOperator.Type) -> Foundation.PredicateExpressions.ArithmeticOperator
- `$s10Foundation20PredicateExpressionsO18ArithmeticOperatorO8subtractyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ArithmeticOperator.subtract(Foundation.PredicateExpressions.ArithmeticOperator.Type) -> Foundation.PredicateExpressions.ArithmeticOperator
- `$s10Foundation20PredicateExpressionsO18ArithmeticOperatorOMa`  
  type metadata accessor for Foundation.PredicateExpressions.ArithmeticOperator
- `$s10Foundation20PredicateExpressionsO18ComparisonOperatorO11greaterThanyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ComparisonOperator.greaterThan(Foundation.PredicateExpressions.ComparisonOperator.Type) -> Foundation.PredicateExpressions.ComparisonOperator
- `$s10Foundation20PredicateExpressionsO18ComparisonOperatorO15lessThanOrEqualyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ComparisonOperator.lessThanOrEqual(Foundation.PredicateExpressions.ComparisonOperator.Type) -> Foundation.PredicateExpressions.ComparisonOperator
- `$s10Foundation20PredicateExpressionsO18ComparisonOperatorO18greaterThanOrEqualyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ComparisonOperator.greaterThanOrEqual(Foundation.PredicateExpressions.ComparisonOperator.Type) -> Foundation.PredicateExpressions.ComparisonOperator
- `$s10Foundation20PredicateExpressionsO18ComparisonOperatorO8lessThanyA2EmFWC`  
  enum case for Foundation.PredicateExpressions.ComparisonOperator.lessThan(Foundation.PredicateExpressions.ComparisonOperator.Type) -> Foundation.PredicateExpressions.ComparisonOperator
- `$s10Foundation20PredicateExpressionsO18ComparisonOperatorOMa`  
  type metadata accessor for Foundation.PredicateExpressions.ComparisonOperator
- `$s10Foundation20PredicateExpressionsO18SequenceStartsWithV4basexvg`  
  Foundation.PredicateExpressions.SequenceStartsWith.base.getter : A
- `$s10Foundation20PredicateExpressionsO18SequenceStartsWithV6prefixq_vg`  
  Foundation.PredicateExpressions.SequenceStartsWith.prefix.getter : B
- `$s10Foundation20PredicateExpressionsO18SequenceStartsWithVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.SequenceStartsWith
- `$s10Foundation20PredicateExpressionsO22StringLocalizedCompareV4rootxvg`  
  Foundation.PredicateExpressions.StringLocalizedCompare.root.getter : A
- `$s10Foundation20PredicateExpressionsO22StringLocalizedCompareV5otherq_vg`  
  Foundation.PredicateExpressions.StringLocalizedCompare.other.getter : B
- `$s10Foundation20PredicateExpressionsO22StringLocalizedCompareVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.StringLocalizedCompare
- `$s10Foundation20PredicateExpressionsO23RangeExpressionContainsV5rangexvg`  
  Foundation.PredicateExpressions.RangeExpressionContains.range.getter : A
- `$s10Foundation20PredicateExpressionsO23RangeExpressionContainsV7elementq_vg`  
  Foundation.PredicateExpressions.RangeExpressionContains.element.getter : B
- `$s10Foundation20PredicateExpressionsO23RangeExpressionContainsVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.RangeExpressionContains
- `$s10Foundation20PredicateExpressionsO28StringCaseInsensitiveCompareV4rootxvg`  
  Foundation.PredicateExpressions.StringCaseInsensitiveCompare.root.getter : A
- `$s10Foundation20PredicateExpressionsO28StringCaseInsensitiveCompareV5otherq_vg`  
  Foundation.PredicateExpressions.StringCaseInsensitiveCompare.other.getter : B
- `$s10Foundation20PredicateExpressionsO28StringCaseInsensitiveCompareVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.StringCaseInsensitiveCompare
- `$s10Foundation20PredicateExpressionsO31StringLocalizedStandardContainsV4rootxvg`  
  Foundation.PredicateExpressions.StringLocalizedStandardContains.root.getter : A
- `$s10Foundation20PredicateExpressionsO31StringLocalizedStandardContainsV5otherq_vg`  
  Foundation.PredicateExpressions.StringLocalizedStandardContains.other.getter : B
- `$s10Foundation20PredicateExpressionsO31StringLocalizedStandardContainsVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.StringLocalizedStandardContains
- `$s10Foundation20PredicateExpressionsO5EqualV3lhsxvg`  
  Foundation.PredicateExpressions.Equal.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO5EqualV3rhsq_vg`  
  Foundation.PredicateExpressions.Equal.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO5EqualVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Equal
- `$s10Foundation20PredicateExpressionsO5EqualVy_xq_GAA08StandardB10ExpressionA2aGRzAaGR_rlMc`  
  protocol conformance descriptor for < where A: Foundation.StandardPredicateExpression, B: Foundation.StandardPredicateExpression> Foundation.PredicateExpressions.Equal<A, B> : Foundation.StandardPredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO5EqualVy_xq_GAA0B10ExpressionAAMc`  
  protocol conformance descriptor for Foundation.PredicateExpressions.Equal<A, B> : Foundation.PredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO5ValueVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Value
- `$s10Foundation20PredicateExpressionsO5ValueVy_xGAA08StandardB10ExpressionAASeRzSERzlMc`  
  protocol conformance descriptor for <A where A: Swift.Decodable, A: Swift.Encodable> Foundation.PredicateExpressions.Value<A> : Foundation.StandardPredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO5ValueVy_xGAA0B10ExpressionAAMc`  
  protocol conformance descriptor for Foundation.PredicateExpressions.Value<A> : Foundation.PredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO7KeyPathV03keyE0s8Sendable_sADCy6OutputQzq_GXcvg`  
  Foundation.PredicateExpressions.KeyPath.keyPath.getter : Swift.KeyPath<A.Output, B> & Swift.Sendable
- `$s10Foundation20PredicateExpressionsO7KeyPathV8evaluateyq_AA0B8BindingsVKF`  
  Foundation.PredicateExpressions.KeyPath.evaluate(Foundation.PredicateBindings) throws -> B
- `$s10Foundation20PredicateExpressionsO7KeyPathVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.KeyPath
- `$s10Foundation20PredicateExpressionsO7KeyPathVy_xq_GAA08StandardB10ExpressionA2aGRzrlMc`  
  protocol conformance descriptor for < where A: Foundation.StandardPredicateExpression> Foundation.PredicateExpressions.KeyPath<A, B> : Foundation.StandardPredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO7KeyPathVy_xq_GAA0B10ExpressionAAMc`  
  protocol conformance descriptor for Foundation.PredicateExpressions.KeyPath<A, B> : Foundation.PredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO8NegationV7wrappedxvg`  
  Foundation.PredicateExpressions.Negation.wrapped.getter : A
- `$s10Foundation20PredicateExpressionsO8NegationVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Negation
- `$s10Foundation20PredicateExpressionsO8NotEqualV3lhsxvg`  
  Foundation.PredicateExpressions.NotEqual.lhs.getter : A
- `$s10Foundation20PredicateExpressionsO8NotEqualV3rhsq_vg`  
  Foundation.PredicateExpressions.NotEqual.rhs.getter : B
- `$s10Foundation20PredicateExpressionsO8NotEqualVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.NotEqual
- `$s10Foundation20PredicateExpressionsO8VariableVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.Variable
- `$s10Foundation20PredicateExpressionsO8VariableVy_xGAA08StandardB10ExpressionAAMc`  
  protocol conformance descriptor for Foundation.PredicateExpressions.Variable<A> : Foundation.StandardPredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO8VariableVy_xGAA0B10ExpressionAAMc`  
  protocol conformance descriptor for Foundation.PredicateExpressions.Variable<A> : Foundation.PredicateExpression in Foundation
- `$s10Foundation20PredicateExpressionsO9ForceCastV5inputxvg`  
  Foundation.PredicateExpressions.ForceCast.input.getter : A
- `$s10Foundation20PredicateExpressionsO9ForceCastVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.ForceCast
- `$s10Foundation20PredicateExpressionsO9build_ArgyAC5ValueVy_xGxlFZ`  
  static Foundation.PredicateExpressions.build_Arg<A>(A) -> Foundation.PredicateExpressions.Value<A>
- `$s10Foundation20PredicateExpressionsO9build_ArgyxxAA0B10ExpressionRzlFZ`  
  static Foundation.PredicateExpressions.build_Arg<A where A: Foundation.PredicateExpression>(A) -> A

## Data (71)

- `$s10Foundation4DataV013base64EncodedB07optionsACSo27NSDataBase64EncodingOptionsV_tF`  
  Foundation.Data.base64EncodedData(options: __C.NSDataBase64EncodingOptions) -> Foundation.Data
- `$s10Foundation4DataV06InlineB0VyAESWcfC`  
  Foundation.Data.InlineData.init(Swift.UnsafeRawBufferPointer) -> Foundation.Data.InlineData
- `$s10Foundation4DataV10LargeSliceV21ensureUniqueReferenceyyF`  
  Foundation.Data.LargeSlice.ensureUniqueReference() -> ()
- `$s10Foundation4DataV10contentsOf7optionsAcA3URLVh_So20NSDataReadingOptionsVtKcfC`  
  Foundation.Data.init(contentsOf: __shared Foundation.URL, options: __C.NSDataReadingOptions) throws -> Foundation.Data
- `$s10Foundation4DataV11DeallocatorO12_deallocatoryySv_Sitcvg`  
  Foundation.Data.Deallocator._deallocator.getter : (Swift.UnsafeMutableRawPointer, Swift.Int) -> ()
- `$s10Foundation4DataV11DeallocatorO4freeyA2EmFWC`  
  enum case for Foundation.Data.Deallocator.free(Foundation.Data.Deallocator.Type) -> Foundation.Data.Deallocator
- `$s10Foundation4DataV11DeallocatorO4noneyA2EmFWC`  
  enum case for Foundation.Data.Deallocator.none(Foundation.Data.Deallocator.Type) -> Foundation.Data.Deallocator
- `$s10Foundation4DataV11DeallocatorO6customyAEySv_SitccAEmFWC`  
  enum case for Foundation.Data.Deallocator.custom(Foundation.Data.Deallocator.Type) -> ((Swift.UnsafeMutableRawPointer, Swift.Int) -> ()) -> Foundation.Data.Deallocator
- `$s10Foundation4DataV11DeallocatorOMa`  
  type metadata accessor for Foundation.Data.Deallocator
- `$s10Foundation4DataV11InlineSliceV21ensureUniqueReferenceyyF`  
  Foundation.Data.InlineSlice.ensureUniqueReference() -> ()
- `$s10Foundation4DataV11bytesNoCopy5count11deallocatorACSv_SiAC11DeallocatorOtcfC`  
  Foundation.Data.init(bytesNoCopy: Swift.UnsafeMutableRawPointer, count: Swift.Int, deallocator: Foundation.Data.Deallocator) -> Foundation.Data
- `$s10Foundation4DataV11descriptionSSvg`  
  Foundation.Data.description.getter : Swift.String
- `$s10Foundation4DataV12makeIteratorAC0D0VyF`  
  Foundation.Data.makeIterator() -> Foundation.Data.Iterator
- `$s10Foundation4DataV13_copyContents12initializingAC8IteratorV_SitSrys5UInt8VG_tF`  
  Foundation.Data._copyContents(initializing: Swift.UnsafeMutableBufferPointer<Swift.UInt8>) -> (Foundation.Data.Iterator, Swift.Int)
- `$s10Foundation4DataV13base64Encoded7optionsACSgACh_So27NSDataBase64DecodingOptionsVtcfC`  
  Foundation.Data.init(base64Encoded: __shared Foundation.Data, options: __C.NSDataBase64DecodingOptions) -> Foundation.Data?
- `$s10Foundation4DataV13base64Encoded7optionsACSgSSh_So27NSDataBase64DecodingOptionsVtcfC`  
  Foundation.Data.init(base64Encoded: __shared Swift.String, options: __C.NSDataBase64DecodingOptions) -> Foundation.Data?
- `$s10Foundation4DataV15_RepresentationO10resetBytes2inySnySiG_tF`  
  Foundation.Data._Representation.resetBytes(in: Swift.Range<Swift.Int>) -> ()
- `$s10Foundation4DataV15_RepresentationO15replaceSubrange_4with5countySnySiG_SVSgSitF`  
  Foundation.Data._Representation.replaceSubrange(_: Swift.Range<Swift.Int>, with: Swift.UnsafeRawPointer?, count: Swift.Int) -> ()
- `$s10Foundation4DataV15_RepresentationO15reserveCapacityyySiF`  
  Foundation.Data._Representation.reserveCapacity(Swift.Int) -> ()
- `$s10Foundation4DataV15_RepresentationO22withUnsafeMutableBytesyxxSwKXEKlF`  
  Foundation.Data._Representation.withUnsafeMutableBytes<A>((Swift.UnsafeMutableRawBufferPointer) throws -> A) throws -> A
- `$s10Foundation4DataV15_RepresentationO5countAESi_tcfC`  
  Foundation.Data._Representation.init(count: Swift.Int) -> Foundation.Data._Representation
- `$s10Foundation4DataV15_RepresentationO5countSivs`  
  Foundation.Data._Representation.count.setter : Swift.Int
- `$s10Foundation4DataV15_RepresentationO8capacityAESi_tcfC`  
  Foundation.Data._Representation.init(capacity: Swift.Int) -> Foundation.Data._Representation
- `$s10Foundation4DataV15_RepresentationON`  
  type metadata for Foundation.Data._Representation
- `$s10Foundation4DataV15_RepresentationO_5countAeA02__B7StorageC_SitcfC`  
  Foundation.Data._Representation.init(_: Foundation.__DataStorage, count: Swift.Int) -> Foundation.Data._Representation
- `$s10Foundation4DataV15_RepresentationOyACSnySiGcig`  
  Foundation.Data._Representation.subscript.getter : (Swift.Range<Swift.Int>) -> Foundation.Data
- `$s10Foundation4DataV15_RepresentationOyAESWcfC`  
  Foundation.Data._Representation.init(Swift.UnsafeRawBufferPointer) -> Foundation.Data._Representation
- `$s10Foundation4DataV15_RepresentationOys5UInt8VSicig`  
  Foundation.Data._Representation.subscript.getter : (Swift.Int) -> Swift.UInt8
- `$s10Foundation4DataV15_RepresentationOys5UInt8VSicis`  
  Foundation.Data._Representation.subscript.setter : (Swift.Int) -> Swift.UInt8
- `$s10Foundation4DataV15withUnsafeBytesyxxSWKXEKlF`  
  Foundation.Data.withUnsafeBytes<A>((Swift.UnsafeRawBufferPointer) throws -> A) throws -> A
- `$s10Foundation4DataV16debugDescriptionSSvg`  
  Foundation.Data.debugDescription.getter : Swift.String
- `$s10Foundation4DataV19base64EncodedString7optionsSSSo27NSDataBase64EncodingOptionsV_tF`  
  Foundation.Data.base64EncodedString(options: __C.NSDataBase64EncodingOptions) -> Swift.String
- `$s10Foundation4DataV22withUnsafeMutableBytesyxxSwKXEKlF`  
  Foundation.Data.withUnsafeMutableBytes<A>((Swift.UnsafeMutableRawBufferPointer) throws -> A) throws -> A
- `$s10Foundation4DataV2eeoiySbAC_ACtFZ`  
  static Foundation.Data.== infix(Foundation.Data, Foundation.Data) -> Swift.Bool
- `$s10Foundation4DataV4hash4intoys6HasherVz_tF`  
  Foundation.Data.hash(into: inout Swift.Hasher) -> ()
- `$s10Foundation4DataV5countACSi_tcfC`  
  Foundation.Data.init(count: Swift.Int) -> Foundation.Data
- `$s10Foundation4DataV5countSivg`  
  Foundation.Data.count.getter : Swift.Int
- `$s10Foundation4DataV5range2of7options2inSnySiGSgAC_So19NSDataSearchOptionsVAItF`  
  Foundation.Data.range(of: Foundation.Data, options: __C.NSDataSearchOptions, in: Swift.Range<Swift.Int>?) -> Swift.Range<Swift.Int>?
- `$s10Foundation4DataV5write2to7optionsyAA3URLV_So20NSDataWritingOptionsVtKF`  
  Foundation.Data.write(to: Foundation.URL, options: __C.NSDataWritingOptions) throws -> ()
- `$s10Foundation4DataV6append10contentsOfySays5UInt8VG_tF`  
  Foundation.Data.append(contentsOf: [Swift.UInt8]) -> ()
- `$s10Foundation4DataV6append10contentsOfyx_tSTRzs5UInt8V7ElementRtzlF`  
  Foundation.Data.append<A where A: Swift.Sequence, A.Element == Swift.UInt8>(contentsOf: A) -> ()
- `$s10Foundation4DataV6append_5countySPys5UInt8VG_SitF`  
  Foundation.Data.append(_: Swift.UnsafePointer<Swift.UInt8>, count: Swift.Int) -> ()
- `$s10Foundation4DataV6appendyyACF`  
  Foundation.Data.append(Foundation.Data) -> ()
- `$s10Foundation4DataV6appendyySRyxGlF`  
  Foundation.Data.append<A>(Swift.UnsafeBufferPointer<A>) -> ()
- `$s10Foundation4DataV6bufferACSRyxG_tclufC`  
  Foundation.Data.init<A>(buffer: Swift.UnsafeBufferPointer<A>) -> Foundation.Data
- `$s10Foundation4DataV7subdata2inACSnySiG_tF`  
  Foundation.Data.subdata(in: Swift.Range<Swift.Int>) -> Foundation.Data
- `$s10Foundation4DataV8IteratorV4nexts5UInt8VSgyF`  
  Foundation.Data.Iterator.next() -> Swift.UInt8?
- `$s10Foundation4DataV8IteratorVMa`  
  type metadata accessor for Foundation.Data.Iterator
- `$s10Foundation4DataV8IteratorVStAAMc`  
  protocol conformance descriptor for Foundation.Data.Iterator : Swift.IteratorProtocol in Foundation
- `$s10Foundation4DataV8IteratorV_2atAeC_SitcfC`  
  Foundation.Data.Iterator.init(_: Foundation.Data, at: Swift.Int) -> Foundation.Data.Iterator
- `$s10Foundation4DataV8endIndexSivg`  
  Foundation.Data.endIndex.getter : Swift.Int
- `$s10Foundation4DataV9copyBytes2to5countySpys5UInt8VG_SitF`  
  Foundation.Data.copyBytes(to: Swift.UnsafeMutablePointer<Swift.UInt8>, count: Swift.Int) -> ()
- `$s10Foundation4DataV9hashValueSivg`  
  Foundation.Data.hashValue.getter : Swift.Int
- `$s10Foundation4DataV9repeating5countACs5UInt8V_SitcfC`  
  Foundation.Data.init(repeating: Swift.UInt8, count: Swift.Int) -> Foundation.Data
- `$s10Foundation4DataVAA0B8ProtocolAAMc`  
  protocol conformance descriptor for Foundation.Data : Foundation.DataProtocol in Foundation
- `$s10Foundation4DataVAA15ContiguousBytesAAWP`  
  protocol witness table for Foundation.Data : Foundation.ContiguousBytes in Foundation
- `$s10Foundation4DataVMn`  
  nominal type descriptor for Foundation.Data
- `$s10Foundation4DataVN`  
  type metadata for Foundation.Data
- `$s10Foundation4DataVSEAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.Encodable in Foundation
- `$s10Foundation4DataVSHAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.Hashable in Foundation
- `$s10Foundation4DataVSKAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.BidirectionalCollection in Foundation
- `$s10Foundation4DataVSQAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.Equatable in Foundation
- `$s10Foundation4DataVSTAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.Sequence in Foundation
- `$s10Foundation4DataVSeAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.Decodable in Foundation
- `$s10Foundation4DataVSlAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.Collection in Foundation
- `$s10Foundation4DataVSmAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.RangeReplaceableCollection in Foundation
- `$s10Foundation4DataVs21_ObjectiveCBridgeableAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift._ObjectiveCBridgeable in Foundation
- `$s10Foundation4DataVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for Foundation.Data : Swift.CustomStringConvertible in Foundation
- `$s10Foundation4DataVyACxcSTRzs5UInt8V7ElementRtzlufC`  
  Foundation.Data.init<A where A: Swift.Sequence, A.Element == Swift.UInt8>(A) -> Foundation.Data
- `$s10Foundation4DataVyACxcSXRzs17FixedWidthInteger5BoundRpzluig`  
  Foundation.Data.subscript.getter : <A where A: Swift.RangeExpression, A.Bound: Swift.FixedWidthInteger>(A) -> Foundation.Data
- `$s10Foundation4DataVyACxcSXRzs17FixedWidthInteger5BoundRpzluis`  
  Foundation.Data.subscript.setter : <A where A: Swift.RangeExpression, A.Bound: Swift.FixedWidthInteger>(A) -> Foundation.Data

## Calendar (61)

- `$s10Foundation8CalendarV10IdentifierO7iso8601yA2EmFWC`  
  enum case for Foundation.Calendar.Identifier.iso8601(Foundation.Calendar.Identifier.Type) -> Foundation.Calendar.Identifier
- `$s10Foundation8CalendarV10IdentifierO9gregorianyA2EmFWC`  
  enum case for Foundation.Calendar.Identifier.gregorian(Foundation.Calendar.Identifier.Type) -> Foundation.Calendar.Identifier
- `$s10Foundation8CalendarV10IdentifierOMa`  
  type metadata accessor for Foundation.Calendar.Identifier
- `$s10Foundation8CalendarV10identifierA2C10IdentifierOh_tcfC`  
  Foundation.Calendar.init(identifier: __shared Foundation.Calendar.Identifier) -> Foundation.Calendar
- `$s10Foundation8CalendarV10startOfDay3forAA4DateVAG_tF`  
  Foundation.Calendar.startOfDay(for: Foundation.Date) -> Foundation.Date
- `$s10Foundation8CalendarV12dateInterval2of3forAA04DateD0VSgAC9ComponentO_AA0G0VtF`  
  Foundation.Calendar.dateInterval(of: Foundation.Calendar.Component, for: Foundation.Date) -> Foundation.DateInterval?
- `$s10Foundation8CalendarV12dateInterval2of5start8interval3forSbAC9ComponentO_AA4DateVzSdzALtF`  
  Foundation.Calendar.dateInterval(of: Foundation.Calendar.Component, start: inout Foundation.Date, interval: inout Swift.Double, for: Foundation.Date) -> Swift.Bool
- `$s10Foundation8CalendarV12firstWeekdaySivg`  
  Foundation.Calendar.firstWeekday.getter : Swift.Int
- `$s10Foundation8CalendarV12firstWeekdaySivs`  
  Foundation.Calendar.firstWeekday.setter : Swift.Int
- `$s10Foundation8CalendarV13isDateInTodayySbAA0D0VF`  
  Foundation.Calendar.isDateInToday(Foundation.Date) -> Swift.Bool
- `$s10Foundation8CalendarV14MatchingPolicyO35nextTimePreservingSmallerComponentsyA2EmFWC`  
  enum case for Foundation.Calendar.MatchingPolicy.nextTimePreservingSmallerComponents(Foundation.Calendar.MatchingPolicy.Type) -> Foundation.Calendar.MatchingPolicy
- `$s10Foundation8CalendarV14MatchingPolicyO8nextTimeyA2EmFWC`  
  enum case for Foundation.Calendar.MatchingPolicy.nextTime(Foundation.Calendar.MatchingPolicy.Type) -> Foundation.Calendar.MatchingPolicy
- `$s10Foundation8CalendarV14MatchingPolicyOMa`  
  type metadata accessor for Foundation.Calendar.MatchingPolicy
- `$s10Foundation8CalendarV14dateComponents2in4fromAA04DateD0VAA8TimeZoneV_AA0G0VtF`  
  Foundation.Calendar.dateComponents(in: Foundation.TimeZone, from: Foundation.Date) -> Foundation.DateComponents
- `$s10Foundation8CalendarV14dateComponents_4from2toAA04DateD0VShyAC9ComponentOG_A2HtF`  
  Foundation.Calendar.dateComponents(_: Swift.Set<Foundation.Calendar.Component>, from: Foundation.DateComponents, to: Foundation.DateComponents) -> Foundation.DateComponents
- `$s10Foundation8CalendarV14dateComponents_4from2toAA04DateD0VShyAC9ComponentOG_AA0G0VAMtF`  
  Foundation.Calendar.dateComponents(_: Swift.Set<Foundation.Calendar.Component>, from: Foundation.Date, to: Foundation.Date) -> Foundation.DateComponents
- `$s10Foundation8CalendarV14dateComponents_4fromAA04DateD0VShyAC9ComponentOG_AA0F0VtF`  
  Foundation.Calendar.dateComponents(_: Swift.Set<Foundation.Calendar.Component>, from: Foundation.Date) -> Foundation.DateComponents
- `$s10Foundation8CalendarV15SearchDirectionO7forwardyA2EmFWC`  
  enum case for Foundation.Calendar.SearchDirection.forward(Foundation.Calendar.SearchDirection.Type) -> Foundation.Calendar.SearchDirection
- `$s10Foundation8CalendarV15SearchDirectionOMa`  
  type metadata accessor for Foundation.Calendar.SearchDirection
- `$s10Foundation8CalendarV16isDateInTomorrowySbAA0D0VF`  
  Foundation.Calendar.isDateInTomorrow(Foundation.Date) -> Swift.Bool
- `$s10Foundation8CalendarV17isDateInYesterdayySbAA0D0VF`  
  Foundation.Calendar.isDateInYesterday(Foundation.Date) -> Swift.Bool
- `$s10Foundation8CalendarV18RepeatedTimePolicyO5firstyA2EmFWC`  
  enum case for Foundation.Calendar.RepeatedTimePolicy.first(Foundation.Calendar.RepeatedTimePolicy.Type) -> Foundation.Calendar.RepeatedTimePolicy
- `$s10Foundation8CalendarV18RepeatedTimePolicyOMa`  
  type metadata accessor for Foundation.Calendar.RepeatedTimePolicy
- `$s10Foundation8CalendarV19_bridgeToObjectiveCSo10NSCalendarCyF`  
  Foundation.Calendar._bridgeToObjectiveC() -> __C.NSCalendar
- `$s10Foundation8CalendarV19autoupdatingCurrentACvgZ`  
  static Foundation.Calendar.autoupdatingCurrent.getter : Foundation.Calendar
- `$s10Foundation8CalendarV24standaloneWeekdaySymbolsSaySSGvg`  
  Foundation.Calendar.standaloneWeekdaySymbols.getter : [Swift.String]
- `$s10Foundation8CalendarV29shortStandaloneWeekdaySymbolsSaySSGvg`  
  Foundation.Calendar.shortStandaloneWeekdaySymbols.getter : [Swift.String]
- `$s10Foundation8CalendarV33veryShortStandaloneWeekdaySymbolsSaySSGvg`  
  Foundation.Calendar.veryShortStandaloneWeekdaySymbols.getter : [Swift.String]
- `$s10Foundation8CalendarV36_unconditionallyBridgeFromObjectiveCyACSo10NSCalendarCSgFZ`  
  static Foundation.Calendar._unconditionallyBridgeFromObjectiveC(__C.NSCalendar?) -> Foundation.Calendar
- `$s10Foundation8CalendarV4date13bySettingHour6minute6second2of14matchingPolicy012repeatedTimeK09directionAA4DateVSgSi_S2iAmC08MatchingK0OAC08RepeatedmK0OAC15SearchDirectionOtF`  
  Foundation.Calendar.date(bySettingHour: Swift.Int, minute: Swift.Int, second: Swift.Int, of: Foundation.Date, matchingPolicy: Foundation.Calendar.MatchingPolicy, repeatedTimePolicy: Foundation.Calendar.RepeatedTimePolicy, direction: Foundation.Calendar.SearchDirection) -> Foundation.Date?
- `$s10Foundation8CalendarV4date4fromAA4DateVSgAA0E10ComponentsV_tF`  
  Foundation.Calendar.date(from: Foundation.DateComponents) -> Foundation.Date?
- `$s10Foundation8CalendarV4date8byAdding2to18wrappingComponentsAA4DateVSgAA0iH0V_AISbtF`  
  Foundation.Calendar.date(byAdding: Foundation.DateComponents, to: Foundation.Date, wrappingComponents: Swift.Bool) -> Foundation.Date?
- `$s10Foundation8CalendarV4date8byAdding5value2to18wrappingComponentsAA4DateVSgAC9ComponentO_SiAJSbtF`  
  Foundation.Calendar.date(byAdding: Foundation.Calendar.Component, value: Swift.Int, to: Foundation.Date, wrappingComponents: Swift.Bool) -> Foundation.Date?
- `$s10Foundation8CalendarV5range2of2in3forSnySiGSgAC9ComponentO_AkA4DateVtF`  
  Foundation.Calendar.range(of: Foundation.Calendar.Component, in: Foundation.Calendar.Component, for: Foundation.Date) -> Swift.Range<Swift.Int>?
- `$s10Foundation8CalendarV6isDate_11inSameDayAsSbAA0D0V_AGtF`  
  Foundation.Calendar.isDate(_: Foundation.Date, inSameDayAs: Foundation.Date) -> Swift.Bool
- `$s10Foundation8CalendarV6isDate_7equalTo13toGranularitySbAA0D0V_AhC9ComponentOtF`  
  Foundation.Calendar.isDate(_: Foundation.Date, equalTo: Foundation.Date, toGranularity: Foundation.Calendar.Component) -> Swift.Bool
- `$s10Foundation8CalendarV6localeAA6LocaleVSgvg`  
  Foundation.Calendar.locale.getter : Foundation.Locale?
- `$s10Foundation8CalendarV6localeAA6LocaleVSgvs`  
  Foundation.Calendar.locale.setter : Foundation.Locale?
- `$s10Foundation8CalendarV7compare_2to0D11GranularitySo18NSComparisonResultVAA4DateV_AjC9ComponentOtF`  
  Foundation.Calendar.compare(_: Foundation.Date, to: Foundation.Date, toGranularity: Foundation.Calendar.Component) -> __C.NSComparisonResult
- `$s10Foundation8CalendarV7currentACvgZ`  
  static Foundation.Calendar.current.getter : Foundation.Calendar
- `$s10Foundation8CalendarV8nextDate5after8matching0F6Policy012repeatedTimeG09directionAA0D0VSgAK_AA0D10ComponentsVAC08MatchingG0OAC08RepeatediG0OAC15SearchDirectionOtF`  
  Foundation.Calendar.nextDate(after: Foundation.Date, matching: Foundation.DateComponents, matchingPolicy: Foundation.Calendar.MatchingPolicy, repeatedTimePolicy: Foundation.Calendar.RepeatedTimePolicy, direction: Foundation.Calendar.SearchDirection) -> Foundation.Date?
- `$s10Foundation8CalendarV8timeZoneAA04TimeD0Vvg`  
  Foundation.Calendar.timeZone.getter : Foundation.TimeZone
- `$s10Foundation8CalendarV8timeZoneAA04TimeD0Vvs`  
  Foundation.Calendar.timeZone.setter : Foundation.TimeZone
- `$s10Foundation8CalendarV9ComponentO10nanosecondyA2EmFWC`  
  enum case for Foundation.Calendar.Component.nanosecond(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO10weekOfYearyA2EmFWC`  
  enum case for Foundation.Calendar.Component.weekOfYear(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO3dayyA2EmFWC`  
  enum case for Foundation.Calendar.Component.day(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO4houryA2EmFWC`  
  enum case for Foundation.Calendar.Component.hour(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO4yearyA2EmFWC`  
  enum case for Foundation.Calendar.Component.year(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO5monthyA2EmFWC`  
  enum case for Foundation.Calendar.Component.month(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO6minuteyA2EmFWC`  
  enum case for Foundation.Calendar.Component.minute(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO6secondyA2EmFWC`  
  enum case for Foundation.Calendar.Component.second(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO7weekdayyA2EmFWC`  
  enum case for Foundation.Calendar.Component.weekday(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO8calendaryA2EmFWC`  
  enum case for Foundation.Calendar.Component.calendar(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentO8timeZoneyA2EmFWC`  
  enum case for Foundation.Calendar.Component.timeZone(Foundation.Calendar.Component.Type) -> Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentOMa`  
  type metadata accessor for Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentOMn`  
  nominal type descriptor for Foundation.Calendar.Component
- `$s10Foundation8CalendarV9ComponentOSHAAMc`  
  protocol conformance descriptor for Foundation.Calendar.Component : Swift.Hashable in Foundation
- `$s10Foundation8CalendarV9ComponentOSQAAMc`  
  protocol conformance descriptor for Foundation.Calendar.Component : Swift.Equatable in Foundation
- `$s10Foundation8CalendarV9component_4fromSiAC9ComponentO_AA4DateVtF`  
  Foundation.Calendar.component(_: Foundation.Calendar.Component, from: Foundation.Date) -> Swift.Int
- `$s10Foundation8CalendarVMa`  
  type metadata accessor for Foundation.Calendar
- `$s10Foundation8CalendarVMn`  
  nominal type descriptor for Foundation.Calendar

## JSONEncoder (40)

- `$s10Foundation11JSONEncoderC16OutputFormattingV10sortedKeysAEvgZ`  
  static Foundation.JSONEncoder.OutputFormatting.sortedKeys.getter : Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC16OutputFormattingV13prettyPrintedAEvgZ`  
  static Foundation.JSONEncoder.OutputFormatting.prettyPrinted.getter : Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC16OutputFormattingV22withoutEscapingSlashesAEvgZ`  
  static Foundation.JSONEncoder.OutputFormatting.withoutEscapingSlashes.getter : Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC16OutputFormattingVMa`  
  type metadata accessor for Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC16OutputFormattingVMn`  
  nominal type descriptor for Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC16OutputFormattingVSQAAMc`  
  protocol conformance descriptor for Foundation.JSONEncoder.OutputFormatting : Swift.Equatable in Foundation
- `$s10Foundation11JSONEncoderC16OutputFormattingVs10SetAlgebraAAMc`  
  protocol conformance descriptor for Foundation.JSONEncoder.OutputFormatting : Swift.SetAlgebra in Foundation
- `$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvMTj`  
  dispatch thunk of Foundation.JSONEncoder.outputFormatting.modify : Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvsTj`  
  dispatch thunk of Foundation.JSONEncoder.outputFormatting.setter : Foundation.JSONEncoder.OutputFormatting
- `$s10Foundation11JSONEncoderC19KeyEncodingStrategyO14useDefaultKeysyA2EmFWC`  
  enum case for Foundation.JSONEncoder.KeyEncodingStrategy.useDefaultKeys(Foundation.JSONEncoder.KeyEncodingStrategy.Type) -> Foundation.JSONEncoder.KeyEncodingStrategy
- `$s10Foundation11JSONEncoderC19KeyEncodingStrategyO18convertToSnakeCaseyA2EmFWC`  
  enum case for Foundation.JSONEncoder.KeyEncodingStrategy.convertToSnakeCase(Foundation.JSONEncoder.KeyEncodingStrategy.Type) -> Foundation.JSONEncoder.KeyEncodingStrategy
- `$s10Foundation11JSONEncoderC19KeyEncodingStrategyOMa`  
  type metadata accessor for Foundation.JSONEncoder.KeyEncodingStrategy
- `$s10Foundation11JSONEncoderC19KeyEncodingStrategyOMn`  
  nominal type descriptor for Foundation.JSONEncoder.KeyEncodingStrategy
- `$s10Foundation11JSONEncoderC19keyEncodingStrategyAC03KeydE0OvsTj`  
  dispatch thunk of Foundation.JSONEncoder.keyEncodingStrategy.setter : Foundation.JSONEncoder.KeyEncodingStrategy
- `$s10Foundation11JSONEncoderC20DataEncodingStrategyO6base64yA2EmFWC`  
  enum case for Foundation.JSONEncoder.DataEncodingStrategy.base64(Foundation.JSONEncoder.DataEncodingStrategy.Type) -> Foundation.JSONEncoder.DataEncodingStrategy
- `$s10Foundation11JSONEncoderC20DataEncodingStrategyO6customyAEyAA0C0V_s7Encoder_ptKccAEmFWC`  
  enum case for Foundation.JSONEncoder.DataEncodingStrategy.custom(Foundation.JSONEncoder.DataEncodingStrategy.Type) -> ((Foundation.Data, Swift.Encoder) throws -> ()) -> Foundation.JSONEncoder.DataEncodingStrategy
- `$s10Foundation11JSONEncoderC20DataEncodingStrategyOMa`  
  type metadata accessor for Foundation.JSONEncoder.DataEncodingStrategy
- `$s10Foundation11JSONEncoderC20DataEncodingStrategyOMn`  
  nominal type descriptor for Foundation.JSONEncoder.DataEncodingStrategy
- `$s10Foundation11JSONEncoderC20DateEncodingStrategyO010deferredToC0yA2EmFWC`  
  enum case for Foundation.JSONEncoder.DateEncodingStrategy.deferredToDate(Foundation.JSONEncoder.DateEncodingStrategy.Type) -> Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC20DateEncodingStrategyO16secondsSince1970yA2EmFWC`  
  enum case for Foundation.JSONEncoder.DateEncodingStrategy.secondsSince1970(Foundation.JSONEncoder.DateEncodingStrategy.Type) -> Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC20DateEncodingStrategyO6customyAEyAA0C0V_s7Encoder_ptKccAEmFWC`  
  enum case for Foundation.JSONEncoder.DateEncodingStrategy.custom(Foundation.JSONEncoder.DateEncodingStrategy.Type) -> ((Foundation.Date, Swift.Encoder) throws -> ()) -> Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC20DateEncodingStrategyO7iso8601yA2EmFWC`  
  enum case for Foundation.JSONEncoder.DateEncodingStrategy.iso8601(Foundation.JSONEncoder.DateEncodingStrategy.Type) -> Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC20DateEncodingStrategyOMa`  
  type metadata accessor for Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC20DateEncodingStrategyOMn`  
  nominal type descriptor for Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC20dataEncodingStrategyAC04DatadE0OvsTj`  
  dispatch thunk of Foundation.JSONEncoder.dataEncodingStrategy.setter : Foundation.JSONEncoder.DataEncodingStrategy
- `$s10Foundation11JSONEncoderC20dateEncodingStrategyAC04DatedE0OvsTj`  
  dispatch thunk of Foundation.JSONEncoder.dateEncodingStrategy.setter : Foundation.JSONEncoder.DateEncodingStrategy
- `$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyOMa`  
  type metadata accessor for Foundation.JSONEncoder.NonConformingFloatEncodingStrategy
- `$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyOMn`  
  nominal type descriptor for Foundation.JSONEncoder.NonConformingFloatEncodingStrategy
- `$s10Foundation11JSONEncoderC34nonConformingFloatEncodingStrategyAC03NondefG0OvsTj`  
  dispatch thunk of Foundation.JSONEncoder.nonConformingFloatEncodingStrategy.setter : Foundation.JSONEncoder.NonConformingFloatEncodingStrategy
- `$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj`  
  dispatch thunk of Foundation.JSONEncoder.encode<A where A: Swift.Encodable>(A) throws -> Foundation.Data
- `$s10Foundation11JSONEncoderC8userInfoSDys010CodingUserD3KeyVypGvMTj`  
  dispatch thunk of Foundation.JSONEncoder.userInfo.modify : [Swift.CodingUserInfoKey : Any]
- `$s10Foundation11JSONEncoderC8userInfoSDys010CodingUserD3KeyVypGvsTj`  
  dispatch thunk of Foundation.JSONEncoder.userInfo.setter : [Swift.CodingUserInfoKey : Any]
- `$s10Foundation11JSONEncoderCACycfC`  
  Foundation.JSONEncoder.__allocating_init() -> Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCACycfCTj`  
  dispatch thunk of Foundation.JSONEncoder.__allocating_init() -> Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCACycfCTq`  
  method descriptor for Foundation.JSONEncoder.__allocating_init() -> Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCACycfc`  
  Foundation.JSONEncoder.init() -> Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCMa`  
  type metadata accessor for Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCMm`  
  metaclass for Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCMn`  
  nominal type descriptor for Foundation.JSONEncoder
- `$s10Foundation11JSONEncoderCfd`  
  Foundation.JSONEncoder.deinit

## IndexSet (32)

- `$s10Foundation8IndexSetV08filteredbC014includeIntegerACSbSiKXE_tKF`  
  Foundation.IndexSet.filteredIndexSet(includeInteger: (Swift.Int) throws -> Swift.Bool) throws -> Foundation.IndexSet
- `$s10Foundation8IndexSetV0B0VMa`  
  type metadata accessor for Foundation.IndexSet.Index
- `$s10Foundation8IndexSetV0B0VSQAAMc`  
  protocol conformance descriptor for Foundation.IndexSet.Index : Swift.Equatable in Foundation
- `$s10Foundation8IndexSetV10integersInACSnySiG_tcfC`  
  Foundation.IndexSet.init(integersIn: Swift.Range<Swift.Int>) -> Foundation.IndexSet
- `$s10Foundation8IndexSetV12intersectionyA2CF`  
  Foundation.IndexSet.intersection(Foundation.IndexSet) -> Foundation.IndexSet
- `$s10Foundation8IndexSetV12makeIterators08IndexingE0VyACGyF`  
  Foundation.IndexSet.makeIterator() -> Swift.IndexingIterator<Foundation.IndexSet>
- `$s10Foundation8IndexSetV19_bridgeToObjectiveCSo07NSIndexC0CyF`  
  Foundation.IndexSet._bridgeToObjectiveC() -> __C.NSIndexSet
- `$s10Foundation8IndexSetV2eeoiySbAC_ACtFZ`  
  static Foundation.IndexSet.== infix(Foundation.IndexSet, Foundation.IndexSet) -> Swift.Bool
- `$s10Foundation8IndexSetV36_unconditionallyBridgeFromObjectiveCyACSo07NSIndexC0CSgFZ`  
  static Foundation.IndexSet._unconditionallyBridgeFromObjectiveC(__C.NSIndexSet?) -> Foundation.IndexSet
- `$s10Foundation8IndexSetV5countSivg`  
  Foundation.IndexSet.count.getter : Swift.Int
- `$s10Foundation8IndexSetV5firstSiSgvg`  
  Foundation.IndexSet.first.getter : Swift.Int?
- `$s10Foundation8IndexSetV5shift10startingAt2byySi_SitF`  
  Foundation.IndexSet.shift(startingAt: Swift.Int, by: Swift.Int) -> ()
- `$s10Foundation8IndexSetV6insert10integersInySnySiG_tF`  
  Foundation.IndexSet.insert(integersIn: Swift.Range<Swift.Int>) -> ()
- `$s10Foundation8IndexSetV6insertySb8inserted_Si17memberAfterInserttSiF`  
  Foundation.IndexSet.insert(Swift.Int) -> (inserted: Swift.Bool, memberAfterInsert: Swift.Int)
- `$s10Foundation8IndexSetV6remove10integersInySnySiG_tF`  
  Foundation.IndexSet.remove(integersIn: Swift.Range<Swift.Int>) -> ()
- `$s10Foundation8IndexSetV6removeySiSgSiF`  
  Foundation.IndexSet.remove(Swift.Int) -> Swift.Int?
- `$s10Foundation8IndexSetV7integerACSi_tcfC`  
  Foundation.IndexSet.init(integer: Swift.Int) -> Foundation.IndexSet
- `$s10Foundation8IndexSetV7isEmptySbvg`  
  Foundation.IndexSet.isEmpty.getter : Swift.Bool
- `$s10Foundation8IndexSetV8contains10integersInSbSnySiG_tF`  
  Foundation.IndexSet.contains(integersIn: Swift.Range<Swift.Int>) -> Swift.Bool
- `$s10Foundation8IndexSetV8containsySbSiF`  
  Foundation.IndexSet.contains(Swift.Int) -> Swift.Bool
- `$s10Foundation8IndexSetV9RangeViewVMa`  
  type metadata accessor for Foundation.IndexSet.RangeView
- `$s10Foundation8IndexSetV9RangeViewVSTAAMc`  
  protocol conformance descriptor for Foundation.IndexSet.RangeView : Swift.Sequence in Foundation
- `$s10Foundation8IndexSetV9RangeViewVSlAAMc`  
  protocol conformance descriptor for Foundation.IndexSet.RangeView : Swift.Collection in Foundation
- `$s10Foundation8IndexSetV9rangeViewAC05RangeE0Vvg`  
  Foundation.IndexSet.rangeView.getter : Foundation.IndexSet.RangeView
- `$s10Foundation8IndexSetVACycfC`  
  Foundation.IndexSet.init() -> Foundation.IndexSet
- `$s10Foundation8IndexSetVMa`  
  type metadata accessor for Foundation.IndexSet
- `$s10Foundation8IndexSetVMn`  
  nominal type descriptor for Foundation.IndexSet
- `$s10Foundation8IndexSetVSHAAMc`  
  protocol conformance descriptor for Foundation.IndexSet : Swift.Hashable in Foundation
- `$s10Foundation8IndexSetVSQAAMc`  
  protocol conformance descriptor for Foundation.IndexSet : Swift.Equatable in Foundation
- `$s10Foundation8IndexSetVSTAAMc`  
  protocol conformance descriptor for Foundation.IndexSet : Swift.Sequence in Foundation
- `$s10Foundation8IndexSetVSlAAMc`  
  protocol conformance descriptor for Foundation.IndexSet : Swift.Collection in Foundation
- `$s10Foundation8IndexSetVs0C7AlgebraAAMc`  
  protocol conformance descriptor for Foundation.IndexSet : Swift.SetAlgebra in Foundation

## JSONDecoder (30)

- `$s10Foundation11JSONDecoderC19KeyDecodingStrategyO14useDefaultKeysyA2EmFWC`  
  enum case for Foundation.JSONDecoder.KeyDecodingStrategy.useDefaultKeys(Foundation.JSONDecoder.KeyDecodingStrategy.Type) -> Foundation.JSONDecoder.KeyDecodingStrategy
- `$s10Foundation11JSONDecoderC19KeyDecodingStrategyO20convertFromSnakeCaseyA2EmFWC`  
  enum case for Foundation.JSONDecoder.KeyDecodingStrategy.convertFromSnakeCase(Foundation.JSONDecoder.KeyDecodingStrategy.Type) -> Foundation.JSONDecoder.KeyDecodingStrategy
- `$s10Foundation11JSONDecoderC19KeyDecodingStrategyO6customyAEs06CodingC0_pSaysAG_pGccAEmFWC`  
  enum case for Foundation.JSONDecoder.KeyDecodingStrategy.custom(Foundation.JSONDecoder.KeyDecodingStrategy.Type) -> (([Swift.CodingKey]) -> Swift.CodingKey) -> Foundation.JSONDecoder.KeyDecodingStrategy
- `$s10Foundation11JSONDecoderC19KeyDecodingStrategyOMa`  
  type metadata accessor for Foundation.JSONDecoder.KeyDecodingStrategy
- `$s10Foundation11JSONDecoderC19KeyDecodingStrategyOMn`  
  nominal type descriptor for Foundation.JSONDecoder.KeyDecodingStrategy
- `$s10Foundation11JSONDecoderC19keyDecodingStrategyAC03KeydE0OvsTj`  
  dispatch thunk of Foundation.JSONDecoder.keyDecodingStrategy.setter : Foundation.JSONDecoder.KeyDecodingStrategy
- `$s10Foundation11JSONDecoderC20DataDecodingStrategyO6base64yA2EmFWC`  
  enum case for Foundation.JSONDecoder.DataDecodingStrategy.base64(Foundation.JSONDecoder.DataDecodingStrategy.Type) -> Foundation.JSONDecoder.DataDecodingStrategy
- `$s10Foundation11JSONDecoderC20DataDecodingStrategyO6customyAeA0C0Vs7Decoder_pKccAEmFWC`  
  enum case for Foundation.JSONDecoder.DataDecodingStrategy.custom(Foundation.JSONDecoder.DataDecodingStrategy.Type) -> ((Swift.Decoder) throws -> Foundation.Data) -> Foundation.JSONDecoder.DataDecodingStrategy
- `$s10Foundation11JSONDecoderC20DataDecodingStrategyOMa`  
  type metadata accessor for Foundation.JSONDecoder.DataDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyO010deferredToC0yA2EmFWC`  
  enum case for Foundation.JSONDecoder.DateDecodingStrategy.deferredToDate(Foundation.JSONDecoder.DateDecodingStrategy.Type) -> Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyO16secondsSince1970yA2EmFWC`  
  enum case for Foundation.JSONDecoder.DateDecodingStrategy.secondsSince1970(Foundation.JSONDecoder.DateDecodingStrategy.Type) -> Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyO21millisecondsSince1970yA2EmFWC`  
  enum case for Foundation.JSONDecoder.DateDecodingStrategy.millisecondsSince1970(Foundation.JSONDecoder.DateDecodingStrategy.Type) -> Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyO6customyAeA0C0Vs7Decoder_pKccAEmFWC`  
  enum case for Foundation.JSONDecoder.DateDecodingStrategy.custom(Foundation.JSONDecoder.DateDecodingStrategy.Type) -> ((Swift.Decoder) throws -> Foundation.Date) -> Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyO7iso8601yA2EmFWC`  
  enum case for Foundation.JSONDecoder.DateDecodingStrategy.iso8601(Foundation.JSONDecoder.DateDecodingStrategy.Type) -> Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyOMa`  
  type metadata accessor for Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20DateDecodingStrategyOMn`  
  nominal type descriptor for Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC20dataDecodingStrategyAC04DatadE0OvsTj`  
  dispatch thunk of Foundation.JSONDecoder.dataDecodingStrategy.setter : Foundation.JSONDecoder.DataDecodingStrategy
- `$s10Foundation11JSONDecoderC20dateDecodingStrategyAC04DatedE0OvsTj`  
  dispatch thunk of Foundation.JSONDecoder.dateDecodingStrategy.setter : Foundation.JSONDecoder.DateDecodingStrategy
- `$s10Foundation11JSONDecoderC34NonConformingFloatDecodingStrategyO17convertFromStringyAESS_S2StcAEmFWC`  
  enum case for Foundation.JSONDecoder.NonConformingFloatDecodingStrategy.convertFromString(Foundation.JSONDecoder.NonConformingFloatDecodingStrategy.Type) -> (Swift.String, Swift.String, Swift.String) -> Foundation.JSONDecoder.NonConformingFloatDecodingStrategy
- `$s10Foundation11JSONDecoderC34NonConformingFloatDecodingStrategyOMa`  
  type metadata accessor for Foundation.JSONDecoder.NonConformingFloatDecodingStrategy
- `$s10Foundation11JSONDecoderC34nonConformingFloatDecodingStrategyAC03NondefG0OvsTj`  
  dispatch thunk of Foundation.JSONDecoder.nonConformingFloatDecodingStrategy.setter : Foundation.JSONDecoder.NonConformingFloatDecodingStrategy
- `$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj`  
  dispatch thunk of Foundation.JSONDecoder.decode<A where A: Swift.Decodable>(_: A.Type, from: Foundation.Data) throws -> A
- `$s10Foundation11JSONDecoderC7Combine15TopLevelDecoderAAMc`  
  protocol conformance descriptor for Foundation.JSONDecoder : Combine.TopLevelDecoder in Foundation
- `$s10Foundation11JSONDecoderCACycfC`  
  Foundation.JSONDecoder.__allocating_init() -> Foundation.JSONDecoder
- `$s10Foundation11JSONDecoderCACycfCTq`  
  method descriptor for Foundation.JSONDecoder.__allocating_init() -> Foundation.JSONDecoder
- `$s10Foundation11JSONDecoderCACycfc`  
  Foundation.JSONDecoder.init() -> Foundation.JSONDecoder
- `$s10Foundation11JSONDecoderCMa`  
  type metadata accessor for Foundation.JSONDecoder
- `$s10Foundation11JSONDecoderCMm`  
  metaclass for Foundation.JSONDecoder
- `$s10Foundation11JSONDecoderCMn`  
  nominal type descriptor for Foundation.JSONDecoder
- `$s10Foundation11JSONDecoderCfd`  
  Foundation.JSONDecoder.deinit

## URLResourceValues (30)

- `$s10Foundation17URLResourceValuesV03allC0SDySo16NSURLResourceKeyaypGvg`  
  Foundation.URLResourceValues.allValues.getter : [__C.NSURLResourceKey : Any]
- `$s10Foundation17URLResourceValuesV10isReadableSbSgvg`  
  Foundation.URLResourceValues.isReadable.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV11isAliasFileSbSgvg`  
  Foundation.URLResourceValues.isAliasFile.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV11isDirectorySbSgvg`  
  Foundation.URLResourceValues.isDirectory.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV12creationDateAA0E0VSgvg`  
  Foundation.URLResourceValues.creationDate.getter : Foundation.Date?
- `$s10Foundation17URLResourceValuesV13canonicalPathSSSgvg`  
  Foundation.URLResourceValues.canonicalPath.getter : Swift.String?
- `$s10Foundation17URLResourceValuesV13isRegularFileSbSgvg`  
  Foundation.URLResourceValues.isRegularFile.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV13localizedNameSSSgvg`  
  Foundation.URLResourceValues.localizedName.getter : Swift.String?
- `$s10Foundation17URLResourceValuesV13totalFileSizeSiSgvg`  
  Foundation.URLResourceValues.totalFileSize.getter : Swift.Int?
- `$s10Foundation17URLResourceValuesV14isSymbolicLinkSbSgvg`  
  Foundation.URLResourceValues.isSymbolicLink.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV14typeIdentifierSSSgvg`  
  Foundation.URLResourceValues.typeIdentifier.getter : Swift.String?
- `$s10Foundation17URLResourceValuesV16isUbiquitousItemSbSgvg`  
  Foundation.URLResourceValues.isUbiquitousItem.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV17contentAccessDateAA0F0VSgvg`  
  Foundation.URLResourceValues.contentAccessDate.getter : Foundation.Date?
- `$s10Foundation17URLResourceValuesV17fileAllocatedSizeSiSgvg`  
  Foundation.URLResourceValues.fileAllocatedSize.getter : Swift.Int?
- `$s10Foundation17URLResourceValuesV18hasHiddenExtensionSbSgvs`  
  Foundation.URLResourceValues.hasHiddenExtension.setter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV19volumeTotalCapacitySiSgvg`  
  Foundation.URLResourceValues.volumeTotalCapacity.getter : Swift.Int?
- `$s10Foundation17URLResourceValuesV20isExcludedFromBackupSbSgvs`  
  Foundation.URLResourceValues.isExcludedFromBackup.setter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV22UniformTypeIdentifiersE07contentE0AD6UTTypeVSgvg`  
  (extension in UniformTypeIdentifiers):Foundation.URLResourceValues.contentType.getter : UniformTypeIdentifiers.UTType?
- `$s10Foundation17URLResourceValuesV22fileResourceIdentifierSo9NSCopying_So14NSSecureCodingSo8NSObjectpSgvg`  
  Foundation.URLResourceValues.fileResourceIdentifier.getter : (__C.NSCopying & __C.NSSecureCoding & __C.NSObject)?
- `$s10Foundation17URLResourceValuesV22totalFileAllocatedSizeSiSgvg`  
  Foundation.URLResourceValues.totalFileAllocatedSize.getter : Swift.Int?
- `$s10Foundation17URLResourceValuesV23contentModificationDateAA0F0VSgvg`  
  Foundation.URLResourceValues.contentModificationDate.getter : Foundation.Date?
- `$s10Foundation17URLResourceValuesV23volumeAvailableCapacitySiSgvg`  
  Foundation.URLResourceValues.volumeAvailableCapacity.getter : Swift.Int?
- `$s10Foundation17URLResourceValuesV31ubiquitousItemDownloadingStatusSo015NSURLUbiquitousefG0aSgvg`  
  Foundation.URLResourceValues.ubiquitousItemDownloadingStatus.getter : __C.NSURLUbiquitousItemDownloadingStatus?
- `$s10Foundation17URLResourceValuesV40volumeAvailableCapacityForImportantUsages5Int64VSgvg`  
  Foundation.URLResourceValues.volumeAvailableCapacityForImportantUsage.getter : Swift.Int64?
- `$s10Foundation17URLResourceValuesV4pathSSSgvg`  
  Foundation.URLResourceValues.path.getter : Swift.String?
- `$s10Foundation17URLResourceValuesV8fileSizeSiSgvg`  
  Foundation.URLResourceValues.fileSize.getter : Swift.Int?
- `$s10Foundation17URLResourceValuesV8isHiddenSbSgvg`  
  Foundation.URLResourceValues.isHidden.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesVACycfC`  
  Foundation.URLResourceValues.init() -> Foundation.URLResourceValues
- `$s10Foundation17URLResourceValuesVMa`  
  type metadata accessor for Foundation.URLResourceValues
- `$s10Foundation17URLResourceValuesVMn`  
  nominal type descriptor for Foundation.URLResourceValues

## DateComponents (29)

- `$s10Foundation14DateComponentsV07isValidB02inSbAA8CalendarV_tF`  
  Foundation.DateComponents.isValidDate(in: Foundation.Calendar) -> Swift.Bool
- `$s10Foundation14DateComponentsV10nanosecondSiSgvg`  
  Foundation.DateComponents.nanosecond.getter : Swift.Int?
- `$s10Foundation14DateComponentsV10nanosecondSiSgvs`  
  Foundation.DateComponents.nanosecond.setter : Swift.Int?
- `$s10Foundation14DateComponentsV19_bridgeToObjectiveCSo06NSDateC0CyF`  
  Foundation.DateComponents._bridgeToObjectiveC() -> __C.NSDateComponents
- `$s10Foundation14DateComponentsV2eeoiySbAC_ACtFZ`  
  static Foundation.DateComponents.== infix(Foundation.DateComponents, Foundation.DateComponents) -> Swift.Bool
- `$s10Foundation14DateComponentsV36_unconditionallyBridgeFromObjectiveCyACSo06NSDateC0CSgFZ`  
  static Foundation.DateComponents._unconditionallyBridgeFromObjectiveC(__C.NSDateComponents?) -> Foundation.DateComponents
- `$s10Foundation14DateComponentsV3daySiSgvg`  
  Foundation.DateComponents.day.getter : Swift.Int?
- `$s10Foundation14DateComponentsV3daySiSgvs`  
  Foundation.DateComponents.day.setter : Swift.Int?
- `$s10Foundation14DateComponentsV4dateAA0B0VSgvg`  
  Foundation.DateComponents.date.getter : Foundation.Date?
- `$s10Foundation14DateComponentsV4hourSiSgvg`  
  Foundation.DateComponents.hour.getter : Swift.Int?
- `$s10Foundation14DateComponentsV4hourSiSgvs`  
  Foundation.DateComponents.hour.setter : Swift.Int?
- `$s10Foundation14DateComponentsV4yearSiSgvg`  
  Foundation.DateComponents.year.getter : Swift.Int?
- `$s10Foundation14DateComponentsV4yearSiSgvs`  
  Foundation.DateComponents.year.setter : Swift.Int?
- `$s10Foundation14DateComponentsV5monthSiSgvg`  
  Foundation.DateComponents.month.getter : Swift.Int?
- `$s10Foundation14DateComponentsV5monthSiSgvs`  
  Foundation.DateComponents.month.setter : Swift.Int?
- `$s10Foundation14DateComponentsV5value3forSiSgAA8CalendarV9ComponentO_tF`  
  Foundation.DateComponents.value(for: Foundation.Calendar.Component) -> Swift.Int?
- `$s10Foundation14DateComponentsV6minuteSiSgvg`  
  Foundation.DateComponents.minute.getter : Swift.Int?
- `$s10Foundation14DateComponentsV6minuteSiSgvs`  
  Foundation.DateComponents.minute.setter : Swift.Int?
- `$s10Foundation14DateComponentsV6secondSiSgvg`  
  Foundation.DateComponents.second.getter : Swift.Int?
- `$s10Foundation14DateComponentsV6secondSiSgvs`  
  Foundation.DateComponents.second.setter : Swift.Int?
- `$s10Foundation14DateComponentsV7weekdaySiSgvg`  
  Foundation.DateComponents.weekday.getter : Swift.Int?
- `$s10Foundation14DateComponentsV8calendar8timeZone3era4year5month3day4hour6minute6second10nanosecond7weekday0O7Ordinal7quarter11weekOfMonth0rS4Year0h7ForWeeksU0AcA8CalendarVSg_AA04TimeF0VSgSiSgA13ZtcfC`  
  Foundation.DateComponents.init(calendar: Foundation.Calendar?, timeZone: Foundation.TimeZone?, era: Swift.Int?, year: Swift.Int?, month: Swift.Int?, day: Swift.Int?, hour: Swift.Int?, minute: Swift.Int?, second: Swift.Int?, nanosecond: Swift.Int?, weekday: Swift.Int?, weekdayOrdinal: Swift.Int?, quarter: Swift.Int?, weekOfMonth: Swift.Int?, weekOfYear: Swift.Int?, yearForWeekOfYear: Swift.Int?) -> Foundation.DateComponents
- `$s10Foundation14DateComponentsV8calendarAA8CalendarVSgvs`  
  Foundation.DateComponents.calendar.setter : Foundation.Calendar?
- `$s10Foundation14DateComponentsV8setValue_3forySiSg_AA8CalendarV9ComponentOtF`  
  Foundation.DateComponents.setValue(_: Swift.Int?, for: Foundation.Calendar.Component) -> ()
- `$s10Foundation14DateComponentsV8timeZoneAA04TimeE0VSgvg`  
  Foundation.DateComponents.timeZone.getter : Foundation.TimeZone?
- `$s10Foundation14DateComponentsV8timeZoneAA04TimeE0VSgvs`  
  Foundation.DateComponents.timeZone.setter : Foundation.TimeZone?
- `$s10Foundation14DateComponentsVMa`  
  type metadata accessor for Foundation.DateComponents
- `$s10Foundation14DateComponentsVMn`  
  nominal type descriptor for Foundation.DateComponents
- `$s10Foundation14DateComponentsVSQAAMc`  
  protocol conformance descriptor for Foundation.DateComponents : Swift.Equatable in Foundation

## URL (26)

- `$s10Foundation3URLV10AsyncBytesV04makeC8IteratorAE0cF0VyF`  
  Foundation.URL.AsyncBytes.makeAsyncIterator() -> Foundation.URL.AsyncBytes.AsyncIterator
- `$s10Foundation3URLV10AsyncBytesVMa`  
  type metadata accessor for Foundation.URL.AsyncBytes
- `$s10Foundation3URLV13resourceBytesAC05AsyncD0Vvg`  
  Foundation.URL.resourceBytes.getter : Foundation.URL.AsyncBytes
- `$s10Foundation3URLV14pathComponentsSaySSGvg`  
  Foundation.URL.pathComponents.getter : [Swift.String]
- `$s10Foundation3URLV14resourceValues7forKeys16fromBookmarkDataAA011URLResourceD0VSgShySo16NSURLResourceKeyaG_AA0I0VtFZ`  
  static Foundation.URL.resourceValues(forKeys: Swift.Set<__C.NSURLResourceKey>, fromBookmarkData: Foundation.Data) -> Foundation.URLResourceValues?
- `$s10Foundation3URLV14resourceValues7forKeysAA011URLResourceD0VShySo16NSURLResourceKeyaG_tKF`  
  Foundation.URL.resourceValues(forKeys: Swift.Set<__C.NSURLResourceKey>) throws -> Foundation.URLResourceValues
- `$s10Foundation3URLV17setResourceValuesyyAA011URLResourceE0VKF`  
  Foundation.URL.setResourceValues(Foundation.URLResourceValues) throws -> ()
- `$s10Foundation3URLV18dataRepresentationAA4DataVvg`  
  Foundation.URL.dataRepresentation.getter : Foundation.Data
- `$s10Foundation3URLV20resolvingAliasFileAt7optionsA2Ch_So30NSURLBookmarkResolutionOptionsVtKcfC`  
  Foundation.URL.init(resolvingAliasFileAt: __shared Foundation.URL, options: __C.NSURLBookmarkResolutionOptions) throws -> Foundation.URL
- `$s10Foundation3URLV21resolvingBookmarkData7options10relativeTo08bookmarkE7IsStaleAcA0E0Vh_So30NSURLBookmarkResolutionOptionsVACSghSbztKcfC`  
  Foundation.URL.init(resolvingBookmarkData: __shared Foundation.Data, options: __C.NSURLBookmarkResolutionOptions, relativeTo: __shared Foundation.URL?, bookmarkDataIsStale: inout Swift.Bool) throws -> Foundation.URL
- `$s10Foundation3URLV24checkResourceIsReachableSbyKF`  
  Foundation.URL.checkResourceIsReachable() throws -> Swift.Bool
- `$s10Foundation3URLV25removeCachedResourceValue6forKeyySo013NSURLResourceH0a_tF`  
  Foundation.URL.removeCachedResourceValue(forKey: __C.NSURLResourceKey) -> ()
- `$s10Foundation3URLV27applicationSupportDirectoryACvgZ`  
  static Foundation.URL.applicationSupportDirectory.getter : Foundation.URL
- `$s10Foundation3URLV29removeAllCachedResourceValuesyyF`  
  Foundation.URL.removeAllCachedResourceValues() -> ()
- `$s10Foundation3URLV34withUnsafeFileSystemRepresentationyxxSPys4Int8VGSgKXEKlF`  
  Foundation.URL.withUnsafeFileSystemRepresentation<A>((Swift.UnsafePointer<Swift.Int8>?) throws -> A) throws -> A
- `$s10Foundation3URLV35fileURLWithFileSystemRepresentation11isDirectory10relativeToACSPys4Int8VG_SbACSghtcfC`  
  Foundation.URL.init(fileURLWithFileSystemRepresentation: Swift.UnsafePointer<Swift.Int8>, isDirectory: Swift.Bool, relativeTo: __shared Foundation.URL?) -> Foundation.URL
- `$s10Foundation3URLV6string10relativeToACSgSSh_AFhtcfC`  
  Foundation.URL.init(string: __shared Swift.String, relativeTo: __shared Foundation.URL?) -> Foundation.URL?
- `$s10Foundation3URLV9appending10queryItemsACSayAA12URLQueryItemVG_tF`  
  Foundation.URL.appending(queryItems: [Foundation.URLQueryItem]) -> Foundation.URL
- `$s10Foundation3URLV9appending4path13directoryHintACx_AC09DirectoryF0OtSyRzlF`  
  Foundation.URL.appending<A where A: Swift.StringProtocol>(path: A, directoryHint: Foundation.URL.DirectoryHint) -> Foundation.URL
- `$s10Foundation3URLV9appending9component13directoryHintACx_AC09DirectoryF0OtSyRzlF`  
  Foundation.URL.appending<A where A: Swift.StringProtocol>(component: A, directoryHint: Foundation.URL.DirectoryHint) -> Foundation.URL
- `$s10Foundation3URLVSEAAMc`  
  protocol conformance descriptor for Foundation.URL : Swift.Encodable in Foundation
- `$s10Foundation3URLVSHAAMc`  
  protocol conformance descriptor for Foundation.URL : Swift.Hashable in Foundation
- `$s10Foundation3URLVSQAAMc`  
  protocol conformance descriptor for Foundation.URL : Swift.Equatable in Foundation
- `$s10Foundation3URLVSeAAMc`  
  protocol conformance descriptor for Foundation.URL : Swift.Decodable in Foundation
- `$s10Foundation3URLVs21_ObjectiveCBridgeableAAMc`  
  protocol conformance descriptor for Foundation.URL : Swift._ObjectiveCBridgeable in Foundation
- `$s10Foundation3URLVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for Foundation.URL : Swift.CustomStringConvertible in Foundation

## URLRequest (25)

- `$s10Foundation10URLRequestV10httpMethodSSSgvg`  
  Foundation.URLRequest.httpMethod.getter : Swift.String?
- `$s10Foundation10URLRequestV10httpMethodSSSgvs`  
  Foundation.URLRequest.httpMethod.setter : Swift.String?
- `$s10Foundation10URLRequestV11cachePolicySo017NSURLRequestCacheD0Vvs`  
  Foundation.URLRequest.cachePolicy.setter : __C.NSURLRequestCachePolicy
- `$s10Foundation10URLRequestV15mainDocumentURLAA0E0VSgvg`  
  Foundation.URLRequest.mainDocumentURL.getter : Foundation.URL?
- `$s10Foundation10URLRequestV15timeoutIntervalSdvg`  
  Foundation.URLRequest.timeoutInterval.getter : Swift.Double
- `$s10Foundation10URLRequestV15timeoutIntervalSdvs`  
  Foundation.URLRequest.timeoutInterval.setter : Swift.Double
- `$s10Foundation10URLRequestV19_bridgeToObjectiveCSo12NSURLRequestCyF`  
  Foundation.URLRequest._bridgeToObjectiveC() -> __C.NSURLRequest
- `$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvM`  
  Foundation.URLRequest.allHTTPHeaderFields.modify : [Swift.String : Swift.String]?
- `$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvg`  
  Foundation.URLRequest.allHTTPHeaderFields.getter : [Swift.String : Swift.String]?
- `$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvs`  
  Foundation.URLRequest.allHTTPHeaderFields.setter : [Swift.String : Swift.String]?
- `$s10Foundation10URLRequestV19assumesHTTP3CapableSbvs`  
  Foundation.URLRequest.assumesHTTP3Capable.setter : Swift.Bool
- `$s10Foundation10URLRequestV23httpShouldHandleCookiesSbvg`  
  Foundation.URLRequest.httpShouldHandleCookies.getter : Swift.Bool
- `$s10Foundation10URLRequestV23httpShouldHandleCookiesSbvs`  
  Foundation.URLRequest.httpShouldHandleCookies.setter : Swift.Bool
- `$s10Foundation10URLRequestV2eeoiySbAC_ACtFZ`  
  static Foundation.URLRequest.== infix(Foundation.URLRequest, Foundation.URLRequest) -> Swift.Bool
- `$s10Foundation10URLRequestV36_unconditionallyBridgeFromObjectiveCyACSo12NSURLRequestCSgFZ`  
  static Foundation.URLRequest._unconditionallyBridgeFromObjectiveC(__C.NSURLRequest?) -> Foundation.URLRequest
- `$s10Foundation10URLRequestV3url11cachePolicy15timeoutIntervalAcA3URLV_So017NSURLRequestCacheE0VSdtcfC`  
  Foundation.URLRequest.init(url: Foundation.URL, cachePolicy: __C.NSURLRequestCachePolicy, timeoutInterval: Swift.Double) -> Foundation.URLRequest
- `$s10Foundation10URLRequestV3urlAA3URLVSgvg`  
  Foundation.URLRequest.url.getter : Foundation.URL?
- `$s10Foundation10URLRequestV3urlAA3URLVSgvs`  
  Foundation.URLRequest.url.setter : Foundation.URL?
- `$s10Foundation10URLRequestV5value18forHTTPHeaderFieldSSSgSS_tF`  
  Foundation.URLRequest.value(forHTTPHeaderField: Swift.String) -> Swift.String?
- `$s10Foundation10URLRequestV8addValue_18forHTTPHeaderFieldySS_SStF`  
  Foundation.URLRequest.addValue(_: Swift.String, forHTTPHeaderField: Swift.String) -> ()
- `$s10Foundation10URLRequestV8httpBodyAA4DataVSgvg`  
  Foundation.URLRequest.httpBody.getter : Foundation.Data?
- `$s10Foundation10URLRequestV8httpBodyAA4DataVSgvs`  
  Foundation.URLRequest.httpBody.setter : Foundation.Data?
- `$s10Foundation10URLRequestV8setValue_18forHTTPHeaderFieldySSSg_SStF`  
  Foundation.URLRequest.setValue(_: Swift.String?, forHTTPHeaderField: Swift.String) -> ()
- `$s10Foundation10URLRequestVMa`  
  type metadata accessor for Foundation.URLRequest
- `$s10Foundation10URLRequestVMn`  
  nominal type descriptor for Foundation.URLRequest

## _BridgedStoredNSError (18)

- `$s10Foundation21_BridgedStoredNSErrorMp`  
  protocol descriptor for Foundation._BridgedStoredNSError
- `$s10Foundation21_BridgedStoredNSErrorP4CodeAC_8RawValueSYs17FixedWidthIntegerTn`  
  associated conformance descriptor for Foundation._BridgedStoredNSError.Foundation._BridgedStoredNSError.Code.Swift.RawRepresentable.RawValue: Swift.FixedWidthInteger
- `$s10Foundation21_BridgedStoredNSErrorP4CodeAC_AA06_ErrorE8ProtocolTn`  
  associated conformance descriptor for Foundation._BridgedStoredNSError.Foundation._BridgedStoredNSError.Code: Foundation._ErrorCodeProtocol
- `$s10Foundation21_BridgedStoredNSErrorP4CodeAC_SYTn`  
  associated conformance descriptor for Foundation._BridgedStoredNSError.Foundation._BridgedStoredNSError.Code: Swift.RawRepresentable
- `$s10Foundation21_BridgedStoredNSErrorP8_nsErrorSo0D0CvgTj`  
  dispatch thunk of Foundation._BridgedStoredNSError._nsError.getter : __C.NSError
- `$s10Foundation21_BridgedStoredNSErrorP8_nsErrorSo0D0CvgTq`  
  method descriptor for Foundation._BridgedStoredNSError._nsError.getter : __C.NSError
- `$s10Foundation21_BridgedStoredNSErrorP8_nsErrorxSo0D0C_tcfCTq`  
  method descriptor for Foundation._BridgedStoredNSError.init(_nsError: __C.NSError) -> A
- `$s10Foundation21_BridgedStoredNSErrorPAA06CustomD0Tb`  
  base conformance descriptor for Foundation._BridgedStoredNSError: Foundation.CustomNSError
- `$s10Foundation21_BridgedStoredNSErrorPAA26_ObjectiveCBridgeableErrorTb`  
  base conformance descriptor for Foundation._BridgedStoredNSError: Foundation._ObjectiveCBridgeableError
- `$s10Foundation21_BridgedStoredNSErrorPAAE012_getEmbeddedD0yXlSgyF`  
  (extension in Foundation):Foundation._BridgedStoredNSError._getEmbeddedNSError() -> Swift.AnyObject?
- `$s10Foundation21_BridgedStoredNSErrorPAAE08_bridgedD0xSgSo0D0C_tcfC`  
  (extension in Foundation):Foundation._BridgedStoredNSError.init(_bridgedNSError: __C.NSError) -> A?
- `$s10Foundation21_BridgedStoredNSErrorPAAE13errorUserInfoSDySSypGvg`  
  (extension in Foundation):Foundation._BridgedStoredNSError.errorUserInfo.getter : [Swift.String : Any]
- `$s10Foundation21_BridgedStoredNSErrorPAAE2eeoiySbx_xtFZ`  
  static (extension in Foundation):Foundation._BridgedStoredNSError.== infix(A, A) -> Swift.Bool
- `$s10Foundation21_BridgedStoredNSErrorPAAE4code4CodeQzvg`  
  (extension in Foundation):Foundation._BridgedStoredNSError.code.getter : A.Code
- `$s10Foundation21_BridgedStoredNSErrorPAAE4hash4intoys6HasherVz_tF`  
  (extension in Foundation):Foundation._BridgedStoredNSError.hash(into: inout Swift.Hasher) -> ()
- `$s10Foundation21_BridgedStoredNSErrorPAAE9errorCodeSivg`  
  (extension in Foundation):Foundation._BridgedStoredNSError.errorCode.getter : Swift.Int
- `$s10Foundation21_BridgedStoredNSErrorPAAE_8userInfox4CodeQz_SDySSypGtcfC`  
  (extension in Foundation):Foundation._BridgedStoredNSError.init(_: A.Code, userInfo: [Swift.String : Any]) -> A
- `$s10Foundation21_BridgedStoredNSErrorPSHTb`  
  base conformance descriptor for Foundation._BridgedStoredNSError: Swift.Hashable

## NSURLSession (17)

- `$sSo12NSURLSessionC10FoundationE10AsyncBytesV04makeC8IteratorAE0F0VyF`  
  (extension in Foundation):__C.NSURLSession.AsyncBytes.makeAsyncIterator() -> (extension in Foundation):__C.NSURLSession.AsyncBytes.Iterator
- `$sSo12NSURLSessionC10FoundationE10AsyncBytesV4taskSo0A8DataTaskCvg`  
  (extension in Foundation):__C.NSURLSession.AsyncBytes.task.getter : __C.NSURLSessionDataTask
- `$sSo12NSURLSessionC10FoundationE10AsyncBytesVMa`  
  type metadata accessor for (extension in Foundation):__C.NSURLSession.AsyncBytes
- `$sSo12NSURLSessionC10FoundationE10AsyncBytesVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSURLSession.AsyncBytes
- `$sSo12NSURLSessionC10FoundationE17DataTaskPublisherV7Combine0E0ACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSURLSession.DataTaskPublisher : Combine.Publisher in Foundation
- `$sSo12NSURLSessionC10FoundationE17DataTaskPublisherVMa`  
  type metadata accessor for (extension in Foundation):__C.NSURLSession.DataTaskPublisher
- `$sSo12NSURLSessionC10FoundationE17DataTaskPublisherVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSURLSession.DataTaskPublisher
- `$sSo12NSURLSessionC10FoundationE17dataTaskPublisher3forAbCE04DatadE0VAC10URLRequestV_tF`  
  (extension in Foundation):__C.NSURLSession.dataTaskPublisher(for: Foundation.URLRequest) -> (extension in Foundation):__C.NSURLSession.DataTaskPublisher
- `$sSo12NSURLSessionC10FoundationE17dataTaskPublisher3forAbCE04DatadE0VAC3URLV_tF`  
  (extension in Foundation):__C.NSURLSession.dataTaskPublisher(for: Foundation.URL) -> (extension in Foundation):__C.NSURLSession.DataTaskPublisher
- `$sSo12NSURLSessionC10FoundationE4data3for8delegateAC4DataV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKF`  
  (extension in Foundation):__C.NSURLSession.data(for: Foundation.URLRequest, delegate: __C.NSURLSessionTaskDelegate?) async throws -> (Foundation.Data, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE4data3for8delegateAC4DataV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKFTu`  
  async function pointer to (extension in Foundation):__C.NSURLSession.data(for: Foundation.URLRequest, delegate: __C.NSURLSessionTaskDelegate?) async throws -> (Foundation.Data, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE4data4from8delegateAC4DataV_So13NSURLResponseCtAC3URLV_So0A12TaskDelegate_pSgtYaKF`  
  (extension in Foundation):__C.NSURLSession.data(from: Foundation.URL, delegate: __C.NSURLSessionTaskDelegate?) async throws -> (Foundation.Data, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE4data4from8delegateAC4DataV_So13NSURLResponseCtAC3URLV_So0A12TaskDelegate_pSgtYaKFTu`  
  async function pointer to (extension in Foundation):__C.NSURLSession.data(from: Foundation.URL, delegate: __C.NSURLSessionTaskDelegate?) async throws -> (Foundation.Data, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE5bytes3for8delegateAbCE10AsyncBytesV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKF`  
  (extension in Foundation):__C.NSURLSession.bytes(for: Foundation.URLRequest, delegate: __C.NSURLSessionTaskDelegate?) async throws -> ((extension in Foundation):__C.NSURLSession.AsyncBytes, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE5bytes3for8delegateAbCE10AsyncBytesV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKFTu`  
  async function pointer to (extension in Foundation):__C.NSURLSession.bytes(for: Foundation.URLRequest, delegate: __C.NSURLSessionTaskDelegate?) async throws -> ((extension in Foundation):__C.NSURLSession.AsyncBytes, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE8download4from8delegateAC3URLV_So13NSURLResponseCtAH_So0A12TaskDelegate_pSgtYaKF`  
  (extension in Foundation):__C.NSURLSession.download(from: Foundation.URL, delegate: __C.NSURLSessionTaskDelegate?) async throws -> (Foundation.URL, __C.NSURLResponse)
- `$sSo12NSURLSessionC10FoundationE8download4from8delegateAC3URLV_So13NSURLResponseCtAH_So0A12TaskDelegate_pSgtYaKFTu`  
  async function pointer to (extension in Foundation):__C.NSURLSession.download(from: Foundation.URL, delegate: __C.NSURLSessionTaskDelegate?) async throws -> (Foundation.URL, __C.NSURLResponse)

## Date (16)

- `$s10Foundation4DateV13ISO8601FormatySSAC0cD5StyleVF`  
  Foundation.Date.ISO8601Format(Foundation.Date.ISO8601FormatStyle) -> Swift.String
- `$s10Foundation4DateV18ISO8601FormatStyleV17TimeZoneSeparatorO5colonyA2GmFWC`  
  enum case for Foundation.Date.ISO8601FormatStyle.TimeZoneSeparator.colon(Foundation.Date.ISO8601FormatStyle.TimeZoneSeparator.Type) -> Foundation.Date.ISO8601FormatStyle.TimeZoneSeparator
- `$s10Foundation4DateV18ISO8601FormatStyleV4time26includingFractionalSecondsAESb_tF`  
  Foundation.Date.ISO8601FormatStyle.time(includingFractionalSeconds: Swift.Bool) -> Foundation.Date.ISO8601FormatStyle
- `$s10Foundation4DateV18ISO8601FormatStyleV8timeZone9separatorA2E04TimeG9SeparatorO_tF`  
  Foundation.Date.ISO8601FormatStyle.timeZone(separator: Foundation.Date.ISO8601FormatStyle.TimeZoneSeparator) -> Foundation.Date.ISO8601FormatStyle
- `$s10Foundation4DateV18ISO8601FormatStyleVAA13ParseStrategyAAMc`  
  protocol conformance descriptor for Foundation.Date.ISO8601FormatStyle : Foundation.ParseStrategy in Foundation
- `$s10Foundation4DateV18ISO8601FormatStyleVMn`  
  nominal type descriptor for Foundation.Date.ISO8601FormatStyle
- `$s10Foundation4DateV4fromACs7Decoder_p_tKcfC`  
  Foundation.Date.init(from: Swift.Decoder) throws -> Foundation.Date
- `$s10Foundation4DateV8CloudKit21CKRecordValueProtocolADWP`  
  protocol witness table for Foundation.Date : CloudKit.CKRecordValueProtocol in CloudKit
- `$s10Foundation4DateVSEAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift.Encodable in Foundation
- `$s10Foundation4DateVSHAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift.Hashable in Foundation
- `$s10Foundation4DateVSLAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift.Comparable in Foundation
- `$s10Foundation4DateVSQAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift.Equatable in Foundation
- `$s10Foundation4DateVSeAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift.Decodable in Foundation
- `$s10Foundation4DateV_8strategyAC10ParseInputQz_xtKcAA0D8StrategyRzAC0D6OutputRtzlufC`  
  Foundation.Date.init<A where A: Foundation.ParseStrategy, A.ParseOutput == Foundation.Date>(_: A.ParseInput, strategy: A) throws -> Foundation.Date
- `$s10Foundation4DateVs21_ObjectiveCBridgeableAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift._ObjectiveCBridgeable in Foundation
- `$s10Foundation4DateVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for Foundation.Date : Swift.CustomStringConvertible in Foundation

## NSNotificationCenter (13)

- `$sSo20NSNotificationCenterC10FoundationE13NotificationsC17makeAsyncIteratorAE0G0VyF`  
  (extension in Foundation):__C.NSNotificationCenter.Notifications.makeAsyncIterator() -> (extension in Foundation):__C.NSNotificationCenter.Notifications.Iterator
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsC8IteratorV4nextAC12NotificationVSgyYaF`  
  (extension in Foundation):__C.NSNotificationCenter.Notifications.Iterator.next() async -> Foundation.Notification?
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsC8IteratorV4nextAC12NotificationVSgyYaFTu`  
  async function pointer to (extension in Foundation):__C.NSNotificationCenter.Notifications.Iterator.next() async -> Foundation.Notification?
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsC8IteratorVMa`  
  type metadata accessor for (extension in Foundation):__C.NSNotificationCenter.Notifications.Iterator
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsC8IteratorVScIACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSNotificationCenter.Notifications.Iterator : Swift.AsyncIteratorProtocol in Foundation
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsCMa`  
  type metadata accessor for (extension in Foundation):__C.NSNotificationCenter.Notifications
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsCMn`  
  nominal type descriptor for (extension in Foundation):__C.NSNotificationCenter.Notifications
- `$sSo20NSNotificationCenterC10FoundationE13NotificationsCSciACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSNotificationCenter.Notifications : Swift.AsyncSequence in Foundation
- `$sSo20NSNotificationCenterC10FoundationE13notifications5named6objectAbCE13NotificationsCSo0A4Namea_yXlSgtF`  
  (extension in Foundation):__C.NSNotificationCenter.notifications(named: __C.NSNotificationName, object: Swift.AnyObject?) -> (extension in Foundation):__C.NSNotificationCenter.Notifications
- `$sSo20NSNotificationCenterC10FoundationE9PublisherV7CombineAdCMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSNotificationCenter.Publisher : Combine.Publisher in Foundation
- `$sSo20NSNotificationCenterC10FoundationE9PublisherVMa`  
  type metadata accessor for (extension in Foundation):__C.NSNotificationCenter.Publisher
- `$sSo20NSNotificationCenterC10FoundationE9PublisherVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSNotificationCenter.Publisher
- `$sSo20NSNotificationCenterC10FoundationE9publisher3for6objectAbCE9PublisherVSo0A4Namea_yXlSgtF`  
  (extension in Foundation):__C.NSNotificationCenter.publisher(for: __C.NSNotificationName, object: Swift.AnyObject?) -> (extension in Foundation):__C.NSNotificationCenter.Publisher

## IndexPath (12)

- `$s10Foundation9IndexPathV19_bridgeToObjectiveCSo07NSIndexC0CyF`  
  Foundation.IndexPath._bridgeToObjectiveC() -> __C.NSIndexPath
- `$s10Foundation9IndexPathV36_unconditionallyBridgeFromObjectiveCyACSo07NSIndexC0CSgFZ`  
  static Foundation.IndexPath._unconditionallyBridgeFromObjectiveC(__C.NSIndexPath?) -> Foundation.IndexPath
- `$s10Foundation9IndexPathV5UIKitE3row7sectionACSi_SitcfC`  
  (extension in UIKit):Foundation.IndexPath.init(row: Swift.Int, section: Swift.Int) -> Foundation.IndexPath
- `$s10Foundation9IndexPathV5UIKitE3rowSivg`  
  (extension in UIKit):Foundation.IndexPath.row.getter : Swift.Int
- `$s10Foundation9IndexPathV5UIKitE4item7sectionACSi_SitcfC`  
  (extension in UIKit):Foundation.IndexPath.init(item: Swift.Int, section: Swift.Int) -> Foundation.IndexPath
- `$s10Foundation9IndexPathV5UIKitE4itemSivg`  
  (extension in UIKit):Foundation.IndexPath.item.getter : Swift.Int
- `$s10Foundation9IndexPathV5UIKitE7sectionSivg`  
  (extension in UIKit):Foundation.IndexPath.section.getter : Swift.Int
- `$s10Foundation9IndexPathVSHAAMc`  
  protocol conformance descriptor for Foundation.IndexPath : Swift.Hashable in Foundation
- `$s10Foundation9IndexPathVSLAAMc`  
  protocol conformance descriptor for Foundation.IndexPath : Swift.Comparable in Foundation
- `$s10Foundation9IndexPathVSQAAMc`  
  protocol conformance descriptor for Foundation.IndexPath : Swift.Equatable in Foundation
- `$s10Foundation9IndexPathVSlAAMc`  
  protocol conformance descriptor for Foundation.IndexPath : Swift.Collection in Foundation
- `$s10Foundation9IndexPathVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for Foundation.IndexPath : Swift.CustomStringConvertible in Foundation

## CustomNSError (11)

- `$s10Foundation13CustomNSErrorMp`  
  protocol descriptor for Foundation.CustomNSError
- `$s10Foundation13CustomNSErrorP11errorDomainSSvgZTq`  
  method descriptor for static Foundation.CustomNSError.errorDomain.getter : Swift.String
- `$s10Foundation13CustomNSErrorP13errorUserInfoSDySSypGvgTj`  
  dispatch thunk of Foundation.CustomNSError.errorUserInfo.getter : [Swift.String : Any]
- `$s10Foundation13CustomNSErrorP13errorUserInfoSDySSypGvgTq`  
  method descriptor for Foundation.CustomNSError.errorUserInfo.getter : [Swift.String : Any]
- `$s10Foundation13CustomNSErrorP9errorCodeSivgTj`  
  dispatch thunk of Foundation.CustomNSError.errorCode.getter : Swift.Int
- `$s10Foundation13CustomNSErrorP9errorCodeSivgTq`  
  method descriptor for Foundation.CustomNSError.errorCode.getter : Swift.Int
- `$s10Foundation13CustomNSErrorPAAE11errorDomainSSvgZ`  
  static (extension in Foundation):Foundation.CustomNSError.errorDomain.getter : Swift.String
- `$s10Foundation13CustomNSErrorPAAE13errorUserInfoSDySSypGvg`  
  (extension in Foundation):Foundation.CustomNSError.errorUserInfo.getter : [Swift.String : Any]
- `$s10Foundation13CustomNSErrorPAAE9errorCodeSivg`  
  (extension in Foundation):Foundation.CustomNSError.errorCode.getter : Swift.Int
- `$s10Foundation13CustomNSErrorPAASYRzs17FixedWidthInteger8RawValueSYRpzrlE9errorCodeSivg`  
  (extension in Foundation):Foundation.CustomNSError< where A: Swift.RawRepresentable, A.Swift.RawRepresentable.RawValue: Swift.FixedWidthInteger>.errorCode.getter : Swift.Int
- `$s10Foundation13CustomNSErrorPs5ErrorTb`  
  base conformance descriptor for Foundation.CustomNSError: Swift.Error

## URLComponents (11)

- `$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvM`  
  Foundation.URLComponents.queryItems.modify : [Foundation.URLQueryItem]?
- `$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvg`  
  Foundation.URLComponents.queryItems.getter : [Foundation.URLQueryItem]?
- `$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvs`  
  Foundation.URLComponents.queryItems.setter : [Foundation.URLQueryItem]?
- `$s10Foundation13URLComponentsV18percentEncodedPathSSvM`  
  Foundation.URLComponents.percentEncodedPath.modify : Swift.String
- `$s10Foundation13URLComponentsV19_bridgeToObjectiveCSo15NSURLComponentsCyF`  
  Foundation.URLComponents._bridgeToObjectiveC() -> __C.NSURLComponents
- `$s10Foundation13URLComponentsV24percentEncodedQueryItemsSayAA12URLQueryItemVGSgvg`  
  Foundation.URLComponents.percentEncodedQueryItems.getter : [Foundation.URLQueryItem]?
- `$s10Foundation13URLComponentsV24percentEncodedQueryItemsSayAA12URLQueryItemVGSgvs`  
  Foundation.URLComponents.percentEncodedQueryItems.setter : [Foundation.URLQueryItem]?
- `$s10Foundation13URLComponentsV36_unconditionallyBridgeFromObjectiveCyACSo15NSURLComponentsCSgFZ`  
  static Foundation.URLComponents._unconditionallyBridgeFromObjectiveC(__C.NSURLComponents?) -> Foundation.URLComponents
- `$s10Foundation13URLComponentsV3url10relativeToAA3URLVSgAH_tF`  
  Foundation.URLComponents.url(relativeTo: Foundation.URL?) -> Foundation.URL?
- `$s10Foundation13URLComponentsV4pathSSvM`  
  Foundation.URLComponents.path.modify : Swift.String
- `$s10Foundation13URLComponentsVSQAAMc`  
  protocol conformance descriptor for Foundation.URLComponents : Swift.Equatable in Foundation

## AttributeScopes (11)

- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc`  
  protocol conformance descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.ForegroundColorAttribute : Foundation.AttributedStringKey in UIKit
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV015ForegroundColorB0OMn`  
  nominal type descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.ForegroundColorAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV015ForegroundColorB0ON`  
  type metadata for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.ForegroundColorAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV04FontB0OAA19AttributedStringKeyADMc`  
  protocol conformance descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.FontAttribute : Foundation.AttributedStringKey in UIKit
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV04FontB0OMn`  
  nominal type descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.FontAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV04FontB0ON`  
  type metadata for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.FontAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV15foregroundColorAF010ForegroundgB0Ovg`  
  (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.foregroundColor.getter : (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.ForegroundColorAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV15foregroundColorAF010ForegroundgB0OvpMV`  
  property descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.foregroundColor : (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.ForegroundColorAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV4fontAF04FontB0Ovg`  
  (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.font.getter : (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.FontAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV4fontAF04FontB0OvpMV`  
  property descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.font : (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes.FontAttribute
- `$s10Foundation15AttributeScopesO5UIKitE0D10AttributesVMn`  
  nominal type descriptor for (extension in UIKit):Foundation.AttributeScopes.UIKitAttributes

## Locale (11)

- `$s10Foundation6LocaleV10components14fromIdentifierSDyS2SGSS_tFZ`  
  static Foundation.Locale.components(fromIdentifier: Swift.String) -> [Swift.String : Swift.String]
- `$s10Foundation6LocaleV10identifier14fromComponentsSSSDyS2SG_tFZ`  
  static Foundation.Locale.identifier(fromComponents: [Swift.String : Swift.String]) -> Swift.String
- `$s10Foundation6LocaleV10identifierSSvpMV`  
  property descriptor for Foundation.Locale.identifier : Swift.String
- `$s10Foundation6LocaleV15localizedString13forIdentifierSSSgSS_tF`  
  Foundation.Locale.localizedString(forIdentifier: Swift.String) -> Swift.String?
- `$s10Foundation6LocaleV15localizedString13forRegionCodeSSSgSS_tF`  
  Foundation.Locale.localizedString(forRegionCode: Swift.String) -> Swift.String?
- `$s10Foundation6LocaleV15localizedString15forLanguageCodeSSSgSS_tF`  
  Foundation.Locale.localizedString(forLanguageCode: Swift.String) -> Swift.String?
- `$s10Foundation6LocaleV16isoCurrencyCodesSaySSGvgZ`  
  static Foundation.Locale.isoCurrencyCodes.getter : [Swift.String]
- `$s10Foundation6LocaleV18preferredLanguagesSaySSGvgZ`  
  static Foundation.Locale.preferredLanguages.getter : [Swift.String]
- `$s10Foundation6LocaleVSHAAMc`  
  protocol conformance descriptor for Foundation.Locale : Swift.Hashable in Foundation
- `$s10Foundation6LocaleVSQAAMc`  
  protocol conformance descriptor for Foundation.Locale : Swift.Equatable in Foundation
- `$s10Foundation6LocaleVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for Foundation.Locale : Swift.CustomStringConvertible in Foundation

## _BridgedNSError (10)

- `$s10Foundation15_BridgedNSErrorMp`  
  protocol descriptor for Foundation._BridgedNSError
- `$s10Foundation15_BridgedNSErrorP14_nsErrorDomainSSvgZTq`  
  method descriptor for static Foundation._BridgedNSError._nsErrorDomain.getter : Swift.String
- `$s10Foundation15_BridgedNSErrorP8RawValueSY_s17FixedWidthIntegerTn`  
  associated conformance descriptor for Foundation._BridgedNSError.Swift.RawRepresentable.RawValue: Swift.FixedWidthInteger
- `$s10Foundation15_BridgedNSErrorPAA26_ObjectiveCBridgeableErrorTb`  
  base conformance descriptor for Foundation._BridgedNSError: Foundation._ObjectiveCBridgeableError
- `$s10Foundation15_BridgedNSErrorPAAE08_bridgedC0xSgSo0C0Ch_tcfC`  
  (extension in Foundation):Foundation._BridgedNSError.init(_bridgedNSError: __shared __C.NSError) -> A?
- `$s10Foundation15_BridgedNSErrorPAAE4hash4intoys6HasherVz_tF`  
  (extension in Foundation):Foundation._BridgedNSError.hash(into: inout Swift.Hasher) -> ()
- `$s10Foundation15_BridgedNSErrorPAAE5_codeSivg`  
  (extension in Foundation):Foundation._BridgedNSError._code.getter : Swift.Int
- `$s10Foundation15_BridgedNSErrorPAAE7_domainSSvg`  
  (extension in Foundation):Foundation._BridgedNSError._domain.getter : Swift.String
- `$s10Foundation15_BridgedNSErrorPSHTb`  
  base conformance descriptor for Foundation._BridgedNSError: Swift.Hashable
- `$s10Foundation15_BridgedNSErrorPSYTb`  
  base conformance descriptor for Foundation._BridgedNSError: Swift.RawRepresentable

## NSScanner (10)

- `$sSo9NSScannerC10FoundationE10scanDouble14representationSdSgAbCE20NumberRepresentationO_tF`  
  (extension in Foundation):__C.NSScanner.scanDouble(representation: (extension in Foundation):__C.NSScanner.NumberRepresentation) -> Swift.Double?
- `$sSo9NSScannerC10FoundationE10scanStringySSSgSSF`  
  (extension in Foundation):__C.NSScanner.scanString(Swift.String) -> Swift.String?
- `$sSo9NSScannerC10FoundationE12currentIndexSS0D0Vvg`  
  (extension in Foundation):__C.NSScanner.currentIndex.getter : Swift.String.Index
- `$sSo9NSScannerC10FoundationE12currentIndexSS0D0Vvs`  
  (extension in Foundation):__C.NSScanner.currentIndex.setter : Swift.String.Index
- `$sSo9NSScannerC10FoundationE14scanCharacters4fromSSSgAC12CharacterSetV_tF`  
  (extension in Foundation):__C.NSScanner.scanCharacters(from: Foundation.CharacterSet) -> Swift.String?
- `$sSo9NSScannerC10FoundationE14scanUpToStringySSSgSSF`  
  (extension in Foundation):__C.NSScanner.scanUpToString(Swift.String) -> Swift.String?
- `$sSo9NSScannerC10FoundationE18scanUpToCharacters4fromSSSgAC12CharacterSetV_tF`  
  (extension in Foundation):__C.NSScanner.scanUpToCharacters(from: Foundation.CharacterSet) -> Swift.String?
- `$sSo9NSScannerC10FoundationE20NumberRepresentationO7decimalyA2EmFWC`  
  enum case for (extension in Foundation):__C.NSScanner.NumberRepresentation.decimal((extension in Foundation):__C.NSScanner.NumberRepresentation.Type) -> (extension in Foundation):__C.NSScanner.NumberRepresentation
- `$sSo9NSScannerC10FoundationE20NumberRepresentationOMa`  
  type metadata accessor for (extension in Foundation):__C.NSScanner.NumberRepresentation
- `$sSo9NSScannerC10FoundationE9scanInt3214representations0D0VSgAbCE20NumberRepresentationO_tF`  
  (extension in Foundation):__C.NSScanner.scanInt32(representation: (extension in Foundation):__C.NSScanner.NumberRepresentation) -> Swift.Int32?

## FloatingPointFormatStyle (9)

- `$s10Foundation24FloatingPointFormatStyleV16decimalSeparator8strategyACyxGAA06NumberdE13ConfigurationO07DecimalG15DisplayStrategyV_tF`  
  Foundation.FloatingPointFormatStyle.decimalSeparator(strategy: Foundation.NumberFormatStyleConfiguration.DecimalSeparatorDisplayStrategy) -> Foundation.FloatingPointFormatStyle<A>
- `$s10Foundation24FloatingPointFormatStyleV6formatySSxF`  
  Foundation.FloatingPointFormatStyle.format(A) -> Swift.String
- `$s10Foundation24FloatingPointFormatStyleV6localeACyxGAA6LocaleV_tcfC`  
  Foundation.FloatingPointFormatStyle.init(locale: Foundation.Locale) -> Foundation.FloatingPointFormatStyle<A>
- `$s10Foundation24FloatingPointFormatStyleV8groupingyACyxGAA06NumberdE13ConfigurationO8GroupingVF`  
  Foundation.FloatingPointFormatStyle.grouping(Foundation.NumberFormatStyleConfiguration.Grouping) -> Foundation.FloatingPointFormatStyle<A>
- `$s10Foundation24FloatingPointFormatStyleV8notationyACyxGAA06NumberdE13ConfigurationO8NotationVF`  
  Foundation.FloatingPointFormatStyle.notation(Foundation.NumberFormatStyleConfiguration.Notation) -> Foundation.FloatingPointFormatStyle<A>
- `$s10Foundation24FloatingPointFormatStyleV9precisionyACyxGAA06NumberdE13ConfigurationO9PrecisionVF`  
  Foundation.FloatingPointFormatStyle.precision(Foundation.NumberFormatStyleConfiguration.Precision) -> Foundation.FloatingPointFormatStyle<A>
- `$s10Foundation24FloatingPointFormatStyleVMa`  
  type metadata accessor for Foundation.FloatingPointFormatStyle
- `$s10Foundation24FloatingPointFormatStyleVMn`  
  nominal type descriptor for Foundation.FloatingPointFormatStyle
- `$s10Foundation24FloatingPointFormatStyleVyxGAA0dE0AAMc`  
  protocol conformance descriptor for Foundation.FloatingPointFormatStyle<A> : Foundation.FormatStyle in Foundation

## NSFileHandle (9)

- `$sSo12NSFileHandleC10FoundationE10AsyncBytesVMa`  
  type metadata accessor for (extension in Foundation):__C.NSFileHandle.AsyncBytes
- `$sSo12NSFileHandleC10FoundationE10AsyncBytesVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSFileHandle.AsyncBytes
- `$sSo12NSFileHandleC10FoundationE10AsyncBytesVSciACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSFileHandle.AsyncBytes : Swift.AsyncSequence in Foundation
- `$sSo12NSFileHandleC10FoundationE4read9upToCountAC4DataVSgSi_tKF`  
  (extension in Foundation):__C.NSFileHandle.read(upToCount: Swift.Int) throws -> Foundation.Data?
- `$sSo12NSFileHandleC10FoundationE5bytesAbCE10AsyncBytesVvg`  
  (extension in Foundation):__C.NSFileHandle.bytes.getter : (extension in Foundation):__C.NSFileHandle.AsyncBytes
- `$sSo12NSFileHandleC10FoundationE5write10contentsOfyx_tKAC12DataProtocolRzlF`  
  (extension in Foundation):__C.NSFileHandle.write<A where A: Foundation.DataProtocol>(contentsOf: A) throws -> ()
- `$sSo12NSFileHandleC10FoundationE6offsets6UInt64VyKF`  
  (extension in Foundation):__C.NSFileHandle.offset() throws -> Swift.UInt64
- `$sSo12NSFileHandleC10FoundationE9readToEndAC4DataVSgyKF`  
  (extension in Foundation):__C.NSFileHandle.readToEnd() throws -> Foundation.Data?
- `$sSo12NSFileHandleC10FoundationE9seekToEnds6UInt64VyKF`  
  (extension in Foundation):__C.NSFileHandle.seekToEnd() throws -> Swift.UInt64

## NSRunLoop (8)

- `$sSo9NSRunLoopC10FoundationE16SchedulerOptionsVMa`  
  type metadata accessor for (extension in Foundation):__C.NSRunLoop.SchedulerOptions
- `$sSo9NSRunLoopC10FoundationE16SchedulerOptionsVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSRunLoop.SchedulerOptions
- `$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideV12floatLiteralAGSd_tcfC`  
  (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride.init(floatLiteral: Swift.Double) -> (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride
- `$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideV12millisecondsyAGSiFZ`  
  static (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride.milliseconds(Swift.Int) -> (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride
- `$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideV7secondsyAGSdFZ`  
  static (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride.seconds(Swift.Double) -> (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride
- `$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideVMa`  
  type metadata accessor for (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride
- `$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSRunLoop.SchedulerTimeType.Stride
- `$sSo9NSRunLoopC7Combine9Scheduler10FoundationMc`  
  protocol conformance descriptor for __C.NSRunLoop : Combine.Scheduler in Foundation

## _NSRange (7)

- `$sSo8_NSRangeV10FoundationE12intersectionyABSgABF`  
  (extension in Foundation):__C._NSRange.intersection(__C._NSRange) -> __C._NSRange?
- `$sSo8_NSRangeV10FoundationE_2inABx_q_tcSXRzSyR_SS5IndexV5BoundRtzr0_lufC`  
  (extension in Foundation):__C._NSRange.init<A, B where A: Swift.RangeExpression, B: Swift.StringProtocol, A.Bound == Swift.String.Index>(_: A, in: B) -> __C._NSRange
- `$sSo8_NSRangeV10FoundationEyABxcSXRzs17FixedWidthInteger5BoundRpzlufC`  
  (extension in Foundation):__C._NSRange.init<A where A: Swift.RangeExpression, A.Bound: Swift.FixedWidthInteger>(A) -> __C._NSRange
- `$sSo8_NSRangeVSH10FoundationMc`  
  protocol conformance descriptor for __C._NSRange : Swift.Hashable in Foundation
- `$sSo8_NSRangeVSQ10FoundationMc`  
  protocol conformance descriptor for __C._NSRange : Swift.Equatable in Foundation
- `$sSo8_NSRangeVs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for __C._NSRange : Swift._ObjectiveCBridgeable in Foundation
- `$sSo8_NSRangeVs23CustomStringConvertible10FoundationMc`  
  protocol conformance descriptor for __C._NSRange : Swift.CustomStringConvertible in Foundation

## AttributedString (6)

- `$s10Foundation16AttributedStringV13CharacterViewVSTAAMc`  
  protocol conformance descriptor for Foundation.AttributedString.CharacterView : Swift.Sequence in Foundation
- `$s10Foundation16AttributedStringV17FormattingOptionsVMa`  
  type metadata accessor for Foundation.AttributedString.FormattingOptions
- `$s10Foundation16AttributedStringV17FormattingOptionsVMn`  
  nominal type descriptor for Foundation.AttributedString.FormattingOptions
- `$s10Foundation16AttributedStringV17FormattingOptionsVs10SetAlgebraAAMc`  
  protocol conformance descriptor for Foundation.AttributedString.FormattingOptions : Swift.SetAlgebra in Foundation
- `$s10Foundation16AttributedStringV9localized7options5table6bundle6locale7commentACSSAAE17LocalizationValueV_AC17FormattingOptionsVSSSgSo8NSBundleCSgAA6LocaleVSgs06StaticC0VSgtcfC`  
  Foundation.AttributedString.init(localized: (extension in Foundation):Swift.String.LocalizationValue, options: Foundation.AttributedString.FormattingOptions, table: Swift.String?, bundle: __C.NSBundle?, locale: Foundation.Locale?, comment: Swift.StaticString?) -> Foundation.AttributedString
- `$s10Foundation16AttributedStringVy5ValueQzSgxmcAA0bC3KeyRzluis`  
  Foundation.AttributedString.subscript.setter : <A where A: Foundation.AttributedStringKey>(A.Type) -> A.Value?

## UUID (6)

- `$s10Foundation4UUIDVSEAAMc`  
  protocol conformance descriptor for Foundation.UUID : Swift.Encodable in Foundation
- `$s10Foundation4UUIDVSHAAMc`  
  protocol conformance descriptor for Foundation.UUID : Swift.Hashable in Foundation
- `$s10Foundation4UUIDVSQAAMc`  
  protocol conformance descriptor for Foundation.UUID : Swift.Equatable in Foundation
- `$s10Foundation4UUIDVSeAAMc`  
  protocol conformance descriptor for Foundation.UUID : Swift.Decodable in Foundation
- `$s10Foundation4UUIDVs21_ObjectiveCBridgeableAAMc`  
  protocol conformance descriptor for Foundation.UUID : Swift._ObjectiveCBridgeable in Foundation
- `$s10Foundation4UUIDVs23CustomStringConvertibleAAMc`  
  protocol conformance descriptor for Foundation.UUID : Swift.CustomStringConvertible in Foundation

## NSURLSessionWebSocketTask (6)

- `$sSo25NSURLSessionWebSocketTaskC10FoundationE4send_17completionHandleryAbCE7MessageO_ys5Error_pSgctF`  
  (extension in Foundation):__C.NSURLSessionWebSocketTask.send(_: (extension in Foundation):__C.NSURLSessionWebSocketTask.Message, completionHandler: (Swift.Error?) -> ()) -> ()
- `$sSo25NSURLSessionWebSocketTaskC10FoundationE7MessageO4datayAeC4DataVcAEmFWC`  
  enum case for (extension in Foundation):__C.NSURLSessionWebSocketTask.Message.data((extension in Foundation):__C.NSURLSessionWebSocketTask.Message.Type) -> (Foundation.Data) -> (extension in Foundation):__C.NSURLSessionWebSocketTask.Message
- `$sSo25NSURLSessionWebSocketTaskC10FoundationE7MessageO6stringyAESScAEmFWC`  
  enum case for (extension in Foundation):__C.NSURLSessionWebSocketTask.Message.string((extension in Foundation):__C.NSURLSessionWebSocketTask.Message.Type) -> (Swift.String) -> (extension in Foundation):__C.NSURLSessionWebSocketTask.Message
- `$sSo25NSURLSessionWebSocketTaskC10FoundationE7MessageOMa`  
  type metadata accessor for (extension in Foundation):__C.NSURLSessionWebSocketTask.Message
- `$sSo25NSURLSessionWebSocketTaskC10FoundationE7MessageOMn`  
  nominal type descriptor for (extension in Foundation):__C.NSURLSessionWebSocketTask.Message
- `$sSo25NSURLSessionWebSocketTaskC10FoundationE7receive17completionHandleryys6ResultOyAbCE7MessageOs5Error_pGc_tF`  
  (extension in Foundation):__C.NSURLSessionWebSocketTask.receive(completionHandler: (Swift.Result<(extension in Foundation):__C.NSURLSessionWebSocketTask.Message, Swift.Error>) -> ()) -> ()

## __DataStorage (5)

- `$s10Foundation13__DataStorageC12_deallocatorySv_SitcSgvg`  
  Foundation.__DataStorage._deallocator.getter : ((Swift.UnsafeMutableRawPointer, Swift.Int) -> ())?
- `$s10Foundation13__DataStorageC5bytes6length4copy11deallocator6offsetACSvSg_SiSbySv_SitcSgSitcfc`  
  Foundation.__DataStorage.init(bytes: Swift.UnsafeMutableRawPointer?, length: Swift.Int, copy: Swift.Bool, deallocator: ((Swift.UnsafeMutableRawPointer, Swift.Int) -> ())?, offset: Swift.Int) -> Foundation.__DataStorage
- `$s10Foundation13__DataStorageC6lengthACSi_tcfc`  
  Foundation.__DataStorage.init(length: Swift.Int) -> Foundation.__DataStorage
- `$s10Foundation13__DataStorageC8capacityACSi_tcfc`  
  Foundation.__DataStorage.init(capacity: Swift.Int) -> Foundation.__DataStorage
- `$s10Foundation13__DataStorageC9_capacitySivM`  
  Foundation.__DataStorage._capacity.modify : Swift.Int

## AsyncLineSequence (5)

- `$s10Foundation17AsyncLineSequenceV04makeB8IteratorAC0bF0Vyx_GyF`  
  Foundation.AsyncLineSequence.makeAsyncIterator() -> Foundation.AsyncLineSequence<A>.AsyncIterator
- `$s10Foundation17AsyncLineSequenceV0B8IteratorV4nextSSSgyYaKF`  
  Foundation.AsyncLineSequence.AsyncIterator.next() async throws -> Swift.String?
- `$s10Foundation17AsyncLineSequenceV0B8IteratorV4nextSSSgyYaKFTu`  
  async function pointer to Foundation.AsyncLineSequence.AsyncIterator.next() async throws -> Swift.String?
- `$s10Foundation17AsyncLineSequenceV0B8IteratorVMn`  
  nominal type descriptor for Foundation.AsyncLineSequence.AsyncIterator
- `$s10Foundation17AsyncLineSequenceVMn`  
  nominal type descriptor for Foundation.AsyncLineSequence

## KeyPathComparator (5)

- `$s10Foundation17KeyPathComparatorV03keyC0s07PartialbC0CyxGvg`  
  Foundation.KeyPathComparator.keyPath.getter : Swift.PartialKeyPath<A>
- `$s10Foundation17KeyPathComparatorV5orderAA9SortOrderOvg`  
  Foundation.KeyPathComparator.order.getter : Foundation.SortOrder
- `$s10Foundation17KeyPathComparatorVMa`  
  type metadata accessor for Foundation.KeyPathComparator
- `$s10Foundation17KeyPathComparatorVMn`  
  nominal type descriptor for Foundation.KeyPathComparator
- `$s10Foundation17KeyPathComparatorV_5orderACyxGs0bC0Cyxqd__G_AA9SortOrderOtcSLRd__lufC`  
  Foundation.KeyPathComparator.init<A where A1: Swift.Comparable>(_: Swift.KeyPath<A, A1>, order: Foundation.SortOrder) -> Foundation.KeyPathComparator<A>

## NumberFormatStyleConfiguration (5)

- `$s10Foundation30NumberFormatStyleConfigurationO31DecimalSeparatorDisplayStrategyV6alwaysAEvgZ`  
  static Foundation.NumberFormatStyleConfiguration.DecimalSeparatorDisplayStrategy.always.getter : Foundation.NumberFormatStyleConfiguration.DecimalSeparatorDisplayStrategy
- `$s10Foundation30NumberFormatStyleConfigurationO31DecimalSeparatorDisplayStrategyVMa`  
  type metadata accessor for Foundation.NumberFormatStyleConfiguration.DecimalSeparatorDisplayStrategy
- `$s10Foundation30NumberFormatStyleConfigurationO8GroupingV5neverAEvgZ`  
  static Foundation.NumberFormatStyleConfiguration.Grouping.never.getter : Foundation.NumberFormatStyleConfiguration.Grouping
- `$s10Foundation30NumberFormatStyleConfigurationO8GroupingVMa`  
  type metadata accessor for Foundation.NumberFormatStyleConfiguration.Grouping
- `$s10Foundation30NumberFormatStyleConfigurationO8NotationV10scientificAEvgZ`  
  static Foundation.NumberFormatStyleConfiguration.Notation.scientific.getter : Foundation.NumberFormatStyleConfiguration.Notation

## NSProgress (5)

- `$sSo10NSProgressC10FoundationE14fileTotalCountSiSgvs`  
  (extension in Foundation):__C.NSProgress.fileTotalCount.setter : Swift.Int?
- `$sSo10NSProgressC10FoundationE18fileCompletedCountSiSgvg`  
  (extension in Foundation):__C.NSProgress.fileCompletedCount.getter : Swift.Int?
- `$sSo10NSProgressC10FoundationE18fileCompletedCountSiSgvs`  
  (extension in Foundation):__C.NSProgress.fileCompletedCount.setter : Swift.Int?
- `$sSo10NSProgressC10FoundationE22estimatedTimeRemainingSdSgvg`  
  (extension in Foundation):__C.NSProgress.estimatedTimeRemaining.getter : Swift.Double?
- `$sSo10NSProgressC10FoundationE22estimatedTimeRemainingSdSgvs`  
  (extension in Foundation):__C.NSProgress.estimatedTimeRemaining.setter : Swift.Double?

## NSCoder (5)

- `$sSo7NSCoderC10FoundationE12decodeObject2of6forKeyxSgxm_SStSo8NSObjectCRbzSo8NSCodingRzlF`  
  (extension in Foundation):__C.NSCoder.decodeObject<A where A: __C.NSObject, A: __C.NSCoding>(of: A.Type, forKey: Swift.String) -> A?
- `$sSo7NSCoderC10FoundationE12decodeObject2of6forKeyypSgSayyXlXpGSg_SStF`  
  (extension in Foundation):__C.NSCoder.decodeObject(of: [Swift.AnyObject.Type]?, forKey: Swift.String) -> Any?
- `$sSo7NSCoderC10FoundationE16decodeDictionary12withKeyClass06objectG003forF0SDyxq_GSgxm_q_mSStSo8NSObjectCRbzSo9NSCopyingRzSo14NSSecureCodingRzAKRb_SoAMR_r0_lF`  
  (extension in Foundation):__C.NSCoder.decodeDictionary<A, B where A: __C.NSObject, A: __C.NSCopying, A: __C.NSSecureCoding, B: __C.NSObject, B: __C.NSSecureCoding>(withKeyClass: A.Type, objectClass: B.Type, forKey: Swift.String) -> [A : B]?
- `$sSo7NSCoderC10FoundationE20decodeArrayOfObjects7ofClass6forKeySayxGSgxm_SStSo8NSObjectCRbzSo14NSSecureCodingRzlF`  
  (extension in Foundation):__C.NSCoder.decodeArrayOfObjects<A where A: __C.NSObject, A: __C.NSSecureCoding>(ofClass: A.Type, forKey: Swift.String) -> [A]?
- `$sSo7NSCoderC10FoundationE20decodeTopLevelObject2of6forKeyypSgSayyXlXpGSg_SStKF`  
  (extension in Foundation):__C.NSCoder.decodeTopLevelObject(of: [Swift.AnyObject.Type]?, forKey: Swift.String) throws -> Any?

## NSTimer (5)

- `$sSo7NSTimerC10FoundationE14TimerPublisherC7Combine011ConnectableD0ACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSTimer.TimerPublisher : Combine.ConnectablePublisher in Foundation
- `$sSo7NSTimerC10FoundationE14TimerPublisherC7connect7Combine11Cancellable_pyF`  
  (extension in Foundation):__C.NSTimer.TimerPublisher.connect() -> Combine.Cancellable
- `$sSo7NSTimerC10FoundationE14TimerPublisherCMa`  
  type metadata accessor for (extension in Foundation):__C.NSTimer.TimerPublisher
- `$sSo7NSTimerC10FoundationE14TimerPublisherCMn`  
  nominal type descriptor for (extension in Foundation):__C.NSTimer.TimerPublisher
- `$sSo7NSTimerC10FoundationE7publish5every9tolerance2on2in7optionsAbCE14TimerPublisherCSd_SdSgSo9NSRunLoopCSo0kL4ModeaAnCE16SchedulerOptionsVSgtFZ`  
  static (extension in Foundation):__C.NSTimer.publish(every: Swift.Double, tolerance: Swift.Double?, on: __C.NSRunLoop, in: __C.NSRunLoopMode, options: (extension in Foundation):__C.NSRunLoop.SchedulerOptions?) -> (extension in Foundation):__C.NSTimer.TimerPublisher

## CharacterSet (4)

- `$s10Foundation12CharacterSetV19_bridgeToObjectiveCSo011NSCharacterC0CyF`  
  Foundation.CharacterSet._bridgeToObjectiveC() -> __C.NSCharacterSet
- `$s10Foundation12CharacterSetV36_unconditionallyBridgeFromObjectiveCyACSo011NSCharacterC0CSgFZ`  
  static Foundation.CharacterSet._unconditionallyBridgeFromObjectiveC(__C.NSCharacterSet?) -> Foundation.CharacterSet
- `$s10Foundation12CharacterSetV6insertySb8inserted_s7UnicodeO6ScalarV17memberAfterInserttAIF`  
  Foundation.CharacterSet.insert(Swift.Unicode.Scalar) -> (inserted: Swift.Bool, memberAfterInsert: Swift.Unicode.Scalar)
- `$s10Foundation12CharacterSetVs0C7AlgebraAAMc`  
  protocol conformance descriptor for Foundation.CharacterSet : Swift.SetAlgebra in Foundation

## _ErrorCodeProtocol (4)

- `$s10Foundation18_ErrorCodeProtocolMp`  
  protocol descriptor for Foundation._ErrorCodeProtocol
- `$s10Foundation18_ErrorCodeProtocolP01_B4TypeAC_AA21_BridgedStoredNSErrorTn`  
  associated conformance descriptor for Foundation._ErrorCodeProtocol.Foundation._ErrorCodeProtocol._ErrorType: Foundation._BridgedStoredNSError
- `$s10Foundation18_ErrorCodeProtocolPAAE2teoiySbx_s0B0_ptFZ`  
  static (extension in Foundation):Foundation._ErrorCodeProtocol.~= infix(A, Swift.Error) -> Swift.Bool
- `$s10Foundation18_ErrorCodeProtocolPSQTb`  
  base conformance descriptor for Foundation._ErrorCodeProtocol: Swift.Equatable

## URLError (4)

- `$s10Foundation8URLErrorV4CodeVAA06_ErrorC8ProtocolAAMc`  
  protocol conformance descriptor for Foundation.URLError.Code : Foundation._ErrorCodeProtocol in Foundation
- `$s10Foundation8URLErrorV4CodeVSQAAMc`  
  protocol conformance descriptor for Foundation.URLError.Code : Swift.Equatable in Foundation
- `$s10Foundation8URLErrorVAA21_BridgedStoredNSErrorAAMc`  
  protocol conformance descriptor for Foundation.URLError : Foundation._BridgedStoredNSError in Foundation
- `$s10Foundation8URLErrorVs5ErrorAAMc`  
  protocol conformance descriptor for Foundation.URLError : Swift.Error in Foundation

## Predicate (4)

- `$s10Foundation9PredicateV10expressionAA08StandardB10Expression_pSb6OutputAA0bE0PRts_XPvg`  
  Foundation.Predicate.expression.getter : any Foundation.StandardPredicateExpression<Self.Foundation.PredicateExpression.Output == Swift.Bool>
- `$s10Foundation9PredicateVMa`  
  type metadata accessor for Foundation.Predicate
- `$s10Foundation9PredicateVMn`  
  nominal type descriptor for Foundation.Predicate
- `$s10Foundation9PredicateVyACyxxQp_QPGAA08StandardB10Expression_pSb6OutputAA0bD0PRts_XPAA0B11ExpressionsO8VariableVy_xGxQpXEcfC`  
  Foundation.Predicate.init((repeat Foundation.PredicateExpressions.Variable<A>) -> any Foundation.StandardPredicateExpression<Self.Foundation.PredicateExpression.Output == Swift.Bool>) -> Foundation.Predicate<Pack{repeat A}>

## NSDictionary (4)

- `$sSo12NSDictionaryC10FoundationE10dictionaryA2Bh_tcfC`  
  (extension in Foundation):__C.NSDictionary.init(dictionary: __shared __C.NSDictionary) -> __C.NSDictionary
- `$sSo12NSDictionaryC10FoundationE12makeIteratorAbCE0D0CyF`  
  (extension in Foundation):__C.NSDictionary.makeIterator() -> (extension in Foundation):__C.NSDictionary.Iterator
- `$sSo12NSDictionaryC10FoundationE8IteratorC4nextyp3key_yp5valuetSgyF`  
  (extension in Foundation):__C.NSDictionary.Iterator.next() -> (key: Any, value: Any)?
- `$sSo12NSDictionaryCST10FoundationMc`  
  protocol conformance descriptor for __C.NSDictionary : Swift.Sequence in Foundation

## UICollectionView (4)

- `$sSo16UICollectionViewC5UIKitE16CellRegistrationV7handlerAEy_xq_Gyx_10Foundation9IndexPathVq_tc_tcfC`  
  (extension in UIKit):__C.UICollectionView.CellRegistration.init(handler: (A, Foundation.IndexPath, B) -> ()) -> (extension in UIKit):__C.UICollectionView.CellRegistration<A, B>
- `$sSo16UICollectionViewC5UIKitE25SupplementaryRegistrationV11elementKind7handlerAEy_xGSS_yx_SS10Foundation9IndexPathVtctcfC`  
  (extension in UIKit):__C.UICollectionView.SupplementaryRegistration.init(elementKind: Swift.String, handler: (A, Swift.String, Foundation.IndexPath) -> ()) -> (extension in UIKit):__C.UICollectionView.SupplementaryRegistration<A>
- `$sSo16UICollectionViewC5UIKitE29dequeueConfiguredReusableCell5using3for4itemxAbCE0G12RegistrationVy_xq_G_10Foundation9IndexPathVq_SgtSo0abG0CRbzr0_lF`  
  (extension in UIKit):__C.UICollectionView.dequeueConfiguredReusableCell<A, B where A: __C.UICollectionViewCell>(using: (extension in UIKit):__C.UICollectionView.CellRegistration<A, B>, for: Foundation.IndexPath, item: B?) -> A
- `$sSo16UICollectionViewC5UIKitE38dequeueConfiguredReusableSupplementary5using3forxAbCE0G12RegistrationVy_xG_10Foundation9IndexPathVtSo0afB0CRbzlF`  
  (extension in UIKit):__C.UICollectionView.dequeueConfiguredReusableSupplementary<A where A: __C.UICollectionReusableView>(using: (extension in UIKit):__C.UICollectionView.SupplementaryRegistration<A>, for: Foundation.IndexPath) -> A

## NSKeyedUnarchiver (4)

- `$sSo17NSKeyedUnarchiverC10FoundationE16unarchivedObject7ofClass4fromxSgxm_AC4DataVtKSo8NSObjectCRbzSo8NSCodingRzlFZ`  
  static (extension in Foundation):__C.NSKeyedUnarchiver.unarchivedObject<A where A: __C.NSObject, A: __C.NSCoding>(ofClass: A.Type, from: Foundation.Data) throws -> A?
- `$sSo17NSKeyedUnarchiverC10FoundationE16unarchivedObject9ofClasses4fromypSgSayyXlXpG_AC4DataVtKFZ`  
  static (extension in Foundation):__C.NSKeyedUnarchiver.unarchivedObject(ofClasses: [Swift.AnyObject.Type], from: Foundation.Data) throws -> Any?
- `$sSo17NSKeyedUnarchiverC10FoundationE23decodeTopLevelDecodable_6forKeyxSgxm_SStKSeRzlF`  
  (extension in Foundation):__C.NSKeyedUnarchiver.decodeTopLevelDecodable<A where A: Swift.Decodable>(_: A.Type, forKey: Swift.String) throws -> A?
- `$sSo17NSKeyedUnarchiverC10FoundationE31unarchiveTopLevelObjectWithDatayypSgAC0I0VKFZ`  
  static (extension in Foundation):__C.NSKeyedUnarchiver.unarchiveTopLevelObjectWithData(Foundation.Data) throws -> Any?

## NSSet (4)

- `$sSo5NSSetC10FoundationE12arrayLiteralABypd_tcfC`  
  (extension in Foundation):__C.NSSet.init(arrayLiteral: Any...) -> __C.NSSet
- `$sSo5NSSetC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF`  
  (extension in Foundation):__C.NSSet.makeIterator() -> Foundation.NSFastEnumerationIterator
- `$sSo5NSSetC10FoundationE3setA2Bh_tcfC`  
  (extension in Foundation):__C.NSSet.init(set: __shared __C.NSSet) -> __C.NSSet
- `$sSo5NSSetCST10FoundationMc`  
  protocol conformance descriptor for __C.NSSet : Swift.Sequence in Foundation

## NSArray (4)

- `$sSo7NSArrayC10FoundationE12arrayLiteralABypd_tcfC`  
  (extension in Foundation):__C.NSArray.init(arrayLiteral: Any...) -> __C.NSArray
- `$sSo7NSArrayC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF`  
  (extension in Foundation):__C.NSArray.makeIterator() -> Foundation.NSFastEnumerationIterator
- `$sSo7NSArrayC10FoundationE5arrayA2Bh_tcfC`  
  (extension in Foundation):__C.NSArray.init(array: __shared __C.NSArray) -> __C.NSArray
- `$sSo7NSArrayCST10FoundationMc`  
  protocol conformance descriptor for __C.NSArray : Swift.Sequence in Foundation

## NSObject (4)

- `$sSo8NSObjectC10Foundation27_KeyValueCodingAndObservingACWP`  
  protocol witness table for __C.NSObject : Foundation._KeyValueCodingAndObserving in Foundation
- `$sSo8NSObjectC10FoundationE26KeyValueObservingPublisherVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSObject.KeyValueObservingPublisher
- `$sSo8NSObjectC10FoundationE26KeyValueObservingPublisherVy_xq_G7Combine0F0ACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSObject.KeyValueObservingPublisher<A, B> : Combine.Publisher in Foundation
- `$sSo8NSObjectCs23CustomStringConvertible10FoundationMc`  
  protocol conformance descriptor for __C.NSObject : Swift.CustomStringConvertible in Foundation

## CocoaError (3)

- `$s10Foundation10CocoaErrorV4CodeVAA01_cD8ProtocolAAMc`  
  protocol conformance descriptor for Foundation.CocoaError.Code : Foundation._ErrorCodeProtocol in Foundation
- `$s10Foundation10CocoaErrorV4CodeVSQAAMc`  
  protocol conformance descriptor for Foundation.CocoaError.Code : Swift.Equatable in Foundation
- `$s10Foundation10CocoaErrorVAA21_BridgedStoredNSErrorAAMc`  
  protocol conformance descriptor for Foundation.CocoaError : Foundation._BridgedStoredNSError in Foundation

## DateInterval (3)

- `$s10Foundation12DateIntervalV19_bridgeToObjectiveCSo06NSDateC0CyF`  
  Foundation.DateInterval._bridgeToObjectiveC() -> __C.NSDateInterval
- `$s10Foundation12DateIntervalVSHAAMc`  
  protocol conformance descriptor for Foundation.DateInterval : Swift.Hashable in Foundation
- `$s10Foundation12DateIntervalVSQAAMc`  
  protocol conformance descriptor for Foundation.DateInterval : Swift.Equatable in Foundation

## URLQueryItem (3)

- `$s10Foundation12URLQueryItemV19_bridgeToObjectiveCSo010NSURLQueryC0CyF`  
  Foundation.URLQueryItem._bridgeToObjectiveC() -> __C.NSURLQueryItem
- `$s10Foundation12URLQueryItemV36_unconditionallyBridgeFromObjectiveCyACSo010NSURLQueryC0CSgFZ`  
  static Foundation.URLQueryItem._unconditionallyBridgeFromObjectiveC(__C.NSURLQueryItem?) -> Foundation.URLQueryItem
- `$s10Foundation12URLQueryItemVSQAAMc`  
  protocol conformance descriptor for Foundation.URLQueryItem : Swift.Equatable in Foundation

## ParseStrategy (3)

- `$s10Foundation13ParseStrategyMp`  
  protocol descriptor for Foundation.ParseStrategy
- `$s10Foundation13ParseStrategyP5parsey0B6OutputQz0B5InputQzKFTj`  
  dispatch thunk of Foundation.ParseStrategy.parse(A.ParseInput) throws -> A.ParseOutput
- `$s10Foundation13ParseStrategyPA2A4DateV18ISO8601FormatStyleVRszrlE7iso8601AGvgZ`  
  static (extension in Foundation):Foundation.ParseStrategy< where A == Foundation.Date.ISO8601FormatStyle>.iso8601.getter : Foundation.Date.ISO8601FormatStyle

## PredicateError (3)

- `$s10Foundation14PredicateErrorV12invalidInputACvgZ`  
  static Foundation.PredicateError.invalidInput.getter : Foundation.PredicateError
- `$s10Foundation14PredicateErrorVMa`  
  type metadata accessor for Foundation.PredicateError
- `$s10Foundation14PredicateErrorVs0C0AAMc`  
  protocol conformance descriptor for Foundation.PredicateError : Swift.Error in Foundation

## IntegerFormatStyle (3)

- `$s10Foundation18IntegerFormatStyleV6formatySSxF`  
  Foundation.IntegerFormatStyle.format(A) -> Swift.String
- `$s10Foundation18IntegerFormatStyleV8groupingyACyxGAA06NumbercD13ConfigurationO8GroupingVF`  
  Foundation.IntegerFormatStyle.grouping(Foundation.NumberFormatStyleConfiguration.Grouping) -> Foundation.IntegerFormatStyle<A>
- `$s10Foundation18IntegerFormatStyleVMa`  
  type metadata accessor for Foundation.IntegerFormatStyle

## PredicateExpression (3)

- `$s10Foundation19PredicateExpressionMp`  
  protocol descriptor for Foundation.PredicateExpression
- `$s10Foundation19PredicateExpressionP8evaluatey6OutputQzAA0B8BindingsVKFTj`  
  dispatch thunk of Foundation.PredicateExpression.evaluate(Foundation.PredicateBindings) throws -> A.Output
- `$s10Foundation19PredicateExpressionTL`  
  protocol requirements base descriptor for Foundation.PredicateExpression

## PropertyListDecoder (3)

- `$s10Foundation19PropertyListDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj`  
  dispatch thunk of Foundation.PropertyListDecoder.decode<A where A: Swift.Decodable>(_: A.Type, from: Foundation.Data) throws -> A
- `$s10Foundation19PropertyListDecoderCACycfc`  
  Foundation.PropertyListDecoder.init() -> Foundation.PropertyListDecoder
- `$s10Foundation19PropertyListDecoderCMa`  
  type metadata accessor for Foundation.PropertyListDecoder

## PropertyListEncoder (3)

- `$s10Foundation19PropertyListEncoderC6encodeyAA4DataVxKSERzlFTj`  
  dispatch thunk of Foundation.PropertyListEncoder.encode<A where A: Swift.Encodable>(A) throws -> Foundation.Data
- `$s10Foundation19PropertyListEncoderCACycfc`  
  Foundation.PropertyListEncoder.init() -> Foundation.PropertyListEncoder
- `$s10Foundation19PropertyListEncoderCMa`  
  type metadata accessor for Foundation.PropertyListEncoder

## NSKeyValueObservation (3)

- `$s10Foundation21NSKeyValueObservationC10invalidateyyFTj`  
  dispatch thunk of Foundation.NSKeyValueObservation.invalidate() -> ()
- `$s10Foundation21NSKeyValueObservationCMa`  
  type metadata accessor for Foundation.NSKeyValueObservation
- `$s10Foundation21NSKeyValueObservationCMn`  
  nominal type descriptor for Foundation.NSKeyValueObservation

## AttributeDynamicLookup (3)

- `$s10Foundation22AttributeDynamicLookupO5UIKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0E10AttributesVxG_tcAA016AttributedStringH0Rzluig`  
  (extension in UIKit):Foundation.AttributeDynamicLookup.subscript.getter : <A where A: Foundation.AttributedStringKey>(dynamicMember: Swift.KeyPath<(extension in UIKit):Foundation.AttributeScopes.UIKitAttributes, A>) -> A
- `$s10Foundation22AttributeDynamicLookupO5UIKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0E10AttributesVxG_tcAA016AttributedStringH0RzluipMV`  
  property descriptor for (extension in UIKit):Foundation.AttributeDynamicLookup.subscript<A where A: Foundation.AttributedStringKey>(dynamicMember: Swift.KeyPath<(extension in UIKit):Foundation.AttributeScopes.UIKitAttributes, A>) -> A
- `$s10Foundation22AttributeDynamicLookupOMn`  
  nominal type descriptor for Foundation.AttributeDynamicLookup

## NSKeyValueObservedChange (3)

- `$s10Foundation24NSKeyValueObservedChangeV03newC0xSgvg`  
  Foundation.NSKeyValueObservedChange.newValue.getter : A?
- `$s10Foundation24NSKeyValueObservedChangeV03oldC0xSgvg`  
  Foundation.NSKeyValueObservedChange.oldValue.getter : A?
- `$s10Foundation24NSKeyValueObservedChangeVMn`  
  nominal type descriptor for Foundation.NSKeyValueObservedChange

## NSFastEnumerationIterator (3)

- `$s10Foundation25NSFastEnumerationIteratorV4nextypSgyF`  
  Foundation.NSFastEnumerationIterator.next() -> Any?
- `$s10Foundation25NSFastEnumerationIteratorVMa`  
  type metadata accessor for Foundation.NSFastEnumerationIterator
- `$s10Foundation25NSFastEnumerationIteratorVStAAMc`  
  protocol conformance descriptor for Foundation.NSFastEnumerationIterator : Swift.IteratorProtocol in Foundation

## _ObjectiveCBridgeableError (3)

- `$s10Foundation26_ObjectiveCBridgeableErrorMp`  
  protocol descriptor for Foundation._ObjectiveCBridgeableError
- `$s10Foundation26_ObjectiveCBridgeableErrorP15_bridgedNSErrorxSgSo0F0Ch_tcfCTq`  
  method descriptor for Foundation._ObjectiveCBridgeableError.init(_bridgedNSError: __shared __C.NSError) -> A?
- `$s10Foundation26_ObjectiveCBridgeableErrorPs0D0Tb`  
  base conformance descriptor for Foundation._ObjectiveCBridgeableError: Swift.Error

## _KeyValueCodingAndObserving (3)

- `$s10Foundation27_KeyValueCodingAndObservingPAAE010willChangeC03forys0B4PathCyxqd__Gn_tlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObserving.willChangeValue<A>(for: __owned Swift.KeyPath<A, A1>) -> ()
- `$s10Foundation27_KeyValueCodingAndObservingPAAE09didChangeC03forys0B4PathCyxqd__Gn_tlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObserving.didChangeValue<A>(for: __owned Swift.KeyPath<A, A1>) -> ()
- `$s10Foundation27_KeyValueCodingAndObservingPAAE7observe_7options13changeHandlerAA05NSKeyC11ObservationCs0B4PathCyxqd__G_So0kcF7OptionsVyx_AA0kC14ObservedChangeVyqd__GtctlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObserving.observe<A>(_: Swift.KeyPath<A, A1>, options: __C.NSKeyValueObservingOptions, changeHandler: (A, Foundation.NSKeyValueObservedChange<A1>) -> ()) -> Foundation.NSKeyValueObservation

## NSError (3)

- `$sSo7NSErrorC10FoundationE5_codeSivg`  
  (extension in Foundation):__C.NSError._code.getter : Swift.Int
- `$sSo7NSErrorC10FoundationE7_domainSSvg`  
  (extension in Foundation):__C.NSError._domain.getter : Swift.String
- `$sSo7NSErrorCs5Error10FoundationMc`  
  protocol conformance descriptor for __C.NSError : Swift.Error in Foundation

## NSNumber (3)

- `$sSo8NSNumberC10FoundationE12floatLiteralABSd_tcfC`  
  (extension in Foundation):__C.NSNumber.init(floatLiteral: Swift.Double) -> __C.NSNumber
- `$sSo8NSNumberC10FoundationE14booleanLiteralABSb_tcfC`  
  (extension in Foundation):__C.NSNumber.init(booleanLiteral: Swift.Bool) -> __C.NSNumber
- `$sSo8NSNumberC10FoundationE14integerLiteralABSi_tcfC`  
  (extension in Foundation):__C.NSNumber.init(integerLiteral: Swift.Int) -> __C.NSNumber

## NSString (3)

- `$sSo8NSStringC10FoundationE13stringLiteralABs12StaticStringV_tcfC`  
  (extension in Foundation):__C.NSString.init(stringLiteral: Swift.StaticString) -> __C.NSString
- `$sSo8NSStringC10FoundationE6format_A2Bh_s7CVarArg_pdtcfC`  
  (extension in Foundation):__C.NSString.init(format: __shared __C.NSString, _: Swift.CVarArg...) -> __C.NSString
- `$sSo8NSStringC10FoundationE6stringA2Bh_tcfC`  
  (extension in Foundation):__C.NSString.init(string: __shared __C.NSString) -> __C.NSString

## FormatStyle (2)

- `$s10Foundation11FormatStyleP6formaty0B6OutputQz0B5InputQzFTj`  
  dispatch thunk of Foundation.FormatStyle.format(A.FormatInput) -> A.FormatOutput
- `$s10Foundation11FormatStylePAAs8DurationVAAE04TimebC0VRszrlE4time7patternA2G7PatternV_tFZ`  
  static (extension in Foundation):Foundation.FormatStyle< where A == (extension in Foundation):Swift.Duration.TimeFormatStyle>.time(pattern: (extension in Foundation):Swift.Duration.TimeFormatStyle.Pattern) -> (extension in Foundation):Swift.Duration.TimeFormatStyle

## DataProtocol (2)

- `$s10Foundation12DataProtocolPAAE9copyBytes2to4fromSiSw_qd__tSXRd__5BoundQyd__5IndexRtzlF`  
  (extension in Foundation):Foundation.DataProtocol.copyBytes<A where A1: Swift.RangeExpression, A.Index == A1.Bound>(to: Swift.UnsafeMutableRawBufferPointer, from: A1) -> Swift.Int
- `$s10Foundation12DataProtocolPAAE9copyBytes2toSiSw_tF`  
  (extension in Foundation):Foundation.DataProtocol.copyBytes(to: Swift.UnsafeMutableRawBufferPointer) -> Swift.Int

## Notification (2)

- `$s10Foundation12NotificationV4name6object8userInfoACSo18NSNotificationNamea_ypSgSDys11AnyHashableVypGSgtcfC`  
  Foundation.Notification.init(name: __C.NSNotificationName, object: Any?, userInfo: [Swift.AnyHashable : Any]?) -> Foundation.Notification
- `$s10Foundation12NotificationV6objectypSgvg`  
  Foundation.Notification.object.getter : Any?

## ListFormatStyle (2)

- `$s10Foundation15ListFormatStyleV5WidthO5shortyAEyxq__GAGmAA0cD0RzSTR_7ElementQy_0C5InputRtzSS0C6OutputRtzr0_lFWC`  
  enum case for Foundation.ListFormatStyle.Width.short<A, B where A: Foundation.FormatStyle, B: Swift.Sequence, A.FormatInput == B.Element, A.FormatOutput == Swift.String>(Foundation.ListFormatStyle<A, B>.Width.Type) -> Foundation.ListFormatStyle<A, B>.Width
- `$s10Foundation15ListFormatStyleV5WidthO8standardyAEyxq__GAGmAA0cD0RzSTR_7ElementQy_0C5InputRtzSS0C6OutputRtzr0_lFWC`  
  enum case for Foundation.ListFormatStyle.Width.standard<A, B where A: Foundation.FormatStyle, B: Swift.Sequence, A.FormatInput == B.Element, A.FormatOutput == Swift.String>(Foundation.ListFormatStyle<A, B>.Width.Type) -> Foundation.ListFormatStyle<A, B>.Width

## PredicateBindings (2)

- `$s10Foundation17PredicateBindingsVMa`  
  type metadata accessor for Foundation.PredicateBindings
- `$s10Foundation17PredicateBindingsVyAcA0B11ExpressionsO8VariableVy_xG_xtxQpcRvzlufC`  
  Foundation.PredicateBindings.init<each A>(repeat (Foundation.PredicateExpressions.Variable<A>, A)) -> Foundation.PredicateBindings

## _AsyncBytesBuffer (2)

- `$s10Foundation17_AsyncBytesBufferV06reloadD7AndNexts5UInt8VSgyYaKF`  
  Foundation._AsyncBytesBuffer.reloadBufferAndNext() async throws -> Swift.UInt8?
- `$s10Foundation17_AsyncBytesBufferV06reloadD7AndNexts5UInt8VSgyYaKFTu`  
  async function pointer to Foundation._AsyncBytesBuffer.reloadBufferAndNext() async throws -> Swift.UInt8?

## TimeZone (2)

- `$s10Foundation8TimeZoneVSHAAMc`  
  protocol conformance descriptor for Foundation.TimeZone : Swift.Hashable in Foundation
- `$s10Foundation8TimeZoneVSQAAMc`  
  protocol conformance descriptor for Foundation.TimeZone : Swift.Equatable in Foundation

## NSExpression (2)

- `$sSo12NSExpressionC10FoundationE10forKeyPathABs0dE0Cyxq_G_tcr0_lufC`  
  (extension in Foundation):__C.NSExpression.init<A, B>(forKeyPath: Swift.KeyPath<A, B>) -> __C.NSExpression
- `$sSo12NSExpressionC10FoundationE6format_ABSSh_s7CVarArg_pdtcfC`  
  (extension in Foundation):__C.NSExpression.init(format: __shared Swift.String, _: Swift.CVarArg...) -> __C.NSExpression

## NSOrderedSet (2)

- `$sSo12NSOrderedSetC10FoundationE12makeIteratorAC017NSFastEnumerationE0VyF`  
  (extension in Foundation):__C.NSOrderedSet.makeIterator() -> Foundation.NSFastEnumerationIterator
- `$sSo12NSOrderedSetCST10FoundationMc`  
  protocol conformance descriptor for __C.NSOrderedSet : Swift.Sequence in Foundation

## NSFileManager (2)

- `$sSo13NSFileManagerC10FoundationE10enumerator2at26includingPropertiesForKeys7options12errorHandlerSo21NSDirectoryEnumeratorCSgAC3URLV_SaySo16NSURLResourceKeyaGSgSo0M18EnumerationOptionsVSbAM_s5Error_ptcSgtF`  
  (extension in Foundation):__C.NSFileManager.enumerator(at: Foundation.URL, includingPropertiesForKeys: [__C.NSURLResourceKey]?, options: __C.NSDirectoryEnumerationOptions, errorHandler: ((Foundation.URL, Swift.Error) -> Swift.Bool)?) -> __C.NSDirectoryEnumerator?
- `$sSo13NSFileManagerC10FoundationE13replaceItemAt_04witheF006backupE4Name7optionsAC3URLVSgAI_AISSSgSo0abE18ReplacementOptionsVtKF`  
  (extension in Foundation):__C.NSFileManager.replaceItemAt(_: Foundation.URL, withItemAt: Foundation.URL, backupItemName: Swift.String?, options: __C.NSFileManagerItemReplacementOptions) throws -> Foundation.URL?

## NSData (2)

- `$sSo6NSDataCST10FoundationMc`  
  protocol conformance descriptor for __C.NSData : Swift.Sequence in Foundation
- `$sSo6NSDataCSl10FoundationMc`  
  protocol conformance descriptor for __C.NSData : Swift.Collection in Foundation

## CGPoint (2)

- `$sSo7CGPointV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF`  
  (extension in Foundation):__C.CGPoint._bridgeToObjectiveC() -> __C.NSValue
- `$sSo7CGPointVs21_ObjectiveCBridgeable10FoundationMc`  
  protocol conformance descriptor for __C.CGPoint : Swift._ObjectiveCBridgeable in Foundation

## UIButton (2)

- `$sSo8UIButtonC5UIKitE13ConfigurationV15attributedTitle10Foundation16AttributedStringVSgvg`  
  (extension in UIKit):__C.UIButton.Configuration.attributedTitle.getter : Foundation.AttributedString?
- `$sSo8UIButtonC5UIKitE13ConfigurationV15attributedTitle10Foundation16AttributedStringVSgvs`  
  (extension in UIKit):__C.UIButton.Configuration.attributedTitle.setter : Foundation.AttributedString?

## POSIXError (1)

- `$s10Foundation10POSIXErrorVAA21_BridgedStoredNSErrorAAMc`  
  protocol conformance descriptor for Foundation.POSIXError : Foundation._BridgedStoredNSError in Foundation

## StringStyle (1)

- `$s10Foundation11StringStyleVMn`  
  nominal type descriptor for Foundation.StringStyle

## ContiguousBytes (1)

- `$s10Foundation15ContiguousBytesMp`  
  protocol descriptor for Foundation.ContiguousBytes

## NSLocalizedString (1)

- `$s10Foundation17NSLocalizedString_9tableName6bundle5value7commentS2S_SSSgSo8NSBundleCS2StF`  
  Foundation.NSLocalizedString(_: Swift.String, tableName: Swift.String?, bundle: __C.NSBundle, value: Swift.String, comment: Swift.String) -> Swift.String

## StandardPredicateExpression (1)

- `$s10Foundation27StandardPredicateExpressionMp`  
  protocol descriptor for Foundation.StandardPredicateExpression

## _KeyValueCodingAndObservingPublishing (1)

- `$s10Foundation37_KeyValueCodingAndObservingPublishingPAASo8NSObjectCRbzrlE9publisher3for7optionsAeAE0bcF9PublisherVy_xqd__Gs0B4PathCyxqd__G_So05NSKeycF7OptionsVtlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObservingPublishing< where A: __C.NSObject>.publisher<A>(for: Swift.KeyPath<A, A1>, options: __C.NSKeyValueObservingOptions) -> (extension in Foundation):__C.NSObject.KeyValueObservingPublisher<A, A1>

## NSLog (1)

- `$s10Foundation5NSLogyySS_s7CVarArg_pdtF`  
  Foundation.NSLog(Swift.String, Swift.CVarArg...) -> ()

## SortOrder (1)

- `$s10Foundation9SortOrderO2eeoiySbAC_ACtFZ`  
  static Foundation.SortOrder.== infix(Foundation.SortOrder, Foundation.SortOrder) -> Swift.Bool

## CFErrorRef (1)

- `$sSo10CFErrorRefas5Error10FoundationMc`  
  protocol conformance descriptor for __C.CFErrorRef : Swift.Error in Foundation

## NSIndexSet (1)

- `$sSo10NSIndexSetCST10FoundationMc`  
  protocol conformance descriptor for __C.NSIndexSet : Swift.Sequence in Foundation

## NSPredicate (1)

- `$sSo11NSPredicateC10FoundationE6format_ABSSh_s7CVarArg_pdtcfC`  
  (extension in Foundation):__C.NSPredicate.init(format: __shared Swift.String, _: Swift.CVarArg...) -> __C.NSPredicate

## NSEnumerator (1)

- `$sSo12NSEnumeratorC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF`  
  (extension in Foundation):__C.NSEnumerator.makeIterator() -> Foundation.NSFastEnumerationIterator

## NSUndoManager (1)

- `$sSo13NSUndoManagerC10FoundationE12registerUndo10withTarget7handleryx_yxctRlzClF`  
  (extension in Foundation):__C.NSUndoManager.registerUndo<A where A: AnyObject>(withTarget: A, handler: (A) -> ()) -> ()

## NSKeyedArchiver (1)

- `$sSo15NSKeyedArchiverC10FoundationE15encodeEncodable_6forKeyyx_SStKSERzlF`  
  (extension in Foundation):__C.NSKeyedArchiver.encodeEncodable<A where A: Swift.Encodable>(_: A, forKey: Swift.String) throws -> ()

## CKFetchRecordZoneChangesOperation (1)

- `$sSo33CKFetchRecordZoneChangesOperationC8CloudKitE06recordC16FetchResultBlockySo08CKRecordC2IDC_s0J0OySo19CKServerChangeTokenC06serveroP0_10Foundation4DataVSg06clientopS0Sb10moreComingts5Error_pGtcSgvs`  
  (extension in CloudKit):__C.CKFetchRecordZoneChangesOperation.recordZoneFetchResultBlock.setter : ((__C.CKRecordZoneID, Swift.Result<(serverChangeToken: __C.CKServerChangeToken, clientChangeTokenData: Foundation.Data?, moreComing: Swift.Bool), Swift.Error>) -> ())?

## CGRect (1)

- `$sSo6CGRectV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF`  
  (extension in Foundation):__C.CGRect._bridgeToObjectiveC() -> __C.NSValue

## CGSize (1)

- `$sSo6CGSizeV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF`  
  (extension in Foundation):__C.CGSize._bridgeToObjectiveC() -> __C.NSValue
