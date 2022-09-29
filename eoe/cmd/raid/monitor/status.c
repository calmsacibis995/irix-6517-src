#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/usraid_admin.h>

int raid_fd;	/* open RAID device */

status_open(char *path)
{
	if ((raid_fd = open(path, O_RDONLY)) < 0) {
		if (chdir("/dev") < 0) {
			perror("/dev");
			exit(1);
		}
		if ((raid_fd = open(path, O_RDONLY)) < 0) {
			if (chdir("/dev/rdsk") < 0) {
				perror("/dev/rdsk");
				exit(1);
			}
			if ((raid_fd = open(path, O_RDONLY)) < 0) {
				perror(path);
				exit(1);
			}
		}
	}
}

int
status_check()
{
	struct usraid_units_down downs;
	int found, i;

	if (ioctl(raid_fd, USRAID_RET_DOWN, &downs) < 0) {
		perror("return units down");
		exit(1);
	}
	for (found = i = 0; i < 5; i++) {
		if (downs.drive[i]) {
			found++;
		}
	}
	return(found);
}

status_close()
{
	close(raid_fd);
}
