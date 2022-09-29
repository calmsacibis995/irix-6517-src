/* Declarations for the Curses ("C") mode of netstat */

#define BOS (LINES-1)			/* Index of last line on screen */
extern WINDOW *win;
extern int Cfirst;
extern enum cmode {NORM, ZERO, DELTA} cmode;

extern void quit(int);
extern void fail(char *, ...);

extern int ck_y_off(int, int, int *);
extern void move_print(int *, int *, int, int, char *, ...);

extern void initif(void);
extern void zeroif(void);
extern void sprif(int, int *, ns_off_t);
extern void initmb(void);
extern void zeromb(void);
extern void sprmb(int, int *);
extern void initstream(void);
extern void zerostream(void);
extern void sprstrm(int, int *, ns_off_t);
extern void initip(void);
extern void zeroip(void);
extern void sprip(int, int *);
extern void initicmp(void);
extern void zeroicmp(void);
extern void spricmp(int, int *);
extern void initudp(void);
extern void zeroudp(void);
extern void sprudp(int, int *);
extern void inittcp(void);
extern void zerotcp(void);
extern void sprtcp(int, int *);
extern void initmulti(void);
extern void zeromulti(void);
extern void sprmulti(int, ns_off_t, ns_off_t);
extern void initsock(void);
extern void zerosock(void);
extern void sprsock(int, int *);
