#include "tkm.h"
#include "assert.h"

/*
 * Error returns. This are out-ot-band (OOB) errors from
 * the message system that tell if there was a messaging error and
 * whether the messaging system never sent the message or never received
 * a reponse
 * In the NOREPLY case, we assume that the message got to the receiving end.
 * In the NOSEND case, we know the message was never sent and we
 * try to back out of the operation keeping as much state as possible.
 */
#define ENOREPLY	5000
#define ENOSEND		5001

/*
 * format of message
 */
#define MESG_SIZE 128
#define MESG_ERROR 0x01
#define MESG_DONE 0x02
#define MESG_RPC  0x04

typedef struct mesg {
	int flags;
	tks_ch_t handle;		/* client handle */
	int op;				/* opcode */
	int service_id;
	char request[MESG_SIZE];
	char response[MESG_SIZE];
} mesg_t;

typedef struct mbox *mbox_t;
extern int writembox(struct mbox *, mesg_t *);
extern int readmbox(struct mbox *, mesg_t **);
extern int callmbox(struct mbox *, mesg_t *);
extern void replymbox(struct mbox *, mesg_t *);
mesg_t *getmesg(void);
void freemesg(mesg_t *);

extern void initmbox(char *);
extern struct mbox *allocmbox(void);
extern void freembox(struct mbox *);
extern void freemboxmsg(void *m);
extern void nodedownmbox(int);

extern struct mbox *ntombox(int);
extern void setmbox(int, struct mbox *);
