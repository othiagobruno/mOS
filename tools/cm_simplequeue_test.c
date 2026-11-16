/* Host: CMSimpleQueue do openbot_cm.c (anel de ponteiros). → "cmsimplequeue ok" */
#include <stdio.h>
int CMSimpleQueueCreate(void *, int, void **);
int CMSimpleQueueEnqueue(void *, const void *);
const void *CMSimpleQueueDequeue(void *);
const void *CMSimpleQueueGetHead(void *);
int CMSimpleQueueGetCount(void *), CMSimpleQueueGetCapacity(void *), CMSimpleQueueReset(void *);
int main(void) {
    void *q = 0; int v[5] = {1, 2, 3, 4, 5}, ok = 1;
    ok &= CMSimpleQueueCreate(0, 3, &q) == 0 && CMSimpleQueueGetCapacity(q) == 3;
    ok &= !CMSimpleQueueEnqueue(q, &v[0]) && !CMSimpleQueueEnqueue(q, &v[1]) && !CMSimpleQueueEnqueue(q, &v[2]);
    ok &= CMSimpleQueueEnqueue(q, &v[3]) != 0 && CMSimpleQueueGetCount(q) == 3;   /* cheia */
    ok &= CMSimpleQueueDequeue(q) == &v[0] && CMSimpleQueueGetHead(q) == &v[1];
    ok &= !CMSimpleQueueEnqueue(q, &v[3]) && CMSimpleQueueDequeue(q) == &v[1] && CMSimpleQueueDequeue(q) == &v[2] && CMSimpleQueueDequeue(q) == &v[3];
    ok &= CMSimpleQueueDequeue(q) == 0 && CMSimpleQueueGetCount(q) == 0;
    ok &= !CMSimpleQueueEnqueue(q, &v[4]) && !CMSimpleQueueReset(q) && CMSimpleQueueGetCount(q) == 0;
    puts(ok ? "cmsimplequeue ok" : "cmsimplequeue FALHA");
    return !ok;
}
