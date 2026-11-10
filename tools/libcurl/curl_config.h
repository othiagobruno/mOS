#ifndef HEADER_CURL_CONFIG_MOS_H
#define HEADER_CURL_CONFIG_MOS_H
/*
 * mOS — curl 8.23.0 como /usr/lib/libcurl.4.dylib (tools/build-libcurl.ps1).
 * Alvo Apple LP64 (x86_64-macos / aarch64-macos). TLS via mbed TLS (USE_MBEDTLS,
 * Apache-2.0), com verificação de certificado ligada. O bundle padrão é
 * /usr/share/curl/cacert.pem (a lista da Mozilla já em assets/certs/cacert.pem).
 * Sem LDAP, HTTP/2, HTTP/3, SSH, brotli, zstd, libidn e libpsl.
 * HTTP, ficheiro local e zlib (HAVE_LIBZ) entram. A licença do curl é a do COPYING.
 */
#define OS "apple-darwin"
#define CURL_OS "apple-darwin"

#define CURL_DISABLE_LDAP 1
#define CURL_DISABLE_LDAPS 1
#define CURL_DISABLE_DICT 1
#define CURL_DISABLE_GOPHER 1
#define CURL_DISABLE_IMAP 1
#define CURL_DISABLE_MQTT 1
#define CURL_DISABLE_POP3 1
#define CURL_DISABLE_RTSP 1
#define CURL_DISABLE_SMB 1
#define CURL_DISABLE_SMTP 1
#define CURL_DISABLE_TELNET 1
#define CURL_DISABLE_TFTP 1
#define CURL_DISABLE_KERBEROS_AUTH 1
#define CURL_DISABLE_NEGOTIATE_AUTH 1
#define CURL_DISABLE_AWS 1
#define CURL_DISABLE_HTTPSIG 1

#define HAVE_LIBZ 1
#define HAVE_ZLIB_H 1

#define USE_MBEDTLS 1
#define CURL_CA_BUNDLE "/usr/share/curl/cacert.pem"

#define STDC_HEADERS 1
#define HAVE_STDBOOL_H 1
#define HAVE_BOOL_T 1
#define HAVE_LONGLONG 1
#define HAVE_LL 1
#define HAVE_VARIADIC_MACROS_C99 1
#define HAVE_VARIADIC_MACROS_GCC 1

#define SIZEOF_INT 4
#define SIZEOF_SHORT 2
#define SIZEOF_LONG 8
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SIZE_T 8
#define SIZEOF_TIME_T 8
#define SIZEOF_OFF_T 8
#define SIZEOF_CURL_OFF_T 8
#define SIZEOF_CURL_SOCKET_T 4

#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_UN_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_POLL_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_SYS_FILIO_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_POLL_H 1
#define HAVE_PWD_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_ASSERT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_TIME_H 1
#define HAVE_UTIME_H 1
#define HAVE_SETJMP_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_NET_IF_H 1
#define HAVE_IFADDRS_H 1

#define HAVE_SOCKET 1
#define HAVE_SELECT 1
#define HAVE_POLL 1
#define HAVE_POLL_FINE 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETADDRINFO_THREADSAFE 1
#define HAVE_FREEADDRINFO 1
#define HAVE_GETNAMEINFO 1
#define HAVE_GETHOSTBYNAME 1
#define HAVE_GETHOSTNAME 1
#define HAVE_GETPEERNAME 1
#define HAVE_GETSOCKNAME 1
#define HAVE_GETSOCKOPT 1
#define HAVE_SETSOCKOPT 1
#define HAVE_RECV 1
#define HAVE_SEND 1
#define HAVE_CONNECT 1
#define HAVE_BIND 1
#define HAVE_LISTEN 1
#define HAVE_ACCEPT 1
#define HAVE_IOCTL 1
#define HAVE_IOCTL_FIONBIO 1
#define HAVE_FCNTL 1
#define HAVE_FCNTL_O_NONBLOCK 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GMTIME_R 1
#define HAVE_LOCALTIME_R 1
#define HAVE_SIGACTION 1
#define HAVE_SIGSETJMP 1
#define HAVE_SIGNAL 1
#define HAVE_ALARM 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR_R 1
#define HAVE_POSIX_STRERROR_R 1
#define HAVE_STRTOK_R 1
#define HAVE_STRTOLL 1
#define HAVE_SNPRINTF 1
#define HAVE_PIPE 1
#define HAVE_SOCKETPAIR 1
#define HAVE_FTRUNCATE 1
#define HAVE_FSEEKO 1
#define HAVE_UTIME 1
#define HAVE_UTIMES 1
#define HAVE_SETLOCALE 1
#define HAVE_CLOCK_GETTIME_MONOTONIC 1
#define HAVE_ARC4RANDOM 1
#define HAVE_FNMATCH 1
#define HAVE_GETEUID 1
#define HAVE_GETPPID 1
#define HAVE_GETPWUID 1
#define HAVE_GETPWUID_R 1
#define HAVE_IF_NAMETOINDEX 1
#define HAVE_INET_NTOP 1
#define HAVE_INET_PTON 1
#define HAVE_SUSECONDS_T 1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1
#define HAVE_STRUCT_TIMEVAL 1
#define HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID 1
#define HAVE_SA_FAMILY_T 1
#define HAVE_SIG_ATOMIC_T 1

#define USE_UNIX_SOCKETS 1

#define CURL_EXTERN_SYMBOL __attribute__((__visibility__("default")))

#endif /* HEADER_CURL_CONFIG_MOS_H */
