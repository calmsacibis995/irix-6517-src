# include "fx.h"
# include "dklabel.h"


extern char *fx_version;	/* from vers.c */

int _gotsgilab;
int verbose_sgiinfo;
char nofloplablmsg[] = "no sgiinfo for floppy disks\n";

void
show_sgiinfo_func(void)
{
register char *g;

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(nofloplablmsg);
		return;
	}
#endif /* SMFD_NUMBER */
    if (!_gotsgilab){
	verbose_sgiinfo = 0;
	readin_sgiinfo_func();
	_gotsgilab = 1;
    }
    g = "   %6s = %-24s";
    setoff("sgi-info");
    printf(g, "serial", LP(&sg)->d_serial);
	if(strlen(LP(&sg)->d_serial) > 24)
		newline();
    printf(g, "name", LP(&sg)->d_name);
	newline();
	if(LP(&sg)->d_magic2 == D_MAGIC2)
		printf("   Created by \"%s\"\n", LP(&sg)->d_fxvers);
}


void
set_sgiinfo_func(void)
{
    STRBUF s1, s2;
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(nofloplablmsg);
		return;
	}
#endif /* SMFD_NUMBER */
    strncpy(s1.c, LP(&sg)->d_serial, sizeof LP(&sg)->d_serial);
    s1.c[sizeof LP(&sg)->d_serial] = '\0';
    argstring(s1.c, s1.c, "serial");
    strncpy(s2.c, LP(&sg)->d_name, sizeof LP(&sg)->d_name);
    s2.c[sizeof LP(&sg)->d_name] = '\0';
    argstring(s2.c, s2.c, "name");
    strncpy(LP(&sg)->d_serial, s1.c, sizeof LP(&sg)->d_serial);
    strncpy(LP(&sg)->d_name, s2.c, sizeof LP(&sg)->d_name);
	strncpy(LP(&sg)->d_fxvers, fx_version, sizeof(LP(&sg)->d_fxvers)-1);
	LP(&sg)->d_magic2 = D_MAGIC2;
    changed = 1;
    _gotsgilab = 1;
    show_sgiinfo_func();
}


void
create_sgiinfo_func(void)
{
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(nofloplablmsg);
		return;
	}
#endif
	if(drivernum == SCSI_NUMBER) {
		CBLOCK b;
		scsiset_label(&b, 0);
    	_gotsgilab = 1;
		changed = 1;
		bcopy(LP(&b)->d_serial, LP(&sg)->d_serial, sizeof LP(&b)->d_serial);
		bcopy(LP(&b)->d_name, LP(&sg)->d_name, sizeof LP(&b)->d_name);
		strncpy(LP(&sg)->d_fxvers, fx_version, sizeof(LP(&sg)->d_fxvers)-1);
		LP(&sg)->d_magic2 = D_MAGIC2;
	}
	else
	{
		printf("create sgiinfo same as readin sgiinfo\n");
		verbose_sgiinfo = 0;
		readin_sgiinfo_func();
	}
}

/* note that this is NOT a menu item for SCSI, it is only called from
 * show_sgiinfo_func.
*/
void
readin_sgiinfo_func(void)
{
    CBLOCK b;
	if(verbose_sgiinfo)
		printf("...reading in sgiinfo\n");
    if( readsgilabel(&b, 1) < 0 )
	return;
    bcopy(LP(&b)->d_serial, LP(&sg)->d_serial, sizeof LP(&b)->d_serial);
    bcopy(LP(&b)->d_name, LP(&sg)->d_name, sizeof LP(&b)->d_name);
}

char *sgi_name = "sgilabel";

/* return -1 (errwarn value) if can't find/read, -2 if magic wrong,
	0 if read and magic OK */
readsgilabel(CBLOCK *bp, int complain)
{
    register struct volume_directory *vd;
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(nofloplablmsg);
		return 0;
	}
#endif
    if( (vd = findent(sgi_name, 0)) == 0 || vd->vd_nbytes == 0 )
		return -1;	/* don't complain if not found.  Nothing needs
			it, and new sash autosetup mode for 3rd party disks
			won't create it; 4/97 */
    if( gread(vd->vd_lbn, (char *)bp, 1) < 0 )
	return errwarn("can't read sgilabel");
    if( LP(bp)->d_magic != D_MAGIC ) {
	if(complain)	/* sometimes we will print an errmsg in caller
		after this fails, and it's annoying to see 2 or 3
		errors for the same thing... */
		errwarn("invalid sgilabel, ignored");
	return -2;
    }
    return 0;
}

void
update_sgi(void)
{
    update_dt(sgi_name, &sg, sizeof *LP(&sg));
}
