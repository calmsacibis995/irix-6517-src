extern int nopkg(void);
void	dm_init () {}

int	dmi () { return nopkg(); }

int	dm_data_event () { return nopkg(); }
int	dm_namesp_event () { return nopkg(); }
int

/*	The following stubs are for routines needed for the X/Open 
 *	version of DMAPI.
 */

dm_send_destroy_event()
{
	return nopkg();
}


int
dm_send_mount_event()
{
	return nopkg();
}


int
dm_send_namesp_event()
{
	return nopkg();
}


void
dm_send_unmount_event()
{
}


int
dm_vp_to_handle ()
{
	return nopkg();
}

