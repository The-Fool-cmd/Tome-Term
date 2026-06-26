#ifndef TUI_H
#define TUI_H

/*
 * tui.h - ncurses front end.
 *
 * listLoop is the entry point and owns the whole screen lifecycle: it
 * initialises ncurses, runs the main event loop, and restores the terminal on
 * exit. The remaining declarations are exposed mainly for cross-referencing
 * between the list and details views.
 */

#include <ncurses.h>
#include <sqlite3.h>
#include "model.h"

/* Initialises ncurses with the given foreground/background colours. Returns 0
 * on success, non-zero if the terminal lacks colour support. */
int initTerminal(int fg_color, int bg_color);

/* Restores the terminal to its pre-initTerminal state. */
int killTerminal();

/* Repaints a list item's highlight: clears prev_idx's rows and reverses
 * curr_idx's. Pass -1 for either index to skip it. */
void highlightList(WINDOW *win, int prev_idx, int curr_idx, int rows_per_item);

/* Runs the details view for one book until the user navigates back (left).
 * Edits are written through to the database and reflected in list_pane (the
 * list item at list_idx) so the list stays in sync. */
void detailsLoop(WINDOW *win, sqlite3 *db, Book *book, WINDOW *list_pane, int list_idx);

/* Main event loop: builds the panes, dispatches navigation/edit/filter/new/
 * delete actions, and runs until the user quits. */
void listLoop(sqlite3 *db);

#endif
