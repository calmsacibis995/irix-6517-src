#ifndef _DEBUG_H_
#define _DEBUG_H_

extern int __debug;

void cleanup();
void fatal(char *format, ...);
void DBG(int flag, char *format, ...);

#define D_HASH    0x00000001
#define D_SIGNAL  0x00000002
#define D_THREAD  0x00000004
#define D_ESI     0x00000008
#define D_EVENT   0x00000010
#define D_CO      0x00000020
#define D_DSDEBUG 0x00010000

#endif
