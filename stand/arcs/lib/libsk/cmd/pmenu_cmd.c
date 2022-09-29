#ident	"lib/libsk/cmd/pmenu_cmd.c:  $Revision: 1.85 $"

/*
 * PROM (not shared with sash) Menu code
 *
 * The menu mechanics are in lib/menu.c, this file defines the contents
 * and implementation of the menu.
 *
 */

#if !IP19 && !IP20 && !IP21 && !IP22 && !IP26
/* no tpsc or tape install in standalone */
#define _NO_TAPE_INSTALL	1
#endif

#ifdef _NO_TAPE_INSTALL
#define NDEVICES        2
#define DEV_REMDIR      0
#define DEV_CDROM       1
#define DEV_TAPE        100
#define DEV_REMTAPE     101
#else
#define NDEVICES        4
#define DEV_REMTAPE     0
#define DEV_REMDIR      1
#define DEV_CDROM       2
#define DEV_TAPE        3
#endif

#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <arcs/signal.h>

#include <sys/param.h>
#include <net/in.h>
#include <menu.h>
#include <parser.h>
#include <stringlist.h>

#include <saioctl.h>
#include <setjmp.h>

#include <gfxgui.h>
#include <style.h>

#include <libsc.h>
#include <libsk.h>

#include <pcbm/pcbm.h>			/* icons for gui_sa_getsource() */
#if defined(IP24)
#include <pcbm/indy_instcd.h>
#include <pcbm/indy_insttape.h>
#include <pcbm/indy_instrdir.h>
#include <pcbm/indy_instrtape.h>
#elif defined(IP22) || defined(IP26) || defined(IP28)
#include <pcbm/i2_instcd.h>
#include <pcbm/i2_instrdir.h>
#ifndef _NO_TAPE_INSTALL
#include <pcbm/i2_insttape.h>
#include <pcbm/i2_instrtape.h>
#endif
#elif defined(IP30)
#include <pcbm/sr_instcd.h>
#include <pcbm/sr_instrdir.h>
#else
#include <pcbm/instcd.h>
#include <pcbm/instrdir.h>
#ifndef _NO_TAPE_INSTALL
#include <pcbm/insttape.h>
#include <pcbm/instrtape.h>
#endif
#endif

#if	!IP20 && !EVEREST	/* all new machines have PC keyboards */
#define KBD_SELECTOR		/* PC keyboards do not auto-select */
#endif

#include <pcbm/sysstart.h>		/* icons for prom menu */
#include <pcbm/install.h>
#include <pcbm/diags.h>
#include <pcbm/recover.h>
#include <pcbm/console.h>

#ifdef KBD_SELECTOR
#include <pcbm/kbdlay.h>
#endif

#ifdef KBD_SELECTOR
#define	KBD_MENU_ITEM	        1
#else
#define	KBD_MENU_ITEM	        0
#endif

#define	EXTRA_MENU_ITEMS         0

#define ITEMS_IN_MENU           (5 + KBD_MENU_ITEM + EXTRA_MENU_ITEMS)

#if _MIPS_SIM == _ABI64
#define SASHNAME	"sash64"
#else /* _MIPS_SIM */
#define SASHNAME	"sashARCS"
#endif

int menu_boot_sys(void); 
int menu_doinstall(void);
int menu_boot_ide(void); 
int menu_manualmode(void);
int menu_recovery(void);
#ifdef KBD_SELECTOR
int menu_kbdlayout(void);
#endif

static int gui_sa_getsource(char *, char *, char *);

#ifndef SN0
extern char *fixupname(COMPONENT *);
#else
extern char *kl_fixupname(COMPONENT *);
extern void sn0_dev2eng(COMPONENT *, char *) ;
#endif
static int sa_getsource(char *);
static void appendComponentList(COMPONENT **list, COMPONENT *c);
static void giveup(char *,unsigned long);
static void returnmsg(void);
static int menu_boot(char *, char *, char *, char *);
static void showlist(COMPONENT *root, CONFIGTYPE type, struct radioList *list,
		     int gui);
static void boot_failed(int rc, char *path, char *type);

static int instsrc;

static int tapedevset;
int needsa;

static void bhandler(struct Button *b, __scunsigned_t value);
static int pressed;

/*
 * prom menu's items
 */
static mitem_t prom_menu_items[ITEMS_IN_MENU];

/*
 * prom menu
 */
menu_t prom_menu = {
	prom_menu_items,
	1,
	"System Maintenance Menu",
	"Option?",
	"",
	ITEMS_IN_MENU
};


/*
   Cannot initialize prom_menu_items[] during definition because we need to
   modify the flags field. if prom_menu_items[] was initialized during
   definition, it would be put into the prom and could not be changed during
   run time
*/
void
init_prom_menu(void)
{
        int i = 0;

/* 1 */
	prom_menu_items[i].prompt = "Start System";
	prom_menu_items[i].service = menu_boot_sys;
	prom_menu_items[i].bits = &sysstart;
	i++;

/* 2 */
	prom_menu_items[i].prompt = "Install System Software";
	prom_menu_items[i].service = menu_doinstall;
	prom_menu_items[i].bits = &install;
	prom_menu.item[i].flags |= M_PASSWD;
	i++;

/* 3 */
	prom_menu_items[i].prompt = "Run Diagnostics";
	prom_menu_items[i].service = menu_boot_ide;
	prom_menu_items[i].bits = &diags;
	prom_menu.item[i].flags |= M_PASSWD;
	i++;

/* 4 */	
	prom_menu_items[i].prompt = "Recover System";
	prom_menu_items[i].service = menu_recovery;
	prom_menu_items[i].bits = &recover;
	prom_menu.item[i].flags |= M_PASSWD;
	i++;

/* 5 */
	prom_menu_items[i].prompt = "Enter Command Monitor";
	prom_menu_items[i].service = menu_manualmode;
	prom_menu_items[i].bits = &console;
	prom_menu.item[i].flags |= M_PASSWD;	
	i++;

#ifdef KBD_SELECTOR
/* -6 or -7 */
	prom_menu_items[i].prompt = "Select Keyboard Layout";
	prom_menu_items[i].service = menu_kbdlayout;
	prom_menu_items[i].bits = &kbdlay;
	prom_menu_items[i].flags |= M_GUIONLY;
	i++;
#endif

	/*
	** to strain software loading/ custom boot features
	** through password validation, nuke out "dangerous"
	** automatic entries, ie install software/ recover system
	*/
}


/*
 * Start system menu option implementation
 *
 */
int
menu_boot_sys(void)
{
	extern int Verbose;

	if (!Verbose && doGui()) {
		setGuiMode(1,0);
	}
	else {
		p_panelmode();
		p_clear();
	}

	if (autoboot(0, 0, 0) != ESUCCESS)
		giveup("Autoboot failed",0);

	return 0;
}


/*
 * Install System Software menu option implementation
 *
 * Run sash with the -m option.  The idea is to
 * keep all the mini-root swill out of the prom.
 *
 */
int
menu_doinstall(void)
{
	char sash_path[128];
	int rc;

	if (doGui()) {
		if (gui_sa_getsource(sash_path,"Install System Software",
				     "Install"))
			return 0;
	}
	else {
		p_panelmode();
		p_clear();
		p_center();
		p_printf("\n\nInstalling System Software...\n\n");
		returnmsg();
		if (sa_getsource(sash_path))
			return 0;
	}

	Signal(SIGINT,SIGDefault);	/* calls EnterInteractiveMode() */

	rc = menu_boot(sash_path, "installation tools", "OSLoadOptions=mini", 0);

	boot_failed(rc, sash_path, "Installation");

	return 0;
}

/*
 * Boot the standalone diagnostics
 *
 * The diagnostic is in the volume header along with sash.  A
 * boot command, for a diskfull machine, is formed by replacing
 * the word "ide" to the SystemPartition variable, and for a
 * diskless machine, the last component in the SystemPartition variable,
 * is replaced with "stand/ide".
 */

#define MAXIDEPATHLEN 64

int
menu_boot_ide(void)
{
	char *p, *oboot, idepath[MAXIDEPATHLEN];
	char *diag_arg = getenv("diagmode");	/* pass to ide script */
	char *diskless;
	COMPONENT *c;
	ULONG fd;
	int rc;

	Signal(SIGINT,SIGDefault);
        (void)ioctl(0,TIOCINTRCHAR,(long)"\003\033");	/* add <esc> */

	p_panelmode();
	p_clear();
	p_center();

#ifdef SN
	p_printf("\n\nDiagnostic program not supported in this configuration.\n\n");
	return 0;
#else

	p_printf("\n\nStarting diagnostic program...\n\n");
	returnmsg();
	
	/*  If CD-ROM drive is present and sashARCS (or sash64 on 64 bit
	 * PROMS) exists in part(8), try and boot it.
	 */
	if (c = find_type(GetChild(NULL),CDROMController)) {
		p = getpath(c);

		p_center();
		p_printf("Checking for Distribution CD-ROM on %s.\n",p);

		strcpy(idepath,p);
		strcat(p,"part(8)" SASHNAME);

		if (Open(p,OpenReadOnly,&fd) == ESUCCESS) {
			Close(fd);

			p_center();
			p_printf("Booting IDE from Distribution CD-ROM.\n\n");

			strcat(idepath,"part(7)/stand/ide.");
			strcat(idepath,inv_findcpu());
			rc = menu_boot(p,0,idepath,diag_arg);
			goto boot_done;
		}

		p_center();
		p_printf("\nDistribution CD-ROM not found."
			 "  Booting installed IDE.\n");
	}

	/* Try local disk, or /stand/ide on diskless.
	 */
	if ( (oboot = getenv("SystemPartition")) == 0 || *oboot == '\0' ) {
		giveup ("No SystemPartition set", 0);
		return 0;
	}
	else
		strcpy(idepath,oboot);

	if ( (p = rindex(idepath,')')) == 0 )  {
		giveup ("SystemPartition is not set correctly", 0);
		return 0;
	}

	diskless = getenv ("diskless");
	if (diskless && (*diskless == '1')) {
		char *pp;

		if (pp = index(p, ':'))
			p = pp;
		if (pp = rindex(p, '/'))
			p = pp;
		strcpy(p+1,"stand/ide");
	}
	else {
		strcpy(p+1,"ide");
		diskless = 0;
	}

	rc = menu_boot(idepath, 0, 0, diag_arg);
	
	/* ide from the volume header failed.   If not diskless try
	 * to pick it up from /stand or /usr/stand (on unified disk).
	 */
	if (rc && !diskless) {
		strcpy(idepath,oboot);
		strcat(idepath,"sash");
		rc = menu_boot(idepath,0,"stand/ide",diag_arg); 
		if (rc)
			rc = menu_boot(idepath,0,"usr/stand/ide",diag_arg); 
	}

boot_done:
	instsrc = NDEVICES;	/* Make sure don't get remote message */
	boot_failed(rc, idepath, "Diagnostics");
	return 0;
#endif /* SN */
}

/*
 * Give the user what they ask for, the old ">> "
 *
 */
int
menu_manualmode(void)
{
	extern void EnterInteractiveMode();

	setTpButtonAction(EnterInteractiveMode,TPBDONE,WBONCHANGE);
	p_textmode();
	p_clear();

	printf("Command Monitor.  Type \"exit\" %sto return to the menu.\n",
	       isgraphic(StandardOut) ? "or click on \"done\" " : "");

	return 1;
}

/*
 * Boot the mini-root in crash recovery mode
 *
 */
int
menu_recovery(void)
{
	static char *msg = "System Recovery";
	char sash_path[128];
	int rc;

	if (doGui()) {
		if (gui_sa_getsource(sash_path,msg,"Accept"))
			return 0;
	}
	else {
		p_panelmode();
		p_clear();
		p_center();
		p_printf("\n\n%s...\n\n",msg);
		returnmsg();
		if (sa_getsource(sash_path))
			return 0;
	}

	rc = menu_boot (sash_path, "recovery tools", "OSLoadOptions=mini", "initstate=3");

	boot_failed(rc, sash_path, "Recovery");

	return 0;
}

#ifdef KBD_SELECTOR

#define CUSTOM 15
char *kbdlayout[] = {
	"BE", "DE", "de_CH", "DK",
	"ES", "FI", "FR", "fr_CH",
	"GB", "IT", "JP", "NO",
	"PT", "SE", "US", "Custom",
	0
};

#define KBDLMARGIN	(DIALOGBDW+DIALOGMARGIN+40)
#define KBDLW	595
#define KBDLH	260

/*
 * Put put selector box to pick keyboard map from supported maps.  Systems
 * with PC keyboards cannot auto-select so let the user select with the
 * mouse.
 */
int
menu_kbdlayout(void)
{
	static char *title = "Keyboard Layout";
	struct _radioButton *radio;
	int basex, i, ch, x,y, ty;
	struct radioList *list;
	struct Canvas *canvas;
	struct Button *accept;
	struct Button *cancel;
	__psint_t fnd;
	char *keybd;

	pressed = 0;

	keybd = getenv("keybd");
	if (!keybd)
		keybd = "US";

	setGuiMode(1,GUI_NOLOGO);

	canvas = createCanvas(KBDLW,KBDLH);
	moveObject(guiobj(canvas),canvas->gui.x1,((gfxHeight() - KBDLH)/3)*2);

	textSize(title,&x,&y,ncenB18);
	x = (gfxWidth()-x)>>1;
	y = ty = canvas->gui.y2 - DIALOGBDW - DIALOGMARGIN - y;
	moveTextPoint(x,y);
	(void)createText(title,ncenB18);

	x = canvas->gui.x1 + KBDLMARGIN;
	y = canvas->gui.y1 + DIALOGBDW + 2*DIALOGMARGIN + TEXTBUTH + 30;
	list = createRadioList(x,y,KBDLW-2*KBDLMARGIN,500);
	list->gui.y2 = ty - DIALOGMARGIN - 20;
	list->gui.parent = &canvas->gui;

	for (fnd=i=ch=0; kbdlayout[i]; i++) {
		ch = 0;

		if (!fnd && (!strcmp(keybd,kbdlayout[i]) ||
			    (kbdlayout[i+1] == 0))) {
			fnd = i+1;
			ch = 1;
		}

		radio = appendRadioList(list,kbdlayout[i],ch);
		if (radio) radio->userdata = (void *)(__psint_t)i;

		if (!i) {
			x = basex = radio->button.gui.x1;
			y = radio->button.gui.y1;
		}
		else if (i%4 != 0) {
			x += 140;
			moveObject(guiobj(&(radio->button)),x,y);
		}
		else {
			x = basex;
			y -= 30;
			moveObject(guiobj(&(radio->button)),x,y);
		}
	}

	/* If not already custom, need to use setenv */
	if (fnd != (CUSTOM+1))
		invalidateButton((struct Button *)&radio->button,1);

	x = canvas->gui.x2 - DIALOGBDW - DIALOGMARGIN - TEXTBUTW;
	y = canvas->gui.y1+DIALOGBDW + DIALOGMARGIN;
	accept = createButton(x,y,TEXTBUTW,TEXTBUTH);
	addButtonText(accept,"Apply");
	addButtonCallBack(accept,bhandler,50);
	setDefaultButton(accept,1);

	x = accept->gui.x1 - (TEXTBUTW + BUTTONGAP);
	cancel = createButton(x,y,TEXTBUTW,TEXTBUTH);
	addButtonText(cancel,"Cancel");
	addButtonCallBack(cancel,bhandler,51);

	guiRefresh();

	while(1) {
		if (GetReadStatus(0) == ESUCCESS) {
			ch = getchar() & 0xff;

			if (ch == '\033')
				pressed = 51;
			else if ((ch == '\n') || (ch == '\r'))
				pressed = 50;
			else if ((ch >= 'a') && (ch <= 'z')) {
				struct _radioButton *old;/* item select */
				int sel = ch - 'a';

				old = setRadioListSelection(list,sel);
				if (old) {
					drawObject(guiobj(&(old->button)));
					drawObject(guiobj(&(list->selected->button)));
				}
			}
		}
		if (pressed) {
			if (pressed == 50) {		/* Apply */
				struct _radioButton *item;
				item = getRadioListSelection(list);
				if (!item)
					break;
				fnd = (__psint_t)item->userdata;
				/* save new value in nvram and re-map kbd */
				if (fnd != CUSTOM) {
					setenv("keybd",kbdlayout[fnd]);
					pckm_setupmap();
				}
				break;
			}

			if (pressed == 51)		/* Cancel */
				break;

			pressed = 0;
		}
	}

	return(0);
}
#endif

static void
boot_failed(int rc, char *path, char *type)
{
	char errbuf[256];

	switch (rc) {
	case ENOENT:
		if (instsrc == DEV_REMTAPE || instsrc == DEV_REMDIR)
			sprintf(errbuf,"%s tools not found at %s:%s\n",
				type, getenv("netinsthost"), getenv("netinstfile"));
		else 
			sprintf(errbuf,"%s tools not found\n",type);
		break;
	case EADDRNOTAVAIL:
	case ETIMEDOUT:
	case ECONNABORTED:
		sprintf(errbuf, "Network error while loading %s tools",type);
		break;
	case ENXIO:
		if (instsrc == DEV_REMTAPE || instsrc == DEV_REMDIR) {
			sprintf(errbuf,"Unable to reach server %s",getenv("netinsthost"));
			break;
		}
		/*FALLSTHROUGH*/
	default:
		sprintf(errbuf,"Cannot load %s -- %s.\n",path,
			arcs_strerror(rc));
		break;
	}
	giveup(errbuf,0);
}

/*
 * Called when some kind of fatal error occurs
 */
static void
giveup(char *string, unsigned long arg)
{
	static int buttons[] = {DIALOGCONTINUE,DIALOGPREVDEF,-1};
	char buff[256];		/* > LINESIZE */

	if (!tapedevset)
		setenv("tapedevice","");

	if (isGuiMode()) {
		setGuiMode(1,GUI_NOLOGO);		/* clear screen */
		sprintf(buff,string,arg);
		popupDialog(buff,buttons,DIALOGWARNING,DIALOGCENTER);
		return;
	}

	p_curson ();
	printf(string,arg);
	setTpButtonAction(EnterInteractiveMode,TPBRETURN,WBNOW);
	printf("\nUnable to continue; press <enter> to return to the menu: ");
	gets(buff);
	EnterInteractiveMode();		/* doesn't return from here */
}

static int ndevs;
static void
dev2eng(COMPONENT *c, char *buf)
{
	static char *ctrlmsg = " on controller %d";
	char *host = getenv("netinsthost");
	char *file = getenv("netinstfile");
	COMPONENT *parent;
	char nbuf[32];

#ifndef SN0
	parent = GetParent(c);
#else
	parent = NULL ;
#endif

	switch (c->Type) {
	case CDROMController:
#ifdef SN0
		panic("dev2eng: GetParent does not workfor CDROM on SN0\n") ;
#endif
		sprintf(buf,"Local %sCD-ROM drive %d",
			(parent->Type == SCSIAdapter) ? "SCSI " : "",
			c->Key);
		if (parent->Key) {
			sprintf(nbuf,ctrlmsg,parent->Key);
			strcat(buf,nbuf);
		}
		break;
	case NetworkController:
		sprintf(nbuf,"on network %d",c->Key);
		if (instsrc == DEV_REMTAPE) {
			char *tmp,*l,*r;
			if (strcmp(file,"/dev/tape")) {
				l = " [";
				tmp = file;
				r = "]";
			}
			else
				l = r = tmp = "";
			sprintf(buf,"Remote tape%s%s%s on server %s%s.",
				l,tmp,r,host,(ndevs > 1) ? nbuf : "");
		}
		else
			sprintf(buf,"Remote directory %s from server %s%s.",
				file,host,(ndevs > 1) ? nbuf : "");
		break;
	case TapeController:
#ifdef SN0
		panic("dev2eng: GetParent does not workfor TAPE on SN0\n") ;
#endif
		sprintf(buf,"Local %sTape drive %d",
			(parent->Type == SCSIAdapter) ? "SCSI " : "",
			c->Key);
		if (parent->Key) {
			sprintf(nbuf,ctrlmsg,parent->Key);
			strcat(buf,nbuf);
		}
		break;
	}
}

static int insertpostop(char *source, COMPONENT *c, struct radioList *l);
static int netpreop(struct radioList *l, int which);

#define DEVBUTW		80
#define DEVBUTGAP	16
static struct devices {
	char *place;
	char *name;
	CONFIGTYPE type;
	int (*preop)(struct radioList *, int which);
	int (*postop)(char *, COMPONENT *, struct radioList *);
	struct pcbm *map;
} devices[] = {
#ifdef _NO_TAPE_INSTALL
	"Remote","Directory",NetworkController,netpreop,0,&instrdir,
	"Local","CD-ROM",CDROMController,0,insertpostop,&instcd,
#else
	"Remote","Tape",NetworkController,netpreop,insertpostop,&instrtape,
	"Remote","Directory",NetworkController,netpreop,0,&instrdir,
	"Local","CD-ROM",CDROMController,0,insertpostop,&instcd,
	"Local","Tape",	TapeController,0,insertpostop,&insttape
#endif
};
static int devcnt[NDEVICES];

static int
netpreop(struct radioList *l, int which)
{
	extern char netaddr_default[];
	static char *ipprompt = "Enter your IP address:";
	static char *hostprompt = "Enter the name of the remote host:";
	static char *tapeprompt = "Enter the remote tape device:";
	static char *dirprompt = "Enter the remote directory:";
	static char *netinsthost = "netinsthost";
	static char *netinstfile = "netinstfile";
	static char *netaddr = "netaddr";
	static int lastwhich;
	char *prompt,*cp;
	char buf[LINESIZE];
	char tok[LINESIZE];
	int rc;

	/* reset netinstfile if switching between tape and directory */
	if (which != lastwhich)
		setenv(netinstfile,"");
	lastwhich = which;

	/* make sure we have a valid IP address
	 */
check_IP_address:
	cp = getenv(netaddr);
	if (!cp || !strcmp(cp, netaddr_default)) {
		if (l) {
			rc = popupGets(ipprompt,&l->gui,0,buf,LINESIZE);
			if (rc == 1)			/* cancel button */
				return(0);
		}
		else {
			printf("%s ",ipprompt);
			gets(buf);
		}

		if ((token(buf,tok) == 1) &&
		    (inet_addr(tok).s_addr != (unsigned)-1))
			setenv(netaddr,tok);

		goto check_IP_address;		/* recheck for default */
	}

	/* get hostname of remote system
	 */
enter_hostname:
	cp = getenv(netinsthost);
	if (l) {
		rc = popupGets(hostprompt,&l->gui,cp,buf,LINESIZE);
		if (rc == 1)				/* cancel button */
			return(0);
	}
	else {
		printf("%s ", hostprompt);
		if (cp) printf("[%s] ",cp);
		gets(buf);
		if (cp && (buf[0] == '\0'))
			strcpy(buf,cp);
	}
	if (token(buf,tok) != 1)
		goto enter_hostname;
	/* if we are given a file take it!
	 */
	if (cp=index(tok,':')) {
		*cp = '\0';
		setenv(netinsthost,tok);
		setenv(netinstfile,cp+1);
		goto done;		/* skip remote file */
	}
	setenv(netinsthost,buf);

	/* Get the remote filename
	 */
enter_file:
	cp = getenv(netinstfile);
	if ((which == DEV_REMTAPE) && !cp)
		cp = "/dev/tape";

	prompt = (which == DEV_REMTAPE) ? tapeprompt : dirprompt;
	if (l) {
		rc = popupGets(prompt, &l->gui,cp,buf,LINESIZE);
		if (rc == 1)
			return(0);
	}
	else {
		printf("%s ", prompt);
		if (cp) printf("[%s] ",cp);
		gets(buf);
		if (cp && (buf[0] == '\0'))
			strcpy(buf,cp);
	}
	if (token(buf,tok) != 1)
		goto enter_file;
	setenv(netinstfile,tok);

done:
	needsa = (which != DEV_REMTAPE);	/* no sa for tapes */

	return(1);
}

/*ARGSUSED*/
static int
insertpostop(char *source, COMPONENT *c, struct radioList *l)
{
	static int buttons[] = {DIALOGCONTINUE,DIALOGPREVDEF,
				DIALOGCANCEL,DIALOGPREVESC,
				-1};
	char buf[80];
	int rc;

	sprintf(buf,"Insert the installation %s%s",
		(c->Type == CDROMController) ? "CD-ROM" : "tape",
		l ? " now." : ", then press <enter>: ");

	rc = popupAlignDialog(buf,&l->gui,buttons,DIALOGQUESTION,0);

	/* popupDialog return 1 -> cancel, 0 -> continue */
	return((rc == 1) ? 0 : 1);
}

/*ARGSUSED*/
static void
bhandler(struct Button *b, __scunsigned_t value)
{
	pressed = (int)value;
}

static int
gui_sa_getsource(char *source, char *title, char *doit)
{
#define GS_WIDTH	700
#define GS_SMHEIGHT	380
#define GS_HEIGHT	600
	char *tapedevice = getenv("tapedevice");
	struct Button *buttons[NDEVICES];
	struct Button *cancel, *accept;
	int seldev, lastn, maxdevs;
	struct radioList *list;
	struct Canvas *canvas;
	int gs_width = 700;
	int i,x,y,ch;
	
	seldev = lastn = maxdevs = pressed = 0;

	setGuiMode(1,GUI_NOLOGO);

	for (i=0 ; i < NDEVICES; i++) {
#ifdef SN0
                devcnt[i] = kl_count_type(devices[i].type);
#else
                devcnt[i] = count_type(GetChild(NULL),devices[i].type);
#endif


		if (devcnt[i] > maxdevs)
			maxdevs = devcnt[i];
	}

	y = (maxdevs <= 5) ? GS_SMHEIGHT : GS_HEIGHT;

/* XXX - clip point on text string to prevent overflow? */
	/* Squish window to fit in low resolution */
	i = gfxWidth();
	if (i < gs_width) gs_width = i;
	i = gfxHeight();
	if (i < y) y = i;

	canvas = createCanvas(gs_width,y);

	/* put small window at 2/3 y if not shrunk */
	if ((y == GS_SMHEIGHT) && (gs_width == GS_WIDTH))
		moveObject(guiobj(canvas),canvas->gui.x1,
			   ((gfxHeight() - y)/3)*2);

	textSize(title,&x,&y,ncenB18);
	x = (gfxWidth()-x)>>1;
	y = canvas->gui.y2 - DIALOGBDW - DIALOGMARGIN - y;
	moveTextPoint(x,y);
	(void)createText(title,ncenB18);

	x = canvas->gui.x1 + ((gs_width - (NDEVICES*DEVBUTW) -
			       ((NDEVICES-1)*DEVBUTGAP))>>1);
	y -= (DEVBUTW+FORMVERTMARGIN);
	for (i=0 ; i < NDEVICES; i++) {
		struct Button *b;
		struct Text *t;

		b = createButton(x,y,DEVBUTW,DEVBUTW);
		addButtonCallBack(b,bhandler,i+1);
		addButtonImage(b,devices[i].map);
		if (devcnt[i] == 0) {
			invalidateButton(b,1);
		}
		else if ((i == DEV_CDROM) || (i == DEV_TAPE && !pressed))
			/* default to local cd, or tape if present */
			pressed = i+1;

		buttons[i] = b;

		t = createText(devices[i].place,helvR10);
		centerObject(guiobj(t),(struct gui_obj*)b);
		moveObject(guiobj(t),t->gui.x1,y-15);
		t = createText(devices[i].name,helvR10);
		centerObject(guiobj(t),guiobj(b));
		moveObject(guiobj(t),t->gui.x1,y-15-fontHeight(helvR10)-1);

		x += DEVBUTW + DEVBUTGAP;
	}
	y -= 15+fontHeight(helvR10)-1;		/* text labels */

	x = canvas->gui.x1+DIALOGBDW+DIALOGMARGIN+30;
	y -= FORMVERTMARGIN;
	i = y-canvas->gui.y1-DIALOGBDW-DIALOGMARGIN-TEXTBUTH-FORMVERTMARGIN;
	list = createRadioList(x,y-i,gs_width-2*(DIALOGBDW+DIALOGMARGIN+30),i);
	list->gui.parent = &canvas->gui;

	x = canvas->gui.x2 - DIALOGBDW - DIALOGMARGIN - TEXTBUTW;
	y = canvas->gui.y1+DIALOGBDW + DIALOGMARGIN;
	accept = createButton(x,y,TEXTBUTW,TEXTBUTH);
	addButtonText(accept,doit);
	addButtonCallBack(accept,bhandler,20);
	setDefaultButton(accept,1);
	invalidateButton(accept,1);

	x = accept->gui.x1 - (TEXTBUTW + BUTTONGAP);
	cancel = createButton(x,y,TEXTBUTW,TEXTBUTH);
	addButtonText(cancel,"Cancel");
	addButtonCallBack(cancel,bhandler,21);

	/*  If tapedevice is set, show that promptly, and cancel possible
	 * local device default.
	 */
	if (tapedevice) {
		appendRadioList(list,tapedevice,1);
		invalidateButton(accept,0);
		pressed = 0;
	}

	guiRefresh();

	for(;;) {
		if (GetReadStatus(0) == ESUCCESS) {
			ch = getchar() & 0xff;

			/* convert keypress into button action */
			if ((ch == '\n') || (ch == '\r'))
				pressed = 20;		/* Accept */
			else if ((ch >= '1') && (ch <= (char)('0'+NDEVICES)))
				pressed = ch - '0';	/* select device */
			else if ((ch >= 'a') && (ch <= 'z')) {
				struct _radioButton *old;/* item select */
				int sel = ch - 'a';

				old = setRadioListSelection(list,sel);
				if (old) {
					drawObject(guiobj(&(old->button)));
					drawObject(guiobj(&(list->selected->button)));
				}
			}
			else if (ch == '\033')
				pressed = 21;		/* Cancel */
		}
		if (pressed) {
			if (pressed <= NDEVICES) {
				seldev = pressed - 1;
				for (i=0; i < NDEVICES; i++) {
					stickButton(buttons[i],i==seldev);
					drawObject(guiobj(buttons[i]));
				}
				resetRadioList(list);
				needsa = 0;		/* reset */
				if (devices[seldev].preop) {
					i = (*devices[seldev].preop)(list,
								pressed-1);
					resetRadioList(list);
					if (i == 0) {
						stickButton(buttons[seldev],0);
						drawObject(guiobj(buttons[seldev]));
						goto reset;
					}
				}
				instsrc = seldev;
				ndevs = devcnt[seldev];
#ifdef SN0
                                kl_showlist(NULL,
                                         devices[seldev].type,list,1);
#else
                                showlist(GetChild(NULL),
                                         devices[seldev].type,list,1);
#endif

				if (tapedevice)
					appendRadioList(list,tapedevice,0);
				drawObject(guiobj(list));
			}
			else if (pressed == 20) {		/* Accept */
				struct _radioButton *item;
				int iscdrom;

				if ((accept->gui.flags & _INVALID) != 0)
					goto reset;
				
				item = getRadioListSelection(list);
				if (!item)
					goto cancel;

				if (item->userdata) {
					iscdrom = (((COMPONENT *)
						    item->userdata)->Type ==
						   CDROMController);
					tapedevset = 0;
#ifdef SN0
                                        strcpy(source,kl_fixupname((COMPONENT *)
                                                        item->userdata));
#else
                                        strcpy(source,fixupname((COMPONENT *)
                                                        item->userdata));
#endif

				}
				else {
					/* tapedevice */
					tapedevset = 1;
					needsa = iscdrom = 0;
					strcpy(source,item->text);
				}

				resetRadioList(list);

				/*  If selected other than $tapedevice run
				 * the postop for the type selected.
				 */
				if (item->userdata && devices[seldev].postop) {
					if (!(*devices[seldev].postop)(source,
						(COMPONENT *)item->userdata,
						list)) {
						drawObject(guiobj(list));
						stickButton(buttons[seldev],0);
						drawObject(guiobj(buttons[seldev]));
						goto reset;
					}
				}

				if (needsa)
					strcat(source,"/sa");

				setenv("tapedevice",source);

				if (iscdrom)
					strcat(source,SASHNAME);
				else
					strcat(source,"(" SASHNAME ")");

				setGuiMode(1,GUI_NOLOGO);	/* free gui */
				return(0);
			}
			else if (pressed == 21) {	/* Cancel */
cancel:
				setGuiMode(1,GUI_NOLOGO);	/* free gui */
				return(EAGAIN);
			}
reset:
			/*  Turn on/off accept button depending on the list.
			 */
			if (list->curitems != lastn) {
				invalidateButton(accept,(list->curitems == 0));
				drawObject(guiobj(accept));
			}
			lastn = list->curitems;
				
			ch = pressed = 0;
		}
	}
	/*NOTREACHED*/
	return 0;
}

static SIGNALHANDLER oldhand;	/* need to be global for stand longjmp() */
static char *oldintr;
static jmp_buf sags_jb;
static void
sags_esc(void)
{
	longjmp(sags_jb,1);
}

/* tty version */
#define MAXLIST		26			/* a-z */
static int
sa_getsource(char *source)
{
	char *tapedevice = getenv("tapedevice");
	int seldev, curitems, curitem;
	COMPONENT *list[MAXLIST+1];
	int devflags[NDEVICES];
#define SA_INVALID	0x01
	char buf[LINESIZE];
	char tok[LINESIZE];
	int i,ch;

	/* system menu code will reset signal stuff */
	oldhand = Signal(SIGINT,(SIGNALHANDLER)sags_esc);
	oldintr = (char *)ioctl(0,TIOCINTRCHAR,(long)"\003\033");

	if (setjmp(sags_jb)) {
		ioctl(0,TIOCINTRCHAR,(long)oldintr);
		Signal(SIGINT,oldhand);
		return(EAGAIN);
	}

	ch = curitem = curitems = 0;
	seldev = -1;

	bzero(list,sizeof(list));

	for (i=0; i < NDEVICES; i++) {
		devflags[i] = 0;
#if defined(SN0)
		if (kl_find_type(devices[i].type, NULL) == 0) {
#else
		if (find_type(GetChild(NULL),devices[i].type) == 0) {
#endif
			devflags[i] |= SA_INVALID;
		}
		else if ((i==DEV_CDROM || (i == DEV_TAPE && !ch)))
			/* default to local cd, or tape if present */
			ch = '1' + i;
	}

	/*  If tapedevice is set, show that promptly, and cancel possible
	 * local device default.
	 */
	if (tapedevice) {
		appendComponentList(list,(COMPONENT *)(__psint_t)1);
		curitem = ch = 0;
		curitems =1;
	}

	for (;;) {
		if ((ch == '\n') || (ch == '\r')) {
			int iscdrom;

			if (list[curitem] == 0) {
				printf("\nno source selected.\n");
				goto printlist;
			}

			if ((long)list[curitem] == 1) {
				strcpy(source,tapedevice);
				needsa = iscdrom = 0;
				tapedevset = 1;
			}
			else {
#ifndef SN0
				strcpy(source,fixupname(list[curitem]));
#else
				strcpy(source,kl_fixupname(list[curitem]));
#endif
				iscdrom=(list[curitem]->Type==CDROMController);
				tapedevset = 0;

				if (devices[seldev].postop) {
					printf("\n\n");
					if (!(*devices[seldev].postop)(
						source,list[curitem],0))
						goto printlist;
				}

			}

			bzero(list,sizeof(list));

			if (needsa)
				strcat(source,"/sa");

			setenv("tapedevice",source);

			if (iscdrom)
				strcat(source, SASHNAME);
			else
				strcat(source,"(" SASHNAME ")");

			printf("\n\n");

			ioctl(0,TIOCINTRCHAR,(long)oldintr);
			Signal(SIGINT,oldhand);

			return(0);
		}
		else if ((ch >= '1') && (ch <= (char)('0'+NDEVICES))) {
			int newseldev = ch - '1';


			if (devflags[newseldev] & SA_INVALID) {
				printf("\n%s %s not available.\n",
				       devices[newseldev].place,
				       devices[newseldev].name);
				goto printlist;
			}

			seldev = newseldev;

			bzero(list,sizeof(list));
			needsa = 0;		/* reset */

			if (devices[seldev].preop) {
				printf("\n\n");
				i = (*devices[seldev].preop)(0,seldev);
				bzero(list,sizeof(list));
				if (i == 0) {
					seldev = -1;
					goto printlist;
				}
			}

			instsrc = seldev;
			ndevs = devcnt[seldev];

#if defined(SN0)
			kl_showlist((void *)NULL, devices[seldev].type,
				 (void *)list,0);
#else
			showlist(GetChild(NULL),devices[seldev].type,
				 (struct radioList *)list,0);
#endif

			if (tapedevice)
				appendComponentList(list,
						    (COMPONENT *)(__psint_t)1);

			for (curitems=0; list[curitems] != 0; curitems++) ;
			curitem = 0;
		}
		else if ((ch >= 'a') && (ch <= 'z')) {
			int sel = ch - 'a';

			if (sel < curitems && curitems)
				curitem = sel;
			else {
				printf("\ndevice '%c' not available\n",ch);
				goto printlist;
			}
		}
		else if (ch)
			printf("\ninvalid input: '%c'\n",ch);

		/* print list */
printlist:
		printf("\n\n");
		for (i=0; i < NDEVICES; i++) {
			if (devflags[i] & SA_INVALID)
				printf("X");
			else
				printf("%d",i+1);
			printf( (i == seldev) ? ")[%s %s]  " : ") %s %s  ",
				devices[i].place, devices[i].name);
		}
		printf("\n");
		for (i=0; list[i] != 0; i++ ) {
			char buf[100];
			char *name;
			
			printf("      %c%c) ",
			       (i == curitem) ? '*' : ' ','a'+i);
			if ((long)list[i] != 1) {
#ifdef SN0
				sn0_dev2eng(list[i],buf);
#else
				dev2eng(list[i],buf);
#endif
				name = buf;
			}
			else
				name = tapedevice;
			printf("%s\n",name);
		}
		printf("\nEnter 1-%c to select source type, ",
		       '0' + NDEVICES);
		if (i != 0) {
			printf((i==1)?"a":"a-%c",'a'+i-1);
			printf(" to select the source, ");
		}
		printf("<esc> to quit,\nor <enter> to start: ");

		gets(buf);
		if (buf[0] == '\0')
			ch = '\n';
		else {
			token(buf,tok);		/* skip blanks */
			ch = tok[0];
		}
	}
	/*NOTREACHED*/
	return 0;
}


static void
showlist(COMPONENT *c, CONFIGTYPE type, struct radioList *list, int gui)
{
	char buf[100];

	if (c == NULL)
		return;

	if (c->Type == type) {
		if (gui) {
			struct _radioButton *b;
			dev2eng(c,buf);
			b = appendRadioList(list,buf,0);
			if (b) b->userdata = (void *)c;
		}
		else {
			appendComponentList((COMPONENT **)list,c);
		}
	}

	showlist(GetChild(c), type, list, gui);
	showlist(GetPeer(c), type, list, gui);

	return;
}

static void
appendComponentList(COMPONENT **list, COMPONENT *c)
{
	int i;

	for (i=0 ; i < MAXLIST; i++)
		if (list[i] == 0) {
			list[i] = c;
			break;
		}
}

static int
menu_boot(char *path, char *msg, char *a1, char *a2)
{
    struct string_list newargv;
    char *errstring = "No memory for argument %d (%s)\n";

    init_str(&newargv);
    if (new_str1(path, &newargv)) {
	printf(errstring, 0, path);
	return(ENOMEM);
    }
    if (a1 && new_str1(a1, &newargv)) {
	printf(errstring, 1, a1);
	return(ENOMEM);
    }
    if (a2 && new_str1(a2, &newargv)) {
	printf(errstring, 2, a2);
	return(ENOMEM);
    }

    if (msg && isGuiMode()) {
	    struct Dialog *d;
	    char buf[128];

	    /* popup booting message since it can take a while... */
	    if (instsrc == DEV_REMTAPE || instsrc == DEV_REMDIR)
		sprintf(buf,"Obtaining %s from %s:%s",msg,
				getenv("netinsthost"),getenv("netinstfile"));
	    else
		sprintf(buf,"Obtaining %s",msg);
		
	    d = _popupDialog(buf,0,DIALOGPROGRESS,DIALOGCENTER);
	    drawObject(guiobj(d));
    }

#if 0
    {
	int i ;

	printf("pmenu_cmd: Execute %s, %d\n", path, newargv.strcnt) ;
	printf("-------------\n") ;
	for(i=0; newargv.strptrs[i]; i++)
		printf("%s\n", newargv.strptrs[i]) ;
	printf("-------------\n") ;
	printenv_cmd(1, NULL, NULL, NULL) ;
    }
#endif

    return((int)Execute ((CHAR *)path, (LONG)newargv.strcnt,
	    (CHAR **)newargv.strptrs, (CHAR **)environ));
}

static void
returnmsg(void)
{
    extern int m_graphics;

    /* XXX - when GUI is complete graphics case is un-needed */
    if (m_graphics)
	    p_printf("Press \001 Esc \006 to return to the menu.\n\n");
    else
	    p_printf("Press <Esc> to return to the menu.\n\n");
}
