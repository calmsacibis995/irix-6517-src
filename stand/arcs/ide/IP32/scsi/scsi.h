/*
 * Copyright(c) 1993 Silicon Graphics Inc.
 */

#undef stob

extern int scsi(int argc, char *argv[]);
extern int stob(int n); 
extern int gread(daddr_t, char *, int);
extern char *cylsub(daddr_t, char *);
extern daddr_t exarea(daddr_t);
extern char *cylsub(daddr_t, char *);
extern int gioctl(int, char *);
extern void scsi_readcapacity(int *);
extern void init_ex(void);
extern int sequential_func(void);
extern void adderr(int, daddr_t, char *);

/*
 * Codes for IP32 diagnostics.
 */
#define CODE_NO_DISKS		1
#define CODE_NO_CTLR		2
#define CODE_DISK_FAILED	4
#define CODE_CTLR_FAILED	8

#define TEST_BASIC		0x01
#define TEST_EXCISE		0x08
#define TEST_BASIC_INIT		0x10
#define TEST_BASIC_DONE		0x20
#define TEST_EXCISE_INIT	0x40
#define TEST_EXCISE_DONE	0x80
