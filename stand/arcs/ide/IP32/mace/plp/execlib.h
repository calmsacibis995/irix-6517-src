#if !defined(__EXECLIB_H__)
#define __EXECLIB_H__

typedef int (*Function)();

typedef struct {
    char      *name;
    Function  callback;
    char      *helpString;
    long      functionalUnit;
    long      testNumber;
} Test;

void Delay(unsigned long timeout);

#endif
