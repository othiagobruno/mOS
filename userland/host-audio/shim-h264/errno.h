#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern int *__error(void);
#define errno (*__error())
#define EINTR 4
#define EAGAIN 35       /* valores do macOS */
#define ETIMEDOUT 60
#ifdef __cplusplus
}
#endif
