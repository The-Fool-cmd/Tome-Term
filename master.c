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

int main(int argc, char **argv) {
	test_function();
	return 0;
}
