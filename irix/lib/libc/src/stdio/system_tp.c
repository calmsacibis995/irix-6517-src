#pragma weak __tp_system_pre = ___tp_system_pre

#include "libc_interpose.h"

/*
 * trace point stub for perf package
 */

char *
___tp_system_pre(const char *s)
{
	return (char *)s;
}
