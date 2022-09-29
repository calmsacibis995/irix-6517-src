/*
 * Structures passed back and forth.
 */
#define	TYPE_DRIVE	1	/* a drive */
#define	TYPE_SLOT	2	/* a storage slot */
#define	TYPE_ROBOT	3	/* a robotic arm */
#define	TYPE_PORT	4	/* an import/export hole */

int setup(char *devname);
void cleanup(void);
int printconfig(char *devname);
int devpositions(void);
int movemedia(int src, int srctype, int dst, int dsttype);
