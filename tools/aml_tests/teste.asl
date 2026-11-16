/* mOS — autoteste do interpretador AML (tools/aml_host.c --teste tools/aml_tests/teste.aml)
 * Compilar: iasl -p tools/aml_tests/teste tools/aml_tests/teste.asl
 * \TEST devolve 0 quando tudo passa, ou o número da primeira verificação que falhou. */
DefinitionBlock ("", "DSDT", 2, "MOS", "AMLTEST", 1)
{
    OperationRegion (MEMR, SystemMemory, 0x10000, 0x100)
    Field (MEMR, ByteAcc, NoLock, Preserve) { MB00, 8, MB01, 8, , 4, MN0, 4, MW00, 16 }
    Field (MEMR, DWordAcc, NoLock, Preserve) { Offset (0x10), MD10, 32, MQ, 3, , 1, MR, 5 }
    OperationRegion (IDXR, SystemIO, 0x70, 2)
    Field (IDXR, ByteAcc, NoLock, Preserve) { IDX, 8, DAT, 8 }
    IndexField (IDX, DAT, ByteAcc, NoLock, Preserve) { Offset (0x10), CI10, 8, CI11, 8 }
    OperationRegion (ECR, EmbeddedControl, 0, 0x100)
    Field (ECR, ByteAcc, NoLock, Preserve) { Offset (0x40), EC40, 8, EC41, 16 }
    Name (PKG0, Package () { 1, "dois", Buffer () { 3, 4 }, Package () { 5 } })
    Name (BUF0, Buffer (8) { 0x11, 0x22, 0x33, 0x44 })
    CreateDWordField (BUF0, 0, BD0)
    CreateWordField (BUF0, 4, BW4)
    CreateBitField (BUF0, 1, BB1)
    CreateField (BUF0, 12, 8, BF12)
    Name (STR0, "mOS")
    Name (INT0, 0x1234)
    Name (FAIL, 0)
    Mutex (MTX0, 0)
    Event (EVT0)
    Scope (\_SB)
    {
        Device (DEV0)
        {
            Name (_HID, EisaId ("PNP0C0C"))
            Method (_STA) { Return (0x0F) }
            Name (CNT, 0)
            Method (_INI) { CNT++ }
        }
        Device (DEV1)
        {
            Name (_HID, "ACPI0007")
            Method (_STA) { Return (0) }
            Device (SUB) { Name (_ADR, 1) Method (_INI) { \FAIL = 1 } }
        }
    }
    Method (FACT, 1, Serialized) { If (Arg0 <= 1) { Return (1) } Return (Arg0 * FACT (Arg0 - 1)) }
    Method (SETR, 1) { Arg0 = 0x55 }
    Method (DYN, 0, Serialized) { Name (TMP, 7) Return (TMP + 1) }
    Method (GPKG, 0) { Return (Package () { 0x10, 0x20 }) }
    Method (TEST, 0, Serialized)
    {
        If (((1 + 2) * 3) != 9) { Return (1) }
        If (((0x10 >> 2) != 4) || ((1 << 4) != 16)) { Return (2) }
        If (((0xF0 | 0x0F) != 0xFF) || ((0xF0 & 0x3C) != 0x30) || ((0xFF ^ 0x0F) != 0xF0)) { Return (3) }
        Divide (17, 5, Local0, Local1)
        If ((Local0 != 2) || (Local1 != 3)) { Return (4) }
        If ((17 % 5) != 2) { Return (5) }
        If (FACT (10) != 3628800) { Return (6) }
        Local0 = 0
        Local1 = 0
        While (Local0 < 10)
        {
            Local0++
            If (Local0 == 3) { Continue }
            If (Local0 == 8) { Break }
            Local1 += Local0
        }
        If (Local1 != 25) { Return (7) }
        Local0 = Concatenate ("ab", "cd")
        If (Local0 != "abcd") { Return (8) }
        If (SizeOf (STR0) != 3) { Return (9) }
        Local0 = Concatenate ("v=", 0x1F)
        If (SizeOf (Local0) != 18) { Return (10) }
        If ((ToInteger ("0x1F") != 31) || (ToInteger ("123") != 123)) { Return (11) }
        If (ToDecimalString (305) != "305") { Return (12) }
        If (Mid ("abcdef", 2, 3) != "cde") { Return (13) }
        If (BD0 != 0x44332211) { Return (14) }
        BW4 = 0xBEEF
        If ((DerefOf (BUF0 [4]) != 0xEF) || (DerefOf (BUF0 [5]) != 0xBE)) { Return (15) }
        If (BB1 != 0) { Return (16) }
        BB1 = 1
        If (DerefOf (BUF0 [0]) != 0x13) { Return (17) }
        If (BF12 != 0x32) { Return (18) }
        If (SizeOf (PKG0) != 4) { Return (19) }
        If (DerefOf (PKG0 [1]) != "dois") { Return (20) }
        Local0 = DerefOf (PKG0 [3])
        If (DerefOf (Local0 [0]) != 5) { Return (21) }
        PKG0 [0] = 42
        If (DerefOf (PKG0 [0]) != 42) { Return (22) }
        Local0 = Package () { 1, 3, 5, 7, 9 }
        If (Match (Local0, MEQ, 7, MTR, 0, 0) != 3) { Return (23) }
        If (Match (Local0, MGT, 4, MLT, 8, 0) != 2) { Return (24) }
        If (Match (Local0, MEQ, 8, MTR, 0, 0) != Ones) { Return (25) }
        SETR (RefOf (Local2))
        If (Local2 != 0x55) { Return (26) }
        Local3 = RefOf (INT0)
        If (DerefOf (Local3) != 0x1234) { Return (27) }
        If ((ObjectType (INT0) != 1) || (ObjectType (STR0) != 2) || (ObjectType (BUF0) != 3) ||
            (ObjectType (PKG0) != 4) || (ObjectType (\_SB.DEV0) != 6) || (ObjectType (FACT) != 8) ||
            (ObjectType (MTX0) != 9)) { Return (28) }
        If (!CondRefOf (\_SB.DEV0._HID) || CondRefOf (\NADA)) { Return (29) }
        MB00 = 0xAB
        MW00 = 0x1234
        MN0 = 0x0C
        If ((MB00 != 0xAB) || (MW00 != 0x1234) || (MN0 != 0x0C)) { Return (30) }
        MD10 = 0xDEADBEEF
        If (MD10 != 0xDEADBEEF) { Return (31) }
        MQ = 5
        MR = 0x1F
        If ((MQ != 5) || (MR != 0x1F) || (MD10 != 0xDEADBEEF)) { Return (32) }
        CI10 = 0x5A
        CI11 = 0xA5
        If ((CI10 != 0x5A) || (CI11 != 0xA5)) { Return (33) }
        If ((DYN () != 8) || (DYN () != 8)) { Return (34) }
        If (CondRefOf (\NADA.TMP)) { Return (35) }
        If (Acquire (MTX0, 0xFFFF)) { Return (36) }
        Release (MTX0)
        Signal (EVT0)
        If (Wait (EVT0, 0)) { Return (37) }
        If (!Wait (EVT0, 0)) { Return (38) }
        If (!_OSI ("Windows 2015") || _OSI ("Darwin")) { Return (39) }
        Local0 = ToBuffer ("AB")
        If (SizeOf (Local0) != 3) { Return (40) }
        If ((FromBCD (0x1234) != 1234) || (ToBCD (1234) != 0x1234)) { Return (41) }
        If ((FindSetLeftBit (0x80) != 8) || (FindSetRightBit (0x80) != 8) || (FindSetRightBit (0) != 0)) { Return (42) }
        If (FAIL) { Return (43) }
        If (\_SB.DEV0.CNT != 1) { Return (44) }
        Local0 = 3
        Local1 = Package (Local0) {}
        If (SizeOf (Local1) != 3) { Return (45) }
        Local0 = ConcatenateResTemplate (ResourceTemplate () { IO (Decode16, 0x62, 0x62, 1, 1) },
                                         ResourceTemplate () { IRQNoFlags () { 9 } })
        If (SizeOf (Local0) != 13) { Return (46) }
        EC40 = 0x77
        EC41 = 0x1122
        If ((EC40 != 0x77) || (EC41 != 0x1122)) { Return (47) }
        Local0 = GPKG ()
        If (DerefOf (Local0 [1]) != 0x20) { Return (48) }
        Local0 = 0
        For (Local1 = 0, Local1 < 5, Local1++) { Local0 += Local1 }
        If (Local0 != 10) { Return (49) }
        Switch (ToInteger (Local0))
        {
            Case (9) { Return (50) }
            Case (Package () { 10, 11 }) { Local0 = 0x99 }
            Default { Return (51) }
        }
        If (Local0 != 0x99) { Return (52) }
        Return (0)
    }
}
