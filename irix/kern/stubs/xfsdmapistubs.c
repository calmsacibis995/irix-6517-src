
/*	Stub routines needed for the X/open implementration of the DMAPI
 * 	spec.
 */

extern int nopkg(void);

int	xfs_dm_fcntl() { return nopkg(); }

int	xfs_dm_send_create_event() { return 0; }

int	xfs_dm_send_data_event() { return nopkg(); }
