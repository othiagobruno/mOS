/*
 * mOS — expat_config.h para o alvo macOS do mOS (não o config de Linux shipado no tarball).
 * Entropia por arc4random_buf (o libSystem exporta), sem getrandom/SYS_getrandom/dev_urandom.
 * x86_64 e arm64 do mOS são little-endian (BYTEORDER 1234). Usado por tools/build-expat.sh (-I nesta pasta).
 */
#ifndef MOS_EXPAT_CONFIG_H
#define MOS_EXPAT_CONFIG_H 1

#define BYTEORDER 1234
#define HAVE_ARC4RANDOM_BUF 1
#define HAVE_DLFCN_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GETPAGESIZE 1
#define HAVE_INTTYPES_H 1
#define HAVE_MMAP 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1

#define PACKAGE "expat"
#define PACKAGE_NAME "expat"
#define PACKAGE_STRING "expat 2.6.4"
#define PACKAGE_TARNAME "expat"
#define PACKAGE_VERSION "2.6.4"
#define VERSION "2.6.4"

#define XML_CONTEXT_BYTES 1024
#define XML_DTD 1
#define XML_GE 1
#define XML_NS 1

#endif /* MOS_EXPAT_CONFIG_H */
