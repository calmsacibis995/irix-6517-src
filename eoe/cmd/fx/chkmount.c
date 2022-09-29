#include <mountinfo.h>
#include <stdio.h>

/* chkmounts()
 *
 * Check to see if the drive being opened has mounted filesystems.
 * Done even if not in expert mode, since you can re-partition
 * the drive even in 'naive' mode. 
 * Check to see if we are part of a mounted logical volume also.
 *
 * ARGUMENTS:
 *  special - pathname to chr device of vol slice on disk to check
 *
 * RETURN VALUE:
 *  0 - no mounted partitions on disk
 *  1 - mounted partitions exist on device
 */

chkmounts(char *special)
{
	mnt_check_state_t *check_state;
	int ret;
	extern int do_overlap_checks;

	if (!do_overlap_checks)
		return 0;

	if (mnt_check_init(&check_state) == -1)
		return 0;

	ret = mnt_find_mounted_partitions(check_state, special);
	if (ret > 0) {
		puts("");
		mnt_causes_show(check_state, stdout, "fx");
		mnt_plist_show(check_state, stdout, "fx");
		puts("");
	}
	mnt_check_end(check_state);
	if (ret > 0)
		return 1;

	return 0;
}
