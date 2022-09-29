/*
   common.h - sample include file for constants / data structures
              which are common between driver.c and user.c
*/

/*
   data structure mapped into user space so that the driver
   and user mode can share some memory.
*/
typedef struct mappedstructure
{
  int foo;
  int bar;
  int baz;
  unsigned char useless[4096*15-1];
} mappedstructure;


#define TS
