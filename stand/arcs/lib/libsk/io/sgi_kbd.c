#if IP20 || EVEREST
#ident	"lib/libsk/io/sgi_kbd.c: $Revision: 1.32 $"

/*
 * sgi_kbd.c - keyboard functions for standard sgi keyboard
 *
 *	config_keyboard() and kb_translate() are called directly
 *	by the duart driver
 */

#include <libsc.h>
#include <sys/types.h>
#include <sys/gfx_kb.h>
#include <sys/kbd.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>

#define BREAKKEY	1
#define PAUSEKEY	160
#define LEFTALTKEY	143
#define MAXKBDBUT	83

typedef struct {
	unsigned short code;			/* gl device code */
	unsigned short type;			/* type of key */
	unsigned char *value[KB_BINDING_MAX+1];	/* value of binding */
} KeyMap;

extern void 		consputc(u_char, int);
extern int _prom;
#define DELAY	us_delay

static int kbdtype;
static unsigned int kbdstate, kbdcntrlstate;

#ifdef _STANDALONE
int sgikbd_install(void)	{return(1);}	/* needed for configuration */
#endif


/*
 * Try to read a character from the keyboard port, timing out if none
 * arrives soon enough.  If we time out, return -1, otherwise return
 * the character read.
 */
static int
kbd_getstat(void)
{
    int c, nattempts = 100;

    while (--nattempts > 0 && (c = consgetc(du_keyboard_port())) == 0)
	DELAY(1000);
    if (nattempts == 0)
	return -1;

    /* strip off high bit set by consgetc() */
    return c & 0xFF;
}

static void
lampfunc(int action,int val)
{
    if ( action )
	kbdcntrlstate |= val;
    else
	kbdcntrlstate &= ~val;
    consputc(kbdcntrlstate, du_keyboard_port());
}

void
lampon(int val)
{
    lampfunc(1, val);
}

void
lampoff(int val)
{
    lampfunc(0, val);
}

void
bell()
{
    consputc(kbdcntrlstate | NEWKB_SHORTBEEP, du_keyboard_port());
}

static COMPONENT sgikbd = {
	PeripheralClass,		/* Class */
	KeyboardPeripheral,		/* Type */
	(IDENTIFIERFLAG)(ReadOnly|ConsoleIn|Input), /* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* Identifier Length */
	0				/* Identifier */
};

/*
 * output a control character to the keyboard
 */
#define KBD_PUTCTRL(cx) consputc(cx, du_keyboard_port()); DELAY(5000);

int
config_keyboard(COMPONENT *serial)
{
    register int i, j, c, foundkb;
    unsigned int tmpstate;
    static int beeped;
    char *nogfxkbd;

    /*
     * Try to find the keyboard 3 times, allowing time for the
     * keyboard to reset itself.  We give it three chances to
     * avoid failing due to a glitch.
     */
    foundkb = 0;
    kbdstate = 0;
    for (j = 0; (j < 3) && !foundkb; j++) {
	consputc(CONFIG_REQUEST, du_keyboard_port());
	/*
	   We give keyboard 5 chances to response the request,
	   if don't, then we request again. (Keyboard should
	   reponse 2 bytes. 1st byte is 0x6e 2nd byte will tell
	   what kind of keyboard it is.
	*/
	for (i = 0; (i < 5) && !foundkb; i++) {
		if ((c = kbd_getstat()) < 0)
		    continue;
		switch (c) {
		case CONFIG_BYTE_NEWKB:
		    foundkb = 1;
		    if (((c = kbd_getstat()) < 0) || (c > 0x0f))
			    continue;
		    kbdtype = c;
		    kbd_updateKeyMap(kbdtype);

		    tmpstate = kbdcntrlstate = NEWKB_AUTOREPEAT;
		    if (_prom) {
			/* walk leds through a pattern, and beep */
			DELAY(NEWKB_SETDS4);
			KBD_PUTCTRL(NEWKB_SETDS5);
			KBD_PUTCTRL(NEWKB_SETDS6);
			KBD_PUTCTRL(NEWKB_SETDS7);
			KBD_PUTCTRL(NEWKB_OFFDS);

			if (!beeped) tmpstate |= NEWKB_SHORTBEEP;
		    }
		    /* enable auto-repeat and key-click */
		    consputc(tmpstate, du_keyboard_port());
		    break;
		case CONFIG_BYTE_OLDKB:
		    /* drag this one along */
		    foundkb = 1;
		    kbdtype = US_KBD;

		    tmpstate = kbdcntrlstate = 0;
		    if (_prom) {
			KBD_PUTCTRL(KBD_LEDCMD | KBD_LED0 | KBD_LED3);
			KBD_PUTCTRL(KBD_LEDCMD | KBD_LED0 | KBD_LED4);
			KBD_PUTCTRL(KBD_LEDCMD | KBD_LED0 | KBD_LED5);
			KBD_PUTCTRL(KBD_LEDCMD | KBD_LED0 | KBD_LED6);
			consputc(KBD_LEDCMD | KBD_LED0, du_keyboard_port());
			if (!beeped) tmpstate |= KBD_BEEPCMD|KBD_SHORTBEEP;
		    }
		    consputc(tmpstate, du_keyboard_port());
		    break;
		default:
		    DELAY(10000);
		    continue;
		}
	}
    }


    nogfxkbd = getenv("nogfxkbd");
    if (foundkb || (nogfxkbd && *nogfxkbd == '1')) {
	/* add keyboard to inventory! */
	extern int _z8530_strat();
	COMPONENT *kbd;

	/*  If no keyboard, but gfxnokbd update keymap to contain standard
	 * map incase keyboard is plugged in later.
	 */
	if (!foundkb)
		kbd_updateKeyMap(0);

	beeped=1;

	kbd = AddChild(serial,&sgikbd,(void *)NULL);
	if (kbd == (COMPONENT *)NULL) cpu_acpanic("sgi keyboard");
	RegisterDriverStrategy(kbd,_z8530_strat);
	return(ESUCCESS);
    }
    else
	return(ENXIO);
}


#define MAX_KEYS        128
KeyMap  kbd_keyMap_updated_array[MAX_KEYS];
KeyMap  *kbd_keyMap_updated;

/*
 * kb_translate:
 *	- translate a key code into a given string of characters, returning
 *	  the number of characters decoded into buf
 */
int
kb_translate (int key, char *buf)
{
    register int 				i, j;
    register KeyMap 				*kbd;
    register int 				key_down;

#ifdef KEYDEBUG
    char b[20];
    sprintf(b,"key %d\n",key);
    tnDBG(b);				/* print on serial port */
#endif

    key_down = ((key & 0x80) != 0x80);
    key &= ~0x80;
    kbd = kbd_keyMap_updated + key;

    /*
     * Handle shift, control, alt, capslock, and numlock keys
     */
    if (kbd->type == KB_KEYTYPE_SHIFT) {
	if (key_down)
	    kbdstate |= STATE_SHIFT;
	else
	    kbdstate &= ~STATE_SHIFT;
	return (0);
    }
    else if (kbd->type == KB_KEYTYPE_CONTROL) {
	if (key_down)
	    kbdstate |= STATE_CONTROL;
	else
	    kbdstate &= ~STATE_CONTROL;
	return (0);
    }
    else if ((kbd->type == KB_KEYTYPE_ALT) ||
	     (kbd->type == KB_KEYTYPE_COMPOSE))	{	/* left alt key */
	if (key_down)
	    kbdstate |= STATE_ALT;
	else
	    kbdstate &= ~STATE_ALT;
	return (0);
    }		
    else if (kbd->type == KB_KEYTYPE_CAPSLOCK) {
	if (!key_down) {
	    kbdstate ^= STATE_CAPSLOCK;
	    if (kbdstate & STATE_CAPSLOCK)
		lampon (LAMP_CAPSLOCK);
	    else
		lampoff (LAMP_CAPSLOCK);
	}
	return (0);
    }
    else if (kbd->type == KB_KEYTYPE_NUMLOCK) {
	if (!key_down) {
	    kbdstate ^= STATE_NUMLOCK;
	    if (kbdstate & STATE_NUMLOCK)
		lampon (LAMP_NUMLOCK);
	    else
		lampoff (LAMP_NUMLOCK);
	}
	return (0);
    }

    /* throw away key releases */
    if (!key_down)
	return (0);

    if (kbd->code == BREAKKEY) {
	*buf = 0;
	return (-1);
    }

    if (kbdstate & STATE_ALT)
	i = KB_KEYTYPE_ALT + 1;
    else if (kbdstate & STATE_CONTROL) {
	if ((key == PAUSEKEY) || (kbd->type == KB_KEYTYPE_CAPSCTRL)) {
	    *buf = 0;
	    return (-1);
	}
	else
	    i = KB_KEYTYPE_CONTROL + 1;
    }
    else if ((kbdstate & STATE_SHIFT) || 
	     ((kbdstate & STATE_NUMLOCK) && (kbd->type == KB_KEYTYPE_PAD)) ||
	     ((kbdstate & STATE_CAPSLOCK) && (kbd->type == KB_KEYTYPE_LETTER)))
	i = KB_KEYTYPE_SHIFT + 1;
    else if ((kbdstate & STATE_CAPSLOCK) && (kbd->type == KB_KEYTYPE_CAPSCTRL))
	i = KB_KEYTYPE_CONTROL + 1;
    else
	i = 0;

    j = 0;
    if (kbd->value[i])
	for ( ; buf[j] = kbd->value[i][j] ; j++) ;


    return(j);
}

extern	KeyMap kbd_keyMap_danish[];
extern	short kbd_numKeys_danish;

extern	KeyMap kbd_keyMap_german[];
extern	short kbd_numKeys_german;

extern	KeyMap kbd_keyMap_french[];
extern	short kbd_numKeys_french;

extern	KeyMap kbd_keyMap_belgian[];
extern	short kbd_numKeys_belgian;

extern	KeyMap kbd_keyMap_italian[];
extern	short kbd_numKeys_italian;

extern	KeyMap kbd_keyMap_norwegian[];
extern	short kbd_numKeys_norwegian;

extern	KeyMap kbd_keyMap_swiss_french[];
extern	short kbd_numKeys_swiss_french;

extern	KeyMap kbd_keyMap_swiss_german[];
extern	short kbd_numKeys_swiss_german;

extern	KeyMap kbd_keyMap_spanish[];
extern	short kbd_numKeys_spanish;

extern	KeyMap kbd_keyMap_swedish[];
extern	short kbd_numKeys_swedish;

extern	KeyMap kbd_keyMap_united_kingdom[];
extern	short kbd_numKeys_united_kingdom;

extern	KeyMap kbd_keyMap_standard[];
extern	short kbd_numKeys_standard;

extern	KeyMap kbd_keyMap_portuguese[];
extern	short kbd_numKeys_portuguese;

struct kbd_type {
	char *name;
	KeyMap *map;
	short *count;
} kbd_types[] = {
	{ "USA", kbd_keyMap_standard, &kbd_numKeys_standard, },
	{ "DEU", kbd_keyMap_german, &kbd_numKeys_german, },
	{ "FRA", kbd_keyMap_french, &kbd_numKeys_french, },
	{ "ITA", kbd_keyMap_italian, &kbd_numKeys_italian, },
	{ "DNK", kbd_keyMap_danish, &kbd_numKeys_danish, },
	{ "ESP", kbd_keyMap_spanish, &kbd_numKeys_spanish, },
	{ "CHE-D", kbd_keyMap_swiss_german, &kbd_numKeys_swiss_german, },
	{ "SWE", kbd_keyMap_swedish, &kbd_numKeys_swedish, },
	{ "GBR", kbd_keyMap_united_kingdom, &kbd_numKeys_united_kingdom, },
	{ "BEL", kbd_keyMap_belgian, &kbd_numKeys_belgian, },
	{ "NOR", kbd_keyMap_norwegian, &kbd_numKeys_norwegian, },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ "PRT", kbd_keyMap_portuguese, &kbd_numKeys_portuguese, },
	{ "CHE-F", kbd_keyMap_swiss_french, &kbd_numKeys_swiss_french, }
};
#define	KBD_TYPES	(sizeof(kbd_types) / sizeof(struct kbd_type))

/*
 * Update the keyMap now that the type of the keyboard is known.
 */
void
kbd_updateKeyMap(int kbdtype)
{
    char *keybd = getenv("keybd");
    register KeyMap *km;
    register int updateCount;
    register int kc;
    register int i, j;

    if (_prom) {
	/*
	   copy key map into RAM such that we can modify it
	*/
	for (i = 0; i < kbd_numKeys_standard; i++) {
	    kbd_keyMap_updated_array[i].code = kbd_keyMap_standard[i].code;
	    kbd_keyMap_updated_array[i].type = kbd_keyMap_standard[i].type;

	    for (j = 0; j < (KB_BINDING_MAX + 1); j++)
		kbd_keyMap_updated_array[i].value[j] = kbd_keyMap_standard[i].value[j];
	}   /* for */
	kbd_keyMap_updated = kbd_keyMap_updated_array;
    }
    else {
	kbd_keyMap_updated = kbd_keyMap_standard;
    }

    if (kbdtype >= KBD_TYPES) {
	printf ("No translation table for this keyboard. Default to US keyboard.\n");
	return;
    }   /* if */

    /* Check keybd:
     *	  - unset -> use autoselect unless swiss -> use swiss french.
     *	  - match of keyboard table exactly -> use that map.
     *	  - "D" || "d" and swiss -> use swiss german.
     *	  - swiss -> use swiss french.
     */
    if (keybd) {
	    for (i = 0; i < KBD_TYPES; i++) {
		    if (kbd_types[i].name &&
			!strcasecmp(kbd_types[i].name,keybd)) {
			    kbdtype = i;
			    break;
		    }
	    }
    }
    if (kbdtype == 6) {			/* swiss special case */
	    if (!keybd || (*keybd != 'd' && *keybd != 'D'))
		    kbdtype = 15;	/* swiss french */
    }

    if (kbdtype == KB_TYPE_AMERICAN) {
	return;
    }

    km = kbd_types[kbdtype].map;
    if (!km) return;
    updateCount = *kbd_types[kbdtype].count;
    while (--updateCount >= 0) {
	/* convert gl code to key code */
	/* convert gl id to real keyboard encoding */
	if (km->code >= LEFTALTKEY)
	    kc = km->code - LEFTALTKEY + MAXKBDBUT;
	else
	    kc = km->code - 1;
	/* zot */
	bcopy(km, &kbd_keyMap_updated[kc], sizeof(*km));
	km++;
    }
}

/*
 * Keymap table
 */

#define UC	unsigned char *

#ifdef FULLMAP
#define n(X)	(UC)(X)			/* normal */
#define s(X)	(UC)(X)			/* shift */
#define c(X)	(UC)(X)			/* cntrl */
#define a(X)	(UC)(X)			/* alt */
#else
#define n(X)	0
#define s(X)	0
#define c(X)	0
#define a(X)	0
#endif

/* standard */
KeyMap kbd_keyMap_standard[] = {
{ 1, 8, 0,0,0,0 },
{ 2, 8, 0,0,0,0 },
{ 3, 1, 0,0,0,0 },
{ 4, 4, 0,0,0,0 },
{ 5, 0, 0,0,0,0 },
{ 6, 0, 0,0,0,0 },
{ 7, 8, (UC)"\033",	s("\033[120q"),	c("\033[121q"),	a("\033[122q") },
{ 8, 8, (UC)"1",	(UC)"!",	c("\033[049q"),	a("\033[058q") },
{ 9, 8, (UC)"\011",	(UC)"\033[Z",	c("\033[072q"),	a("\033[073q") },
{ 10, 6, (UC)"q",	(UC)"Q",	(UC)"\021",	a("\033[074q") },
{ 11, 6, (UC)"a",	(UC)"A",	(UC)"\001",	a("\033[087q") },
{ 12, 6, (UC)"s",	(UC)"S",	(UC)"\023",	a("\033[088q") },
{ 13, 8, 0,0,0,0 },
{ 14, 8, (UC)"2",	(UC)"@",	c("\000"),	a("\033[059q") },
{ 15, 8, (UC)"3",	(UC)"#",	c("\033[050q"), a("\033[060q") },
{ 16, 6, (UC)"w",	(UC)"W",	(UC)"\027",	a("\033[075q") },
{ 17, 6, (UC)"e",	(UC)"E",	(UC)"\005",	a("\033[076q") },
{ 18, 6, (UC)"d",	(UC)"D",	(UC)"\004",	a("\033[089q") },
{ 19, 6, (UC)"f",	(UC)"F",	(UC)"\006",	a("\033[090q") },
{ 20, 6, (UC)"z",	(UC)"Z",	(UC)"\032",	a("\033[101q") },
{ 21, 6, (UC)"x",	(UC)"X",	(UC)"\030",	a("\033[102q") },
{ 22, 8, (UC)"4",	(UC)"$",	c("\033[051q"),	a("\033[061q") },
{ 23, 8, (UC)"5",	(UC)"%",	c("\033[052q"),	a("\033[062q") },
{ 24, 6, (UC)"r",	(UC)"R",	(UC)"\022",	a("\033[077q") },
{ 25, 6, (UC)"t",	(UC)"T",	(UC)"\024",	a("\033[078q") },
{ 26, 6, (UC)"g",	(UC)"G",	(UC)"\007",	a("\033[091q") },
{ 27, 6, (UC)"h",	(UC)"H",	(UC)"\010",	a("\033[092q") },
{ 28, 6, (UC)"c",	(UC)"C",	(UC)"\003",	a("\033[103q") },
{ 29, 6, (UC)"v",	(UC)"V",	(UC)"\026",	a("\033[104q") },
{ 30, 8, (UC)"6",	(UC)"^",	(UC)"\036",	a("\033[063q") },
{ 31, 8, (UC)"7",	(UC)"&",	c("\033[053q"),	a("\033[064q") },
{ 32, 6, (UC)"y",	(UC)"Y",	(UC)"\031",	a("\033[079q") },
{ 33, 6, (UC)"u",	(UC)"U",	(UC)"\025",	a("\033[080q") },
{ 34, 6, (UC)"j",	(UC)"J",	(UC)"\012",	a("\033[093q") },
{ 35, 6, (UC)"k",	(UC)"K",	(UC)"\013",	a("\033[094q") },
{ 36, 6, (UC)"b",	(UC)"B",	(UC)"\002",	a("\033[105q") },
{ 37, 6, (UC)"n",	(UC)"N",	(UC)"\016",	a("\033[106q") },
{ 38, 8, (UC)"8",	(UC)"*",	c("\033[054q"),	a("\033[065q") },
{ 39, 8, (UC)"9",	(UC)"(",	c("\033[055q"),	a("\033[066q") },
{ 40, 6, (UC)"i",	(UC)"I",	(UC)"\011",	a("\033[081q") },
{ 41, 6, (UC)"o",	(UC)"O",	(UC)"\017",	a("\033[082q") },
{ 42, 6, (UC)"l",	(UC)"L",	(UC)"\014",	a("\033[095q") },
{ 43, 8, (UC)";",	(UC)":",	c("\033[096q"),	a("\033[097q") },
{ 44, 6, (UC)"m",	(UC)"M",	(UC)"\015",	a("\033[107q") },
{ 45, 8, (UC)",",	(UC)"<",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)")",	c("\033[056q"),	a("\033[067q") },
{ 47, 8, (UC)"-",	(UC)"_",	(UC)"\037",	a("\033[068q") },
{ 48, 6, (UC)"p",	(UC)"P",	(UC)"\020",	a("\033[083q") },
{ 49, 8, (UC)"[",	(UC)"{",	(UC)"\033",	a("\033[084q") },
{ 50, 8, (UC)"'",	(UC)"\"",	c("\033[098q"),	a("\033[099q") },
{ 51, 8, (UC)"\015",	(UC)"\015",	(UC)"\015",	a("\033[100q") },
{ 52, 8, (UC)".",	(UC)">",	c("\033[110q"),	a("\033[111q") },
{ 53, 8, (UC)"/",	(UC)"?",	c("\033[112q"),	a("\033[113q") },
{ 54, 8, (UC)"=",	(UC)"+",	c("\033[069q"),	a("\033[070q") },
{ 55, 8, (UC)"`",	(UC)"~",	c("\033[057q"),	a("\033[115q") },
{ 56, 8, (UC)"]",	(UC)"}",	(UC)"\035",	a("\033[085q") },
{ 57, 8, (UC)"\\",	(UC)"|",	(UC)"\034",	a("\033[086q") },
{ 58, 7, n("\033[146q"),(UC)"1",	c("\033[176q"),	(UC)"\001" },
{ 59, 7, n("\033[@"),	(UC)"0",	c("\033[178q"),	(UC)"\000" },
{ 60, 8, (UC)"\012",	(UC)"\012",	(UC)"\012",	(UC)"\012" },
{ 61, 8, (UC)"\010",	(UC)"\010",	(UC)"\177",	a("\033[071q") },
{ 62, 8, (UC)"\177",	s("\033[P"),	c("\033[142q"),	a("\033[M") },
{ 63, 7, (UC)"\033[D",	(UC)"4",	c("\033[174q"),	(UC)"\004" },
{ 64, 7, (UC)"\033[B",	(UC)"2",	c("\033[186q"),	(UC)"\002" },
{ 65, 7, n("\033[154q"),(UC)"3",	c("\033[194q"),	(UC)"\003" },
{ 66, 7, n("\033[P"),	(UC)".",	c("\033[196q"),	a("\033[197q") },
{ 67, 7, n("\033[H"),	(UC)"7",	c("\033[172q"),	(UC)"\007" },
{ 68, 7, (UC)"\033[A",	(UC)"8",	c("\033[182q"),	(UC)"\010" },
{ 69, 7, n("\033[000q"),(UC)"5",	c("\033[184q"),	(UC)"\005" },
{ 70, 7, (UC)"\033[C",	(UC)"6",	c("\033[192q"),	(UC)"\006" },
{ 71, 7, n("\033[002q"),s("\033[014q"),	c("\033[026q"),	a("\033[038q") },
{ 72, 7, n("\033[001q"),s("\033[013q"),	c("\033[025q"),	a("\033[037q") },
{ 73, 8, (UC)"\033[D",	s("\033[158q"),	c("\033[159q"),	a("\033[160q") },
{ 74, 8, (UC)"\033[B",	s("\033[164q"),	c("\033[165q"),	a("\033[166q") },
{ 75, 7, n("\033[150q"),(UC)"9",	c("\033[190q"),	a("\011") },
{ 76, 7, (UC)"-",	(UC)"-",	c("\033[198q"),	a("\033[199q") },
{ 77, 7, (UC)",",	(UC)",",	(UC)",",	(UC)"," },
{ 78, 7, n("\033[004q"),s("\033[016q"),	c("\033[028q"),	a("\033[040q") },
{ 79, 7, n("\033[003q"),s("\033[015q"),	c("\033[027q"),	a("\033[039q") },
{ 80, 8, (UC)"\033[C",	s("\033[167q"),	c("\033[168q"),	a("\033[169q") },
{ 81, 8, (UC)"\033[A",	s("\033[161q"),	c("\033[162q"),	a("\033[163q") },
{ 82, 7, (UC)"\015",	(UC)"\015",	(UC)"\015",	a("\033[100q") },
{ 83, 8, (UC)" ",	(UC)" ",	(UC)" ",	(UC)" " },
{143, 9, 0,0,0,0 },
{144, 2, 0,0,0,0 },
{145, 1, 0,0,0,0 },
{146, 8, (UC)"\033[001q",(UC)"\033[013q",(UC)"\033[025q",(UC)"\033[037q" },
{147, 8, (UC)"\033[002q",(UC)"\033[014q",(UC)"\033[026q",(UC)"\033[038q" },
{148, 8, (UC)"\033[003q",(UC)"\033[015q",(UC)"\033[027q",(UC)"\033[039q" },
{149, 8, (UC)"\033[004q",(UC)"\033[016q",(UC)"\033[028q",(UC)"\033[040q" },
{150, 8, (UC)"\033[005q",(UC)"\033[017q",(UC)"\033[029q",(UC)"\033[041q" },
{151, 8, (UC)"\033[006q",(UC)"\033[018q",(UC)"\033[030q",(UC)"\033[042q" },
{152, 8, (UC)"\033[007q",(UC)"\033[019q",(UC)"\033[031q",(UC)"\033[043q" },
{153, 8, (UC)"\033[008q",(UC)"\033[020q",(UC)"\033[032q",(UC)"\033[044q" },
{154, 8, (UC)"\033[009q",(UC)"\033[021q",(UC)"\033[033q",(UC)"\033[045q" },
{155, 8, (UC)"\033[010q",(UC)"\033[022q",(UC)"\033[034q",(UC)"\033[046q" },
{156, 8, (UC)"\033[011q",(UC)"\033[023q",(UC)"\033[035q",(UC)"\033[047q" },
{157, 8, (UC)"\033[012q",(UC)"\033[024q",(UC)"\033[036q",(UC)"\033[048q" },
{158, 8, (UC)"\033[209q",s("\033[210q"),c("\033[211q"),	a("\033[212q") },
{159, 8, n("\033[213q"),s("\033[214q"),	c("\033[215q"),	a("\033[216q") },
{160, 8, (UC)"\033[217q",s("\033[218q"),0,		(UC)"\177" },
{161, 8, (UC)"\033[@",	s("\033[139q"),	c("\033[140q"),	a("\033[141q") },
{162, 8, (UC)"\033[H",	s("\033[143q"),	c("\033[144q"),	a("\033[145q") },
{163, 8, (UC)"\033[150q",s("\033[151q"),c("\033[152q"),a("\033[153q") },
{164, 8, (UC)"\033[146q",s("\033[147q"),c("\033[148q"),a("\033[149q") },
{165, 8, (UC)"\033[154q",s("\033[155q"),c("\033[156q"),a("\033[157q") },
{166, 5, 0,	0,	(UC)"\023",	a("\033[170q") },
{167, 7, (UC)"/",	(UC)"/",	c("\033[179q"),	a("\033[180q") },
{168, 7, (UC)"*",	(UC)"*",	c("\033[187q"),	a("\033[188q") },
{169, 7, (UC)"+",	(UC)"+",	c("\033[200q"),	a("\033[201q") },
{170, 8, (UC)"<",	(UC)">",	0,		0 }
};
short kbd_numKeys_standard = sizeof(kbd_keyMap_standard) / sizeof(KeyMap);

/* german */
KeyMap kbd_keyMap_german[] = {
{ 10, 6, (UC)"q",	(UC)"Q",	(UC)"\021",	(UC)"@" },
{ 14, 8, (UC)"2",	(UC)"\"",	(UC)"\000",	(UC)"\262" },
{ 15, 8, (UC)"3",	(UC)"\247",	c("\033[050q"),	(UC)"\263" },
{ 20, 6, (UC)"y",	(UC)"Y",	(UC)"\031",	a("\033[079q") },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	a("\033[063q") },
{ 31, 8, (UC)"7",	(UC)"/",	c("\033[053q"),	(UC)"{" },
{ 32, 6, (UC)"z",	(UC)"Z",	(UC)"\032",	a("\033[101q") },
{ 38, 8, (UC)"8",	(UC)"(",	c("\033[054q"),	(UC)"[" },
{ 39, 8, (UC)"9",	(UC)")",	c("\033[055q"),	(UC)"]" },
{ 43, 6, (UC)"\366",	(UC)"\326",	c("\033[096q"),	a("\033[097q") },
{ 44, 6, (UC)"m",	(UC)"M",	(UC)"\015",	(UC)"\265" },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"), a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	c("\033[056q"),	(UC)"}" },
{ 47, 8, (UC)"\337",	(UC)"?",	0,		(UC)"\\" },
{ 49, 6, (UC)"\374",	(UC)"\334",	(UC)"\033",	a("\033[084q") },
{ 50, 6, (UC)"\344",	(UC)"\304",	c("\033[098q"),	a("\033[099q") },
{ 52, 8, (UC)".",	(UC)":",	c("\033[110q"),	a("\033[111q") },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	a("\033[113q") },
{ 54, 8, (UC)"\264",	(UC)"`",	c("\033[069q"),	0 },
{ 55, 8, (UC)"^",	(UC)"\260",	c("\033[057q"), a("\033[115q") },
{ 56, 8, (UC)"+",	(UC)"*",	0,		(UC)"~" },
{ 57, 8, (UC)"#",	(UC)"'",	0,		0 },
{ 66, 7, n("\033[P"),	(UC)",",	c("\033[196q"), a("\033[197q") },
{170, 8, (UC)"<",	(UC)">",	0,		(UC)"|" }
};
short kbd_numKeys_german = sizeof(kbd_keyMap_german) / sizeof(KeyMap);


/* french */
KeyMap kbd_keyMap_french[] = {
{  8, 8, (UC)"&",	(UC)"1",	0,		0},
{ 10, 6, (UC)"a",	(UC)"A",	(UC)"\001",	a("\033[087q") },
{ 11, 6, (UC)"q",	(UC)"Q",	(UC)"\021",	a("\033[074q") },
{ 14,10, (UC)"\351",	(UC)"2",	(UC)"\311",	(UC)"~" },
{ 15, 8, (UC)"\"",	(UC)"3",	0,		(UC)"#" },
{ 16, 6, (UC)"z",	(UC)"Z",	(UC)"\032",	a("\033[101q") },
{ 20, 6, (UC)"w",	(UC)"W",	(UC)"\027",	a("\033[075q") },
{ 22, 8, (UC)"'",	(UC)"4",	0,		(UC)"{" },
{ 23, 8, (UC)"(",	(UC)"5",	0,		(UC)"[" },
{ 30, 8, (UC)"-",	(UC)"6",	0,		(UC)"|" },
{ 31,10, (UC)"\350",	(UC)"7",	(UC)"\310",	(UC)"`" },
{ 38, 8, (UC)"_",	(UC)"8",	(UC)"\037",	(UC)"\\" },
{ 39,10, (UC)"\347",	(UC)"9",	(UC)"\307",	(UC)"^" },
{ 43, 6, (UC)"m",	(UC)"M",	(UC)"\015",	0 },
{ 44, 8, (UC)",",	(UC)"?",	0,		0 },
{ 45, 8, (UC)";",	(UC)".",	0,		0 },
{ 46,10, (UC)"\340",	(UC)"0",	(UC)"\300",	(UC)"@" },
{ 47, 8, (UC)")",	(UC)"\260",	0,		(UC)"]" },
{ 49, 8, (UC)"^",	(UC)"\250",	0,		0 },
{ 50,10, (UC)"\371",	(UC)"%",	(UC)"\331",	0 },
{ 52, 8, (UC)":",	(UC)"/",	0,		0 },
{ 53, 8, (UC)"!",	(UC)"\247",	0,		0 },
{ 54, 8, (UC)"=",	(UC)"+",	0,		(UC)"}" },
{ 55, 8, (UC)"\262",	0,		0,		0 },
{ 56, 8, (UC)"$",	(UC)"\243",	0,		(UC)"\244" },
{ 57, 8, (UC)"*",	(UC)"\265",	0,		0 }
};
short kbd_numKeys_french = sizeof(kbd_keyMap_french) / sizeof(KeyMap);

/* belgian */
KeyMap kbd_keyMap_belgian[] = {
{  8, 8, (UC)"&",	(UC)"1",	0,		(UC)"|"},
{ 10, 6, (UC)"a",	(UC)"A",	(UC)"\001",	0 },
{ 11, 6, (UC)"q",	(UC)"Q",	(UC)"\021",	0 },
{ 14,10, (UC)"\351",	(UC)"2",	(UC)"\311",	(UC)"@" },
{ 15, 8, (UC)"\"",	(UC)"3",	0,		(UC)"#" },
{ 16, 6, (UC)"z",	(UC)"Z",	(UC)"\032",	0 },
{ 20, 6, (UC)"w",	(UC)"W",	(UC)"\027",	0 },
{ 22, 8, (UC)"'",	(UC)"4",	0,		0 },
{ 23, 8, (UC)"(",	(UC)"5",	0,		0 },
{ 30, 8, (UC)"\247",	(UC)"6",	0,		(UC)"^" },
{ 31,10, (UC)"\350",	(UC)"7",	(UC)"\310",	0 },
{ 38, 8, (UC)"!",	(UC)"8",	(UC)"\037",	0 },
{ 39,10, (UC)"\347",	(UC)"9",	(UC)"\307",	(UC)"{" },
{ 43, 6, (UC)"m",	(UC)"M",	(UC)"\015",	0 },
{ 44, 8, (UC)",",	(UC)"?",	0,		0 },
{ 45, 8, (UC)";",	(UC)".",	0,		0 },
{ 46,10, (UC)"\340",	(UC)"0",	(UC)"\300",	(UC)"}" },
{ 47, 8, (UC)")",	(UC)"\260",	0,		0 },
{ 49, 8, (UC)"^",	(UC)"\250",	0,		(UC)"[" },
{ 50,10, (UC)"\371",	(UC)"%",	(UC)"\331",	(UC)"'" },
{ 52, 8, (UC)":",	(UC)"/",	0,		0 },
{ 53, 8, (UC)"=",	(UC)"+",	0,		(UC)"~" },
{ 54, 8, (UC)"-",	(UC)"_",	0,		0 },
{ 55, 8, (UC)"\262",	(UC)"\263",	0,		0 },
{ 56, 8, (UC)"$",	(UC)"*",	0,		(UC)"]" },
{ 57, 8, (UC)"\265",	(UC)"\243",	0,		(UC)"`" }
};
short kbd_numKeys_belgian = sizeof(kbd_keyMap_belgian) / sizeof(KeyMap);

/* italian */
KeyMap kbd_keyMap_italian[] = {
{ 14, 8, (UC)"2",	(UC)"\"",	0,		0 },
{ 15, 8, (UC)"3",	(UC)"\243",	0,		0 },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	0 },
{ 31, 8, (UC)"7",	(UC)"/",	0,		0 },
{ 38, 8, (UC)"8",	(UC)"(",	0,		0 },
{ 39, 8, (UC)"9",	(UC)")",	0,		0 },
{ 43,10, (UC)"\362",	(UC)"\347",	(UC)"\322",	(UC)"@" },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		0 },
{ 47, 8, (UC)"'",	(UC)"?",	0,		(UC)"`" },
{ 49,10, (UC)"\350",	(UC)"\351",	(UC)"\310",	(UC)"[" },
{ 50,10, (UC)"\340",	(UC)"\260",	(UC)"\300",	(UC)"#" },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54,10, (UC)"\354",	(UC)"^",	(UC)"\314",	(UC)"~" },
{ 55, 8, (UC)"\\",	(UC)"|",	0,		0 },
{ 56, 8, (UC)"+",	(UC)"*",	0,		(UC)"]" },
{ 57,10, (UC)"\371",	(UC)"\247",	(UC)"\331",	0 },
};
short kbd_numKeys_italian = sizeof(kbd_keyMap_italian) / sizeof(KeyMap);

/* danish */
KeyMap kbd_keyMap_danish[] = {
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"#",	0,		(UC)"\243" },
{ 22, 8, (UC)"4",	(UC)"\244",	0,		(UC)"$" },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	0 },
{ 31, 8, (UC)"7",	(UC)"/",	0,		(UC)"{" },
{ 38, 8, (UC)"8",	(UC)"(",	0,		(UC)"[" },
{ 39, 8, (UC)"9",	(UC)")",	0,		(UC)"]" },
{ 43, 6, (UC)"\346",	(UC)"\306",	0,		0 },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		(UC)"}" },
{ 47, 8, (UC)"+",	(UC)"?",	0,		0 },
{ 49, 6, (UC)"\345",	(UC)"\305",	0,		0 },
{ 50, 6, (UC)"\370",	(UC)"\330",		0,		0 },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"\264",	(UC)"`",	0,		(UC)"|" },
{ 55, 8, (UC)"\275",	(UC)"\247",	0,		0 },
{ 56, 8, (UC)"\250",	(UC)"^",	0,		(UC)"~" },
{ 57, 8, (UC)"'",	(UC)"*",	0,		0 },
{170, 8, (UC)"<",	(UC)">",	0,		(UC)"\\" }
};
short kbd_numKeys_danish = sizeof(kbd_keyMap_danish) / sizeof(KeyMap);

/* spanish */
KeyMap kbd_keyMap_spanish[] = {
{ 8,  8, (UC)"1",	(UC)"!",	0,		(UC)"|" },
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"\260",	0,		(UC)"#" },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	(UC)"\254" },
{ 31, 8, (UC)"7",	(UC)"/",	0,		0 },
{ 38, 8, (UC)"8",	(UC)"(",	0,		0 },
{ 39, 8, (UC)"9",	(UC)")",	0,		0 },
{ 43, 6, (UC)"\361",	(UC)"\321",	0,		(UC)"~" },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		0 },
{ 47, 8, (UC)"\264",	(UC)"?",	0,		0 },
{ 49, 8, (UC)"`",	(UC)"^",	0,		(UC)"[" },
{ 50, 8, (UC)"'",	(UC)"\250",	0,		(UC)"{" },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"\241",	(UC)"\277",	0,		0 },
{ 55, 8, (UC)"\272",	(UC)"\252",	0,		(UC)"\\" },
{ 56, 8, (UC)"+",	(UC)"*",	0,		(UC)"]" },
{ 57, 6, (UC)"\347",	(UC)"\307",	0,		(UC)"}" },
};
short kbd_numKeys_spanish = sizeof(kbd_keyMap_spanish) / sizeof(KeyMap);

/* swiss_german */
KeyMap kbd_keyMap_swiss_german[] = {
{ 8,  8, (UC)"1",	(UC)"+",	0,		(UC)"|" },
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"*",	0,		(UC)"#" },
{ 20, 6, (UC)"y",	(UC)"Y",	(UC)"\031",	0 },
{ 22, 8, (UC)"4",	(UC)"\347",	0,		0 },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	(UC)"\254" },
{ 31, 8, (UC)"7",	(UC)"/",	0,		(UC)"\246" },
{ 32, 6, (UC)"z",	(UC)"Z",	(UC)"\032",	0 },
{ 38, 8, (UC)"8",	(UC)"(",	0,		(UC)"\242" },
{ 39, 8, (UC)"9",	(UC)")",	0,		0 },
{ 43,10, (UC)"\366",	(UC)"\351",	(UC)"\326",	0 },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		0 },
{ 47, 8, (UC)"'",	(UC)"?",	0,		(UC)"\264" },
{ 49,10, (UC)"\374",	(UC)"\350",	(UC)"\334",	(UC)"[" },
{ 50,10, (UC)"\344",	(UC)"\340",	(UC)"\304",	(UC)"{" },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"^",	(UC)"`",	0,		(UC)"~" },
{ 55, 8, (UC)"\247",	(UC)"\260",	0,		0 },
{ 56, 8, (UC)"\250",	(UC)"!",	0,		(UC)"]" },
{ 57, 8, (UC)"$",	(UC)"\243",	0,		(UC)"}" },
{170, 8, (UC)"<",	(UC)">",	0,		(UC)"\\" }
};
short kbd_numKeys_swiss_german = sizeof(kbd_keyMap_swiss_german)/sizeof(KeyMap);

/* swedish */
KeyMap kbd_keyMap_swedish[] = {
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"#",	0,		(UC)"\243" },
{ 22, 8, (UC)"4",	(UC)"\244",	0,		(UC)"$" },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	0 },
{ 31, 8, (UC)"7",	(UC)"/",	0,		(UC)"{" },
{ 38, 8, (UC)"8",	(UC)"(",	0,		(UC)"[" },
{ 39, 8, (UC)"9",	(UC)")",	0,		(UC)"]" },
{ 43, 6, (UC)"\366",	(UC)"\326",	0,		0 },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[1091") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		(UC)"}" },
{ 47, 8, (UC)"+",	(UC)"?",	0,		(UC)"\\" },
{ 48, 6, (UC)"p",	(UC)"P",	(UC)"\020",	0 },
{ 49, 8, (UC)"\345",	(UC)"\305",	0,		0 },
{ 50, 6, (UC)"\344",	(UC)"\304",	0,		0 },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"\264",	(UC)"`",	0,		0 },
{ 55, 8, (UC)"\247",	(UC)"\275",	(UC)"\003",	0 },
{ 56, 8, (UC)"\250",	(UC)"^",	(UC)"\003",	(UC)"~" },
{ 57, 8, (UC)"'",	(UC)"*",	(UC)"\003",	0 },
{ 66, 7, n("\033[P"),	(UC)",",	0,		0 },
{170, 8, (UC)"<",	(UC)">",	0,		(UC)"|" }
};
short kbd_numKeys_swedish = sizeof(kbd_keyMap_swedish) / sizeof(KeyMap);

/* united_kingdom */
KeyMap kbd_keyMap_united_kingdom[] = {
{ 14, 8, (UC)"2",	(UC)"\"",	0,		0 },
{ 15, 8, (UC)"3",	(UC)"\243",	0,		0 },
{ 49, 8, (UC)"[",	(UC)"{",	0,		0 },
{ 50, 8, (UC)"'",	(UC)"@",	0,		0 },
{ 55, 8, (UC)"`",	(UC)"\254",	0,		(UC)"|" },
{ 57, 8, (UC)"#",	(UC)"~",	0,		0 },
{170, 8, (UC)"\\",	(UC)"\246",	c("\034"),	a("\033[086q") },
};
short kbd_numKeys_united_kingdom = sizeof(kbd_keyMap_united_kingdom) / sizeof(KeyMap);

/* norwegian */
KeyMap kbd_keyMap_norwegian[] = {
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"#",	0,		(UC)"\243" },
{ 22, 8, (UC)"4",	(UC)"\244",	0,		(UC)"$" },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	0 },
{ 31, 8, (UC)"7",	(UC)"/",	0,		(UC)"{" },
{ 38, 8, (UC)"8",	(UC)"(",	0,		(UC)"[" },
{ 39, 8, (UC)"9",	(UC)")",	0,		(UC)"]" },
{ 43, 6, (UC)"\370",	(UC)"\330",		0,		0 },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		(UC)"}" },
{ 47, 8, (UC)"+",	(UC)"?",	0,		0 },
{ 49, 6, (UC)"\345",	(UC)"\305",	0,		0 },
{ 50, 6, (UC)"\346",	(UC)"\306",	0,		0 },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"\\",	(UC)"`",	0,		(UC)"\264" },
{ 55, 8, (UC)"|",	(UC)"\247",	0,		0 },
{ 56, 8, (UC)"\250",	(UC)"^",	0,		(UC)"~" },
{ 57, 8, (UC)"'",	(UC)"*",	0,		0 },
};
short kbd_numKeys_norwegian = sizeof(kbd_keyMap_norwegian) / sizeof(KeyMap);

KeyMap kbd_keyMap_portuguese[]= {
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"#",	0,		(UC)"\243" },
{ 22, 8, (UC)"4",	(UC)"$",	0,		(UC)"\247" },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	0 },
{ 31, 8, (UC)"7",	(UC)"/",	0,		(UC)"{" },
{ 38, 8, (UC)"8",	(UC)"(",	0,		(UC)"[" },
{ 39, 8, (UC)"9",	(UC)")",	0,		(UC)"]" },
{ 43, 6, (UC)"\347",	(UC)"\307",	0,		0 },
{ 45, 8, (UC)",",	(UC)";",	0,		0 },
{ 46, 8, (UC)"0",	(UC)"=",	0,		(UC)"}" },
{ 47, 8, (UC)"'",	(UC)"?",	0,		0 },
{ 49, 8, (UC)"+",	(UC)"*",	0,		(UC)"\250" },
{ 50, 8, (UC)"\272",	(UC)"\252",	0,		0 },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"\253",	(UC)"\273",	0,		0 },
{ 55, 8, (UC)"\\",	(UC)"|",	0,		0 },
{ 56, 8, (UC)"\264",	(UC)"`",	0,		0 },
{ 57, 8, (UC)"~",	(UC)"^",	0,		0 },
};
short kbd_numKeys_portuguese = sizeof(kbd_keyMap_portuguese) / sizeof(KeyMap);


/* swiss_french */
KeyMap kbd_keyMap_swiss_french[] = {
{ 8,  8, (UC)"1",	(UC)"+",	0,		(UC)"|" },
{ 14, 8, (UC)"2",	(UC)"\"",	0,		(UC)"@" },
{ 15, 8, (UC)"3",	(UC)"*",	0,		(UC)"#" },
{ 20, 6, (UC)"y",	(UC)"Y",	(UC)"\031",	0 },
{ 22, 8, (UC)"4",	(UC)"\347",	0,		0 },
{ 30, 8, (UC)"6",	(UC)"&",	(UC)"\036",	(UC)"\254" },
{ 31, 8, (UC)"7",	(UC)"/",	0,		(UC)"\246" },
{ 32, 6, (UC)"z",	(UC)"Z",	(UC)"\032",	0 },
{ 38, 8, (UC)"8",	(UC)"(",	0,		(UC)"\242" },
{ 39, 8, (UC)"9",	(UC)")",	0,		0 },
{ 43,10, (UC)"\351",	(UC)"\366",	(UC)"\311",	0 },
{ 45, 8, (UC)",",	(UC)";",	c("\033[108q"),	a("\033[109q") },
{ 46, 8, (UC)"0",	(UC)"=",	0,		0 },
{ 47, 8, (UC)"'",	(UC)"?",	0,		(UC)"\264" },
{ 49,10, (UC)"\350",	(UC)"\374",	(UC)"\310",	(UC)"[" },
{ 50,10, (UC)"\340",	(UC)"\344",	(UC)"\300",	(UC)"{" },
{ 52, 8, (UC)".",	(UC)":",	0,		0 },
{ 53, 8, (UC)"-",	(UC)"_",	(UC)"\037",	0 },
{ 54, 8, (UC)"^",	(UC)"`",	0,		(UC)"~" },
{ 55, 8, (UC)"\247",	(UC)"\260",	0,		0 },
{ 56, 8, (UC)"\250",	(UC)"!",	0,		(UC)"]" },
{ 57, 8, (UC)"$",	(UC)"\243",	0,		(UC)"}" },
{170, 8, (UC)"<",	(UC)">",	0,		(UC)"\\" }
};
short kbd_numKeys_swiss_french = sizeof(kbd_keyMap_swiss_french)/sizeof(KeyMap);
#else
extern int _prom;
#endif
