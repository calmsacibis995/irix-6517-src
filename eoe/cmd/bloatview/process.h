/*
 * process.h
 *
 * Header file for process.c
 */

#define IRIX "Irix"

typedef struct tagPidList {
    pid_t pid;
    struct tagPidList *next, *prev;
} PIDLIST;

typedef struct tagProgram {
    /*
     * Stuff collected by GetProcInfo
     */
    char *progName;
    char *mapName, *mapType;
    long size, resSize, weightSize, privSize;
    int pid;
    int nProc;
    void *vaddr;

    /*
     * Stuff used in drawing
     */
    int value;
    int secondValue;
    int labelOffset;
    unsigned long color;
    unsigned int skip : 1;
    unsigned int special : 1;
    unsigned int print : 1;
    long top, height, center;

    PIDLIST *pids;

    /*
     * List linkage
     */
    struct tagProgram *next, *prev;
} PROGRAM;

extern void GetProcInfo(char *procName, pid_t pid, PROGRAM **all,
			PROGRAM **proc);

extern void FreeBloat(PROGRAM *bloat);

extern void GetObjInfo(char *objName, PROGRAM **all, PROGRAM **objp);
