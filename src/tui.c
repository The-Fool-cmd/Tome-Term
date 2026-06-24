#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "database.h"
#include "model.h"
#include "tui.h"

#define QUIT 'q'
#define ROWS_PER_BOOK 3
#define LIST_OCCUPATION 0.666f
#define STATE_QUIT 1
#define STATE_DEFAULT 2
#define BORDER_CROP 2

/* Updates the state given using provided input */
int updateState(int8_t *state, char *input) {
  switch (*input) {
  case (QUIT): {
    *state = STATE_QUIT;
    break;
  }
  }
  return 0;
}

int initTerminal(int fg_color, int bg_color) {
  initscr();
  noecho();
  cbreak();    // disables line buffering
  curs_set(0); // invisible cursor
  // keypad(stdscr, TRUE);
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

/* This places a book listing inside of the pane starting from a given row
 * Currently assumes ROWS_PER_BOOK is 3
 */
int listElement(WINDOW *list, int startRow, int maxCols, Book *book) {
  if (ROWS_PER_BOOK != 3) {
    return 1;
  }
  mvwprintw(list, startRow, BORDER_CROP, "%s", book->title);

  // 2 genres if possible, 1 otherwise
  mvwprintw(list, startRow + 1, BORDER_CROP, "genres: %s; rating: %d",
            book->genres, book->rating);

  char dashes[maxCols - BORDER_CROP];
  memset(dashes, '-', maxCols - BORDER_CROP - 1);
  dashes[maxCols - BORDER_CROP - 1] = '\0';
  mvwprintw(list, startRow + 2, BORDER_CROP, "%s", dashes);

  return 0;
}

/* This generates the entire list of books and places them in the pane */
WINDOW *listPane(int lengthX, Book *bookList, int listLength) {
  int pane_width = (int)lengthX * LIST_OCCUPATION;
  int pane_height = listLength * ROWS_PER_BOOK + BORDER_CROP;
  WINDOW *pane = newpad(pane_height, pane_width);
  for (int i = 0; i < listLength; i++) {
    int result = listElement(pane, 3 * i + 1, pane_width, &bookList[i]);
    if (result != 0) {
      return NULL;
    }
  }
  box(pane, 0, 0);
  return pane;
}

void test_function(sqlite3* db) {
  int maxX = 0, maxY = 0;
  initTerminal(COLOR_WHITE, COLOR_BLACK);
  refresh();
  maxY = getmaxy(stdscr);
  maxX = getmaxx(stdscr);
  if (maxX < 64) {
    killTerminal();
    printf("Terminal too small, please zoom out!\n");
    return;
  }
  char *genres1 = "Sci-Fi, Adventure";
  char *genres2 = "Dystopian, Political";

  Book bookList[] = {{.title = "Dune",
                      .rating = 5,
                      .genres = genres1,
                      .author = "Frank Herbert",
                      .completion = 1,
                      .length = 412,
                      .review = "A classic."},
                     {.title = "1984",
                      .rating = 4,
                      .genres = genres2,
                      .author = "George Orwell",
                      .completion = 1,
                      .length = 328,
                      .review = "Chilling."}};
  int listLength = 2;

  WINDOW *pane = listPane(maxX, bookList, listLength);
  if (pane == NULL) {
    killTerminal();
    printf("An error occured when creating the listPane. Did you change "
           "ROWS_PER_BOOK?\n");
  }
  prefresh(pane, 0, 0, 0, 0, maxY, maxX * LIST_OCCUPATION + BORDER_CROP);
  int input = getch();
  while (input != QUIT) {
    input = getch();
  };
  killTerminal();
  printf("%d\n", input);
  (void)db;
}
