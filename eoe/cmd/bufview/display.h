/* constants needed for display.c */

/* "type" argument for new_message function */

#define  MT_standout  1
#define  MT_delayed   2

void reset_display(void);
char *display_order(void);
char *display_flags(uint64_t, uint64_t, char *);
