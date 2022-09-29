#ident "$Revision: 1.1 $"

extern int rmtclose(int);
extern int rmthost(char **);
extern int rmtioctl(int, void *);
extern int rmtopen(const char *, int, ...);
extern int rmtread(char *, int);
extern void rmtreset(void);
extern int rmtseek(int, int);
extern int rmtwrite(char *, int);
