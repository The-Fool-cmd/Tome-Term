#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../include/database.h"
#include "../include/model.h"
#include "../include/tui.h"

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
  keypad(stdscr, TRUE);
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
  if (maxCols <= BORDER_CROP) {
    return 1;
  }
  mvwprintw(list, startRow, BORDER_CROP, "%s", book->title);

  mvwprintw(list, startRow + 1, BORDER_CROP, "genres: %s; rating: %d",
            book->genres, book->rating);

  char dashes[maxCols - BORDER_CROP];
  memset(dashes, '-', maxCols - BORDER_CROP - 1);
  dashes[maxCols - BORDER_CROP - 1] = '\0';
  mvwprintw(list, startRow + 2, BORDER_CROP, "%s", dashes);

  return 0;
}

void highlightList(WINDOW *win, int prev_idx, int curr_idx, int rows_per_item) {
  int inner_width = getmaxx(win) - 2;
  if (prev_idx >= 0) {
    int prev_start = prev_idx * rows_per_item + 1;
    for (int r = 0; r < rows_per_item; r++) {
      mvwchgat(win, prev_start + r, 1, inner_width, A_NORMAL, 0, NULL);
    }
  }
  if (curr_idx >= 0) {
    int curr_start = curr_idx * rows_per_item + 1;
    for (int r = 0; r < rows_per_item; r++) {
      mvwchgat(win, curr_start + r, 1, inner_width, A_REVERSE, 0, NULL);
    }
  }
}

/* This generates the entire list of books and places them in the pane */
WINDOW *listPane(int lengthX, int minHeight, Book **bookList, int listLength) {
  int pane_width = (int)lengthX * LIST_OCCUPATION;
  int content_height = listLength * ROWS_PER_BOOK + BORDER_CROP;
  int pane_height = content_height + minHeight;
  WINDOW *pane = newpad(pane_height, pane_width);
  for (int i = 0; i < listLength; i++) {
    int result = listElement(pane, 3 * i + 1, pane_width, bookList[i]);
    if (result != 0) {
      return NULL;
    }
  }
  box(pane, 0, 0);
  return pane;
}

int displayContents(WINDOW *win, int rows, int cols, Book *book) {
  mvwprintw(win, 0, 1, "Title: %s", book->title);
  mvwprintw(win, 1, 1, "Author: %s", book->author);
  mvwprintw(win, 2, 1, "Genres: %s", book->genres);
  mvwprintw(win, 3, 1, "Length: %d", book->length);
  mvwprintw(win, 4, 1, "Rating: %d/10", book->rating);
  mvwprintw(win, 5, 1, "Completion: %d%%", book->completion);

  int row = 6;
  int col = 1;
  mvwprintw(win, row, col, "Review: ");
  col += 8;

  int prev_row = row;
  int prev_end_col = col;

  char *copy = strdup(book->review);
  if (copy == NULL) {
    return 0;
  }
  char *word = strtok(copy, " \t\n");
  while (word != NULL) {
    int word_len = (int)strlen(word);
    if (col > 1 && col + word_len - 1 > cols) {
      row++;
      col = 1;
      if (row > rows) {
        int ellipsis_col = prev_end_col - 3;
        if (ellipsis_col < 1)
          ellipsis_col = 1;
        mvwprintw(win, prev_row, ellipsis_col, "...");
        free(copy);
        return 1;
      }
    }
    prev_row = row;
    prev_end_col = col + word_len;
    mvwprintw(win, row, col, "%s ", word);
    col += word_len + 1;
    word = strtok(NULL, " \t\n");
  }
  free(copy);
  return 0;
}

WINDOW *displayWin(int lengthX, int winHeight, Book *book) {
  int winWidth = (int)lengthX * (1 - LIST_OCCUPATION);
  WINDOW *win = newwin(winHeight, winWidth, 0, (int)lengthX * LIST_OCCUPATION);
  box(win, 0, 0);
  displayContents(win, winHeight - BORDER_CROP, winWidth - BORDER_CROP, book);
  return win;
}

void updateDisplayWin(WINDOW *win, int rows, int cols, Book *book) {
  wclear(win);
  box(win, 0, 0);
  displayContents(win, rows - BORDER_CROP, cols - BORDER_CROP, book);
  wrefresh(win);
  return;
}

void listLoop(sqlite3 *db) {
  int maxX = 0, maxY = 0;
  initTerminal(COLOR_WHITE, COLOR_BLACK);
  refresh();
  maxY = getmaxy(stdscr);
  maxX = getmaxx(stdscr);
  if (maxX < 64) {
    killTerminal();
    printf("Terminal not wide enough, please zoom out!\n");
    return;
  }
  if (maxY < 32) {
    killTerminal();
    printf("Terminal not tall enough, please zoom out!\n");
    return;
  }
  /*
  char *genres1 = "Sci-Fi, Adventure";
  char *genres2 = "Dystopian, Political";

  Book b1 = {.title = "Dune",
             .rating = 5,
             .genres = genres1,
             .author = "Frank Herbert",
             .completion = 1,
             .length = 412,
             .review =
                 "A classic. TESTING DEBUGGING DEBUGGING DEBUGGING DEBUGGING"};
  Book b2 = {.title = "1984",
             .rating = 4,
             .genres = genres2,
             .author = "George Orwell",
             .completion = 1,
             .length = 328,
             .review = "Chilling. TESTING DEBUGGING DEBUGGING TESTING DEBUGGING
  DEBUGGING"}; insertDB(db, &b1); insertDB(db, &b2);
  */
  int listLength = 0;
  Book **bookPtrs = readDB(db, &listLength);

  WINDOW *leftPane = listPane(maxX, maxY, bookPtrs, listLength);
  if (leftPane == NULL) {
    killTerminal();
    printf("An error occured when creating the listPane. Did you change "
           "ROWS_PER_BOOK?\n");
    return;
  }
  int prev_idx = -1;
  int curr_idx = 0;
  highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
  prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
           maxX * LIST_OCCUPATION + BORDER_CROP);
  WINDOW *rightPane = displayWin(maxX, maxY, bookPtrs[curr_idx]);
  wrefresh(rightPane);
  int input = getch();
  while (input != QUIT) {
    input = getch();
    // NEED TO UPDATE THE DISPLAY WINDOW AS WELL
    switch (input) {
    case (KEY_UP): {
      if (curr_idx == 0) {
        break;
      }
      prev_idx = curr_idx;
      curr_idx--;
      highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
      prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
               maxX * LIST_OCCUPATION + BORDER_CROP);
      updateDisplayWin(rightPane, maxX, maxY, bookPtrs[curr_idx]);
      break;
    }
    case (KEY_DOWN): {
      if (curr_idx == listLength - 1) {
        break;
      }
      prev_idx = curr_idx;
      curr_idx++;
      highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
      prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
               maxX * LIST_OCCUPATION + BORDER_CROP);
      updateDisplayWin(rightPane, maxX, maxY, bookPtrs[curr_idx]);
      break;
    }
    }
  };
  killTerminal();
  printf("%d\n", input);
  (void)db;
}
