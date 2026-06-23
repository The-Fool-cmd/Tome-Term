/* Handles initialisation of the ncurses TUI */
int initTerminal(int fg_color, int bg_color);
/* Updated the TUI using provided matrix and terminal dimensions */
int updateTerminal(char** terminalMatrix, int maxX, int maxY);
/* Cleans up ncurses TUI. Does NOT end the process */
int killTerminal();
