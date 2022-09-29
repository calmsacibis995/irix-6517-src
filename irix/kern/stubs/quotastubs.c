
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/quota.h>

int efs_quotactl() {return nopkg();}
/* ARGSUSED */
int qt_closedq(struct mount *a, int b) {return 0;}
/* ARGSUSED */
struct dquot *qt_getinoquota(struct inode *a) {return 0;}
/* ARGSUSED */
int qt_chkdq(struct inode *a, long b, int c, int *d) {return 0;}
/* ARGSUSED */
int qt_chkiq(struct mount *a, struct inode *b, u_int c, int d) {return 0;}

/* ARGSUSED */
void qt_dqrele(struct dquot *a) { }
