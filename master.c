#include "tui.h"
#include <ncurses.h>
#include <stdint.h>

/* Main input code for the entire program
 * Handles communication with the terminal via tui.c
 */

#define EMPTY '.'
#define SOLID '@'
#define QUIT 'q'
#define STATE_PAUSED 0
#define STATE_PLAYING 1
#define STATE_QUIT 2

/* Updates the state given using provided input */
int updateStateAndSpeed(int8_t *state, char *input) {
  switch (*input) {
  case (QUIT): {
    *state = STATE_QUIT;
    break;
  }
  }
  return 0;
}

int main(int argc, char **argv) {
  int8_t state = STATE_PAUSED;
  char input = ' ';

  initTerminal(COLOR_MAGENTA, COLOR_BLACK);

  /* MAIN SIMULATION LOOP */
  while (state != STATE_QUIT) {
    input = getch();

  }
  // Termination procedure
  killTerminal();
  return 0;
}
