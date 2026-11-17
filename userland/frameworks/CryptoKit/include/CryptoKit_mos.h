/* mOS — módulo clang subjacente do CryptoKit. O CryptoKit é só Swift; daqui saem apenas os dois tipos C que a ABI das
 * chaves do Secure Enclave cita (os nomes mangled são __C.SecAccessControlRef e __C.LAContext, sem módulo). */
#pragma once
typedef struct __attribute__((objc_bridge(id))) __SecAccessControl *SecAccessControlRef;
__attribute__((objc_root_class))
@interface LAContext
@end
