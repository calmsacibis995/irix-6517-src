#ident	"lib/libsc/cmd/passwd_cmd.c:  $Revision: 1.35 $"

/*
 * passwd_cmd - prom password commands
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include "parser.h"
#include <arcs/errno.h>
#include <arcs/io.h>
#include <gfxgui.h>
#include <libsc.h>
#include <libsk.h>

#if IP20
#include <sys/IP20nvram.h>
#endif
#if IP22 || IP26 || IP28
#include <sys/IP22nvram.h>
#endif
#if IP30
#include <sys/RACER/IP30nvram.h>
#endif
#if IP32
#include <sys/IP32flash.h>
#endif
#if EVEREST
#include <sys/EVEREST/nvram.h>
#endif
#if SN0
#include <sys/SN/nvram.h>
#endif

#if IP22 || IP26 || IP28 || IP30 || IPXX
#define PASSWD_JUMPER
#endif

#ifdef IP32
/*
 * This module currently uses the nvram_offset model.  This model breaks
 * on the IP32.  A more abstract mechanism to access nvram is needed or the
 * IP32 must construct a nvram model.  (We should probably do the latter)
 */
#include <sys/sbd.h>
#define PASSWD     "passwd_key"
#define NETPASSWD  "netpasswd_key"
#define PASSWD_JUMPER
#define PASSWD_LEN       8
#undef	NVLEN_PASSWD_KEY
#undef	NVOFF_PASSWD_KEY
#define NVLEN_PASSWD_KEY (2*PASSWD_LEN+2)
#define NVOFF_PASSWD_KEY (2*PASSWD_LEN+2)
extern int syssetenv(char*, char*);
#endif

int validate_passwd(void);

static char passwd_key[NVLEN_PASSWD_KEY+1];
static void prom_encrypt(char *,char *);
static void getpass(char *, int, struct Dialog *d, struct TextField *t);

/*ARGSUSED*/
int
passwd_cmd(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	char in_key[LINESIZE+1];
	char test_key[PASSWD_LEN*2+1];
	char confirm_key[PASSWD_LEN*2+1];

#ifdef PASSWD_JUMPER
	if (jumper_off()) {
#if defined(IP32)
          _errputs("Warning: Password bypass jumper on. "
#else
		_errputs("Warning: Password jumper has been removed.  "
#endif
		     "Cannot set PROM password.\n");
		return 0;
	}
#endif

	do {
		do {
		    printf( "Enter new password: " );
		    getpass( in_key, PASSWD_LEN, 0, 0 );
		} while ( *in_key == '\000' );
		prom_encrypt( in_key, test_key );

		printf( "Confirm new password: " );
		getpass( in_key, PASSWD_LEN, 0, 0 );
		prom_encrypt( in_key, confirm_key );

	} while( strcmp( test_key, confirm_key ) );

#if defined(IP32)
        if (syssetenv(PASSWD, test_key)) {
          _errputs("Failed to set password.\n");
#else
	if (cpu_set_nvram_offset(NVOFF_PASSWD_KEY,NVLEN_PASSWD_KEY,test_key)) {
		_errputs("Failed to store password in NVRAM.\n");
#endif
		return 0;
	}
	strcpy(passwd_key, test_key);

	return 0;
}

/*ARGSUSED*/
/*
 * resetpw - reset password
 */
/*ARGSUSED*/
int
resetpw_cmd(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
#if defined(IP32)
  if (syssetenv(PASSWD, "")) {
    _errputs("Unable to clear password.\n");
#else
	if (cpu_set_nvram_offset(NVOFF_PASSWD_KEY, NVLEN_PASSWD_KEY, "")) {
		_errputs("Unable to clear password.\n");
#endif
		return 0;
	}

	if ( passwd_key[0] )
		printf("Password cleared.\n");
	passwd_key[0] = '\0';

	return 0;
}

/*
 * illegal_passwd - returns 1 if password is unreasonable
 * used for backward compatibility with old nvrams
 */
int
illegal_passwd(char *passwd)
{
    int i;

    /* password should have 2*PASSWD_LEN lower case characters
     * and a NULL termination
     */
    for (i = 0; i < PASSWD_LEN*2; ++i) {
	if (passwd[i] < 'a' || passwd[i] > 'z')
	    return 1;
    }
    if (passwd[i])
	return 1;
    return 0;
}

/*
 * validate password -  If a password is set,
 * then it must be entered before entering manual mode.
 */
int
validate_passwd(void)
{
#if defined(IP32)
  char *passwd_ptr;
#endif
	char *msg = "Enter password:";
	char in_key[LINESIZE+1];
	char test_key[PASSWD_LEN*2+1];
	static int checkedforpw;
	struct Dialog *d = 0;
	struct TextField *t = 0;

	if ( !checkedforpw ) {
		checkedforpw = 1;
#if defined(IP32)
          if (passwd_ptr = getenv(PASSWD)) strcpy(passwd_key, passwd_ptr);
#else
		strcpy(passwd_key,
		    cpu_get_nvram_offset(NVOFF_PASSWD_KEY,NVLEN_PASSWD_KEY));
#endif
		/*
		 * the password is encrypted onto a lowercase alphabet string
		 * lets do some sanity checking.  If not reasonable, ignore
		 */
		if (passwd_key[0] < 'a' || passwd_key[0] > 'z')
			passwd_key[0] = '\0';
	}

	if (!passwd_key[0])
		return 1;

#ifdef PASSWD_JUMPER
	/*  We have a password.  Check the jumper to see if password is
	 * enforced.
	 */
	if (jumper_off()) {
		static int continue_buttons[]={DIALOGCONTINUE,DIALOGPREVDEF,-1};
#if defined(IP32)
                char *jmpr = "Warning: Password bypass jumper on.  Not "
#else
		char *jmpr = "Warning: Password jumper has been removed.  Not "
#endif
			     "enforcing PROM password.";
		if (isGuiMode()) {
			bell();
			popupDialog(jmpr,continue_buttons,DIALOGWARNING,
					DIALOGCENTER);
		}
		else {
			puts("\007\n");
			puts(jmpr);
			puts("\n\n");
		}
		return 1;
	}
#endif

	if (isGuiMode()) {
		struct Button *b;
		d = createDialog(msg,DIALOGQUESTION,DIALOGBIGFONT);
		b = addDialogButton(d,DIALOGACCEPT);
		setDefaultButton(b,1);
		addDialogButton(d,DIALOGCANCEL);
		t = addDialogTextField(d,160,TFIELDDOTCURSOR|
				       TFIELDNOUSERIN|TFIELDBIGFONT);
		drawObject(guiobj(d));
		drawObject(guiobj(t));
		setRedrawUnder(guiobj(d));
	}
	else
		printf(msg);

	getpass( in_key, PASSWD_LEN, d, t );
	
	prom_encrypt( in_key, test_key );

	if (isGuiMode()) {
		deleteObject(guiobj(t));
		deleteObject(guiobj(d));
	}

	if (!strcmp(test_key, passwd_key))
		return 1;
	
	return 0;
}

/*
 * prom_encrypt -- encrypt a key onto a printable string.
 *		   to validate permission, reencrypt and compare
 *	           result with stored printable string.
 *
 * the base string should be PASSWD_LEN*2 characters in length
 * xor mask need only be PASSWD_LEN characters in length
 */
static void
prom_encrypt( char *cin, char *cout )
{
    static char base_string[PASSWD_LEN*2+1] = "cafebabedeadbeef";
    static char xor_mask[PASSWD_LEN+1]      = "#!$%^&*@";

    char src_key[PASSWD_LEN+1];
    char xored_key[PASSWD_LEN+1];
    int i;

    /*
    ** null out encryption key. copy null terminated input key to src_key
    */
    for (i=0; i < PASSWD_LEN+1; i++)
	src_key[i] = '\0';

    for (i=0; i < PASSWD_LEN; i++) {
	if (cin[i] == '\0' )
		break;
	src_key[i] = cin[i];
    }

    /*
    ** xor the source key with a random bit mask, then add each nibble
    ** to a printable character.  The resulting printable string may
    ** be left in the clear.
    */
    for (i=0; i < PASSWD_LEN; i++)
	xored_key[i] = src_key[i] ^ xor_mask[i];
	
    for (i=0; i < PASSWD_LEN*2+1; i++)
	cout[i] = '\0';

    for (i=0; i < PASSWD_LEN; i++) {
	cout[i<<1] = base_string[i<<1] + ((xored_key[i] & 0xf0)>>4);
	cout[(i<<1)+1] = base_string[(i<<1)+1] + (xored_key[i] & 0x0f);
    }
}

static void
getpass(char *bp, int maxlen, struct Dialog *d, struct TextField *t)
{
	char inbuf[80];
	char dummybuf[80];
	char *cp;

	if (t)
		setTextFieldBuffer(t,dummybuf,80);
	
	for (cp = inbuf;;) {
		if (GetReadStatus(StandardIn) == ESUCCESS) {
			int c = getchar();

			if ( c == '\b' ) {
				if ( cp != inbuf ) {
					cp -= 1;
					if (t) putcTextField(t,c);
				}
			} else {
				if ( c == '\n' || c == '\r' || c == -1 )
					break;
				else
					if ( cp != inbuf+78 ) {
						*cp++ = c;
						if (t) putcTextField(t,' ');
					}
			}
		}
		else if (d && (d->flags&DIALOGCLICKED)) {
			if (d->which == 1) {
				/* clicked cancel! -- rewind buffer to return
				 * null password.
				 */
				cp = inbuf;
			}
			/* else clicked accept -- break to enter passwd */
			break;
		}
	}
	*cp = '\0';

	strncpy(bp,inbuf,maxlen);

	if (!d)
		putchar('\n');

	bp[maxlen] = '\0';
}

#if IP22 || IP26 || IP28
#include <sys/sbd.h>
int
jumper_off(void)
{
	return(*K1_LIO_2_ISR_ADDR&LIO_PASSWD);
}
#elif IP32
/**************
 * jumper_off()
 *-------------
 */
int
jumper_off(void)
{
	uint64_t Jumper_value = *(uint64_t*)(PHYS_TO_K1(ISA_FLASH_NIC_REG)) & ISA_PWD_CLEAR;
	return (Jumper_value == 0) ? 1 : 0;
}

#elif IPXX
int
jumper_off(void)
{
	return((int)getenv("passwdjumper"));
}
#endif
