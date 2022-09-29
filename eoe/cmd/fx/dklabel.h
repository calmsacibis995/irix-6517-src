/* Representing the sgilabel on each SGI disk. */
#ident "fx/dklabel.h:	$Revision: 1.24 $"

#include "sys/scsi.h"

/*	For historical reasons, the stuff in disk_label below is still kept
	on the disk, but is now called the "sgilabel". It is no longer in
	any fixed place, but is just one of the things located via the
	volume directory'.

	Only a few of the disk_label fields in this struct
	significant, and they are only for human being's convenience.
	No code needs to them to talk to the drive, as the old drive
	types that used some of it are no longer supported.
*/

/* sgilabel structure. NOTE this is sgilabel, NOT the volume header. */

struct	disk_label {
	u_int	d_magic;		/* magic cookie */
	u_char d_unused1[90];
	char	d_name[50];		/* name of drive */
	char	d_serial[50];		/* serial # of drive */
	u_char d_unused2[84];
	u_int	d_magic2;		/* magic cookie if it has the fx version */
	char	d_fxvers[64];	/* fx version that created the sgilabel */
};

# define LP(x)		((struct disk_label *)(x))

/* d_magic value */
#define	D_MAGIC	0x072959
#define	D_MAGIC2	0x121053	/* has fx version in it */
