/*
 * mOS SDK (variante com interop ObjC, docs/swift-objc-runtime.md, fase 5) — o módulo clang Foundation do overlay Swift.
 * As declarações ficam em MOSFoundationObjC.h (módulo _MOSFoundationObjC), que o FoundationEssentials também importa
 * para as pontes Data/URL/Date... com o ABI da Apple (no mesmo módulo do tipo, sem contexto de extensão no nome).
 */
#pragma once
#include <Foundation/MOSFoundationObjC.h>
