#ifndef TUI_H
#define TUI_H

#include <sqlite3.h>

int initTerminal(int fg_color, int bg_color);
int updateTerminal(char** terminalMatrix, int maxX, int maxY);
int killTerminal();
void test_function(sqlite3 *db);

#endif
