#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <sqlite3.h>
#include "model.h"

int initTerminal(int fg_color, int bg_color);
int killTerminal();
void highlightList(WINDOW *win, int prev_idx, int curr_idx, int rows_per_item);
void detailsLoop(WINDOW *win, sqlite3 *db, Book *book, WINDOW *list_pane, int list_idx);
void listLoop(sqlite3 *db);

#endif
