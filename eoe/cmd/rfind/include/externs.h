#ifndef externs_h
#define externs_h

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <mntent.h>

extern FILE *fdup (const FILE *fp, const char *type);
extern char *pathcomp(char *p);
extern char *dirname (char *);
extern char *findrawpath (char *);
extern char *getaliases (char *ap);
extern char *mountdev(char *);
extern char *my_getcwd (char *, size_t, int);
extern char *my_getwd (char *);
extern char *my_realpath (const char *file_name, char *resolved_name);
extern char *pathcomp (char *);
extern char *quicksum (char *);
extern char *strndup (const char *s1, const size_t sz);
extern char *trim_lofs (char *);
extern char Pathname[];
extern int *rfind (u_int, char **, CLIENT *);
extern int havethetime(void);	/* non-zero if time remains in allowed quantum */
extern int isancestor (char *par, char *chld, ulong_t parlen);    /* is par ancestor of chld  ? */
extern int ismounted (char *);
extern int opendumpfile(void);
extern int ping_nfs_server(struct mntent *mntp);
extern int readb (int, char *, int, int);
extern int time_to_garbage_collect(void);
extern uint64_t hash(uint64_t seed);
extern void dump(void);
extern void dumpqsumflag(void);
extern void dumpsumflag();		/* if (havethetime()) toggle rootino's FV_QFLAG */
extern void error(char *s);    /* user provided error printer */
extern void fenvloop(void);
extern void genprimenum(void);
extern void initqsumflag(void);	/* set qsumflag from FV_QFLAG in rootino */
extern void inumloop();		/* debug tool: show spec'd inode contents */
extern void lockdumpfile(void);
extern void main_find (u_int argc, char *argv[]);
extern void markunused (ino64_t from, ino64_t to);
extern void mhinit(void);
extern void pwd(ino64_t dino);
extern void qsumfenvtreewalk (ino64_t dirino);
extern void rcsfenvtreewalk (ino64_t dirino);
extern void readrcs (char *, char **, char **, char **, char **);
extern void rpcflushall(void);
extern void showaliases(void);
extern void symlinkfenvtreewalk (ino64_t dirino);
extern void warn (char *s, char *t);
extern void ypshowaliases(void);

#endif
