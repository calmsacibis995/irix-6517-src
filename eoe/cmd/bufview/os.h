#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>

#define setbuffer(f, b, s)	setvbuf((f), (b), (b) ? _IOFBF : _IONBF, (s))
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#define memzero(a, b)		memset((a), 0, (b))
typedef void sigret_t;
