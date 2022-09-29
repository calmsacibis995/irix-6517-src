#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include "term.h"

int term_init(term_state *term, int dumb)
{
	term->dumb = dumb;

	if (term->dumb)
		return 1;

	term->win = initscr();
	if (term->win == NULL)
		return 0;

	cbreak();
	noecho();
	nonl();
	intrflush(stdscr,FALSE);
	keypad(stdscr,TRUE);
	nodelay(term->win, TRUE);

	term->x = COLS - 1;
	term->y = LINES - 1;

	return 1;
}

/*ARGSUSED*/
void term_destroy(term_state *term)
{
	if (!term->dumb)
		endwin();
}

/*ARGSUSED*/
int term_get_lines(term_state *term)
{
	if (term->dumb)
		return 24;
	else
		return LINES;
}

/*ARGSUSED*/
int term_get_columns(term_state *term)
{
	if (term->dumb)
		return 80;
	else
		return COLS;
}

void term_draw_line(term_state *term, int ln, const char *str)
{
	if (term->dumb)
		printf("%s\n", str);
	else
		mvwprintw((WINDOW*) term->win, ln, 0, "%s", str);
}

void term_draw_bold_line(term_state *term, int ln, const char *str)
{
	if (term->dumb)
		printf("%s\n", str);
	else {
		standout();
			term_draw_line(term, ln, str);
		standend();
	}
}

void term_update(term_state *term)
{
	if (term->dumb) {
		printf("\n");
	}
	else {
		wmove((WINDOW*) term->win, term->y, term->x);
		wrefresh((WINDOW*) term->win);
	}
}

void term_clear(term_state *term)
{
	if (!term->dumb)
		werase((WINDOW*) term->win);
}

void term_getstr(term_state *term,
	int x, int y, const char *prompt, char *str, int len)
{
	if (term->dumb) {
		printf("%s: ", prompt);
		fflush(NULL);
		fgets(str, len, stdin);
	}
	else {
		standout();
			mvwprintw((WINDOW*) term->win, y, x, "%s: ", prompt);
		standend();
		wrefresh((WINDOW*) term->win);

		echo();
		nodelay((WINDOW*) term->win, FALSE);
		wgetnstr((WINDOW*) term->win, str, len);
		nodelay((WINDOW*) term->win, TRUE);
		noecho();
	}
}

int term_getch(term_state *term)
{
	int ch;

	if (term->dumb)
		return fgetc(stdin);
	else {
		ch = wgetch((WINDOW*) term->win);
		if (ch != ERR)
			return ch;
		else
			return 0;
	}
}

int term_getch_wait(term_state *term)
{
	int ch;

	if (term->dumb)
		ch = fgetc(stdin);
	else {
		nodelay((WINDOW*) term->win, FALSE);
		ch = wgetch((WINDOW*) term->win);
		nodelay((WINDOW*) term->win, TRUE);
	}

	return ch;
}

void term_goto(term_state *term, int x, int y)
{
	if (!term->dumb) {
		term->x = x;
		term->y = y;
		wmove((WINDOW*) term->win, term->y, term->x);
	}
}

int term_draw_lines(term_state *term, int line_start, const char **text)
{
  int line;

  for (line = line_start; *text != NULL; text++)
    term_draw_line(term, line++, *text);

  return line - line_start;
}
