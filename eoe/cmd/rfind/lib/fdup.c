#include <stdio.h>
#include <unistd.h>

/*
 * Creates a second file stream on a second file descriptor.
 * See fdopen(3) for the "type" argument.  Returns NULL on error.
 */

FILE *fdup (const FILE *oldfp, const char *type) {
	FILE *newfp;
	int oldfd, newfd;

	if (oldfp == NULL) return NULL;
	oldfd = fileno(oldfp);
	if (oldfd < 0) return NULL;
	if ((newfd = dup(oldfd)) < 0) return NULL;
	if ((newfp = fdopen (newfd, type)) == NULL) {
		close(newfd);
		return NULL;
	}
	return newfp;
}
