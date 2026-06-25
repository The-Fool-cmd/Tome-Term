#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <sqlite3.h>

int initTerminal(int fg_color, int bg_color);
int updateTerminal(char** terminalMatrix, int maxX, int maxY);
int killTerminal();
void highlightList(WINDOW *win, int prev_idx, int curr_idx, int rows_per_item);
void listLoop(sqlite3 *db);

#endif
