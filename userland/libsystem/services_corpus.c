/*
 * mOS libSystem — APIs de serviços que o mOS não tem, com a resposta que um Mac sem o serviço daria
 * (docs/lacunas/: leonardo, larissa, natalia, patricia, otavio):
 *   - OpenCL: nenhuma plataforma (CL_PLATFORM_NOT_FOUND_KHR); o app cai no caminho de CPU;
 *   - vmnet e Hypervisor: sem a extensão de virtualização (VMNET_FAILURE, HV_UNSUPPORTED);
 *   - PC/SC (cartões inteligentes): SCARD_E_NO_SERVICE; libpcap: sem dispositivo de captura;
 *   - libxar: arquivo sem assinatura; Network.framework (nw_framer): sem protocolo próprio;
 *   - DNS-SD: o mDNSResponder não roda (kDNSServiceErr_ServiceNotRunning, igual ao bunsys.c);
 *   - GLU (gluCheckExtension, gluUnProject) e VNImageRectForNormalizedRect: matemática de verdade;
 *   - o lote pequeno do libSystem.B (vis/strvis, wordexp, gcvt/fcvt, carg/cabs, tempnam, errc, htons...).
 */
#include "sys.h"
#include <stdarg.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strdup(const char *s);
char *strchr(const char *s, int c);
char *strstr(const char *h, const char *n);
int snprintf(char *b, size_t n, const char *fmt, ...);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
long write(int fd, const void *b, unsigned long n);
char *getenv(const char *n);
int access(const char *p, int m);
int getpid(void);
void exit(int code);
char *strerror(int e);
extern const char *__progname;
double atan2(double, double), hypot(double, double), pow(double, double), sqrt(double);
float powf(float, float);
long double atan2l(long double, long double), hypotl(long double, long double);

/* ================================================================ OpenCL */
#define CL_SUCCESS 0
#define CL_DEVICE_NOT_FOUND (-1)
#define CL_INVALID_VALUE (-30)
#define CL_INVALID_PLATFORM (-32)
#define CL_INVALID_DEVICE (-33)
#define CL_INVALID_CONTEXT (-34)
#define CL_INVALID_COMMAND_QUEUE (-36)
#define CL_INVALID_MEM_OBJECT (-38)
#define CL_INVALID_PROGRAM (-44)
#define CL_INVALID_KERNEL (-48)
#define CL_INVALID_EVENT (-58)
#define CL_PLATFORM_NOT_FOUND_KHR (-1001)
static void *cl_null(i32 *err, i32 code) { if (err) *err = code; return 0; }
i32 clGetPlatformIDs(u32 n, void **plats, u32 *num) {
    if ((!plats && !num) || (plats && !n)) return CL_INVALID_VALUE;
    if (num) *num = 0;
    return CL_PLATFORM_NOT_FOUND_KHR;
}
i32 clGetPlatformInfo(void *p, u32 name, size_t sz, void *v, size_t *ret) { (void)p; (void)name; (void)sz; (void)v; if (ret) *ret = 0; return CL_INVALID_PLATFORM; }
i32 clGetDeviceIDs(void *p, u64 type, u32 n, void **devs, u32 *num) { (void)p; (void)type; (void)n; (void)devs; if (num) *num = 0; return CL_INVALID_PLATFORM; }
i32 clGetDeviceInfo(void *d, u32 name, size_t sz, void *v, size_t *ret) { (void)d; (void)name; (void)sz; (void)v; if (ret) *ret = 0; return CL_INVALID_DEVICE; }
void *clCreateContext(const void *props, u32 n, void *const *devs, void *cb, void *ud, i32 *err) { (void)props; (void)n; (void)devs; (void)cb; (void)ud; return cl_null(err, CL_INVALID_DEVICE); }
void *clCreateContextFromType(const void *props, u64 type, void *cb, void *ud, i32 *err) { (void)props; (void)type; (void)cb; (void)ud; return cl_null(err, CL_DEVICE_NOT_FOUND); }
void *clCreateCommandQueue(void *ctx, void *dev, u64 props, i32 *err) { (void)ctx; (void)dev; (void)props; return cl_null(err, CL_INVALID_CONTEXT); }
void *clCreateBuffer(void *ctx, u64 flags, size_t sz, void *host, i32 *err) { (void)ctx; (void)flags; (void)sz; (void)host; return cl_null(err, CL_INVALID_CONTEXT); }
void *clCreateImage(void *ctx, u64 flags, const void *fmt, const void *desc, void *host, i32 *err) { (void)ctx; (void)flags; (void)fmt; (void)desc; (void)host; return cl_null(err, CL_INVALID_CONTEXT); }
void *clCreateProgramWithSource(void *ctx, u32 n, const char **src, const size_t *len, i32 *err) { (void)ctx; (void)n; (void)src; (void)len; return cl_null(err, CL_INVALID_CONTEXT); }
void *clCreateKernel(void *prog, const char *name, i32 *err) { (void)prog; (void)name; return cl_null(err, CL_INVALID_PROGRAM); }
i32 clBuildProgram(void *p, u32 n, void *const *d, const char *o, void *cb, void *ud) { (void)p; (void)n; (void)d; (void)o; (void)cb; (void)ud; return CL_INVALID_PROGRAM; }
i32 clGetProgramBuildInfo(void *p, void *d, u32 name, size_t sz, void *v, size_t *ret) { (void)p; (void)d; (void)name; (void)sz; (void)v; if (ret) *ret = 0; return CL_INVALID_PROGRAM; }
i32 clGetMemObjectInfo(void *m, u32 name, size_t sz, void *v, size_t *ret) { (void)m; (void)name; (void)sz; (void)v; if (ret) *ret = 0; return CL_INVALID_MEM_OBJECT; }
i32 clGetImageInfo(void *m, u32 name, size_t sz, void *v, size_t *ret) { (void)m; (void)name; (void)sz; (void)v; if (ret) *ret = 0; return CL_INVALID_MEM_OBJECT; }
i32 clGetEventProfilingInfo(void *e, u32 name, size_t sz, void *v, size_t *ret) { (void)e; (void)name; (void)sz; (void)v; if (ret) *ret = 0; return CL_INVALID_EVENT; }
i32 clGetSupportedImageFormats(void *ctx, u64 flags, u32 type, u32 n, void *fmts, u32 *num) { (void)ctx; (void)flags; (void)type; (void)n; (void)fmts; if (num) *num = 0; return CL_INVALID_CONTEXT; }
i32 clSetKernelArg(void *k, u32 i, size_t sz, const void *v) { (void)k; (void)i; (void)sz; (void)v; return CL_INVALID_KERNEL; }
#define CL_Q(name, ...) i32 name(void *q, ##__VA_ARGS__)
i32 clEnqueueNDRangeKernel(void *q, void *k, u32 dim, const size_t *off, const size_t *g, const size_t *l, u32 n, const void *w, void **e) {
    (void)q; (void)k; (void)dim; (void)off; (void)g; (void)l; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
void *clEnqueueMapBuffer(void *q, void *m, u32 blk, u64 fl, size_t off, size_t sz, u32 n, const void *w, void **e, i32 *err) {
    (void)q; (void)m; (void)blk; (void)fl; (void)off; (void)sz; (void)n; (void)w; if (e) *e = 0; return cl_null(err, CL_INVALID_COMMAND_QUEUE);
}
void *clEnqueueMapImage(void *q, void *m, u32 blk, u64 fl, const size_t *o, const size_t *r, size_t *rp, size_t *sp, u32 n, const void *w, void **e, i32 *err) {
    (void)q; (void)m; (void)blk; (void)fl; (void)o; (void)r; (void)rp; (void)sp; (void)n; (void)w; if (e) *e = 0; return cl_null(err, CL_INVALID_COMMAND_QUEUE);
}
i32 clEnqueueUnmapMemObject(void *q, void *m, void *p, u32 n, const void *w, void **e) { (void)q; (void)m; (void)p; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueFillBuffer(void *q, void *m, const void *pat, size_t ps, size_t off, size_t sz, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)pat; (void)ps; (void)off; (void)sz; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueReadBuffer(void *q, void *m, u32 b, size_t off, size_t sz, void *p, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)b; (void)off; (void)sz; (void)p; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueWriteBuffer(void *q, void *m, u32 b, size_t off, size_t sz, const void *p, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)b; (void)off; (void)sz; (void)p; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueReadImage(void *q, void *m, u32 b, const size_t *o, const size_t *r, size_t rp, size_t sp, void *p, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)b; (void)o; (void)r; (void)rp; (void)sp; (void)p; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueWriteImage(void *q, void *m, u32 b, const size_t *o, const size_t *r, size_t rp, size_t sp, const void *p, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)b; (void)o; (void)r; (void)rp; (void)sp; (void)p; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueCopyImage(void *q, void *a, void *b, const size_t *so, const size_t *dor, const size_t *r, u32 n, const void *w, void **e) {
    (void)q; (void)a; (void)b; (void)so; (void)dor; (void)r; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueCopyBufferToImage(void *q, void *a, void *b, size_t so, const size_t *dor, const size_t *r, u32 n, const void *w, void **e) {
    (void)q; (void)a; (void)b; (void)so; (void)dor; (void)r; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueReadBufferRect(void *q, void *m, u32 b, const size_t *bo, const size_t *ho, const size_t *r, size_t brp, size_t bsp,
                            size_t hrp, size_t hsp, void *p, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)b; (void)bo; (void)ho; (void)r; (void)brp; (void)bsp; (void)hrp; (void)hsp; (void)p; (void)n; (void)w;
    if (e) *e = 0;
    return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueWriteBufferRect(void *q, void *m, u32 b, const size_t *bo, const size_t *ho, const size_t *r, size_t brp, size_t bsp,
                             size_t hrp, size_t hsp, const void *p, u32 n, const void *w, void **e) {
    (void)q; (void)m; (void)b; (void)bo; (void)ho; (void)r; (void)brp; (void)bsp; (void)hrp; (void)hsp; (void)p; (void)n; (void)w;
    if (e) *e = 0;
    return CL_INVALID_COMMAND_QUEUE;
}
i32 clEnqueueCopyBufferRect(void *q, void *a, void *b, const size_t *so, const size_t *dor, const size_t *r, size_t srp, size_t ssp,
                            size_t drp, size_t dsp, u32 n, const void *w, void **e) {
    (void)q; (void)a; (void)b; (void)so; (void)dor; (void)r; (void)srp; (void)ssp; (void)drp; (void)dsp; (void)n; (void)w;
    if (e) *e = 0;
    return CL_INVALID_COMMAND_QUEUE;
}
i32 clWaitForEvents(u32 n, void *const *ev) { (void)n; (void)ev; return CL_INVALID_EVENT; }
i32 clFlush(void *q) { (void)q; return CL_INVALID_COMMAND_QUEUE; }
i32 clFinish(void *q) { (void)q; return CL_INVALID_COMMAND_QUEUE; }
i32 clReleaseProgram(void *o) { (void)o; return CL_INVALID_PROGRAM; }
i32 clReleaseMemObject(void *o) { (void)o; return CL_INVALID_MEM_OBJECT; }
i32 clReleaseKernel(void *o) { (void)o; return CL_INVALID_KERNEL; }
i32 clReleaseEvent(void *o) { (void)o; return CL_INVALID_EVENT; }
i32 clReleaseContext(void *o) { (void)o; return CL_INVALID_CONTEXT; }
i32 clReleaseCommandQueue(void *o) { (void)o; return CL_INVALID_COMMAND_QUEUE; }
i32 clRetainCommandQueue(void *o) { (void)o; return CL_INVALID_COMMAND_QUEUE; }
i32 clRetainContext(void *o) { (void)o; return CL_INVALID_CONTEXT; }
i32 clRetainMemObject(void *o) { (void)o; return CL_INVALID_MEM_OBJECT; }
i32 clRetainEvent(void *o) { (void)o; return CL_INVALID_EVENT; }
i32 clRetainKernel(void *o) { (void)o; return CL_INVALID_KERNEL; }
i32 clRetainProgram(void *o) { (void)o; return CL_INVALID_PROGRAM; }

/* ================================================================ vmnet e Hypervisor */
#define VMNET_FAILURE 1001
#define HV_UNSUPPORTED ((i32)0xfae9400f)
typedef struct { void *isa; int flags, res; void (*invoke)(void *, ...); } blk_t;
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cs_t;
/* as chaves do vmnet são const char * (xpc_dictionary), não CFString */
const char *vmnet_operation_mode_key = "vmnet_operation_mode";
const char *vmnet_max_packet_size_key = "vmnet_max_packet_size";
const char *vmnet_mtu_key = "vmnet_mtu";
const char *vmnet_interface_id_key = "vmnet_interface_id";
const char *vmnet_allocate_mac_address_key = "vmnet_allocate_mac_address";
const char *vmnet_mac_address_key = "vmnet_mac_address";
const char *vmnet_subnet_mask_key = "vmnet_subnet_mask";
const char *vmnet_start_address_key = "vmnet_start_address";
const char *vmnet_end_address_key = "vmnet_end_address";
const char *vmnet_shared_interface_name_key = "vmnet_shared_interface_name";
const char *vmnet_network_identifier_key = "vmnet_network_identifier";
const char *vmnet_enable_isolation_key = "vmnet_enable_isolation";
const char *vmnet_enable_tso_key = "vmnet_enable_tso";
const char *vmnet_enable_checksum_offload_key = "vmnet_enable_checksum_offload";
const char *vmnet_estimated_packets_available_key = "vmnet_estimated_packets_available";
void dispatch_async(void *q, void *blk);
/* o bloco de término recebe (VMNET_FAILURE, NULL) na fila pedida, como sem o com.apple.vmnet */
void *vmnet_start_interface(void *desc, void *queue, blk_t *done) {
    (void)desc; (void)queue;
    if (done && done->invoke) done->invoke(done, (u32)VMNET_FAILURE, (void *)0);
    return 0;
}
u32 vmnet_stop_interface(void *iface, void *queue, blk_t *done) {
    (void)iface; (void)queue;
    if (done && done->invoke) done->invoke(done, (u32)VMNET_FAILURE);
    return VMNET_FAILURE;
}
u32 vmnet_read(void *iface, void *pkts, int *count) { (void)iface; (void)pkts; if (count) *count = 0; return VMNET_FAILURE; }
u32 vmnet_write(void *iface, void *pkts, int *count) { (void)iface; (void)pkts; if (count) *count = 0; return VMNET_FAILURE; }
u32 vmnet_interface_set_event_callback(void *iface, u32 mask, void *queue, void *cb) { (void)iface; (void)mask; (void)queue; (void)cb; return VMNET_FAILURE; }
void *vmnet_copy_shared_interface_list(void) { return 0; }
i32 hv_vm_create(u64 flags) { (void)flags; return HV_UNSUPPORTED; }
i32 hv_vm_destroy(void) { return HV_UNSUPPORTED; }
i32 hv_vm_map(void *uva, u64 gpa, size_t sz, u64 flags) { (void)uva; (void)gpa; (void)sz; (void)flags; return HV_UNSUPPORTED; }
i32 hv_vm_unmap(u64 gpa, size_t sz) { (void)gpa; (void)sz; return HV_UNSUPPORTED; }
i32 hv_vm_protect(u64 gpa, size_t sz, u64 flags) { (void)gpa; (void)sz; (void)flags; return HV_UNSUPPORTED; }
i32 hv_vcpu_create(u32 *vcpu, u64 flags) { (void)flags; if (vcpu) *vcpu = 0; return HV_UNSUPPORTED; }
i32 hv_vcpu_destroy(u32 vcpu) { (void)vcpu; return HV_UNSUPPORTED; }
i32 hv_vcpu_read_register(u32 v, u32 r, u64 *val) { (void)v; (void)r; if (val) *val = 0; return HV_UNSUPPORTED; }
i32 hv_vcpu_write_register(u32 v, u32 r, u64 val) { (void)v; (void)r; (void)val; return HV_UNSUPPORTED; }
i32 hv_vcpu_read_msr(u32 v, u32 m, u64 *val) { (void)v; (void)m; if (val) *val = 0; return HV_UNSUPPORTED; }
i32 hv_vcpu_write_msr(u32 v, u32 m, u64 val) { (void)v; (void)m; (void)val; return HV_UNSUPPORTED; }
i32 hv_vcpu_enable_native_msr(u32 v, u32 m, u8 on) { (void)v; (void)m; (void)on; return HV_UNSUPPORTED; }
i32 hv_vcpu_run(u32 v) { (void)v; return HV_UNSUPPORTED; }
i32 hv_vcpu_run_until(u32 v, u64 deadline) { (void)v; (void)deadline; return HV_UNSUPPORTED; }
i32 hv_vcpu_interrupt(u32 *v, u32 n) { (void)v; (void)n; return HV_UNSUPPORTED; }
i32 hv_vcpu_invalidate_tlb(u32 v) { (void)v; return HV_UNSUPPORTED; }
i32 hv_vcpu_flush(u32 v) { (void)v; return HV_UNSUPPORTED; }
i32 hv_vmx_read_capability(u32 field, u64 *val) { (void)field; if (val) *val = 0; return HV_UNSUPPORTED; }
i32 hv_vmx_vcpu_read_vmcs(u32 v, u32 f, u64 *val) { (void)v; (void)f; if (val) *val = 0; return HV_UNSUPPORTED; }
i32 hv_vmx_vcpu_write_vmcs(u32 v, u32 f, u64 val) { (void)v; (void)f; (void)val; return HV_UNSUPPORTED; }

/* ================================================================ PC/SC, libpcap, libxar */
#define SCARD_E_NO_SERVICE ((i32)0x8010001D)
#define SCARD_E_INVALID_HANDLE ((i32)0x80100003)
i32 SCardEstablishContext(u32 scope, const void *r1, const void *r2, i32 *ctx) { (void)scope; (void)r1; (void)r2; if (ctx) *ctx = 0; return SCARD_E_NO_SERVICE; }
i32 SCardReleaseContext(i32 ctx) { (void)ctx; return SCARD_E_INVALID_HANDLE; }
i32 SCardIsValidContext(i32 ctx) { (void)ctx; return SCARD_E_INVALID_HANDLE; }
i32 SCardListReaders(i32 ctx, const char *groups, char *readers, u32 *len) { (void)ctx; (void)groups; (void)readers; if (len) *len = 0; return SCARD_E_NO_SERVICE; }
i32 SCardConnect(i32 ctx, const char *reader, u32 share, u32 proto, i32 *card, u32 *active) {
    (void)ctx; (void)reader; (void)share; (void)proto; if (card) *card = 0; if (active) *active = 0; return SCARD_E_NO_SERVICE;
}
i32 SCardReconnect(i32 card, u32 share, u32 proto, u32 init, u32 *active) { (void)card; (void)share; (void)proto; (void)init; if (active) *active = 0; return SCARD_E_INVALID_HANDLE; }
i32 SCardDisconnect(i32 card, u32 disp) { (void)card; (void)disp; return SCARD_E_INVALID_HANDLE; }
i32 SCardBeginTransaction(i32 card) { (void)card; return SCARD_E_INVALID_HANDLE; }
i32 SCardEndTransaction(i32 card, u32 disp) { (void)card; (void)disp; return SCARD_E_INVALID_HANDLE; }
i32 SCardStatus(i32 card, char *names, u32 *nlen, u32 *state, u32 *proto, u8 *atr, u32 *atrlen) {
    (void)card; (void)names; if (nlen) *nlen = 0; if (state) *state = 0; if (proto) *proto = 0; (void)atr; if (atrlen) *atrlen = 0;
    return SCARD_E_INVALID_HANDLE;
}
i32 SCardGetStatusChange(i32 ctx, u32 timeout, void *readers, u32 n) { (void)ctx; (void)timeout; (void)readers; (void)n; return SCARD_E_NO_SERVICE; }
i32 SCardTransmit(i32 card, const void *spci, const u8 *in, u32 inlen, void *rpci, u8 *out, u32 *outlen) {
    (void)card; (void)spci; (void)in; (void)inlen; (void)rpci; (void)out; if (outlen) *outlen = 0; return SCARD_E_INVALID_HANDLE;
}
i32 SCardGetAttrib(i32 card, u32 id, u8 *attr, u32 *len) { (void)card; (void)id; (void)attr; if (len) *len = 0; return SCARD_E_INVALID_HANDLE; }
i32 SCardCancel(i32 ctx) { (void)ctx; return SCARD_E_INVALID_HANDLE; }
/* libpcap: sem BPF no mOS. pcap_create devolve NULL com a mensagem no errbuf (PCAP_ERRBUF_SIZE = 256) */
#define PCAP_ERRBUF_SIZE 256
static const char pcap_msg[] = "captura de pacotes indisponível no mOS (sem /dev/bpf)";
static void pcap_err(char *e) { if (e) { size_t n = sizeof pcap_msg; if (n > PCAP_ERRBUF_SIZE) n = PCAP_ERRBUF_SIZE; memcpy(e, pcap_msg, n); e[n - 1] = 0; } }
void *pcap_create(const char *dev, char *err) { (void)dev; pcap_err(err); return 0; }
void *pcap_open_live(const char *dev, int snap, int promisc, int ms, char *err) { (void)dev; (void)snap; (void)promisc; (void)ms; pcap_err(err); return 0; }
int pcap_findalldevs(void **devs, char *err) { if (devs) *devs = 0; (void)err; return 0; }   /* nenhuma interface */
void pcap_freealldevs(void *devs) { (void)devs; }
int pcap_set_timeout(void *p, int ms) { (void)p; (void)ms; return -1; }
int pcap_set_snaplen(void *p, int n) { (void)p; (void)n; return -1; }
int pcap_set_promisc(void *p, int on) { (void)p; (void)on; return -1; }
int pcap_set_immediate_mode(void *p, int on) { (void)p; (void)on; return -1; }
int pcap_set_buffer_size(void *p, int n) { (void)p; (void)n; return -1; }
int pcap_activate(void *p) { (void)p; return -1; }                 /* PCAP_ERROR */
void pcap_close(void *p) { (void)p; }
int pcap_next_ex(void *p, void **hdr, const u8 **data) { (void)p; if (hdr) *hdr = 0; if (data) *data = 0; return -1; }
char *pcap_geterr(void *p) { (void)p; return (char *)pcap_msg; }
const char *pcap_lib_version(void) { return "libpcap (mOS, sem captura)"; }
/* libxar: os arquivos (.pkg) que o instalador do mOS abre não trazem assinatura para estas funções */
void *xar_signature_first(void *x) { (void)x; return 0; }
void *xar_signature_next(void *s) { (void)s; return 0; }
const char *xar_signature_type(void *s) { (void)s; return 0; }
int xar_signature_get_x509certificate_count(void *s) { (void)s; return 0; }
int xar_signature_get_x509certificate_data(void *s, int i, const u8 **data, u32 *len) { (void)s; (void)i; if (data) *data = 0; if (len) *len = 0; return -1; }
int xar_signature_copy_signed_data(void *s, u8 **data, u32 *len, u8 **sig, u32 *slen, long *off) {
    (void)s; if (data) *data = 0; if (len) *len = 0; if (sig) *sig = 0; if (slen) *slen = 0; if (off) *off = 0; return -1;
}

/* ================================================================ Network.framework: framers */
void *nw_framer_create_definition(const char *id, u32 flags, void *start) { (void)id; (void)flags; (void)start; return 0; }
void *nw_framer_create_options(void *def) { (void)def; return 0; }
void *nw_framer_message_create(void *framer) { (void)framer; return 0; }
void nw_framer_set_output_handler(void *f, void *h) { (void)f; (void)h; }
void nw_framer_set_input_handler(void *f, void *h) { (void)f; (void)h; }
void nw_framer_set_stop_handler(void *f, void *h) { (void)f; (void)h; }
void nw_framer_set_cleanup_handler(void *f, void *h) { (void)f; (void)h; }
void nw_framer_set_wakeup_handler(void *f, void *h) { (void)f; (void)h; }
u8 nw_framer_parse_output(void *f, size_t min, size_t max, u8 *buf, void *parse) { (void)f; (void)min; (void)max; (void)buf; (void)parse; return 0; }
u8 nw_framer_parse_input(void *f, size_t min, size_t max, u8 *buf, void *parse) { (void)f; (void)min; (void)max; (void)buf; (void)parse; return 0; }
void nw_framer_deliver_input(void *f, const u8 *b, size_t n, void *msg, u8 complete) { (void)f; (void)b; (void)n; (void)msg; (void)complete; }
u8 nw_framer_deliver_input_no_copy(void *f, size_t n, void *msg, u8 complete) { (void)f; (void)n; (void)msg; (void)complete; return 0; }
void nw_framer_write_output(void *f, const u8 *b, size_t n) { (void)f; (void)b; (void)n; }
void nw_framer_mark_ready(void *f) { (void)f; }
void nw_framer_mark_failed_with_error(void *f, int err) { (void)f; (void)err; }
void nw_framer_async(void *f, blk_t *b) { (void)f; if (b && b->invoke) b->invoke(b); }

/* ================================================================ DNS-SD (igual ao bunsys.c: serviço parado) */
#define kDNSServiceErr_ServiceNotRunning (-65563)
int DNSServiceSetDispatchQueue(void *sd, void *q) { (void)sd; (void)q; return kDNSServiceErr_ServiceNotRunning; }
int DNSServiceResolve(void **sd, u32 flags, u32 iface, const char *name, const char *type, const char *dom, void *cb, void *ctx) {
    (void)flags; (void)iface; (void)name; (void)type; (void)dom; (void)cb; (void)ctx; if (sd) *sd = 0; return kDNSServiceErr_ServiceNotRunning;
}
int DNSServiceBrowse(void **sd, u32 flags, u32 iface, const char *type, const char *dom, void *cb, void *ctx) {
    (void)flags; (void)iface; (void)type; (void)dom; (void)cb; (void)ctx; if (sd) *sd = 0; return kDNSServiceErr_ServiceNotRunning;
}
int DNSServiceRegisterRecord(void *sd, void **rec, u32 flags, u32 iface, const char *name, u16 t, u16 c, u16 len, const void *data, u32 ttl, void *cb, void *ctx) {
    (void)sd; (void)flags; (void)iface; (void)name; (void)t; (void)c; (void)len; (void)data; (void)ttl; (void)cb; (void)ctx;
    if (rec) *rec = 0;
    return kDNSServiceErr_ServiceNotRunning;
}
int DNSServiceNATPortMappingCreate(void **sd, u32 flags, u32 iface, u32 proto, u16 ip, u16 ep, u32 ttl, void *cb, void *ctx) {
    (void)flags; (void)iface; (void)proto; (void)ip; (void)ep; (void)ttl; (void)cb; (void)ctx; if (sd) *sd = 0; return kDNSServiceErr_ServiceNotRunning;
}
int DNSServiceEnumerateDomains(void **sd, u32 flags, u32 iface, void *cb, void *ctx) { (void)flags; (void)iface; (void)cb; (void)ctx; if (sd) *sd = 0; return kDNSServiceErr_ServiceNotRunning; }
int DNSServiceAddRecord(void *sd, void **rec, u32 flags, u16 t, u16 len, const void *data, u32 ttl) { (void)sd; (void)flags; (void)t; (void)len; (void)data; (void)ttl; if (rec) *rec = 0; return kDNSServiceErr_ServiceNotRunning; }
int DNSServiceUpdateRecord(void *sd, void *rec, u32 flags, u16 len, const void *data, u32 ttl) { (void)sd; (void)rec; (void)flags; (void)len; (void)data; (void)ttl; return kDNSServiceErr_ServiceNotRunning; }
int DNSServiceRemoveRecord(void *sd, void *rec, u32 flags) { (void)sd; (void)rec; (void)flags; return kDNSServiceErr_ServiceNotRunning; }
/* TXT records: montagem e leitura de verdade (formato de rótulos com comprimento) */
typedef struct { u8 *buf; u16 cap, len; u8 own; } txtref_t;
void TXTRecordCreate(void *ref, u16 cap, void *buf) {
    txtref_t *t = ref;
    t->buf = buf; t->cap = buf ? cap : 0; t->len = 0; t->own = 0;
}
void TXTRecordDeallocate(void *ref) { txtref_t *t = ref; if (t->own) free(t->buf); t->buf = 0; t->len = t->cap = 0; }
u16 TXTRecordGetLength(const void *ref) { return ((const txtref_t *)ref)->len; }
const void *TXTRecordGetBytesPtr(const void *ref) { return ((const txtref_t *)ref)->buf; }
int TXTRecordSetValue(void *ref, const char *key, u8 vlen, const void *val) {
    txtref_t *t = ref;
    size_t kl = strlen(key), item = kl + (val ? 1u + vlen : 0u);
    if (item > 255) return -65540;                /* kDNSServiceErr_Invalid */
    if ((size_t)t->len + item + 1 > t->cap) {
        if (t->buf && !t->own) return -65541;     /* kDNSServiceErr_NoMemory */
        size_t nc = (size_t)t->len + item + 1 + 64;
        if (nc > 65535) return -65541;
        u8 *n = realloc(t->buf, nc);
        if (!n) return -65541;
        t->buf = n; t->cap = (u16)nc; t->own = 1;
    }
    u8 *p = t->buf + t->len;
    *p++ = (u8)item;
    memcpy(p, key, kl); p += kl;
    if (val) { *p++ = '='; memcpy(p, val, vlen); }
    t->len = (u16)(t->len + item + 1);
    return 0;
}
static const u8 *txt_find(u16 len, const u8 *txt, const char *key, u8 *ilen) {
    size_t kl = strlen(key);
    for (u16 i = 0; i < len;) {
        u8 n = txt[i];
        const u8 *it = txt + i + 1;
        if (i + 1u + n > len) break;
        if (n >= kl && !strncmp((const char *)it, key, kl) && (n == kl || it[kl] == '=')) { *ilen = n; return it; }
        i = (u16)(i + 1 + n);
    }
    return 0;
}
const void *TXTRecordGetValuePtr(u16 len, const void *txt, const char *key, u8 *vlen) {
    u8 n;
    const u8 *it = txt_find(len, txt, key, &n);
    size_t kl = strlen(key);
    if (!it || n == kl) { if (vlen) *vlen = 0; return 0; }
    if (vlen) *vlen = (u8)(n - kl - 1);
    return it + kl + 1;
}
int TXTRecordContainsKey(u16 len, const void *txt, const char *key) { u8 n; return txt_find(len, txt, key, &n) != 0; }
u16 TXTRecordGetCount(u16 len, const void *txt) {
    u16 c = 0;
    for (u16 i = 0; i < len; i = (u16)(i + 1 + ((const u8 *)txt)[i])) c++;
    return c;
}

/* ================================================================ GLU e Vision */
u8 gluCheckExtension(const u8 *name, const u8 *list) {
    if (!name || !list) return 0;
    size_t n = strlen((const char *)name);
    for (const char *p = (const char *)list; (p = strstr(p, (const char *)name)); p += n) {
        int start = p == (const char *)list || p[-1] == ' ';
        int end = p[n] == 0 || p[n] == ' ';
        if (start && end) return 1;
    }
    return 0;
}
/* inverte (projeção × modelo), leva a janela para [-1,1] e desfaz a perspectiva (igual ao SGI GLU) */
static int inv4(const double *m, double *o) {
    double a[4][8];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { a[i][j] = m[j * 4 + i]; a[i][4 + j] = i == j; }
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if ((a[r][c] < 0 ? -a[r][c] : a[r][c]) > (a[p][c] < 0 ? -a[p][c] : a[p][c])) p = r;
        if (a[p][c] == 0) return 0;
        if (p != c) for (int k = 0; k < 8; k++) { double t = a[c][k]; a[c][k] = a[p][k]; a[p][k] = t; }
        double d = a[c][c];
        for (int k = 0; k < 8; k++) a[c][k] /= d;
        for (int r = 0; r < 4; r++) if (r != c) { double f = a[r][c]; for (int k = 0; k < 8; k++) a[r][k] -= f * a[c][k]; }
    }
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) o[j * 4 + i] = a[i][4 + j];
    return 1;
}
int gluUnProject(double wx, double wy, double wz, const double *model, const double *proj, const int *vp, double *ox, double *oy, double *oz) {
    double pm[16], inv[16];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {       /* coluna-maior: pm = proj * model */
        double s = 0;
        for (int k = 0; k < 4; k++) s += proj[k * 4 + i] * model[j * 4 + k];
        pm[j * 4 + i] = s;
    }
    if (!inv4(pm, inv)) return 0;
    double in[4] = {(wx - vp[0]) / vp[2] * 2 - 1, (wy - vp[1]) / vp[3] * 2 - 1, wz * 2 - 1, 1}, out[4];
    for (int i = 0; i < 4; i++) out[i] = inv[0 * 4 + i] * in[0] + inv[1 * 4 + i] * in[1] + inv[2 * 4 + i] * in[2] + inv[3 * 4 + i] * in[3];
    if (out[3] == 0) return 0;
    *ox = out[0] / out[3]; *oy = out[1] / out[3]; *oz = out[2] / out[3];
    return 1;
}
typedef struct { double x, y, w, h; } vrect_t;
vrect_t VNImageRectForNormalizedRect(vrect_t r, size_t w, size_t h) {
    vrect_t o = {r.x * (double)w, r.y * (double)h, r.w * (double)w, r.h * (double)h};
    return o;
}
typedef struct { double x, y; } vpt_t;
vpt_t VNImagePointForNormalizedPoint(vpt_t p, size_t w, size_t h) { vpt_t o = {p.x * (double)w, p.y * (double)h}; return o; }
vrect_t VNNormalizedRectForImageRect(vrect_t r, size_t w, size_t h) {
    vrect_t o = {w ? r.x / (double)w : 0, h ? r.y / (double)h : 0, w ? r.w / (double)w : 0, h ? r.h / (double)h : 0};
    return o;
}

/* ================================================================ libSystem.B: o lote pequeno */
const char *locale_charset(void) { return "UTF-8"; }
void sasl_dispose(void **conn) { if (conn) *conn = 0; }
size_t wcsnlen(const int *s, size_t max) { size_t i = 0; while (i < max && s[i]) i++; return i; }
int iswascii(int c) { return c >= 0 && c < 128; }
u16 htons(u16 v) { return (u16)((v << 8) | (v >> 8)); }
const char *_NSGetProgname(void) { return __progname; }
const char *const kSBXProfilePureComputation = "pure-computation";
const char *const kSBXProfileNoNetwork = "no-network";
const char *const kSBXProfileNoInternet = "no-internet";
const char *const kSBXProfileNoWrite = "no-write";
const char *const kSBXProfileNoWriteExceptTemporary = "no-write-except-temporary";
int pthread_setconcurrency(int n) { return n < 0 ? 22 : 0; }
int pthread_getconcurrency(void) { return 0; }
int pthread_cpu_number_np(size_t *cpu) { if (!cpu) return 22; *cpu = 0; return 0; }
int posix_spawnattr_set_qos_class_np(void *a, u32 qos) { (void)a; (void)qos; return 0; }
int posix_spawnattr_get_qos_class_np(const void *a, u32 *qos) { (void)a; if (qos) *qos = 0x15; return 0; }   /* QOS_CLASS_DEFAULT */
/* aio: o mOS não tem fila assíncrona; nada está em andamento */
int aio_cancel(int fd, void *cb) { (void)fd; (void)cb; return 2; }    /* AIO_ALLDONE */
int lio_listio(int mode, void *const *list, int n, void *sig) { (void)mode; (void)list; (void)n; (void)sig; cx_errno = 78; return -1; }
/* coalizões e consultas de página: sem dados */
int coalition_info_resource_usage(u64 cid, void *buf, size_t sz) { (void)cid; (void)buf; (void)sz; cx_errno = 78; return -1; }
int mach_vm_page_query(u32 task, u64 addr, int *disp, int *ref) { (void)task; (void)addr; if (disp) *disp = 1; if (ref) *ref = 0; return 0; }   /* VM_PAGE_QUERY_PAGE_PRESENT */
void *sbrk(intptr_t inc) { if (!inc) return (void *)0x1000; cx_errno = 12; return (void *)-1; }   /* ENOMEM: o malloc do mOS não usa brk */
void mig_strncpy(char *d, const char *s, int n) {
    if (n <= 0) return;
    int i = 0;
    for (; i < n - 1 && s && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}
/* gcvt/fcvt (antigas de stdlib): pelo printf */
char *gcvt(double v, int nd, char *buf) { snprintf(buf, 64, "%.*g", nd > 0 ? nd : 1, v); return buf; }
char *fcvt(double v, int nd, int *dec, int *sign) {
    static char b[400];
    if (nd < 0) nd = 0;
    if (nd > 300) nd = 300;
    *sign = v < 0;
    if (v < 0) v = -v;
    char t[400];
    snprintf(t, sizeof t, "%.*f", nd, v);
    char *dot = strchr(t, '.');
    int ip = dot ? (int)(dot - t) : (int)strlen(t);
    size_t o = 0;
    for (char *p = t; *p; p++) if (*p != '.') b[o++] = *p;
    b[o] = 0;
    char *q = b;                                  /* sem os zeros à esquerda da parte inteira */
    while (*q == '0' && ip > 0) { q++; ip--; }
    *dec = ip;
    return q;
}
/* fmtcheck: fmt se os especificadores baterem com os de def, senão def (BSD) */
static int next_spec(const char **p) {
    for (; **p; (*p)++) {
        if (**p != '%') continue;
        (*p)++;
        if (**p == '%') continue;
        while (**p && strchr("-+ #0123456789.*hlLqjzt", **p)) (*p)++;
        if (!**p) return 0;
        return *(*p)++;
    }
    return 0;
}
const char *fmtcheck(const char *fmt, const char *def) {
    if (!fmt) return def;
    const char *a = fmt, *b = def;
    for (;;) {
        int x = next_spec(&a), y = next_spec(&b);
        if (x != y) return def;
        if (!x) return fmt;
    }
}
/* vis/strvis/strunvis com VIS_OCTAL e o estilo C (\n, \t...) */
#define VIS_OCTAL 0x01
#define VIS_CSTYLE 0x02
#define VIS_SP 0x04
#define VIS_TAB 0x08
#define VIS_NL 0x10
#define VIS_SAFE 0x20
#define VIS_NOSLASH 0x40
char *vis(char *dst, int c, int flag, int next) {
    (void)next;
    u8 ch = (u8)c;
    int graph = ch > 32 && ch < 127;
    if ((graph && ch != '\\') || (ch == ' ' && !(flag & VIS_SP)) || (ch == '\t' && !(flag & VIS_TAB)) ||
        (ch == '\n' && !(flag & VIS_NL)) || ((flag & VIS_SAFE) && (ch == '\b' || ch == '\a' || ch == '\r'))) {
        *dst++ = (char)ch;
        *dst = 0;
        return dst;
    }
    if (ch == '\\' && !(flag & VIS_NOSLASH)) { *dst++ = '\\'; *dst++ = '\\'; *dst = 0; return dst; }
    if (flag & VIS_CSTYLE) {
        const char *m = 0;
        switch (ch) { case '\n': m = "n"; break; case '\r': m = "r"; break; case '\b': m = "b"; break; case '\a': m = "a"; break;
                      case '\v': m = "v"; break; case '\t': m = "t"; break; case '\f': m = "f"; break; case ' ': m = "s"; break;
                      case 0: m = "000"; break; }
        if (m) { *dst++ = '\\'; while (*m) *dst++ = *m++; *dst = 0; return dst; }
    }
    if ((flag & VIS_OCTAL) || ch == ' ' || ch >= 0x80 || (flag & VIS_CSTYLE)) {
        *dst++ = '\\';
        *dst++ = (char)('0' + (ch >> 6));
        *dst++ = (char)('0' + ((ch >> 3) & 7));
        *dst++ = (char)('0' + (ch & 7));
        *dst = 0;
        return dst;
    }
    if (!(flag & VIS_NOSLASH)) *dst++ = '\\';
    if (ch & 0x80) { *dst++ = 'M'; ch &= 0x7f; }
    if (ch < 32 || ch == 127) { *dst++ = '^'; *dst++ = ch == 127 ? '?' : (char)(ch + '@'); }
    else { *dst++ = '-'; *dst++ = (char)ch; }
    *dst = 0;
    return dst;
}
int strvis(char *dst, const char *src, int flag) {
    char *s = dst;
    for (; *src; src++) dst = vis(dst, (u8)*src, flag, (u8)src[1]);
    *dst = 0;
    return (int)(dst - s);
}
int strnvis(char *dst, const char *src, size_t len, int flag) {
    char tmp[8], *s = dst, *end = dst + (len ? len - 1 : 0);
    for (; *src; src++) {
        char *e = vis(tmp, (u8)*src, flag, (u8)src[1]);
        size_t n = (size_t)(e - tmp);
        if (dst + n > end) break;
        memcpy(dst, tmp, n);
        dst += n;
    }
    if (len) *dst = 0;
    return (int)(dst - s);
}
int strunvis(char *dst, const char *src) {
    char *s = dst;
    while (*src) {
        if (*src != '\\') { *dst++ = *src++; continue; }
        src++;
        switch (*src) {
        case 'n': *dst++ = '\n'; src++; break; case 't': *dst++ = '\t'; src++; break; case 'r': *dst++ = '\r'; src++; break;
        case 'b': *dst++ = '\b'; src++; break; case 'a': *dst++ = '\a'; src++; break; case 'v': *dst++ = '\v'; src++; break;
        case 'f': *dst++ = '\f'; src++; break; case 's': *dst++ = ' '; src++; break; case '\\': *dst++ = '\\'; src++; break;
        case 'M': {
            src++;
            if (*src == '-') { src++; *dst++ = (char)(*src++ | 0x80); }
            else if (*src == '^') { src++; *dst++ = (char)((*src == '?' ? 127 : *src - '@') | 0x80); src++; }
            else return -1;
            break;
        }
        case '^': src++; *dst++ = *src == '?' ? 127 : (char)(*src - '@'); src++; break;
        case '-': src++; *dst++ = *src++; break;
        default:
            if (*src >= '0' && *src <= '7') {
                int v = 0;
                for (int k = 0; k < 3 && *src >= '0' && *src <= '7'; k++) v = v * 8 + (*src++ - '0');
                *dst++ = (char)v;
            } else if (*src) *dst++ = *src++;
            else return -1;
        }
    }
    *dst = 0;
    return (int)(dst - s);
}
/* wordexp: divide em palavras com aspas e \; ~ no começo e $VAR/${VAR} expandidos. Sem substituição de comando
 * (WRDE_NOCMD é o que o código seguro pede) */
typedef struct { size_t we_wordc; char **we_wordv; size_t we_offs; } wordexp_t;
#define WRDE_APPEND 0x01
#define WRDE_DOOFFS 0x02
#define WRDE_NOSPACE 4
#define WRDE_CMDSUB 3
#define WRDE_SYNTAX 6
static int we_push(wordexp_t *w, char *s) {
    size_t base = (w->we_wordv ? 0 : 0);
    (void)base;
    size_t total = w->we_offs + w->we_wordc;
    char **v = realloc(w->we_wordv, (total + 2) * sizeof *v);
    if (!v) { free(s); return WRDE_NOSPACE; }
    w->we_wordv = v;
    v[total] = s;
    v[total + 1] = 0;
    w->we_wordc++;
    return 0;
}
/* $NOME ou ${NOME} em *pp: avança e devolve o valor ("" se não definida); 0 se não é variável; WE_BAD se mal fechada */
static const char we_bad_s[1];
#define WE_BAD (we_bad_s)
static const char *we_var(const char **pp) {
    const char *p = *pp;
    if (*p != '$') return 0;
    p++;
    int brace = *p == '{';
    if (brace) p++;
    if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return 0;
    char name[128];
    size_t k = 0;
    while (k + 1 < sizeof name && (*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')))
        name[k++] = *p++;
    name[k] = 0;
    if (brace) { if (*p != '}') return WE_BAD; p++; }
    *pp = p;
    const char *v = getenv(name);
    return v ? v : "";
}
int wordexp(const char *words, wordexp_t *w, int flags) {
    if (!(flags & WRDE_APPEND)) {
        size_t offs = (flags & WRDE_DOOFFS) ? w->we_offs : 0;
        w->we_wordc = 0;
        w->we_offs = offs;
        w->we_wordv = calloc(offs + 1, sizeof(char *));
        if (!w->we_wordv) return WRDE_NOSPACE;
    }
    const char *p = words;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        size_t cap = 64, n = 0;
        char *o = malloc(cap);
        if (!o) return WRDE_NOSPACE;
#define PUT(ch) do { if (n + 1 >= cap) { cap *= 2; char *t_ = realloc(o, cap); if (!t_) { free(o); return WRDE_NOSPACE; } o = t_; } o[n++] = (ch); } while (0)
        int first = 1;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
            if (*p == '`' || (*p == '$' && p[1] == '(')) { free(o); return WRDE_CMDSUB; }
            if (*p == '|' || *p == '&' || *p == ';' || *p == '<' || *p == '>' || *p == '(' || *p == ')' || *p == '{' || *p == '}') {
                if (!(*p == '{' && n && o[n - 1] == '$')) { free(o); return WRDE_SYNTAX; }
            }
            if (first && *p == '~' && (p[1] == '/' || !p[1] || p[1] == ' ')) {
                const char *h = getenv("HOME");
                for (; h && *h; h++) PUT(*h);
                p++;
            } else if (*p == '\'') {
                p++;
                while (*p && *p != '\'') PUT(*p++);
                if (*p != '\'') { free(o); return WRDE_SYNTAX; }
                p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && p[1]) { p++; PUT(*p++); continue; }
                    if (*p == '`' || (*p == '$' && p[1] == '(')) { free(o); return WRDE_CMDSUB; }
                    if (*p == '$') {
                        const char *v = we_var(&p);
                        if (v == WE_BAD) { free(o); return WRDE_SYNTAX; }
                        if (v) { for (; *v; v++) PUT(*v); continue; }
                    }
                    PUT(*p++);
                }
                if (*p != '"') { free(o); return WRDE_SYNTAX; }
                p++;
            } else if (*p == '\\' && p[1]) {
                p++;
                PUT(*p++);
            } else if (*p == '$') {
                const char *v = we_var(&p);
                if (v == WE_BAD) { free(o); return WRDE_SYNTAX; }
                if (v) { for (; *v; v++) PUT(*v); } else PUT(*p++);
            } else PUT(*p++);
            first = 0;
        }
#undef PUT
        o[n] = 0;
        int r = we_push(w, o);
        if (r) return r;
    }
    return 0;
}
void wordfree(wordexp_t *w) {
    if (!w || !w->we_wordv) return;
    for (size_t i = 0; i < w->we_wordc; i++) free(w->we_wordv[w->we_offs + i]);
    free(w->we_wordv);
    w->we_wordv = 0;
    w->we_wordc = 0;
}
/* tempnam: dir de TMPDIR, do argumento ou /tmp, com o prefixo e um número único */
char *tempnam(const char *dir, const char *pfx) {
    static unsigned seq;
    const char *d = getenv("TMPDIR");
    if (!d || access(d, 2)) d = dir && !access(dir, 2) ? dir : "/tmp";
    char *b = malloc(1024);
    if (!b) return 0;
    for (int tries = 0; tries < 1000; tries++) {
        snprintf(b, 1024, "%s%s%.5s%d.%u", d, d[0] && d[strlen(d) - 1] == '/' ? "" : "/", pfx ? pfx : "tmp", getpid(), seq++);
        if (access(b, 0)) return b;
    }
    free(b);
    return 0;
}
/* errc/verrc: err(3) com o código de erro dado */
void verrc(int eval, int code, const char *fmt, va_list ap) {
    char b[1024];
    int n = snprintf(b, sizeof b, "%s: ", __progname ? __progname : "");
    if (fmt && n >= 0 && n < (int)sizeof b) { int k = vsnprintf(b + n, sizeof b - (size_t)n, fmt, ap); if (k > 0) n += k; if (n > (int)sizeof b - 1) n = (int)sizeof b - 1; }
    if (n >= 0 && n < (int)sizeof b) snprintf(b + n, sizeof b - (size_t)n, "%s%s\n", fmt ? ": " : "", strerror(code));
    long w = write(2, b, strlen(b));
    (void)w;
    exit(eval);
}
void errc(int eval, int code, const char *fmt, ...) { va_list ap; va_start(ap, fmt); verrc(eval, code, fmt, ap); va_end(ap); }
void vwarnc(int code, const char *fmt, va_list ap) {
    char b[1024];
    int n = snprintf(b, sizeof b, "%s: ", __progname ? __progname : "");
    if (fmt && n >= 0 && n < (int)sizeof b) { int k = vsnprintf(b + n, sizeof b - (size_t)n, fmt, ap); if (k > 0) n += k; if (n > (int)sizeof b - 1) n = (int)sizeof b - 1; }
    if (n >= 0 && n < (int)sizeof b) snprintf(b + n, sizeof b - (size_t)n, "%s%s\n", fmt ? ": " : "", strerror(code));
    long w = write(2, b, strlen(b));
    (void)w;
}
void warnc(int code, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vwarnc(code, fmt, ap); va_end(ap); }
/* complexos: carg/cabs e as variantes; as trigonométricas long double pela double (libc_corpus4.c) */
double carg(double _Complex z) { return atan2(__imag__ z, __real__ z); }
float cargf(float _Complex z) { return (float)atan2(__imag__ z, __real__ z); }
long double cargl(long double _Complex z) { return atan2l(__imag__ z, __real__ z); }
double cabs(double _Complex z) { return hypot(__real__ z, __imag__ z); }
float cabsf(float _Complex z) { return (float)hypot(__real__ z, __imag__ z); }
long double cabsl(long double _Complex z) { return hypotl(__real__ z, __imag__ z); }
#define LD1(name) \
    double _Complex name(double _Complex z); \
    long double _Complex name##l(long double _Complex z) { \
        double _Complex d; __real__ d = (double)__real__ z; __imag__ d = (double)__imag__ z; \
        double _Complex r = name(d); long double _Complex o; __real__ o = __real__ r; __imag__ o = __imag__ r; return o; }
LD1(ctan) LD1(ctanh) LD1(csin) LD1(csinh) LD1(ccos) LD1(ccosh) LD1(catan) LD1(casin) LD1(casinh) LD1(cacos) LD1(cacosh)
/* compiler-rt: x elevado a inteiro */
double __powidf2(double a, int b) {
    int neg = b < 0;
    unsigned e = neg ? (unsigned)-(long)b : (unsigned)b;
    double r = 1;
    while (e) { if (e & 1) r *= a; a *= a; e >>= 1; }
    return neg ? 1 / r : r;
}
float __powisf2(float a, int b) { return (float)__powidf2(a, b); }
/* simd: pow componente a componente */
typedef float f4v __attribute__((ext_vector_type(4)));
f4v _simd_pow_f4(f4v x, f4v y) { f4v r; for (int i = 0; i < 4; i++) r[i] = powf(x[i], y[i]); return r; }
typedef double d2v __attribute__((ext_vector_type(2)));
d2v _simd_pow_d2(d2v x, d2v y) { d2v r; for (int i = 0; i < 2; i++) r[i] = pow(x[i], y[i]); return r; }
/* variantes _l e o resto da libc */
char *strptime(const char *s, const char *fmt, void *tm);
char *strptime_l(const char *s, const char *fmt, void *tm, void *loc) { (void)loc; return strptime(s, fmt, tm); }
int regcomp(void *re, const char *pat, int flags);
int regcomp_l(void *re, const char *pat, int flags, void *loc) { (void)loc; return regcomp(re, pat, flags); }
/* swapcontext: o getcontext do mOS não guarda contexto (ENOTSUP, posixmore.c); o par falha junto */
int swapcontext(void *o, const void *u) { (void)o; (void)u; cx_errno = 45; return -1; }
/* _sigtramp: o trampolim de sinal do kernel do macOS; o mOS entrega o sinal direto no handler */
void _sigtramp(void *catcher, int style, int sig, void *info, void *uap) {
    (void)style; (void)uap;
    if (catcher) ((void (*)(int, void *, void *))catcher)(sig, info, uap);
}
/* getsect*fromheader (32 bits): com um cabeçalho de 64 bits delegam para a versão _64 (libc_corpus.c) */
typedef struct { u32 magic; } mh_any_t;
void *getsectbynamefromheader_64(const void *mh, const char *seg, const char *sect);
u8 *getsectdatafromheader_64(const void *mh, const char *seg, const char *sect, u64 *size);
void *getsectbynamefromheader(const void *mh, const char *seg, const char *sect) {
    return mh && ((const mh_any_t *)mh)->magic == 0xfeedfacf ? getsectbynamefromheader_64(mh, seg, sect) : 0;
}
char *getsectdatafromheader(const void *mh, const char *seg, const char *sect, u32 *size) {
    u64 s = 0;
    u8 *r = mh && ((const mh_any_t *)mh)->magic == 0xfeedfacf ? getsectdatafromheader_64(mh, seg, sect, &s) : 0;
    if (size) *size = (u32)s;
    return (char *)r;
}
/* dyld: cabeçalho da imagem que contém o endereço (pelo dladdr) e o UUID (o carregador do mOS não guarda) */
typedef struct { const char *fname; void *fbase; const char *sname; void *saddr; } dlinfo_t;
int dladdr(const void *a, dlinfo_t *i);
const void *dyld_image_header_containing_address(const void *addr) { dlinfo_t i; return dladdr(addr, &i) ? i.fbase : 0; }
u8 _dyld_get_image_uuid(const void *mh, u8 *uuid) { (void)mh; if (uuid) memset(uuid, 0, 16); return 0; }
/* getfsstat64 e getmntinfo_r_np: a mesma lista de volumes do getfsstat do mOS */
int getfsstat(void *buf, int size, int flags);
int getfsstat64(void *buf, int size, int flags) { return getfsstat(buf, size, flags); }
