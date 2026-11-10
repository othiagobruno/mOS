#pragma once
#define EINTR 4
#define ENOMEM 12
#define EINVAL 22
#define ETIMEDOUT 110
int *__errno_location(void);
#define errno (*__errno_location())
