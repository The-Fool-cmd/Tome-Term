#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>

int updateTerminal(char **terminalMatrix, int maxX, int maxY) {
  refresh();
  return 0;
}

int initTerminal(int fg_color, int bg_color) {
  initscr();
  noecho();
  cbreak();    // disables line buffering
  curs_set(0); // invisible cursor
  nodelay(stdscr, TRUE);

  if (has_colors() == false) {
    endwin();
    printf("Your terminal doesn't support colours.\n");
    return 1;
  }
  // color handling
  start_color();
  init_pair(1, fg_color, bg_color);
  attron(COLOR_PAIR(1));
  return 0;
}

int killTerminal() {
  attroff(COLOR_PAIR(1));
  endwin();
  return 0;
}
