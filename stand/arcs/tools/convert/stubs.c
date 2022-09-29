/* $Revision: 1.4 $ */

/*LINTLIBRARY*/
#include <stdio.h>
#include "stubs.h"

static count = 3;

/*ARGSUSED*/
int
StubObjRead(char *buffer, int length, int fd)
{
	printf("Object Read Invoked\n");
	return count--;
}

/*ARGSUSED*/
int
StubObjClose(int fd)
{
	printf("Object Close Invoked\n");
	return 0;
}

/*ARGSUSED*/
int
StubObjInitialize(int fd)
{
	printf("Object get_info Invoked\n");
	return 0;
}

/*ARGSUSED*/
int
StubFmtInitialize(int x)
{
	printf("Object get_info Invoked\n");
	return 0;
}

int
StubFmtTerminate(void)
{
	printf("Object get_info Invoked\n");
	return 0;
}

/*ARGSUSED*/
int
StubFmtConvert(char *buffer, int length)
{
	printf("Format convert Invoked\n");
	return 0;
}

int
StubFmtWrite(void)
{
	printf("Format convert Invoked\n");
#if 0	/* XXX -- need to use local buffer */
	while ( length-- > 0 )
		printf("%02x", *buffer++);
	printf( "\n");
#endif

	return 0;
}

int
StubFmtClose(void)
{
	printf("Format close Invoked\n");
	return 0;
}

int
StubFmtSetVersion(int x)
{
	return 0;
}
