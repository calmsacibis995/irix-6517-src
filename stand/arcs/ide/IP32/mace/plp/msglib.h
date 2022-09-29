#if !defined(__MSGLIB_H__)
#define __MSGLIB_H__

void InfoMsg(char *msg);

void ErrorMsg(int testNumber, char *msg);

void StartTest(char *msg, int test, int unit);

int EndTest(char *msg);

#endif
