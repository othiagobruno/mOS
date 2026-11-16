# Stubs restantes da frente Foundation/CoreFoundation

Gerado por `tools/fdn-inventario.py` a partir de `sys_zerados.c`. Total: **155**.

| Grupo | Quantidade |
|---|---:|
| extensões | 23 |
| NSURLSession | 15 |
| NSNotificationCenter | 13 |
| PredicateExpressions | 9 |
| FloatingPointFormatStyle | 9 |
| NSRunLoop | 8 |
| NSURLSessionWebSocketTask | 6 |
| AsyncLineSequence | 5 |
| KeyPathComparator | 5 |
| NumberFormatStyleConfiguration | 5 |
| NSProgress | 5 |
| NSTimer | 5 |
| AttributedString | 4 |
| URL | 4 |
| NSFileHandle | 4 |
| NSKeyedUnarchiver | 4 |
| NSObject | 3 |
| ListFormatStyle | 2 |
| URLResourceValues | 2 |
| _AsyncBytesBuffer | 2 |
| _KeyValueCodingAndObserving | 2 |
| NSExpression | 2 |
| NSOrderedSet | 2 |
| NSData | 2 |
| NSCoder | 2 |
| FormatStyle | 1 |
| StringStyle | 1 |
| DateInterval | 1 |
| NSLocalizedString | 1 |
| IntegerFormatStyle | 1 |
| _KeyValueCodingAndObservingPublishing | 1 |
| Data | 1 |
| Date | 1 |
| NSPredicate | 1 |
| NSKeyedArchiver | 1 |
| CKFetchRecordZoneChangesOperation | 1 |
| NSScanner | 1 |

## extensões (23)

- `$s11Compression12OutputFilterC5writeyyxSgK10Foundation12DataProtocolRzlFTj`  
  dispatch thunk of Compression.OutputFilter.write<A where A: Foundation.DataProtocol>(A?) throws -> ()
- `$s11Compression12OutputFilterC_5using14bufferCapacity9writingToAcA0C9OperationO_AA9AlgorithmOSiy10Foundation4DataVSgKctKcfc`  
  Compression.OutputFilter.init(_: Compression.FilterOperation, using: Compression.Algorithm, bufferCapacity: Swift.Int, writingTo: (Foundation.Data?) throws -> ()) throws -> Compression.OutputFilter
- `$s7Network10NWListenerC7ServiceV4name4type6domain9txtRecordAESSSg_SSAJ10Foundation4DataVSgtcfC`  
  Network.NWListener.Service.init(name: Swift.String?, type: Swift.String, domain: Swift.String?, txtRecord: Foundation.Data?) -> Network.NWListener.Service
- `$s7Network12NWConnectionC4send7content0D7Context10isComplete10completiony10Foundation4DataVSg_AC07ContentE0CSbAC14SendCompletionOtF`  
  Network.NWConnection.send(content: Foundation.Data?, contentContext: Network.NWConnection.ContentContext, isComplete: Swift.Bool, completion: Network.NWConnection.SendCompletion) -> ()
- `$s7Network12NWConnectionC7receive23minimumIncompleteLength07maximumF010completionySi_Siy10Foundation4DataVSg_AC14ContentContextCSgSbAA7NWErrorOSgtctF`  
  Network.NWConnection.receive(minimumIncompleteLength: Swift.Int, maximumLength: Swift.Int, completion: (Foundation.Data?, Network.NWConnection.ContentContext?, Swift.Bool, Network.NWError?) -> ()) -> ()
- `$s8Dispatch0A4DataV10Foundation0B8ProtocolADMc`  
  protocol conformance descriptor for Dispatch.DispatchData : Foundation.DataProtocol in Foundation
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
- `$sSayxGs7CVarArg10FoundationMc`  
  protocol conformance descriptor for [A] : Swift.CVarArg in Foundation
- `$sSci10Foundations5UInt8V7ElementRtzrlE5linesAA17AsyncLineSequenceVyxGvg`  
  (extension in Foundation):Swift.AsyncSequence< where A.Element == Swift.UInt8>.lines.getter : Foundation.AsyncLineSequence<A>
- `$sShyxGs7CVarArg10FoundationMc`  
  protocol conformance descriptor for Swift.Set<A> : Swift.CVarArg in Foundation
- `$sSi10Foundation18_FormatSpecifiableAAWP`  
  protocol witness table for Swift.Int : Foundation._FormatSpecifiable in Foundation
- `$sSy10FoundationE12commonPrefix4with7optionsSSqd___So22NSStringCompareOptionsVtSyRd__lF`  
  (extension in Foundation):Swift.StringProtocol.commonPrefix<A where A1: Swift.StringProtocol>(with: A1, options: __C.NSStringCompareOptions) -> Swift.String
- `$sSy10FoundationE17applyingTransform_7reverseSSSgSo08NSStringC0a_SbtF`  
  (extension in Foundation):Swift.StringProtocol.applyingTransform(_: __C.NSStringTransform, reverse: Swift.Bool) -> Swift.String?
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

## NSURLSession (15)

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

## PredicateExpressions (9)

- `$s10Foundation20PredicateExpressionsO22StringLocalizedCompareV4rootxvg`  
  Foundation.PredicateExpressions.StringLocalizedCompare.root.getter : A
- `$s10Foundation20PredicateExpressionsO22StringLocalizedCompareV5otherq_vg`  
  Foundation.PredicateExpressions.StringLocalizedCompare.other.getter : B
- `$s10Foundation20PredicateExpressionsO22StringLocalizedCompareVMn`  
  nominal type descriptor for Foundation.PredicateExpressions.StringLocalizedCompare
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

## AttributedString (4)

- `$s10Foundation16AttributedStringV17FormattingOptionsVMa`  
  type metadata accessor for Foundation.AttributedString.FormattingOptions
- `$s10Foundation16AttributedStringV17FormattingOptionsVMn`  
  nominal type descriptor for Foundation.AttributedString.FormattingOptions
- `$s10Foundation16AttributedStringV17FormattingOptionsVs10SetAlgebraAAMc`  
  protocol conformance descriptor for Foundation.AttributedString.FormattingOptions : Swift.SetAlgebra in Foundation
- `$s10Foundation16AttributedStringV9localized7options5table6bundle6locale7commentACSSAAE17LocalizationValueV_AC17FormattingOptionsVSSSgSo8NSBundleCSgAA6LocaleVSgs06StaticC0VSgtcfC`  
  Foundation.AttributedString.init(localized: (extension in Foundation):Swift.String.LocalizationValue, options: Foundation.AttributedString.FormattingOptions, table: Swift.String?, bundle: __C.NSBundle?, locale: Foundation.Locale?, comment: Swift.StaticString?) -> Foundation.AttributedString

## URL (4)

- `$s10Foundation3URLV10AsyncBytesV04makeC8IteratorAE0cF0VyF`  
  Foundation.URL.AsyncBytes.makeAsyncIterator() -> Foundation.URL.AsyncBytes.AsyncIterator
- `$s10Foundation3URLV10AsyncBytesVMa`  
  type metadata accessor for Foundation.URL.AsyncBytes
- `$s10Foundation3URLV13resourceBytesAC05AsyncD0Vvg`  
  Foundation.URL.resourceBytes.getter : Foundation.URL.AsyncBytes
- `$s10Foundation3URLV27applicationSupportDirectoryACvgZ`  
  static Foundation.URL.applicationSupportDirectory.getter : Foundation.URL

## NSFileHandle (4)

- `$sSo12NSFileHandleC10FoundationE10AsyncBytesVMa`  
  type metadata accessor for (extension in Foundation):__C.NSFileHandle.AsyncBytes
- `$sSo12NSFileHandleC10FoundationE10AsyncBytesVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSFileHandle.AsyncBytes
- `$sSo12NSFileHandleC10FoundationE10AsyncBytesVSciACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSFileHandle.AsyncBytes : Swift.AsyncSequence in Foundation
- `$sSo12NSFileHandleC10FoundationE5bytesAbCE10AsyncBytesVvg`  
  (extension in Foundation):__C.NSFileHandle.bytes.getter : (extension in Foundation):__C.NSFileHandle.AsyncBytes

## NSKeyedUnarchiver (4)

- `$sSo17NSKeyedUnarchiverC10FoundationE16unarchivedObject7ofClass4fromxSgxm_AC4DataVtKSo8NSObjectCRbzSo8NSCodingRzlFZ`  
  static (extension in Foundation):__C.NSKeyedUnarchiver.unarchivedObject<A where A: __C.NSObject, A: __C.NSCoding>(ofClass: A.Type, from: Foundation.Data) throws -> A?
- `$sSo17NSKeyedUnarchiverC10FoundationE16unarchivedObject9ofClasses4fromypSgSayyXlXpG_AC4DataVtKFZ`  
  static (extension in Foundation):__C.NSKeyedUnarchiver.unarchivedObject(ofClasses: [Swift.AnyObject.Type], from: Foundation.Data) throws -> Any?
- `$sSo17NSKeyedUnarchiverC10FoundationE23decodeTopLevelDecodable_6forKeyxSgxm_SStKSeRzlF`  
  (extension in Foundation):__C.NSKeyedUnarchiver.decodeTopLevelDecodable<A where A: Swift.Decodable>(_: A.Type, forKey: Swift.String) throws -> A?
- `$sSo17NSKeyedUnarchiverC10FoundationE31unarchiveTopLevelObjectWithDatayypSgAC0I0VKFZ`  
  static (extension in Foundation):__C.NSKeyedUnarchiver.unarchiveTopLevelObjectWithData(Foundation.Data) throws -> Any?

## NSObject (3)

- `$sSo8NSObjectC10FoundationE26KeyValueObservingPublisherVMn`  
  nominal type descriptor for (extension in Foundation):__C.NSObject.KeyValueObservingPublisher
- `$sSo8NSObjectC10FoundationE26KeyValueObservingPublisherVy_xq_G7Combine0F0ACMc`  
  protocol conformance descriptor for (extension in Foundation):__C.NSObject.KeyValueObservingPublisher<A, B> : Combine.Publisher in Foundation
- `$sSo8NSObjectCs23CustomStringConvertible10FoundationMc`  
  protocol conformance descriptor for __C.NSObject : Swift.CustomStringConvertible in Foundation

## ListFormatStyle (2)

- `$s10Foundation15ListFormatStyleV5WidthO5shortyAEyxq__GAGmAA0cD0RzSTR_7ElementQy_0C5InputRtzSS0C6OutputRtzr0_lFWC`  
  enum case for Foundation.ListFormatStyle.Width.short<A, B where A: Foundation.FormatStyle, B: Swift.Sequence, A.FormatInput == B.Element, A.FormatOutput == Swift.String>(Foundation.ListFormatStyle<A, B>.Width.Type) -> Foundation.ListFormatStyle<A, B>.Width
- `$s10Foundation15ListFormatStyleV5WidthO8standardyAEyxq__GAGmAA0cD0RzSTR_7ElementQy_0C5InputRtzSS0C6OutputRtzr0_lFWC`  
  enum case for Foundation.ListFormatStyle.Width.standard<A, B where A: Foundation.FormatStyle, B: Swift.Sequence, A.FormatInput == B.Element, A.FormatOutput == Swift.String>(Foundation.ListFormatStyle<A, B>.Width.Type) -> Foundation.ListFormatStyle<A, B>.Width

## URLResourceValues (2)

- `$s10Foundation17URLResourceValuesV16isUbiquitousItemSbSgvg`  
  Foundation.URLResourceValues.isUbiquitousItem.getter : Swift.Bool?
- `$s10Foundation17URLResourceValuesV31ubiquitousItemDownloadingStatusSo015NSURLUbiquitousefG0aSgvg`  
  Foundation.URLResourceValues.ubiquitousItemDownloadingStatus.getter : __C.NSURLUbiquitousItemDownloadingStatus?

## _AsyncBytesBuffer (2)

- `$s10Foundation17_AsyncBytesBufferV06reloadD7AndNexts5UInt8VSgyYaKF`  
  Foundation._AsyncBytesBuffer.reloadBufferAndNext() async throws -> Swift.UInt8?
- `$s10Foundation17_AsyncBytesBufferV06reloadD7AndNexts5UInt8VSgyYaKFTu`  
  async function pointer to Foundation._AsyncBytesBuffer.reloadBufferAndNext() async throws -> Swift.UInt8?

## _KeyValueCodingAndObserving (2)

- `$s10Foundation27_KeyValueCodingAndObservingPAAE010willChangeC03forys0B4PathCyxqd__Gn_tlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObserving.willChangeValue<A>(for: __owned Swift.KeyPath<A, A1>) -> ()
- `$s10Foundation27_KeyValueCodingAndObservingPAAE09didChangeC03forys0B4PathCyxqd__Gn_tlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObserving.didChangeValue<A>(for: __owned Swift.KeyPath<A, A1>) -> ()

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

## NSData (2)

- `$sSo6NSDataCST10FoundationMc`  
  protocol conformance descriptor for __C.NSData : Swift.Sequence in Foundation
- `$sSo6NSDataCSl10FoundationMc`  
  protocol conformance descriptor for __C.NSData : Swift.Collection in Foundation

## NSCoder (2)

- `$sSo7NSCoderC10FoundationE12decodeObject2of6forKeyypSgSayyXlXpGSg_SStF`  
  (extension in Foundation):__C.NSCoder.decodeObject(of: [Swift.AnyObject.Type]?, forKey: Swift.String) -> Any?
- `$sSo7NSCoderC10FoundationE20decodeArrayOfObjects7ofClass6forKeySayxGSgxm_SStSo8NSObjectCRbzSo14NSSecureCodingRzlF`  
  (extension in Foundation):__C.NSCoder.decodeArrayOfObjects<A where A: __C.NSObject, A: __C.NSSecureCoding>(ofClass: A.Type, forKey: Swift.String) -> [A]?

## FormatStyle (1)

- `$s10Foundation11FormatStylePAAs8DurationVAAE04TimebC0VRszrlE4time7patternA2G7PatternV_tFZ`  
  static (extension in Foundation):Foundation.FormatStyle< where A == (extension in Foundation):Swift.Duration.TimeFormatStyle>.time(pattern: (extension in Foundation):Swift.Duration.TimeFormatStyle.Pattern) -> (extension in Foundation):Swift.Duration.TimeFormatStyle

## StringStyle (1)

- `$s10Foundation11StringStyleVMn`  
  nominal type descriptor for Foundation.StringStyle

## DateInterval (1)

- `$s10Foundation12DateIntervalV19_bridgeToObjectiveCSo06NSDateC0CyF`  
  Foundation.DateInterval._bridgeToObjectiveC() -> __C.NSDateInterval

## NSLocalizedString (1)

- `$s10Foundation17NSLocalizedString_9tableName6bundle5value7commentS2S_SSSgSo8NSBundleCS2StF`  
  Foundation.NSLocalizedString(_: Swift.String, tableName: Swift.String?, bundle: __C.NSBundle, value: Swift.String, comment: Swift.String) -> Swift.String

## IntegerFormatStyle (1)

- `$s10Foundation18IntegerFormatStyleV8groupingyACyxGAA06NumbercD13ConfigurationO8GroupingVF`  
  Foundation.IntegerFormatStyle.grouping(Foundation.NumberFormatStyleConfiguration.Grouping) -> Foundation.IntegerFormatStyle<A>

## _KeyValueCodingAndObservingPublishing (1)

- `$s10Foundation37_KeyValueCodingAndObservingPublishingPAASo8NSObjectCRbzrlE9publisher3for7optionsAeAE0bcF9PublisherVy_xqd__Gs0B4PathCyxqd__G_So05NSKeycF7OptionsVtlF`  
  (extension in Foundation):Foundation._KeyValueCodingAndObservingPublishing< where A: __C.NSObject>.publisher<A>(for: Swift.KeyPath<A, A1>, options: __C.NSKeyValueObservingOptions) -> (extension in Foundation):__C.NSObject.KeyValueObservingPublisher<A, A1>

## Data (1)

- `$s10Foundation4DataV6append10contentsOfySays5UInt8VG_tF`  
  Foundation.Data.append(contentsOf: [Swift.UInt8]) -> ()

## Date (1)

- `$s10Foundation4DateV8CloudKit21CKRecordValueProtocolADWP`  
  protocol witness table for Foundation.Date : CloudKit.CKRecordValueProtocol in CloudKit

## NSPredicate (1)

- `$sSo11NSPredicateC10FoundationE6format_ABSSh_s7CVarArg_pdtcfC`  
  (extension in Foundation):__C.NSPredicate.init(format: __shared Swift.String, _: Swift.CVarArg...) -> __C.NSPredicate

## NSKeyedArchiver (1)

- `$sSo15NSKeyedArchiverC10FoundationE15encodeEncodable_6forKeyyx_SStKSERzlF`  
  (extension in Foundation):__C.NSKeyedArchiver.encodeEncodable<A where A: Swift.Encodable>(_: A, forKey: Swift.String) throws -> ()

## CKFetchRecordZoneChangesOperation (1)

- `$sSo33CKFetchRecordZoneChangesOperationC8CloudKitE06recordC16FetchResultBlockySo08CKRecordC2IDC_s0J0OySo19CKServerChangeTokenC06serveroP0_10Foundation4DataVSg06clientopS0Sb10moreComingts5Error_pGtcSgvs`  
  (extension in CloudKit):__C.CKFetchRecordZoneChangesOperation.recordZoneFetchResultBlock.setter : ((__C.CKRecordZoneID, Swift.Result<(serverChangeToken: __C.CKServerChangeToken, clientChangeTokenData: Foundation.Data?, moreComing: Swift.Bool), Swift.Error>) -> ())?

## NSScanner (1)

- `$sSo9NSScannerC10FoundationE9scanInt3214representations0D0VSgAbCE20NumberRepresentationO_tF`  
  (extension in Foundation):__C.NSScanner.scanInt32(representation: (extension in Foundation):__C.NSScanner.NumberRepresentation) -> Swift.Int32?
