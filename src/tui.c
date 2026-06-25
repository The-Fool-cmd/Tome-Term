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

int initTerminal(int fg_color, int bg_color) {
  initscr();
  noecho();
  cbreak();    // disables line buffering
  curs_set(0); // invisible cursor
  keypad(stdscr, TRUE);

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

static const int FIELD_ROWS[] = {1, 2, 3, 4, 5, 6};
#define REVIEW_ROW 7

int displayContents(WINDOW *win, Book *book) {
  int cols = getmaxx(win) - BORDER_CROP;
  int rows = getmaxy(win) - BORDER_CROP;
  mvwprintw(win, FIELD_ROWS[0], 1, "Title: %.*s",  cols - 7,  book->title);
  mvwprintw(win, FIELD_ROWS[1], 1, "Author: %.*s", cols - 8,  book->author);
  mvwprintw(win, FIELD_ROWS[2], 1, "Genres: %.*s", cols - 8,  book->genres);
  mvwprintw(win, FIELD_ROWS[3], 1, "Length: %d", book->length);
  mvwprintw(win, FIELD_ROWS[4], 1, "Rating: %d/10", book->rating);
  mvwprintw(win, FIELD_ROWS[5], 1, "Completion: %d%%", book->completion);

  int row = REVIEW_ROW;
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
    if (col > 1 && col + word_len > cols) {
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
  int winWidth = (int)(lengthX * (1 - LIST_OCCUPATION));
  WINDOW *win = newwin(winHeight, winWidth, 0, (int)(lengthX * LIST_OCCUPATION));
  box(win, 0, 0);
  displayContents(win, book);
  return win;
}

void updateDisplayWin(WINDOW *win, Book *book) {
  wclear(win);
  box(win, 0, 0);
  displayContents(win, book);
  wrefresh(win);
}

static WINDOW *popup_new(int h, int w);
static int popup_confirm(const char *prompt);
static char *popup_input(const char *field_name, const char *current);

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
  if (maxY < 24) {
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
  if (listLength == 0) {
    prefresh(leftPane, 0, 0, 0, 0, maxY - 1,
             (int)(maxX * LIST_OCCUPATION) + BORDER_CROP);
    while (getch() != QUIT)
      ;
    delwin(leftPane);
    free(bookPtrs);
    killTerminal();
    return;
  }
  int prev_idx = -1;
  int curr_idx = 0;
  highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
  prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
           maxX * LIST_OCCUPATION + BORDER_CROP);
  WINDOW *rightPane = displayWin(maxX, maxY, bookPtrs[curr_idx]);
  wrefresh(rightPane);
  int running = 1;
  int input = getch();
  while (running) {
    switch (input) {
    case QUIT: {
      if (popup_confirm("Are you sure?")) {
        running = 0;
      } else {
        touchwin(leftPane);
        prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
                 (int)(maxX * LIST_OCCUPATION) + BORDER_CROP);
        touchwin(rightPane);
        wrefresh(rightPane);
      }
      break;
    }
    case (KEY_UP): {
      if (curr_idx == 0) break;
      prev_idx = curr_idx;
      curr_idx--;
      highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
      prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
               (int)(maxX * LIST_OCCUPATION) + BORDER_CROP);
      updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      break;
    }
    case (KEY_DOWN): {
      if (curr_idx == listLength - 1) break;
      prev_idx = curr_idx;
      curr_idx++;
      highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
      prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
               (int)(maxX * LIST_OCCUPATION) + BORDER_CROP);
      updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      break;
    }
    case KEY_RIGHT:
    case '\n': case '\r': case KEY_ENTER: {
      detailsLoop(rightPane, db, bookPtrs[curr_idx], leftPane, curr_idx);
      touchwin(leftPane);
      prefresh(leftPane, curr_idx * ROWS_PER_BOOK, 0, 0, 0, maxY - 1,
               (int)(maxX * LIST_OCCUPATION) + BORDER_CROP);
      break;
    }
    }
    if (running) input = getch();
  }
  for (int i = 0; i < listLength; i++)
    freeBook(bookPtrs[i]);
  free(bookPtrs);
  delwin(leftPane);
  delwin(rightPane);
  killTerminal();
}
static WINDOW *popup_new(int h, int w) {
  int maxY = getmaxy(stdscr);
  int maxX = getmaxx(stdscr);
  WINDOW *pop = newwin(h, w, (maxY - h) / 2, (maxX - w) / 2);
  keypad(pop, TRUE);
  box(pop, 0, 0);
  return pop;
}

static int popup_confirm(const char *prompt) {
  int h = 7, w = 44;
  WINDOW *pop = popup_new(h, w);
  mvwprintw(pop, 2, (w - (int)strlen(prompt)) / 2, "%s", prompt);
  mvwprintw(pop, 4, w / 2 - 9, "[ Yes ]");
  mvwprintw(pop, 4, w / 2 + 2, "[ No  ]");
  int sel = 0;
  mvwchgat(pop, 4, w / 2 + 2, 7, A_REVERSE, 0, NULL);
  wrefresh(pop);
  int input;
  while ((input = wgetch(pop)) != '\n' && input != '\r' && input != KEY_ENTER) {
    if (input == 27) {
      sel = 0;
      break;
    }
    if (input == KEY_LEFT || input == KEY_RIGHT) {
      sel = !sel;
      mvwchgat(pop, 4, w / 2 - 9, 7, sel ? A_REVERSE : A_NORMAL, 0, NULL);
      mvwchgat(pop, 4, w / 2 + 2, 7, sel ? A_NORMAL : A_REVERSE, 0, NULL);
      wrefresh(pop);
    }
  }
  delwin(pop);
  return sel;
}

/* Returns new heap string (caller frees) or NULL on ESC. */
static char *popup_input(const char *field_name, const char *current) {
  int h = 7, w = 70;
  int inner_w = w - 2;
  WINDOW *pop = popup_new(h, w);
  mvwprintw(pop, 1, 1, "Edit %s:", field_name);
  mvwprintw(pop, 5, 1, "Enter: confirm   ESC: cancel");

  char buf[1024];
  strncpy(buf, current ? current : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  int len = (int)strlen(buf);

  curs_set(1);
  int input;
  do {
    int disp_start = len > inner_w ? len - inner_w : 0;
    mvwprintw(pop, 3, 1, "%-*s", inner_w, buf + disp_start);
    wmove(pop, 3, 1 + (len > inner_w ? inner_w : len));
    wrefresh(pop);
    input = wgetch(pop);
    if (input == 27) {
      curs_set(0);
      delwin(pop);
      return NULL;
    }
    if ((input == KEY_BACKSPACE || input == 127) && len > 0) {
      buf[--len] = '\0';
    } else if (input >= 32 && input < 127 && len < (int)sizeof(buf) - 1) {
      buf[len++] = (char)input;
      buf[len] = '\0';
    }
  } while (input != '\n' && input != '\r' && input != KEY_ENTER);
  curs_set(0);
  delwin(pop);
  return strdup(buf);
}

static void highlightDetails(WINDOW *win, int prev_field, int curr_field) {
  int inner_width = getmaxx(win) - 2;
  int max_row = getmaxy(win) - BORDER_CROP;

  if (prev_field >= 0) {
    if (prev_field < 6)
      mvwchgat(win, FIELD_ROWS[prev_field], 1, inner_width, A_NORMAL, 0, NULL);
    else
      for (int r = REVIEW_ROW; r <= max_row; r++)
        mvwchgat(win, r, 1, inner_width, A_NORMAL, 0, NULL);
  }
  if (curr_field >= 0) {
    if (curr_field < 6)
      mvwchgat(win, FIELD_ROWS[curr_field], 1, inner_width, A_REVERSE, 0, NULL);
    else
      for (int r = REVIEW_ROW; r <= max_row; r++)
        mvwchgat(win, r, 1, inner_width, A_REVERSE, 0, NULL);
  }
}

void detailsLoop(WINDOW *win, sqlite3 *db, Book *book, WINDOW *list_pane, int list_idx) {
  static const char *field_names[] = {
      "title", "author", "genres", "length", "rating", "completion", "review"};
  int curr_field = 0;
  int num_fields = 7;

  highlightDetails(win, -1, curr_field);
  wrefresh(win);

  int input = getch();
  while (input != KEY_LEFT) {
    switch (input) {
    case KEY_UP:
      if (curr_field > 0) {
        highlightDetails(win, curr_field, curr_field - 1);
        curr_field--;
        wrefresh(win);
      }
      break;
    case KEY_DOWN:
      if (curr_field < num_fields - 1) {
        highlightDetails(win, curr_field, curr_field + 1);
        curr_field++;
        wrefresh(win);
      }
      break;
    case KEY_RIGHT:
    case '\n':
    case '\r':
    case KEY_ENTER: {
      char int_buf[32];
      const char *cur;
      switch (curr_field) {
      case 0:
        cur = book->title;
        break;
      case 1:
        cur = book->author;
        break;
      case 2:
        cur = book->genres;
        break;
      case 3:
        snprintf(int_buf, sizeof(int_buf), "%d", book->length);
        cur = int_buf;
        break;
      case 4:
        snprintf(int_buf, sizeof(int_buf), "%d", book->rating);
        cur = int_buf;
        break;
      case 5:
        snprintf(int_buf, sizeof(int_buf), "%d", book->completion);
        cur = int_buf;
        break;
      default:
        cur = book->review;
        break;
      }
      char *new_val = popup_input(field_names[curr_field], cur);
      if (new_val != NULL) {
        switch (curr_field) {
        case 0: free(book->title);  book->title  = new_val; break;
        case 1: free(book->author); book->author = new_val; break;
        case 2: free(book->genres); book->genres = new_val; break;
        case 3: book->length     = atoi(new_val); free(new_val); break;
        case 4: book->rating     = atoi(new_val); free(new_val); break;
        case 5: book->completion = atoi(new_val); free(new_val); break;
        default: free(book->review); book->review = new_val; break;
        }
        updateDB(db, book);
        listElement(list_pane, list_idx * ROWS_PER_BOOK + 1, getmaxx(list_pane), book);
        highlightList(list_pane, -1, list_idx, ROWS_PER_BOOK);
      }
      updateDisplayWin(win, book);
      highlightDetails(win, -1, curr_field);
      wrefresh(win);
      touchwin(list_pane);
      prefresh(list_pane, list_idx * ROWS_PER_BOOK, 0, 0, 0, getmaxy(stdscr) - 1,
               (int)(getmaxx(stdscr) * LIST_OCCUPATION) + BORDER_CROP);
      break;
    }
    }
    input = getch();
  }
  highlightDetails(win, curr_field, -1);
  wrefresh(win);
}
