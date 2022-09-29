#ifndef __TERM_H__
#define __TERM_H__

typedef struct term_state_s {
	void *win;		/* curses terminal */
	int x, y;		/* cursor position */
	int dumb;		/* dumb terminal */
} term_state;

int term_init(term_state *term, int dumb);
void term_destroy(term_state *term);

void term_update(term_state *term);

void term_clear(term_state *term);
int term_get_lines(term_state *term);
int term_get_columns(term_state *term);
void term_draw_line(term_state *term, int ln, const char *str);
void term_draw_bold_line(term_state *term, int ln, const char *str);
void term_getstr(term_state *term,
	int x, int y, const char *prompt, char *str, int len);
int term_getch(term_state *term);
void term_goto(term_state *term, int x, int y);
int term_getch_wait(term_state *term);
int term_draw_lines(term_state *term, int ln, const char **text);

#endif /* __TERM_H__ */
