/*
 * mOS libSystem — unidades e medidas da Foundation: NSUnit, NSDimension, NSUnitConverter, NSUnitConverterLinear,
 * as 22 dimensões da Apple com todas as unidades (símbolo, coeficiente e constante para a unidade base), NSMeasurement
 * e NSMeasurementFormatter. A formatação é a da ICU (unumf com os identificadores de unidade do CLDR), no estilo e no
 * sistema de medidas do locale, como no macOS.
 *
 * Coeficientes: valor na unidade base = coeficiente * valor + constante (conversor linear) ou
 * coeficiente / valor (consumo de combustível em mpg, conversor recíproco).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_fmt_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP cx_lookup(cx_class_t *cls, SEL sel);
double floor(double x);
double fabs(double x);
const char *fdn_locale_ident_of(id loc);

static id cls(const char *n) { return (id)cx_class_named(n); }
static void super_dealloc(id self) {
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static BOOL yes_m(id k, SEL c) { (void)k; (void)c; return YES; }

/* ---------------------------------------------------------------- tabela */
enum { D_ACCEL, D_ANGLE, D_AREA, D_CONC_MASS, D_DISPERSION, D_DURATION, D_ECHARGE, D_ECURRENT, D_EPOTENTIAL,
       D_ERESIST, D_ENERGY, D_FREQ, D_FUEL, D_INFO, D_LENGTH, D_ILLUM, D_MASS, D_POWER, D_PRESSURE, D_SPEED, D_TEMP,
       D_VOLUME, D_N };
static const char *const dim_names[D_N] = {
    "NSUnitAcceleration", "NSUnitAngle", "NSUnitArea", "NSUnitConcentrationMass", "NSUnitDispersion", "NSUnitDuration",
    "NSUnitElectricCharge", "NSUnitElectricCurrent", "NSUnitElectricPotentialDifference", "NSUnitElectricResistance",
    "NSUnitEnergy", "NSUnitFrequency", "NSUnitFuelEfficiency", "NSUnitInformationStorage", "NSUnitLength",
    "NSUnitIlluminance", "NSUnitMass", "NSUnitPower", "NSUnitPressure", "NSUnitSpeed", "NSUnitTemperature", "NSUnitVolume",
};
static const char *const dim_base[D_N] = {
    "metersPerSecondSquared", "degrees", "squareMeters", "gramsPerLiter", "partsPerMillion", "seconds", "coulombs",
    "amperes", "volts", "ohms", "joules", "hertz", "litersPer100Kilometers", "bits", "meters", "lux", "kilograms",
    "watts", "newtonsPerMetersSquared", "metersPerSecond", "kelvin", "liters",
};
typedef struct { int dim; const char *sel, *sym; double coef, konst; const char *cldr; int recip; int sys; } udef_t;
/* sys: 0 métrico (ou neutro), 1 EUA, 2 imperial (Reino Unido) — usado na escala natural e na unidade preferida */
#define K1024 1024.0
static const udef_t U[] = {
    {D_ACCEL, "metersPerSecondSquared", "m/s²", 1, 0, "acceleration-meter-per-square-second", 0, 0},
    {D_ACCEL, "gravity", "g", 9.81, 0, "acceleration-g-force", 0, 0},
    {D_ANGLE, "degrees", "°", 1, 0, "angle-degree", 0, 0},
    {D_ANGLE, "arcMinutes", "ʹ", 0.016667, 0, "angle-arc-minute", 0, 0},
    {D_ANGLE, "arcSeconds", "ʺ", 0.00027778, 0, "angle-arc-second", 0, 0},
    {D_ANGLE, "radians", "rad", 57.2958, 0, "angle-radian", 0, 0},
    {D_ANGLE, "gradians", "grad", 0.9, 0, 0, 0, 0},
    {D_ANGLE, "revolutions", "rev", 360, 0, "angle-revolution", 0, 0},
    {D_AREA, "squareMegameters", "Mm²", 1e12, 0, 0, 0, 0},
    {D_AREA, "squareKilometers", "km²", 1e6, 0, "area-square-kilometer", 0, 0},
    {D_AREA, "squareMeters", "m²", 1, 0, "area-square-meter", 0, 0},
    {D_AREA, "squareCentimeters", "cm²", 0.0001, 0, "area-square-centimeter", 0, 0},
    {D_AREA, "squareMillimeters", "mm²", 0.000001, 0, 0, 0, 0},
    {D_AREA, "squareMicrometers", "µm²", 1e-12, 0, 0, 0, 0},
    {D_AREA, "squareNanometers", "nm²", 1e-18, 0, 0, 0, 0},
    {D_AREA, "squareInches", "in²", 0.00064516, 0, "area-square-inch", 0, 1},
    {D_AREA, "squareFeet", "ft²", 0.092903, 0, "area-square-foot", 0, 1},
    {D_AREA, "squareYards", "yd²", 0.836127, 0, "area-square-yard", 0, 1},
    {D_AREA, "squareMiles", "mi²", 2.59e6, 0, "area-square-mile", 0, 1},
    {D_AREA, "acres", "ac", 4046.86, 0, "area-acre", 0, 1},
    {D_AREA, "ares", "a", 100, 0, 0, 0, 0},
    {D_AREA, "hectares", "ha", 10000, 0, "area-hectare", 0, 0},
    {D_CONC_MASS, "gramsPerLiter", "g/L", 1, 0, 0, 0, 0},
    {D_CONC_MASS, "milligramsPerDeciliter", "mg/dL", 0.01, 0, "concentr-milligram-ofglucose-per-deciliter", 0, 0},
    {D_DISPERSION, "partsPerMillion", "ppm", 1, 0, "concentr-permillion", 0, 0},
    {D_DURATION, "hours", "hr", 3600, 0, "duration-hour", 0, 0},
    {D_DURATION, "minutes", "min", 60, 0, "duration-minute", 0, 0},
    {D_DURATION, "seconds", "s", 1, 0, "duration-second", 0, 0},
    {D_DURATION, "milliseconds", "ms", 0.001, 0, "duration-millisecond", 0, 0},
    {D_DURATION, "microseconds", "µs", 0.000001, 0, "duration-microsecond", 0, 0},
    {D_DURATION, "nanoseconds", "ns", 1e-9, 0, "duration-nanosecond", 0, 0},
    {D_DURATION, "picoseconds", "ps", 1e-12, 0, 0, 0, 0},
    {D_ECHARGE, "coulombs", "C", 1, 0, 0, 0, 0},
    {D_ECHARGE, "megaampereHours", "MAh", 3.6e9, 0, 0, 0, 0},
    {D_ECHARGE, "kiloampereHours", "kAh", 3.6e6, 0, 0, 0, 0},
    {D_ECHARGE, "ampereHours", "Ah", 3600, 0, 0, 0, 0},
    {D_ECHARGE, "milliampereHours", "mAh", 3.6, 0, 0, 0, 0},
    {D_ECHARGE, "microampereHours", "µAh", 0.0036, 0, 0, 0, 0},
    {D_ECURRENT, "megaamperes", "MA", 1e6, 0, 0, 0, 0},
    {D_ECURRENT, "kiloamperes", "kA", 1e3, 0, 0, 0, 0},
    {D_ECURRENT, "amperes", "A", 1, 0, "electric-ampere", 0, 0},
    {D_ECURRENT, "milliamperes", "mA", 0.001, 0, "electric-milliampere", 0, 0},
    {D_ECURRENT, "microamperes", "µA", 0.000001, 0, 0, 0, 0},
    {D_EPOTENTIAL, "megavolts", "MV", 1e6, 0, 0, 0, 0},
    {D_EPOTENTIAL, "kilovolts", "kV", 1e3, 0, 0, 0, 0},
    {D_EPOTENTIAL, "volts", "V", 1, 0, "electric-volt", 0, 0},
    {D_EPOTENTIAL, "millivolts", "mV", 0.001, 0, 0, 0, 0},
    {D_EPOTENTIAL, "microvolts", "µV", 0.000001, 0, 0, 0, 0},
    {D_ERESIST, "megaohms", "MΩ", 1e6, 0, 0, 0, 0},
    {D_ERESIST, "kiloohms", "kΩ", 1e3, 0, 0, 0, 0},
    {D_ERESIST, "ohms", "Ω", 1, 0, "electric-ohm", 0, 0},
    {D_ERESIST, "milliohms", "mΩ", 0.001, 0, 0, 0, 0},
    {D_ERESIST, "microohms", "µΩ", 0.000001, 0, 0, 0, 0},
    {D_ENERGY, "kilojoules", "kJ", 1000, 0, "energy-kilojoule", 0, 0},
    {D_ENERGY, "joules", "J", 1, 0, "energy-joule", 0, 0},
    {D_ENERGY, "kilocalories", "kCal", 4184, 0, "energy-kilocalorie", 0, 0},
    {D_ENERGY, "calories", "cal", 4.184, 0, "energy-calorie", 0, 0},
    {D_ENERGY, "kilowattHours", "kWh", 3.6e6, 0, "energy-kilowatt-hour", 0, 0},
    {D_FREQ, "terahertz", "THz", 1e12, 0, 0, 0, 0},
    {D_FREQ, "gigahertz", "GHz", 1e9, 0, "frequency-gigahertz", 0, 0},
    {D_FREQ, "megahertz", "MHz", 1e6, 0, "frequency-megahertz", 0, 0},
    {D_FREQ, "kilohertz", "kHz", 1e3, 0, "frequency-kilohertz", 0, 0},
    {D_FREQ, "hertz", "Hz", 1, 0, "frequency-hertz", 0, 0},
    {D_FREQ, "millihertz", "mHz", 0.001, 0, 0, 0, 0},
    {D_FREQ, "microhertz", "µHz", 0.000001, 0, 0, 0, 0},
    {D_FREQ, "nanohertz", "nHz", 1e-9, 0, 0, 0, 0},
    {D_FREQ, "framesPerSecond", "FPS", 1, 0, 0, 0, 0},
    {D_FUEL, "litersPer100Kilometers", "L/100km", 1, 0, "consumption-liter-per-100-kilometer", 0, 0},
    {D_FUEL, "milesPerImperialGallon", "mpg", 282.481, 0, "consumption-mile-per-gallon-imperial", 1, 2},
    {D_FUEL, "milesPerGallon", "mpg", 235.215, 0, "consumption-mile-per-gallon", 1, 1},
    {D_INFO, "bytes", "B", 8, 0, "digital-byte", 0, 0},
    {D_INFO, "bits", "bit", 1, 0, "digital-bit", 0, 0},
    {D_INFO, "nibbles", "nibble", 4, 0, 0, 0, 0},
    {D_INFO, "yottabytes", "YB", 8e24, 0, 0, 0, 0},
    {D_INFO, "zettabytes", "ZB", 8e21, 0, 0, 0, 0},
    {D_INFO, "exabytes", "EB", 8e18, 0, 0, 0, 0},
    {D_INFO, "petabytes", "PB", 8e15, 0, "digital-petabyte", 0, 0},
    {D_INFO, "terabytes", "TB", 8e12, 0, "digital-terabyte", 0, 0},
    {D_INFO, "gigabytes", "GB", 8e9, 0, "digital-gigabyte", 0, 0},
    {D_INFO, "megabytes", "MB", 8e6, 0, "digital-megabyte", 0, 0},
    {D_INFO, "kilobytes", "kB", 8e3, 0, "digital-kilobyte", 0, 0},
    {D_INFO, "yottabits", "Yb", 1e24, 0, 0, 0, 0},
    {D_INFO, "zettabits", "Zb", 1e21, 0, 0, 0, 0},
    {D_INFO, "exabits", "Eb", 1e18, 0, 0, 0, 0},
    {D_INFO, "petabits", "Pb", 1e15, 0, 0, 0, 0},
    {D_INFO, "terabits", "Tb", 1e12, 0, "digital-terabit", 0, 0},
    {D_INFO, "gigabits", "Gb", 1e9, 0, "digital-gigabit", 0, 0},
    {D_INFO, "megabits", "Mb", 1e6, 0, "digital-megabit", 0, 0},
    {D_INFO, "kilobits", "kb", 1e3, 0, "digital-kilobit", 0, 0},
    {D_INFO, "yobibytes", "YiB", 8 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "zebibytes", "ZiB", 8 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "exbibytes", "EiB", 8 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "pebibytes", "PiB", 8 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "tebibytes", "TiB", 8 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "gibibytes", "GiB", 8 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "mebibytes", "MiB", 8 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "kibibytes", "KiB", 8 * K1024, 0, 0, 0, 0},
    {D_INFO, "yobibits", "Yib", K1024 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "zebibits", "Zib", K1024 * K1024 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "exbibits", "Eib", K1024 * K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "pebibits", "Pib", K1024 * K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "tebibits", "Tib", K1024 * K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "gibibits", "Gib", K1024 * K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "mebibits", "Mib", K1024 * K1024, 0, 0, 0, 0},
    {D_INFO, "kibibits", "Kib", K1024, 0, 0, 0, 0},
    {D_LENGTH, "megameters", "Mm", 1e6, 0, 0, 0, 0},
    {D_LENGTH, "kilometers", "km", 1000, 0, "length-kilometer", 0, 0},
    {D_LENGTH, "hectometers", "hm", 100, 0, 0, 0, 0},
    {D_LENGTH, "decameters", "dam", 10, 0, 0, 0, 0},
    {D_LENGTH, "meters", "m", 1, 0, "length-meter", 0, 0},
    {D_LENGTH, "decimeters", "dm", 0.1, 0, "length-decimeter", 0, 0},
    {D_LENGTH, "centimeters", "cm", 0.01, 0, "length-centimeter", 0, 0},
    {D_LENGTH, "millimeters", "mm", 0.001, 0, "length-millimeter", 0, 0},
    {D_LENGTH, "micrometers", "µm", 0.000001, 0, "length-micrometer", 0, 0},
    {D_LENGTH, "nanometers", "nm", 1e-9, 0, "length-nanometer", 0, 0},
    {D_LENGTH, "picometers", "pm", 1e-12, 0, "length-picometer", 0, 0},
    {D_LENGTH, "inches", "in", 0.0254, 0, "length-inch", 0, 1},
    {D_LENGTH, "feet", "ft", 0.3048, 0, "length-foot", 0, 1},
    {D_LENGTH, "yards", "yd", 0.9144, 0, "length-yard", 0, 1},
    {D_LENGTH, "miles", "mi", 1609.34, 0, "length-mile", 0, 1},
    {D_LENGTH, "scandinavianMiles", "smi", 10000, 0, "length-mile-scandinavian", 0, 0},
    {D_LENGTH, "lightyears", "ly", 9.461e15, 0, "length-light-year", 0, 0},
    {D_LENGTH, "nauticalMiles", "NM", 1852, 0, "length-nautical-mile", 0, 0},
    {D_LENGTH, "fathoms", "ftm", 1.8288, 0, "length-fathom", 0, 0},
    {D_LENGTH, "furlongs", "fur", 201.168, 0, "length-furlong", 0, 0},
    {D_LENGTH, "astronomicalUnits", "ua", 1.496e11, 0, "length-astronomical-unit", 0, 0},
    {D_LENGTH, "parsecs", "pc", 3.086e16, 0, "length-parsec", 0, 0},
    {D_ILLUM, "lux", "lx", 1, 0, "light-lux", 0, 0},
    {D_MASS, "kilograms", "kg", 1, 0, "mass-kilogram", 0, 0},
    {D_MASS, "grams", "g", 0.001, 0, "mass-gram", 0, 0},
    {D_MASS, "decigrams", "dg", 0.0001, 0, 0, 0, 0},
    {D_MASS, "centigrams", "cg", 0.00001, 0, 0, 0, 0},
    {D_MASS, "milligrams", "mg", 0.000001, 0, "mass-milligram", 0, 0},
    {D_MASS, "micrograms", "µg", 1e-9, 0, "mass-microgram", 0, 0},
    {D_MASS, "nanograms", "ng", 1e-12, 0, 0, 0, 0},
    {D_MASS, "picograms", "pg", 1e-15, 0, 0, 0, 0},
    {D_MASS, "ounces", "oz", 0.0283495, 0, "mass-ounce", 0, 1},
    {D_MASS, "pounds", "lb", 0.453592, 0, "mass-pound", 0, 1},
    {D_MASS, "stones", "st", 6.35029, 0, "mass-stone", 0, 2},
    {D_MASS, "metricTons", "t", 1000, 0, "mass-tonne", 0, 0},
    {D_MASS, "shortTons", "ton", 907.185, 0, "mass-ton", 0, 1},
    {D_MASS, "carats", "ct", 0.0002, 0, "mass-carat", 0, 0},
    {D_MASS, "ouncesTroy", "oz t", 0.03110348, 0, "mass-ounce-troy", 0, 0},
    {D_MASS, "slugs", "slug", 14.5939, 0, 0, 0, 0},
    {D_POWER, "terawatts", "TW", 1e12, 0, 0, 0, 0},
    {D_POWER, "gigawatts", "GW", 1e9, 0, "power-gigawatt", 0, 0},
    {D_POWER, "megawatts", "MW", 1e6, 0, "power-megawatt", 0, 0},
    {D_POWER, "kilowatts", "kW", 1e3, 0, "power-kilowatt", 0, 0},
    {D_POWER, "watts", "W", 1, 0, "power-watt", 0, 0},
    {D_POWER, "milliwatts", "mW", 0.001, 0, "power-milliwatt", 0, 0},
    {D_POWER, "microwatts", "µW", 0.000001, 0, 0, 0, 0},
    {D_POWER, "nanowatts", "nW", 1e-9, 0, 0, 0, 0},
    {D_POWER, "picowatts", "pW", 1e-12, 0, 0, 0, 0},
    {D_POWER, "femtowatts", "fW", 1e-15, 0, 0, 0, 0},
    {D_POWER, "horsepower", "hp", 745.7, 0, "power-horsepower", 0, 1},
    {D_PRESSURE, "newtonsPerMetersSquared", "N/m²", 1, 0, "pressure-pascal", 0, 0},
    {D_PRESSURE, "gigapascals", "GPa", 1e9, 0, 0, 0, 0},
    {D_PRESSURE, "megapascals", "MPa", 1e6, 0, "pressure-megapascal", 0, 0},
    {D_PRESSURE, "kilopascals", "kPa", 1e3, 0, "pressure-kilopascal", 0, 0},
    {D_PRESSURE, "hectopascals", "hPa", 100, 0, "pressure-hectopascal", 0, 0},
    {D_PRESSURE, "inchesOfMercury", "inHg", 3386.39, 0, "pressure-inch-ofhg", 0, 1},
    {D_PRESSURE, "bars", "bar", 1e5, 0, "pressure-bar", 0, 0},
    {D_PRESSURE, "millibars", "mbar", 100, 0, "pressure-millibar", 0, 0},
    {D_PRESSURE, "millimetersOfMercury", "mmHg", 133.322, 0, "pressure-millimeter-ofhg", 0, 0},
    {D_PRESSURE, "poundsForcePerSquareInch", "psi", 6894.76, 0, "pressure-pound-force-per-square-inch", 0, 1},
    {D_SPEED, "metersPerSecond", "m/s", 1, 0, "speed-meter-per-second", 0, 0},
    {D_SPEED, "kilometersPerHour", "km/h", 0.277778, 0, "speed-kilometer-per-hour", 0, 0},
    {D_SPEED, "milesPerHour", "mph", 0.44704, 0, "speed-mile-per-hour", 0, 1},
    {D_SPEED, "knots", "kn", 0.514444, 0, "speed-knot", 0, 0},
    {D_TEMP, "kelvin", "K", 1, 0, "temperature-kelvin", 0, 0},
    {D_TEMP, "celsius", "°C", 1, 273.15, "temperature-celsius", 0, 0},
    {D_TEMP, "fahrenheit", "°F", 0.55555555555556, 255.37222222222427, "temperature-fahrenheit", 0, 1},
    {D_VOLUME, "megaliters", "ML", 1e6, 0, "volume-megaliter", 0, 0},
    {D_VOLUME, "kiloliters", "kL", 1000, 0, 0, 0, 0},
    {D_VOLUME, "liters", "L", 1, 0, "volume-liter", 0, 0},
    {D_VOLUME, "deciliters", "dL", 0.1, 0, "volume-deciliter", 0, 0},
    {D_VOLUME, "centiliters", "cL", 0.01, 0, "volume-centiliter", 0, 0},
    {D_VOLUME, "milliliters", "mL", 0.001, 0, "volume-milliliter", 0, 0},
    {D_VOLUME, "cubicKilometers", "km³", 1e12, 0, "volume-cubic-kilometer", 0, 0},
    {D_VOLUME, "cubicMeters", "m³", 1000, 0, "volume-cubic-meter", 0, 0},
    {D_VOLUME, "cubicDecimeters", "dm³", 1, 0, 0, 0, 0},
    {D_VOLUME, "cubicCentimeters", "cm³", 0.001, 0, "volume-cubic-centimeter", 0, 0},
    {D_VOLUME, "cubicMillimeters", "mm³", 0.000001, 0, 0, 0, 0},
    {D_VOLUME, "cubicInches", "in³", 0.0163871, 0, "volume-cubic-inch", 0, 1},
    {D_VOLUME, "cubicFeet", "ft³", 28.3168, 0, "volume-cubic-foot", 0, 1},
    {D_VOLUME, "cubicYards", "yd³", 764.555, 0, "volume-cubic-yard", 0, 1},
    {D_VOLUME, "cubicMiles", "mi³", 4.168e12, 0, "volume-cubic-mile", 0, 1},
    {D_VOLUME, "acreFeet", "af", 1.233e6, 0, "volume-acre-foot", 0, 1},
    {D_VOLUME, "bushels", "bsh", 35.2391, 0, "volume-bushel", 0, 1},
    {D_VOLUME, "teaspoons", "tsp", 0.00492892, 0, "volume-teaspoon", 0, 1},
    {D_VOLUME, "tablespoons", "tbsp", 0.0147868, 0, "volume-tablespoon", 0, 1},
    {D_VOLUME, "fluidOunces", "fl oz", 0.0295735, 0, "volume-fluid-ounce", 0, 1},
    {D_VOLUME, "cups", "cup", 0.24, 0, "volume-cup", 0, 1},
    {D_VOLUME, "pints", "pt", 0.473176, 0, "volume-pint", 0, 1},
    {D_VOLUME, "quarts", "qt", 0.946353, 0, "volume-quart", 0, 1},
    {D_VOLUME, "gallons", "gal", 3.78541, 0, "volume-gallon", 0, 1},
    {D_VOLUME, "imperialTeaspoons", "tsp", 0.00591939, 0, 0, 0, 2},
    {D_VOLUME, "imperialTablespoons", "tbsp", 0.0177582, 0, 0, 0, 2},
    {D_VOLUME, "imperialFluidOunces", "fl oz", 0.0284131, 0, "volume-fluid-ounce-imperial", 0, 2},
    {D_VOLUME, "imperialPints", "pt", 0.568261, 0, 0, 0, 2},
    {D_VOLUME, "imperialQuarts", "qt", 1.13652, 0, 0, 0, 2},
    {D_VOLUME, "imperialGallons", "gal", 4.54609, 0, "volume-gallon-imperial", 0, 2},
    {D_VOLUME, "metricCups", "metric cup", 0.25, 0, "volume-cup-metric", 0, 0},
};
#define NU ((int)(sizeof U / sizeof U[0]))

/* ---------------------------------------------------------------- classes */
#define KL(v, n) cx_class_t v##_c __asm__("OBJC_CLASS_$_" n); cx_class_t v##_m __asm__("OBJC_METACLASS_$_" n);
KL(unit, "NSUnit") KL(dim, "NSDimension") KL(conv, "NSUnitConverter") KL(lin, "NSUnitConverterLinear")
KL(meas, "NSMeasurement") KL(mf, "NSMeasurementFormatter")
static cx_class_t rec_c, rec_m;
KL(d0, "NSUnitAcceleration")
KL(d1, "NSUnitAngle")
KL(d2, "NSUnitArea")
KL(d3, "NSUnitConcentrationMass")
KL(d4, "NSUnitDispersion")
KL(d5, "NSUnitDuration")
KL(d6, "NSUnitElectricCharge")
KL(d7, "NSUnitElectricCurrent")
KL(d8, "NSUnitElectricPotentialDifference")
KL(d9, "NSUnitElectricResistance")
KL(d10, "NSUnitEnergy")
KL(d11, "NSUnitFrequency")
KL(d12, "NSUnitFuelEfficiency")
KL(d13, "NSUnitInformationStorage")
KL(d14, "NSUnitLength")
KL(d15, "NSUnitIlluminance")
KL(d16, "NSUnitMass")
KL(d17, "NSUnitPower")
KL(d18, "NSUnitPressure")
KL(d19, "NSUnitSpeed")
KL(d20, "NSUnitTemperature")
KL(d21, "NSUnitVolume")
static cx_class_t *const dimc[D_N] = {&d0_c, &d1_c, &d2_c, &d3_c, &d4_c, &d5_c, &d6_c, &d7_c, &d8_c, &d9_c, &d10_c, &d11_c, &d12_c, &d13_c, &d14_c, &d15_c, &d16_c, &d17_c, &d18_c, &d19_c, &d20_c, &d21_c};
static cx_class_t *const dimm[D_N] = {&d0_m, &d1_m, &d2_m, &d3_m, &d4_m, &d5_m, &d6_m, &d7_m, &d8_m, &d9_m, &d10_m, &d11_m, &d12_m, &d13_m, &d14_m, &d15_m, &d16_m, &d17_m, &d18_m, &d19_m, &d20_m, &d21_m};

typedef struct { cx_class_t *isa; double coef, konst; int recip; } conv_t;
typedef struct { cx_class_t *isa; id symbol; id converter; int def; } unit_t;   /* def: índice em U + 1, 0 = criada pelo app */
typedef struct { cx_class_t *isa; double value; id unit; } meas_t;
typedef struct { cx_class_t *isa; u64 options; i64 style; id locale; id nf; } mf_t;

static id g_units[sizeof U / sizeof U[0]];
static volatile int g_lock;

static int dim_of_class(cx_class_t *k) {
    for (; k; k = k->super) for (int d = 0; d < D_N; d++) if (k == dimc[d]) return d;
    return -1;
}
static id conv_new(double coef, double konst, int recip) {
    conv_t *c = (conv_t *)cx_alloc_instance(recip ? &rec_c : &lin_c);
    c->coef = coef; c->konst = konst; c->recip = recip;
    return (id)c;
}
static id unit_for(int i) {
    while (__atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE)) { }
    id u = g_units[i];
    if (!u) {
        unit_t *x = (unit_t *)cx_alloc_instance(dimc[U[i].dim]);
        x->symbol = objc_retain(cx_str(U[i].sym));
        x->converter = conv_new(U[i].coef, U[i].konst, U[i].recip);
        x->def = i + 1;
        g_units[i] = u = (id)x;
    }
    __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE);
    return u;
}
/* o IMP de cada +unidade (todas as classes compartilham; o seletor diz qual) */
static id unit_class_getter(id k, SEL c) {
    int d = dim_of_class((cx_class_t *)k);
    const char *name = (const char *)c;
    for (int i = 0; i < NU; i++) if (U[i].dim == d && !strcmp(U[i].sel, name)) return unit_for(i);
    return 0;
}
static id dim_base_unit(id k, SEL c) {
    (void)c;
    int d = dim_of_class((cx_class_t *)k);
    if (d < 0) return 0;
    for (int i = 0; i < NU; i++) if (U[i].dim == d && !strcmp(U[i].sel, dim_base[d])) return unit_for(i);
    return 0;
}
static id conc_mmol(id k, SEL c, double gpm) {
    (void)k; (void)c;
    unit_t *x = (unit_t *)cx_alloc_instance(dimc[D_CONC_MASS]);
    x->symbol = objc_retain(cx_str("mmol/L"));
    x->converter = conv_new(gpm * 0.001, 0, 0);   /* g/L por mmol/L = massa molar (g/mol) / 1000 */
    return objc_autorelease((id)x);
}

/* ---- NSUnitConverter / NSUnitConverterLinear */
static double lin_to_base(id s, SEL c, double v) { (void)c; conv_t *k = (conv_t *)s; return k->recip ? (v ? k->coef / v : 0) : v * k->coef + k->konst; }
static double lin_from_base(id s, SEL c, double v) { (void)c; conv_t *k = (conv_t *)s; return k->recip ? (v ? k->coef / v : 0) : (v - k->konst) / k->coef; }
static double base_to_base(id s, SEL c, double v) { (void)s; (void)c; return v; }
static id lin_init(id s, SEL c, double coef) { (void)c; ((conv_t *)s)->coef = coef; return s; }
static id lin_init2(id s, SEL c, double coef, double k) { (void)c; ((conv_t *)s)->coef = coef; ((conv_t *)s)->konst = k; return s; }
static double lin_coef(id s, SEL c) { (void)c; return ((conv_t *)s)->coef; }
static double lin_const(id s, SEL c) { (void)c; return ((conv_t *)s)->konst; }
static BOOL lin_equal(id s, SEL c, id o) {
    (void)c;
    if (s == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &lin_c)) return NO;
    conv_t *a = (conv_t *)s, *b = (conv_t *)o;
    return a->coef == b->coef && a->konst == b->konst && a->recip == b->recip;
}
static id self_copy(id s, SEL c) { (void)c; return objc_retain(s); }
static void lin_encode(id s, SEL c, id coder) {
    (void)c;
    MSG(void, coder, "encodeDouble:forKey:", ((conv_t *)s)->coef, cx_str("NS.coefficient"));
    MSG(void, coder, "encodeDouble:forKey:", ((conv_t *)s)->konst, cx_str("NS.constant"));
}
static id lin_init_coder(id s, SEL c, id coder) {
    (void)c;
    ((conv_t *)s)->coef = MSG(double, coder, "decodeDoubleForKey:", cx_str("NS.coefficient"));
    ((conv_t *)s)->konst = MSG(double, coder, "decodeDoubleForKey:", cx_str("NS.constant"));
    return s;
}

/* ---- NSUnit / NSDimension */
static id unit_init_sym(id s, SEL c, id sym) { (void)c; ((unit_t *)s)->symbol = sym ? MSG0(id, sym, "copy") : cx_str(""); return s; }
static id unit_init_sym_conv(id s, SEL c, id sym, id conv) { unit_init_sym(s, c, sym); ((unit_t *)s)->converter = conv ? objc_retain(conv) : 0; return s; }
static id unit_symbol(id s, SEL c) { (void)c; return ((unit_t *)s)->symbol; }
static id dim_converter(id s, SEL c) {
    (void)c;
    unit_t *u = (unit_t *)s;
    if (!u->converter) u->converter = conv_new(1, 0, 0);
    return u->converter;
}
static BOOL unit_equal(id s, SEL c, id o) {
    (void)c;
    if (s == o) return YES;
    if (!o || object_getClass(o) != object_getClass(s)) return NO;
    unit_t *a = (unit_t *)s, *b = (unit_t *)o;
    if (!MSG(BOOL, a->symbol, "isEqualToString:", b->symbol)) return NO;
    if (dim_of_class(object_getClass(s)) < 0 && !cx_is_subclass(object_getClass(s), &dim_c)) return YES;
    return MSG(BOOL, dim_converter(s, 0), "isEqual:", dim_converter(o, 0));
}
static u64 unit_hash(id s, SEL c) { (void)c; return MSG0(u64, ((unit_t *)s)->symbol, "hash"); }
static id unit_desc(id s, SEL c) { (void)c; return ((unit_t *)s)->symbol; }
static void unit_encode(id s, SEL c, id coder) {
    (void)c;
    MSG(void, coder, "encodeObject:forKey:", ((unit_t *)s)->symbol, cx_str("NS.symbol"));
    if (((unit_t *)s)->converter) MSG(void, coder, "encodeObject:forKey:", ((unit_t *)s)->converter, cx_str("NS.converter"));
}
static id unit_init_coder(id s, SEL c, id coder) {
    (void)c;
    id sym = MSG(id, coder, "decodeObjectForKey:", cx_str("NS.symbol"));
    id conv = MSG(id, coder, "decodeObjectForKey:", cx_str("NS.converter"));
    unit_t *u = (unit_t *)s;
    u->symbol = sym ? objc_retain(sym) : cx_str("");
    u->converter = conv ? objc_retain(conv) : 0;
    /* uma unidade conhecida volta como a instância da tabela */
    int d = dim_of_class(object_getClass(s));
    for (int i = 0; d >= 0 && i < NU; i++)
        if (U[i].dim == d && !strcmp(U[i].sym, cx_cstr(u->symbol))) { u->def = i + 1; break; }
    return s;
}
static void unit_dealloc(id s, SEL c) {
    (void)c;
    unit_t *u = (unit_t *)s;
    if (u->def) return;   /* as da tabela são imortais */
    if (u->symbol) objc_release(u->symbol);
    if (u->converter) objc_release(u->converter);
    super_dealloc(s);
}

/* ---- NSMeasurement */
static double to_base(id unit, double v) { return MSG(double, dim_converter(unit, 0), "baseUnitValueFromValue:", v); }
static double from_base(id unit, double v) { return MSG(double, dim_converter(unit, 0), "valueFromBaseUnitValue:", v); }
static int same_dim(id a, id b) {
    if (!a || !b) return 0;
    int da = dim_of_class(object_getClass(a)), db = dim_of_class(object_getClass(b));
    if (da >= 0 || db >= 0) return da == db;
    return cx_is_subclass(object_getClass(a), &dim_c) && object_getClass(a) == object_getClass(b);
}
static id meas_init(id s, SEL c, double v, id unit) { (void)c; ((meas_t *)s)->value = v; ((meas_t *)s)->unit = unit ? objc_retain(unit) : 0; return s; }
static double meas_value(id s, SEL c) { (void)c; return ((meas_t *)s)->value; }
static id meas_unit(id s, SEL c) { (void)c; return ((meas_t *)s)->unit; }
static BOOL meas_can(id s, SEL c, id unit) { (void)c; return (BOOL)same_dim(((meas_t *)s)->unit, unit); }
static id meas_new(double v, id unit) { return objc_autorelease(meas_init(cx_alloc_instance(&meas_c), 0, v, unit)); }
static id meas_convert(id s, SEL c, id unit) {
    (void)c;
    meas_t *m = (meas_t *)s;
    if (!same_dim(m->unit, unit)) return 0;   /* o macOS lança */
    if (MSG(BOOL, m->unit, "isEqual:", unit)) return meas_new(m->value, unit);
    return meas_new(from_base(unit, to_base(m->unit, m->value)), unit);
}
static id meas_add(id s, SEL c, id o) {
    (void)c;
    meas_t *a = (meas_t *)s, *b = (meas_t *)o;
    if (!o || !same_dim(a->unit, b->unit)) return 0;
    if (MSG(BOOL, a->unit, "isEqual:", b->unit)) return meas_new(a->value + b->value, a->unit);
    id base = MSG0(id, (id)object_getClass(a->unit), "baseUnit");
    return meas_new(to_base(a->unit, a->value) + to_base(b->unit, b->value), base);
}
static id meas_sub(id s, SEL c, id o) {
    (void)c;
    meas_t *a = (meas_t *)s, *b = (meas_t *)o;
    if (!o || !same_dim(a->unit, b->unit)) return 0;
    if (MSG(BOOL, a->unit, "isEqual:", b->unit)) return meas_new(a->value - b->value, a->unit);
    id base = MSG0(id, (id)object_getClass(a->unit), "baseUnit");
    return meas_new(to_base(a->unit, a->value) - to_base(b->unit, b->value), base);
}
static BOOL meas_equal(id s, SEL c, id o) {
    (void)c;
    if (s == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &meas_c)) return NO;
    meas_t *a = (meas_t *)s, *b = (meas_t *)o;
    if (MSG(BOOL, a->unit, "isEqual:", b->unit)) return a->value == b->value;
    if (!same_dim(a->unit, b->unit)) return NO;
    return fabs(to_base(a->unit, a->value) - to_base(b->unit, b->value)) < 1e-9 * (fabs(to_base(a->unit, a->value)) + 1);
}
static u64 meas_hash(id s, SEL c) {
    (void)c;
    meas_t *m = (meas_t *)s;
    double b = cx_is_subclass(object_getClass(m->unit), &dim_c) ? to_base(m->unit, m->value) : m->value;
    u64 h;
    memcpy(&h, &b, 8);
    return h;
}
static i64 meas_compare(id s, SEL c, id o) {
    (void)c;
    meas_t *a = (meas_t *)s, *b = (meas_t *)o;
    double x = to_base(a->unit, a->value), y = to_base(b->unit, b->value);
    return x < y ? -1 : x > y ? 1 : 0;
}
static id meas_copy(id s, SEL c) { (void)c; return objc_retain(s); }
static id meas_desc(id s, SEL c) {
    (void)c;
    char b[160];
    meas_t *m = (meas_t *)s;
    snprintf(b, sizeof b, "%g %s", m->value, m->unit ? cx_cstr(((unit_t *)m->unit)->symbol) : "");
    return cx_str(b);
}
static void meas_encode(id s, SEL c, id coder) {
    (void)c;
    MSG(void, coder, "encodeDouble:forKey:", ((meas_t *)s)->value, cx_str("NS.value"));
    MSG(void, coder, "encodeObject:forKey:", ((meas_t *)s)->unit, cx_str("NS.unit"));
}
static id meas_init_coder(id s, SEL c, id coder) {
    (void)c;
    return meas_init(s, 0, MSG(double, coder, "decodeDoubleForKey:", cx_str("NS.value")), MSG(id, coder, "decodeObjectForKey:", cx_str("NS.unit")));
}
static void meas_dealloc(id s, SEL c) { (void)c; if (((meas_t *)s)->unit) objc_release(((meas_t *)s)->unit); super_dealloc(s); }

/* ---- NSMeasurementFormatter */
enum { MF_PROVIDED = 1, MF_NATURAL = 2, MF_TEMP_NO_UNIT = 4 };
static int loc_system(const char *loc) {
    char m[16];
    fdn_loc_info(loc, FDN_LN_MEASUREMENT, m, sizeof m);
    if (!strcmp(m, "U.S.")) return 1;
    if (!strcmp(m, "U.K.")) return 2;
    return 0;
}
/* unidade preferida do locale para uma dimensão (o que o macOS mostra sem providedUnit) */
static int preferred(int d, int sys) {
    const char *name = 0;
    switch (d) {
    case D_LENGTH: name = sys ? "miles" : "kilometers"; break;
    case D_MASS: name = sys == 1 ? "pounds" : sys == 2 ? "stones" : "kilograms"; break;
    case D_TEMP: name = sys == 1 ? "fahrenheit" : "celsius"; break;
    case D_SPEED: name = sys ? "milesPerHour" : "kilometersPerHour"; break;
    case D_VOLUME: name = sys == 1 ? "gallons" : sys == 2 ? "imperialGallons" : "liters"; break;
    case D_AREA: name = sys ? "squareMiles" : "squareKilometers"; break;
    case D_FUEL: name = sys == 1 ? "milesPerGallon" : sys == 2 ? "milesPerImperialGallon" : "litersPer100Kilometers"; break;
    case D_PRESSURE: name = sys == 1 ? "inchesOfMercury" : "hectopascals"; break;
    case D_ENERGY: name = "kilocalories"; break;
    }
    if (!name) return -1;
    for (int i = 0; i < NU; i++) if (U[i].dim == d && !strcmp(U[i].sel, name)) return i;
    return -1;
}
/* escala natural: entre as unidades do sistema com nome no CLDR, a maior em que o valor fica >= 1 */
static int natural(int d, int sys, double base) {
    int best = -1;
    double best_coef = 0;
    for (int i = 0; i < NU; i++) {
        if (U[i].dim != d || !U[i].cldr || U[i].recip || U[i].konst) continue;
        int usys = U[i].sys;
        if (sys == 0 ? usys != 0 : (usys != sys && !(usys == 1 && sys == 2))) continue;
        if (d == D_LENGTH && (!strcmp(U[i].sel, "lightyears") || !strcmp(U[i].sel, "parsecs") || !strcmp(U[i].sel, "astronomicalUnits") ||
                              !strcmp(U[i].sel, "scandinavianMiles") || !strcmp(U[i].sel, "nauticalMiles") || !strcmp(U[i].sel, "fathoms") ||
                              !strcmp(U[i].sel, "furlongs") || !strcmp(U[i].sel, "decimeters") || !strcmp(U[i].sel, "picometers") ||
                              !strcmp(U[i].sel, "micrometers") || !strcmp(U[i].sel, "nanometers"))) continue;
        if (fabs(base) / U[i].coef >= 1 && U[i].coef > best_coef) { best = i; best_coef = U[i].coef; }
    }
    return best;
}
static id mf_init(id s, SEL c) { (void)c; ((mf_t *)s)->style = 2; return s; }
static int width_of(i64 style) { return style == 1 ? 1 : style == 3 ? 2 : 0; }
static int frac_of(mf_t *f) {
    if (f->nf) return (int)MSG0(i64, f->nf, "maximumFractionDigits");
    return 3;
}
static id format_value(mf_t *f, int def, id unit, double v) {
    const char *loc = fdn_locale_ident_of(f->locale);
    char out[256], num[96];
    int width = width_of(f->style);
    if (def > 0 && U[def - 1].cldr && !(U[def - 1].dim == D_TEMP && (f->options & MF_TEMP_NO_UNIT))) {
        if (fdn_unit_format(loc, U[def - 1].cldr, v, width, frac_of(f), out, sizeof out) > 0) return cx_str(out);
    }
    /* sem nome no CLDR (ou temperatura sem unidade): número do locale + símbolo */
    if (f->nf) {
        id n = MSG(id, f->nf, "stringFromNumber:", cx_num_d(v));
        snprintf(num, sizeof num, "%s", n ? cx_cstr(n) : "");
    } else {
        void *nf = fdn_nf_open(1, loc);
        fdn_nf_set_attr(nf, 6, 3);
        fdn_nf_format_double(nf, v, num, sizeof num);
        fdn_nf_close(nf);
    }
    const char *sym = unit ? cx_cstr(((unit_t *)unit)->symbol) : "";
    if (def > 0 && U[def - 1].dim == D_TEMP && (f->options & MF_TEMP_NO_UNIT)) snprintf(out, sizeof out, "%s°", num);
    else snprintf(out, sizeof out, "%s%s%s", num, f->style == 1 ? "" : " ", sym);
    return cx_str(out);
}
static id mf_string(id s, SEL c, id m) {
    (void)c;
    mf_t *f = (mf_t *)s;
    if (!m) return 0;
    meas_t *x = (meas_t *)m;
    id unit = x->unit;
    double v = x->value;
    int def = unit ? ((unit_t *)unit)->def : 0;
    int d = unit ? dim_of_class(object_getClass(unit)) : -1;
    if (d >= 0 && !(f->options & MF_PROVIDED)) {
        int sys = loc_system(fdn_locale_ident_of(f->locale));
        double base = to_base(unit, v);
        int target = -1;
        if ((f->options & MF_NATURAL) && d != D_TEMP) target = natural(d, sys, base);
        if (target < 0) target = preferred(d, sys);
        if (target >= 0) {
            id tu = unit_for(target);
            v = from_base(tu, base);
            unit = tu;
            def = target + 1;
        }
    } else if (d >= 0 && (f->options & MF_NATURAL) && def > 0 && d != D_TEMP) {
        /* unidade dada + escala natural: troca só a escala dentro do mesmo sistema */
        int t = natural(d, U[def - 1].sys, to_base(unit, v));
        if (t >= 0) { unit = unit_for(t); v = from_base(unit, to_base(x->unit, x->value)); def = t + 1; }
    }
    return format_value(f, def, unit, v);
}
static id mf_string_unit(id s, SEL c, id unit) {
    (void)c;
    mf_t *f = (mf_t *)s;
    if (!unit) return 0;
    int def = ((unit_t *)unit)->def;
    if (def > 0 && U[def - 1].cldr) {
        char out[256];
        /* o nome da unidade sem o número: formata 2 (plural) e tira o número e o espaço da frente */
        if (fdn_unit_format(fdn_locale_ident_of(f->locale), U[def - 1].cldr, 2, width_of(f->style), 0, out, sizeof out) > 0) {
            const char *p = out;
            while (*p == '2') p++;
            while (*p == ' ' || (unsigned char)*p == 0xC2 || (unsigned char)*p == 0xA0) p++;
            if (!strncmp(p, "\xe2\x80\xaf", 3)) p += 3;
            if (*p && f->style == 3) return cx_str(p);
        }
    }
    return ((unit_t *)unit)->symbol;
}
static id mf_string_obj(id s, SEL c, id o) { return o && cx_is_subclass(object_getClass(o), &meas_c) ? mf_string(s, c, o) : 0; }
static u64 mf_options(id s, SEL c) { (void)c; return ((mf_t *)s)->options; }
static void mf_set_options(id s, SEL c, u64 v) { (void)c; ((mf_t *)s)->options = v; }
static i64 mf_style(id s, SEL c) { (void)c; return ((mf_t *)s)->style; }
static void mf_set_style(id s, SEL c, i64 v) { (void)c; ((mf_t *)s)->style = v; }
static id mf_locale(id s, SEL c) { (void)c; mf_t *f = (mf_t *)s; return f->locale ? f->locale : MSG0(id, cls("NSLocale"), "currentLocale"); }
static void mf_set_locale(id s, SEL c, id l) { (void)c; mf_t *f = (mf_t *)s; id o = f->locale; f->locale = l ? objc_retain(l) : 0; if (o) objc_release(o); }
static id mf_nf(id s, SEL c) {
    (void)c;
    mf_t *f = (mf_t *)s;
    if (!f->nf) {
        f->nf = MSG0(id, MSG0(id, cls("NSNumberFormatter"), "alloc"), "init");
        MSG(void, f->nf, "setNumberStyle:", (u64)1);
        MSG(void, f->nf, "setMaximumFractionDigits:", (i64)3);
        MSG(void, f->nf, "setLocale:", mf_locale(s, 0));
    }
    return f->nf;
}
static void mf_set_nf(id s, SEL c, id n) { (void)c; mf_t *f = (mf_t *)s; id o = f->nf; f->nf = n ? MSG0(id, n, "copy") : 0; if (o) objc_release(o); }
static void mf_dealloc(id s, SEL c) { (void)c; mf_t *f = (mf_t *)s; if (f->locale) objc_release(f->locale); if (f->nf) objc_release(f->nf); super_dealloc(s); }

/* ---------------------------------------------------------------- registro */
void cx_fdn_units_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_class_t *fmt = cx_class_named("NSFormatter");
    if (!ob || cx_class_named("NSUnit") == &unit_c) return;
    cx_define_class(&conv_c, &conv_m, "NSUnitConverter", ob, 0);
    cx_add_method(&conv_c, "baseUnitValueFromValue:", (void *)base_to_base);
    cx_add_method(&conv_c, "valueFromBaseUnitValue:", (void *)base_to_base);
    cx_define_class(&lin_c, &lin_m, "NSUnitConverterLinear", &conv_c, (u32)(sizeof(conv_t) - 8));
    cx_add_cmethod(&lin_c, "supportsSecureCoding", (void *)yes_m);
    cx_add_method(&lin_c, "initWithCoefficient:", (void *)lin_init); cx_add_method(&lin_c, "initWithCoefficient:constant:", (void *)lin_init2);
    cx_add_method(&lin_c, "coefficient", (void *)lin_coef); cx_add_method(&lin_c, "constant", (void *)lin_const);
    cx_add_method(&lin_c, "baseUnitValueFromValue:", (void *)lin_to_base); cx_add_method(&lin_c, "valueFromBaseUnitValue:", (void *)lin_from_base);
    cx_add_method(&lin_c, "isEqual:", (void *)lin_equal); cx_add_method(&lin_c, "copy", (void *)self_copy);
    cx_add_method(&lin_c, "copyWithZone:", (void *)self_copy);
    cx_add_method(&lin_c, "encodeWithCoder:", (void *)lin_encode); cx_add_method(&lin_c, "initWithCoder:", (void *)lin_init_coder);
    /* o recíproco (mpg) é privado no macOS; aqui uma subclasse interna do linear */
    cx_define_class(&rec_c, &rec_m, "__mOSUnitConverterReciprocal", &lin_c, 0);

    cx_define_class(&unit_c, &unit_m, "NSUnit", ob, (u32)(sizeof(unit_t) - 8));
    cx_add_cmethod(&unit_c, "supportsSecureCoding", (void *)yes_m);
    cx_add_method(&unit_c, "initWithSymbol:", (void *)unit_init_sym); cx_add_method(&unit_c, "symbol", (void *)unit_symbol);
    cx_add_method(&unit_c, "isEqual:", (void *)unit_equal); cx_add_method(&unit_c, "hash", (void *)unit_hash);
    cx_add_method(&unit_c, "description", (void *)unit_desc);
    cx_add_method(&unit_c, "copy", (void *)self_copy); cx_add_method(&unit_c, "copyWithZone:", (void *)self_copy);
    cx_add_method(&unit_c, "encodeWithCoder:", (void *)unit_encode); cx_add_method(&unit_c, "initWithCoder:", (void *)unit_init_coder);
    cx_add_method(&unit_c, "dealloc", (void *)unit_dealloc);
    cx_define_class(&dim_c, &dim_m, "NSDimension", &unit_c, 0);
    cx_add_method(&dim_c, "initWithSymbol:converter:", (void *)unit_init_sym_conv);
    cx_add_method(&dim_c, "converter", (void *)dim_converter);
    cx_add_cmethod(&dim_c, "baseUnit", (void *)dim_base_unit);
    for (int d = 0; d < D_N; d++) cx_define_class(dimc[d], dimm[d], dim_names[d], &dim_c, 0);
    for (int i = 0; i < NU; i++) cx_add_cmethod(dimc[U[i].dim], U[i].sel, (void *)unit_class_getter);
    cx_add_cmethod(dimc[D_CONC_MASS], "millimolesPerLiterWithGramsPerMole:", (void *)conc_mmol);

    cx_define_class(&meas_c, &meas_m, "NSMeasurement", ob, (u32)(sizeof(meas_t) - 8));
    cx_add_cmethod(&meas_c, "supportsSecureCoding", (void *)yes_m);
    cx_add_method(&meas_c, "initWithDoubleValue:unit:", (void *)meas_init);
    cx_add_method(&meas_c, "doubleValue", (void *)meas_value); cx_add_method(&meas_c, "unit", (void *)meas_unit);
    cx_add_method(&meas_c, "canBeConvertedToUnit:", (void *)meas_can);
    cx_add_method(&meas_c, "measurementByConvertingToUnit:", (void *)meas_convert);
    cx_add_method(&meas_c, "measurementByAddingMeasurement:", (void *)meas_add);
    cx_add_method(&meas_c, "measurementBySubtractingMeasurement:", (void *)meas_sub);
    cx_add_method(&meas_c, "isEqual:", (void *)meas_equal); cx_add_method(&meas_c, "hash", (void *)meas_hash);
    cx_add_method(&meas_c, "compare:", (void *)meas_compare); cx_add_method(&meas_c, "description", (void *)meas_desc);
    cx_add_method(&meas_c, "copy", (void *)meas_copy); cx_add_method(&meas_c, "copyWithZone:", (void *)meas_copy);
    cx_add_method(&meas_c, "encodeWithCoder:", (void *)meas_encode); cx_add_method(&meas_c, "initWithCoder:", (void *)meas_init_coder);
    cx_add_method(&meas_c, "dealloc", (void *)meas_dealloc);

    cx_define_class(&mf_c, &mf_m, "NSMeasurementFormatter", fmt ? fmt : ob, (u32)(sizeof(mf_t) - 8));
    cx_add_method(&mf_c, "init", (void *)mf_init);
    cx_add_method(&mf_c, "stringFromMeasurement:", (void *)mf_string); cx_add_method(&mf_c, "stringFromUnit:", (void *)mf_string_unit);
    cx_add_method(&mf_c, "stringForObjectValue:", (void *)mf_string_obj);
    cx_add_method(&mf_c, "unitOptions", (void *)mf_options); cx_add_method(&mf_c, "setUnitOptions:", (void *)mf_set_options);
    cx_add_method(&mf_c, "unitStyle", (void *)mf_style); cx_add_method(&mf_c, "setUnitStyle:", (void *)mf_set_style);
    cx_add_method(&mf_c, "locale", (void *)mf_locale); cx_add_method(&mf_c, "setLocale:", (void *)mf_set_locale);
    cx_add_method(&mf_c, "numberFormatter", (void *)mf_nf); cx_add_method(&mf_c, "setNumberFormatter:", (void *)mf_set_nf);
    cx_add_method(&mf_c, "dealloc", (void *)mf_dealloc);
}
