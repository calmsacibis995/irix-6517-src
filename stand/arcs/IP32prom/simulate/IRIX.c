#include <signal.h>
#include <arcs/errno.h>

#include <IP32.h>

/*
 * NOTE: External routines are manually declared here as there are too
 * many conflicts between IRIX libc and stand/libsc
 */

extern size_t
strlen(const char*);

extern int 
write(int, const void*, unsigned);
extern int
read(int, const void*, unsigned);



int
primitiveWrite(char* addr, long* cnt){
  *cnt = write(1,addr,*cnt);
  return ESUCCESS;
}

int
primitiveRead(char* addr, long* cnt){
  *cnt = read(0,addr,*cnt);
  return ESUCCESS;
}

int
primitiveReadStat(){
}

extern void simInit(void);
extern int _prom;
#ifdef xx
extern void post1(void);
extern void post2(void);
extern void post3(void);

extern void finit1(void);
extern void finit2(void);
extern void finit3(void);
extern post1Entry(int, int);
#endif

extern void fwStart(void);
extern void start(void);

main(int argc, char** argv){
  simInit();
  _prom = 1;			/* Convince code that we are in prom */
/*  start();  */
  fwStart();
}













