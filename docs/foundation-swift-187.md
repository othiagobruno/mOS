# 187 símbolos de Foundation que o WhatsApp importa

Gerado por `tools/gen_fi_thunks.py`. Estado no x86_64.

Real: **104**. Stub justificado: **14**. Total: **118**.

`real` quer dizer que o thunk tem miolo (constante, metadata com tamanho, ou chamada para o C em `fi_behavior.inc`) e o `fitest` entrou nele. O oráculo de macOS (as mesmas frases do `fmttest`) cobre os grupos de data, ISO8601, inteiro, percentual, moeda, compacto, lista, comparação, `String` format, pessoa, attributed string, decimal, measurement e plist. Os irmãos de um grupo compartilham a função C; o teste chama cada thunk, e a frase de macOS é conferida no representante do grupo mais nos caminhos nomeados em `tools/fitest.c`.

`stub` é só o que não dá para implementar sem inventar ABI: os `Mc` (a ordem dos requisitos do protocolo não foi confirmada; a testemunha devolve 0), as quatro pontes para classes ObjC que este libSystem não tem, e o materialize de KeyPath de escrita.

Nada desta lista ficou sem thunk. O que não foi exercitado é o arm64 (os 204 nomes em `fi_exports.c` continuam `ret`), o construtor ObjC de `NSAttributedString` (só entra com `FI_HOST` desligado) e o `machocheck` contra o binário do WhatsApp, que não está neste ambiente.

| Símbolo | Estado | Nota |
|---|---|---|
| `$s10Foundation11MeasurementVyxGSQAAMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation11MeasurementVyxGSHAAMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation11MeasurementVMn` | real |  |
| `$sSo9NSDecimalas23CustomStringConvertible10FoundationMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation11JSONEncoderC20DateEncodingStrategyO9formattedyAESo15NSDateFormatterCcAEmFWC` | real |  |
| `$s10Foundation11JSONDecoderC20DateDecodingStrategyO9formattedyAESo15NSDateFormatterCcAEmFWC` | real |  |
| `$s10Foundation4DateV11FormatStyleVAadAMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation4DateV11FormatStyleVMa` | real |  |
| `$s10Foundation20PersonNameComponentsVMn` | real |  |
| `$s10Foundation16AttributedStringV22MarkdownParsingOptionsV13FailurePolicyO10throwErroryA2GmFWC` | real |  |
| `$s10Foundation16AttributedStringV22MarkdownParsingOptionsV17InterpretedSyntaxO30inlineOnlyPreservingWhitespaceyA2GmFWC` | real |  |
| `$sSo9NSDecimala10FoundationE11FormatStyleVAcdCMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$sSo9NSDecimala10FoundationE11FormatStyleVMa` | real |  |
| `$s10Foundation15ListFormatStyleVyxq_GAA0cD0AAMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation15ListFormatStyleV5WidthO6narrowyAEyxq__GAGmAA0cD0RzSTR_7ElementQy_0C5InputRtzSS0C6OutputRtzr0_lFWC` | real |  |
| `$s10Foundation15ListFormatStyleV0B4TypeO3andyAEyxq__GAGmAA0cD0RzSTR_7ElementQy_0C5InputRtzSS0C6OutputRtzr0_lFWC` | real |  |
| `$s10Foundation15ListFormatStyleV5WidthOMn` | real |  |
| `$s10Foundation15ListFormatStyleV0B4TypeOMn` | real |  |
| `$s10Foundation15ListFormatStyleVMn` | real |  |
| `$sSo22NSMeasurementFormatterC10FoundationE6string4fromSSAC11MeasurementVyxG_tSo6NSUnitCRbzlF` | real |  |
| `$s10Foundation11MeasurementVMa` | real |  |
| `$sSo9NSDecimala10FoundationE36_unconditionallyBridgeFromObjectiveCyABSo0A6NumberCSgFZ` | stub | NSDecimalNumber guarda double, não os 20 bytes do NSDecimal |
| `$sSo9NSDecimala10FoundationEyABSucfC` | real |  |
| `$sSo9NSDecimala10FoundationEyABs5Int64VcfC` | real |  |
| `$sSo9NSDecimala10FoundationE19_bridgeToObjectiveCSo0A6NumberCyF` | stub | NSDecimalNumber guarda double, não os 20 bytes do NSDecimal |
| `$sSo9NSDecimala10FoundationE11significandABvg` | real |  |
| `$sSo9NSDecimala10FoundationE8exponentSivg` | real |  |
| `$s10Foundation3powySo9NSDecimalaAD_SitF` | real |  |
| `$sSo9NSDecimala10FoundationE6negateyyF` | real |  |
| `$sSo8_NSRangeV10FoundationE_2inABx_q_tcSXRzAC24AttributedStringProtocolR_AC0dE0V5IndexV5BoundRtzr0_lufC` | real |  |
| `$s10Foundation4DateV11FormatStyleV04TimeD0V9shortenedAGvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV0bD0V7omittedAGvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV04TimeD0V7omittedAGvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV0bD0V11abbreviatedAGvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV0bD0VMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV04TimeD0VMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV4yearyA2E6SymbolV4YearVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV4YearV13defaultDigitsAIvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV3dayyA2E6SymbolV3DayVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV3DayV13defaultDigitsAIvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV5monthyA2E6SymbolV5MonthVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV5MonthV11abbreviatedAIvgZ` | real |  |
| `$s10Foundation11FormatStylePA2A4DateVABVRszrlE8dateTimeAFvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV5MonthVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV3DayVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV4YearVMa` | real |  |
| `$sSo9NSDecimala10FoundationE11FormatStyleV8CurrencyV6localeAC6LocaleVvg` | real |  |
| `$sSo9NSDecimala10FoundationE11FormatStyleV8CurrencyVMa` | real |  |
| `$sSo9NSDecimala10FoundationE11FormatStyleV8CurrencyV4code6localeAGSS_AC6LocaleVtcfC` | real |  |
| `$sSo9NSDecimala10FoundationE6string6localeABSgSSh_AC6LocaleVSghtcfC` | real |  |
| `$s10Foundation20PersonNameComponentsV06familyC0SSSgvg` | real |  |
| `$s10Foundation20PersonNameComponentsV36_unconditionallyBridgeFromObjectiveCyACSo08NSPersoncD0CSgFZ` | stub | NSPersonNameComponents não é classe deste libSystem |
| `$s10Foundation20PersonNameComponentsV06middleC0SSSgvg` | real |  |
| `$s10Foundation20PersonNameComponentsV05givenC0SSSgvg` | real |  |
| `$s10Foundation20PersonNameComponentsVMa` | real |  |
| `$sSo9NSDecimala10FoundationE11FormatStyleV8CurrencyV12currencyCodeSSvg` | real |  |
| `$s10Foundation19PropertyListEncoderC12outputFormatSo010NSPropertycF0VvsTj` | real |  |
| `$s10Foundation20PersonNameComponentsV9formattedSSyF` | real |  |
| `$sSo18NSAttributedStringC10FoundationEyAbC010AttributedB0VcfC` | real |  |
| `$sSo18NSAttributedStringC10FoundationE8markdown7options7baseURLABSS_AC010AttributedB0V22MarkdownParsingOptionsVAC0G0VSgtKcfC` | real |  |
| `$s10Foundation16AttributedStringV22MarkdownParsingOptionsV24allowsExtendedAttributes17interpretedSyntax13failurePolicy12languageCodeAESb_AE011InterpretedK0OAE07FailureM0OSSSgtcfC` | real |  |
| `$s10Foundation16AttributedStringV22MarkdownParsingOptionsVMa` | real |  |
| `$s10Foundation16AttributedStringV22MarkdownParsingOptionsV17InterpretedSyntaxOMa` | real |  |
| `$s10Foundation16AttributedStringV22MarkdownParsingOptionsV13FailurePolicyOMa` | real |  |
| `$s10Foundation20PersonNameComponentsV19_bridgeToObjectiveCSo08NSPersoncD0CyF` | stub | NSPersonNameComponents não é classe deste libSystem |
| `$s10Foundation20ParseableFormatStylePA2A4DateV0cD0VRszrlE8dateTimeAGvgZ` | real |  |
| `$sSo9NSDecimala10FoundationE11FormatStyleV8notationyAeC06NumbercD13ConfigurationO8NotationVF` | real |  |
| `$s10Foundation30NumberFormatStyleConfigurationO8NotationV11compactNameAEvgZ` | real |  |
| `$s10Foundation20ParseableFormatStylePAASo9NSDecimalaAAE0cD0VRszrlE6numberAGvgZ` | real |  |
| `$s10Foundation30NumberFormatStyleConfigurationO8NotationVMa` | real |  |
| `$sSo9NSDecimala10FoundationE5isNaNSbvg` | real |  |
| `$sST10FoundationE9formattedy12FormatOutputQyd__qd__0C5InputQyd__RszAA0C5StyleRd__lF` | real |  |
| `$s10Foundation15ListFormatStyleV6localeyACyxq_GAA6LocaleVF` | real |  |
| `$s10Foundation11FormatStylePAAE4list4type5widthAA04ListbC0VyAA06StringC0Vqd__GAH0G4TypeOyAJqd___G_AH5WidthOyAJqd___GtAKRszSTRd__SS7ElementRtd__lFZ` | real |  |
| `$s10Foundation18IntegerFormatStyleV4sign8strategyACyxGAA06NumbercD13ConfigurationO19SignDisplayStrategyV_tF` | real |  |
| `$s10Foundation30NumberFormatStyleConfigurationO19SignDisplayStrategyV6always13includingZeroAESb_tFZ` | real |  |
| `$s10Foundation30NumberFormatStyleConfigurationO19SignDisplayStrategyVMa` | real |  |
| `$s10Foundation24FloatingPointFormatStyleV7PercentVyx_GAA0dE0AAMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation24FloatingPointFormatStyleV7PercentVMn` | real |  |
| `$sSo9NSDecimalaSE10FoundationMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$sSo9NSDecimalaSe10FoundationMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation4DateV19IntervalFormatStyleVAA0dE0AAMc` | stub | descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend |
| `$s10Foundation4DateV19IntervalFormatStyleVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV04TimeD0VMn` | real |  |
| `$s10Foundation4DateV11FormatStyleV0bD0VMn` | real |  |
| `$s10Foundation20PersonNameComponentsV06familyC0SSSgvs` | real |  |
| `$s10Foundation20PersonNameComponentsV05givenC0SSSgvs` | real |  |
| `$s10Foundation20PersonNameComponentsVACycfC` | real |  |
| `$s10Foundation20PersonNameComponentsV10nameSuffixSSSgvg` | real |  |
| `$s10Foundation20PersonNameComponentsV10namePrefixSSSgvg` | real |  |
| `$sSB10FoundationE9formattedy12FormatOutputQyd__qd__0C5InputQyd__RszAA0C5StyleRd__lF` | real |  |
| `$s10Foundation24FloatingPointFormatStyleV7PercentV9precisionyAEyx_GAA06NumberdE13ConfigurationO9PrecisionVF` | real |  |
| `$s10Foundation30NumberFormatStyleConfigurationO9PrecisionV14fractionLengthyAESiFZ` | real |  |
| `$s10Foundation24FloatingPointFormatStyleV7PercentV6localeAEyx_GAA6LocaleV_tcfC` | real |  |
| `$s10Foundation30NumberFormatStyleConfigurationO9PrecisionVMa` | real |  |
| `$sSn10FoundationAA4DateVRszrlE9formattedy12FormatOutputQyd__qd__AA0D5StyleRd__SnyACG0D5InputRtd__lF` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV6minuteAEyF` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV4houryAeC0dE0V6SymbolV4HourVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV4HourV13defaultDigits4amPMA2I9AMPMStyleV_tFZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV4HourV9AMPMStyleV11abbreviatedAKvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV6minuteyA2E6SymbolV6MinuteVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV6MinuteV13defaultDigitsAIvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV4houryA2E6SymbolV4HourVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV4date4time6locale8calendar0F4Zone21capitalizationContextA2E0bD0VSg_AE04TimeD0VSgAA6LocaleVAA8CalendarVAA0lI0VAA0cd14CapitalizationK0VtcfC` | real |  |
| `$s10Foundation32FormatStyleCapitalizationContextV7unknownACvgZ` | real |  |
| `$s10Foundation4DateV11FormatStyleV7weekdayyA2E6SymbolV7WeekdayVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV7WeekdayV11abbreviatedAIvgZ` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV4date4time6locale8calendar0G4ZoneAeC0dE0V0bE0VSg_AL04TimeE0VSgAA6LocaleVAA8CalendarVAA0kJ0VtcfC` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV4yearAEyF` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV7weekdayyAeC0dE0V6SymbolV7WeekdayVF` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV3dayAEyF` | real |  |
| `$s10Foundation4DateV19IntervalFormatStyleV5monthyAeC0dE0V6SymbolV5MonthVF` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV7WeekdayVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV4HourVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV4HourV9AMPMStyleVMa` | real |  |
| `$s10Foundation32FormatStyleCapitalizationContextVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV6MinuteVMa` | real |  |
| `$s10Foundation4DateV11FormatStyleV6SymbolV7WeekdayV4wideAIvgZ` | real |  |
