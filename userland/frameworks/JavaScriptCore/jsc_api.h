/* mOS — JavaScriptCore.framework: tipos da API C pública (a ABI documentada do JavaScriptCore), escritos para o mOS.
 * Os apps compilam contra os cabeçalhos do SDK deles; aqui só precisamos das mesmas formas e valores. */
#pragma once

typedef const struct OpaqueJSContextGroup *JSContextGroupRef;
typedef const struct OpaqueJSContext *JSContextRef;
typedef struct OpaqueJSContext *JSGlobalContextRef;
typedef struct OpaqueJSString *JSStringRef;
typedef struct OpaqueJSClass *JSClassRef;
typedef struct OpaqueJSPropertyNameArray *JSPropertyNameArrayRef;
typedef struct OpaqueJSPropertyNameAccumulator *JSPropertyNameAccumulatorRef;
typedef const struct OpaqueJSValue *JSValueRef;
typedef struct OpaqueJSValue *JSObjectRef;
typedef unsigned short JSChar;
typedef void (*JSTypedArrayBytesDeallocator)(void *bytes, void *deallocatorContext);

typedef enum { kJSTypeUndefined, kJSTypeNull, kJSTypeBoolean, kJSTypeNumber, kJSTypeString, kJSTypeObject, kJSTypeSymbol, kJSTypeBigInt } JSType;
typedef enum {
    kJSTypedArrayTypeInt8Array, kJSTypedArrayTypeInt16Array, kJSTypedArrayTypeInt32Array, kJSTypedArrayTypeUint8Array,
    kJSTypedArrayTypeUint8ClampedArray, kJSTypedArrayTypeUint16Array, kJSTypedArrayTypeUint32Array, kJSTypedArrayTypeFloat32Array,
    kJSTypedArrayTypeFloat64Array, kJSTypedArrayTypeArrayBuffer, kJSTypedArrayTypeNone, kJSTypedArrayTypeBigInt64Array,
    kJSTypedArrayTypeBigUint64Array
} JSTypedArrayType;

enum { kJSPropertyAttributeNone = 0, kJSPropertyAttributeReadOnly = 1 << 1, kJSPropertyAttributeDontEnum = 1 << 2, kJSPropertyAttributeDontDelete = 1 << 3 };
typedef unsigned JSPropertyAttributes;
enum { kJSClassAttributeNone = 0, kJSClassAttributeNoAutomaticPrototype = 1 << 1 };
typedef unsigned JSClassAttributes;

typedef void (*JSObjectInitializeCallback)(JSContextRef ctx, JSObjectRef object);
typedef void (*JSObjectFinalizeCallback)(JSObjectRef object);
typedef _Bool (*JSObjectHasPropertyCallback)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName);
typedef JSValueRef (*JSObjectGetPropertyCallback)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exception);
typedef _Bool (*JSObjectSetPropertyCallback)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exception);
typedef _Bool (*JSObjectDeletePropertyCallback)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exception);
typedef void (*JSObjectGetPropertyNamesCallback)(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames);
typedef JSValueRef (*JSObjectCallAsFunctionCallback)(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, unsigned long argumentCount,
                                                     const JSValueRef arguments[], JSValueRef *exception);
typedef JSObjectRef (*JSObjectCallAsConstructorCallback)(JSContextRef ctx, JSObjectRef constructor, unsigned long argumentCount,
                                                         const JSValueRef arguments[], JSValueRef *exception);
typedef _Bool (*JSObjectHasInstanceCallback)(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef *exception);
typedef JSValueRef (*JSObjectConvertToTypeCallback)(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef *exception);

typedef struct { const char *name; JSObjectGetPropertyCallback getProperty; JSObjectSetPropertyCallback setProperty; JSPropertyAttributes attributes; } JSStaticValue;
typedef struct { const char *name; JSObjectCallAsFunctionCallback callAsFunction; JSPropertyAttributes attributes; } JSStaticFunction;

typedef struct {
    int version;
    JSClassAttributes attributes;
    const char *className;
    JSClassRef parentClass;
    const JSStaticValue *staticValues;
    const JSStaticFunction *staticFunctions;
    JSObjectInitializeCallback initialize;
    JSObjectFinalizeCallback finalize;
    JSObjectHasPropertyCallback hasProperty;
    JSObjectGetPropertyCallback getProperty;
    JSObjectSetPropertyCallback setProperty;
    JSObjectDeletePropertyCallback deleteProperty;
    JSObjectGetPropertyNamesCallback getPropertyNames;
    JSObjectCallAsFunctionCallback callAsFunction;
    JSObjectCallAsConstructorCallback callAsConstructor;
    JSObjectHasInstanceCallback hasInstance;
    JSObjectConvertToTypeCallback convertToType;
} JSClassDefinition;

#define JSC_EXPORT __attribute__((visibility("default")))
