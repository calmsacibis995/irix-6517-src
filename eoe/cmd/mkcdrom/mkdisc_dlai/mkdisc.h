/*
 * mkdisc.h
 *
 * Interface for making WORM's using a Sony CDW-E1 or CDW900E encoding
 * unit
 */

int
mkdisc(char *dev, char *image, int max_retries, int data_size, int verbose,
	int pretend_flag, char *cuesheet_file, int audio_flag);
