#pragma once
typedef unsigned long sigset_t;
static inline int sigprocmask(int how, const sigset_t *set, sigset_t *oset) {
    (void)how; (void)set; (void)oset;
    return 0;
}
