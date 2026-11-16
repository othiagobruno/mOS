// mOS — NSCalendarUnit (NSCalendar.Unit) para o SwiftUI.framework. O tools/build-swiftui-objc.py copia o texto abaixo do
// #pragma once para o Foundation.h da cópia do SDK, antes do @interface NSCalendar: o import-as-member do Swift só junta o tipo à classe
// quando os dois estão no mesmo módulo. Some quando o Foundation.h desta variante declarar o NSCalendarUnit.
#pragma once
typedef NS_OPTIONS(NSUInteger, NSCalendarUnit) {
    NSCalendarUnitEra = (1UL << 1),
    NSCalendarUnitYear = (1UL << 2),
    NSCalendarUnitMonth = (1UL << 3),
    NSCalendarUnitDay = (1UL << 4),
    NSCalendarUnitHour = (1UL << 5),
    NSCalendarUnitMinute = (1UL << 6),
    NSCalendarUnitSecond = (1UL << 7),
    NSCalendarUnitWeekday = (1UL << 9),
    NSCalendarUnitWeekdayOrdinal = (1UL << 10),
    NSCalendarUnitQuarter = (1UL << 11),
    NSCalendarUnitWeekOfMonth = (1UL << 12),
    NSCalendarUnitWeekOfYear = (1UL << 13),
    NSCalendarUnitYearForWeekOfYear = (1UL << 14),
    NSCalendarUnitNanosecond = (1UL << 15),
    NSCalendarUnitCalendar = (1UL << 20),
    NSCalendarUnitTimeZone = (1UL << 21),
} NS_SWIFT_NAME(NSCalendar.Unit);
