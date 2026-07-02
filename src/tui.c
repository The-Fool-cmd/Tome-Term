/*
 * tui.c - ncurses front end.
 *
 * Layout: a scrolling list of books on the left (an ncurses pad, so it can be
 * taller than the screen), a details view of the selected book on the right,
 * and a one-row status bar pinned to the bottom of stdscr. listLoop drives the
 * top-level event loop; detailsLoop runs the per-book editor; the popup_*
 * helpers implement the centred modal dialogs.
 *
 * Colour note: ncurses windows and pads do not inherit stdscr's attributes, so
 * every newwin/newpad here calls wbkgd(win, COLOR_PAIR(1)) to apply the theme,
 * and mvwchgat highlights must reference colour pair 1 to invert correctly.
 */

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/database.h"
#include "../include/model.h"
#include "../include/tui.h"

#define QUIT 'q'
#define FG_COLOR COLOR_YELLOW
#define BG_COLOR COLOR_BLACK
#define ROWS_PER_BOOK 3        /* title, metadata, separator */
#define LIST_OCCUPATION 0.666f /* fraction of the width given to the list pane */
#define BORDER_CROP 2          /* cells consumed by a window's box() border */
#define STATUS_BAR_H 1         /* rows reserved at the bottom for the status bar */
#define MIN_TERM_W 64
#define MIN_TERM_H 24

/* Status bar contents for each view (plain ASCII for portability). */
#define STATUSBAR_LIST    " ^/v navigate    enter/> details    n new    bksp delete    f filter    q quit"
#define STATUSBAR_DETAILS " ^/v field    enter/> edit    < back"

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

/* Draws one book's three rows (title, genres+rating, separator) into the list
 * pane starting at startRow. The layout is hard-wired to ROWS_PER_BOOK == 3, so
 * it bails out if that ever changes. Returns 0 on success, 1 on a layout or
 * width violation. */
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

  /* Separator rule spanning the pane's inner width. */
  char dashes[maxCols - BORDER_CROP];
  memset(dashes, '-', maxCols - BORDER_CROP - 1);
  dashes[maxCols - BORDER_CROP - 1] = '\0';
  mvwprintw(list, startRow + 2, BORDER_CROP, "%s", dashes);

  return 0;
}

/* Sets the reverse-video highlight on the currently selected list item and
 * clears it from the previously selected one. Indices map to item slots, not
 * rows; pass -1 to skip either operation. */

void highlightList(WINDOW *win, int prev_idx, int curr_idx, int rows_per_item) {
  int inner_width = getmaxx(win) - 2;
  if (prev_idx >= 0) {
    int prev_start = prev_idx * rows_per_item + 1;
    for (int r = 0; r < rows_per_item; r++) {
      mvwchgat(win, prev_start + r, 1, inner_width, A_NORMAL, 1, NULL);
    }
  }
  if (curr_idx >= 0) {
    int curr_start = curr_idx * rows_per_item + 1;
    for (int r = 0; r < rows_per_item; r++) {
      mvwchgat(win, curr_start + r, 1, inner_width, A_REVERSE, 1, NULL);
    }
  }
}

/* Builds the left-hand list as an ncurses pad sized to hold every book. The pad
 * is at least minHeight tall (so a short list still fills the viewport) plus
 * one ROWS_PER_BOOK block per book. lengthX is the full terminal width; the
 * pane takes LIST_OCCUPATION of it. Returns NULL if any item fails to draw. */
WINDOW *listPane(int lengthX, int minHeight, Book **bookList, int listLength) {
  int pane_width = (int)lengthX * LIST_OCCUPATION;
  int content_height = listLength * ROWS_PER_BOOK + BORDER_CROP;
  int pane_height = content_height + minHeight;
  WINDOW *pane = newpad(pane_height, pane_width);
  wbkgd(pane, COLOR_PAIR(1));
  for (int i = 0; i < listLength; i++) {
    int result = listElement(pane, 3 * i + 1, pane_width, bookList[i]);
    if (result != 0) {
      return NULL;
    }
  }
  box(pane, 0, 0);
  return pane;
}

/* Single source of truth for the details-pane layout, shared by displayContents
 * and highlightDetails. FIELD_ROWS gives the window row of each single-line
 * field; the review is the last field and wraps from REVIEW_ROW downward. */
static const int FIELD_ROWS[] = {1, 2, 3, 4, 5, 6};
#define REVIEW_ROW 7
#define REVIEW_FIELD 6

/* Renders a book's fields into the (already boxed) details window. Single-line
 * fields are truncated to the inner width with %.*s; the review is word-wrapped
 * and gets a trailing "..." if it overflows the pane. Tolerates a NULL book
 * (draws nothing) so the empty-list state can reuse the same window. Returns 1
 * if the review was truncated, 0 otherwise. */
int displayContents(WINDOW *win, Book *book) {
  if (book == NULL) {
    return 0;
  }
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

/* Creates the right-hand details window, occupying the width left over by the
 * list pane, and draws book into it. */
WINDOW *displayWin(int lengthX, int winHeight, Book *book) {
  int winWidth = (int)(lengthX * (1 - LIST_OCCUPATION));
  WINDOW *win = newwin(winHeight, winWidth, 0, (int)(lengthX * LIST_OCCUPATION));
  wbkgd(win, COLOR_PAIR(1));
  box(win, 0, 0);
  displayContents(win, book);
  return win;
}

/* Redraws an existing details window with a (possibly different) book. */
void updateDisplayWin(WINDOW *win, Book *book) {
  wclear(win);
  box(win, 0, 0);
  displayContents(win, book);
  wrefresh(win);
}

/* Paints msg onto the reserved bottom row of stdscr. */
static void drawStatusBar(const char *msg) {
  int maxX = getmaxx(stdscr);
  int maxY = getmaxy(stdscr);
  mvhline(maxY - 1, 0, ' ', maxX);
  mvprintw(maxY - 1, 1, "%s", msg);
  refresh();
}

/* Blits the list pad to the screen, scrolled so the item at idx is in view.
 * The viewport spans from the top of stdscr to just above the status bar. */
static void refreshLeft(WINDOW *pane, int idx) {
  int maxX = getmaxx(stdscr);
  int pane_rows = getmaxy(stdscr) - STATUS_BAR_H;
  prefresh(pane, idx * ROWS_PER_BOOK, 0, 0, 0, pane_rows - 1,
           (int)(maxX * LIST_OCCUPATION) + BORDER_CROP);
}

/* Repaints both panes and the status bar after a popup is dismissed. */
static void restorePanes(WINDOW *left, WINDOW *right, int idx) {
  touchwin(left);
  refreshLeft(left, idx);
  touchwin(right);
  wrefresh(right);
  drawStatusBar(STATUSBAR_LIST);
}

/* Frees the old list and reloads every book. On a DB error it yields a valid
 * empty list rather than leaving the caller with a NULL to dereference. */
static void reloadAll(sqlite3 *db, Book ***bookPtrs, int *listLength) {
  for (int i = 0; i < *listLength; i++) {
    freeBook((*bookPtrs)[i]);
  }
  free(*bookPtrs);
  *bookPtrs = readDB(db, listLength);
  if (*bookPtrs == NULL) {
    *listLength = 0;
    *bookPtrs = (Book **)calloc(1, sizeof(Book *));
  }
}

/* Inserts a blank book, then reloads so the new row carries its assigned id,
 * and rebuilds the list pane around the refreshed data. The caller selects and
 * opens the new (last) entry for editing. */
static void newBookLoop(sqlite3 *db, Book ***bookPtrs, int *listLength, WINDOW **leftPane) {
  Book *book = createBook();
  insertDB(db, book);
  freeBook(book);
  reloadAll(db, bookPtrs, listLength);
  delwin(*leftPane);
  *leftPane = listPane(getmaxx(stdscr), getmaxy(stdscr) - STATUS_BAR_H, *bookPtrs, *listLength);
}

/* Modal dialog and filter helpers, defined after listLoop. */
static WINDOW *popup_new(int h, int w);
static int popup_confirm(const char *prompt);
static char *popup_input(const char *field_name, const char *current);
static int popup_menu(const char **options, int n, const char *title);
static int filterLoop(sqlite3 *db, Book ***bookPtrs, int *listLength, WINDOW **leftPane);

/* Top-level event loop. Initialises ncurses, builds the list and details
 * panes, then dispatches keys: arrows navigate, enter/right opens the details
 * editor, n adds a book, backspace deletes one, f filters, q quits (with
 * confirmation). Owns and frees every Book and window on exit. */
void listLoop(sqlite3 *db) {
  int maxX = 0, maxY = 0;
  if (initTerminal(FG_COLOR, BG_COLOR) != 0) {
    return;
  }
  refresh();
  maxY = getmaxy(stdscr);
  maxX = getmaxx(stdscr);
  if (maxX < MIN_TERM_W) {
    killTerminal();
    printf("Terminal not wide enough, please zoom out!\n");
    return;
  }
  if (maxY < MIN_TERM_H) {
    killTerminal();
    printf("Terminal not tall enough, please zoom out!\n");
    return;
  }
  int listLength = 0;
  Book **bookPtrs = readDB(db, &listLength);
  if (bookPtrs == NULL) {
    killTerminal();
    printf("Failed to read the database.\n");
    return;
  }
  int pane_rows = maxY - STATUS_BAR_H;

  WINDOW *leftPane = listPane(maxX, pane_rows, bookPtrs, listLength);
  if (leftPane == NULL) {
    killTerminal();
    printf("An error occurred when creating the listPane. Did you change "
           "ROWS_PER_BOOK?\n");
    return;
  }
  int prev_idx = -1;
  int curr_idx = 0;
  if (listLength > 0)
    highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
  refreshLeft(leftPane, listLength > 0 ? curr_idx : 0);
  WINDOW *rightPane = displayWin(maxX, pane_rows, listLength > 0 ? bookPtrs[curr_idx] : NULL);
  wrefresh(rightPane);
  drawStatusBar(STATUSBAR_LIST);
  int running = 1;
  int input = getch();
  while (running) {
    switch (input) {
    case QUIT: {
      if (popup_confirm("Are you sure?")) {
        running = 0;
      } else {
        restorePanes(leftPane, rightPane, curr_idx);
      }
      break;
    }
    case KEY_RESIZE: {
      maxY = getmaxy(stdscr);
      maxX = getmaxx(stdscr);
      pane_rows = maxY - STATUS_BAR_H;
      clear();
      refresh();
      delwin(leftPane);
      leftPane = listPane(maxX, pane_rows, bookPtrs, listLength);
      delwin(rightPane);
      rightPane = displayWin(maxX, pane_rows,
                             listLength > 0 ? bookPtrs[curr_idx] : NULL);
      if (listLength > 0)
        highlightList(leftPane, -1, curr_idx, ROWS_PER_BOOK);
      refreshLeft(leftPane, listLength > 0 ? curr_idx : 0);
      wrefresh(rightPane);
      drawStatusBar(STATUSBAR_LIST);
      break;
    }
    case (KEY_UP): {
      if (curr_idx == 0) break;
      prev_idx = curr_idx;
      curr_idx--;
      highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
      refreshLeft(leftPane, curr_idx);
      updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      break;
    }
    case (KEY_DOWN): {
      if (listLength == 0 || curr_idx == listLength - 1) break;
      prev_idx = curr_idx;
      curr_idx++;
      highlightList(leftPane, prev_idx, curr_idx, ROWS_PER_BOOK);
      refreshLeft(leftPane, curr_idx);
      updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      break;
    }
    case KEY_RIGHT:
    case '\n': case '\r': case KEY_ENTER: {
      if (listLength == 0) break;
      detailsLoop(rightPane, db, bookPtrs[curr_idx], leftPane, curr_idx);
      touchwin(leftPane);
      refreshLeft(leftPane, curr_idx);
      drawStatusBar(STATUSBAR_LIST);
      break;
    }
    case KEY_BACKSPACE: {
      if (listLength == 0) break;
      if (!popup_confirm("Delete this book?")) {
        restorePanes(leftPane, rightPane, curr_idx);
        break;
      }
      removeDB(db, bookPtrs[curr_idx]->id);
      reloadAll(db, &bookPtrs, &listLength);
      delwin(leftPane);
      leftPane = listPane(maxX, pane_rows, bookPtrs, listLength);
      if (listLength == 0) {
        refreshLeft(leftPane, 0);
        wclear(rightPane);
        box(rightPane, 0, 0);
        wrefresh(rightPane);
        drawStatusBar(STATUSBAR_LIST);
        break;
      }
      if (curr_idx >= listLength) curr_idx = listLength - 1;
      prev_idx = -1;
      highlightList(leftPane, -1, curr_idx, ROWS_PER_BOOK);
      refreshLeft(leftPane, curr_idx);
      updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      drawStatusBar(STATUSBAR_LIST);
      break;
    }
    case 'n': {
      newBookLoop(db, &bookPtrs, &listLength, &leftPane);
      if (listLength == 0) {
        refreshLeft(leftPane, 0);
        wclear(rightPane);
        box(rightPane, 0, 0);
        wrefresh(rightPane);
        drawStatusBar(STATUSBAR_LIST);
        break;
      }
      curr_idx = listLength - 1;
      prev_idx = -1;
      highlightList(leftPane, -1, curr_idx, ROWS_PER_BOOK);
      refreshLeft(leftPane, curr_idx);
      updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      detailsLoop(rightPane, db, bookPtrs[curr_idx], leftPane, curr_idx);
      touchwin(leftPane);
      refreshLeft(leftPane, curr_idx);
      drawStatusBar(STATUSBAR_LIST);
      break;
    }
    case 'f': {
      if (filterLoop(db, &bookPtrs, &listLength, &leftPane))
        curr_idx = prev_idx = -1;
      if (listLength > 0) {
        if (curr_idx < 0) curr_idx = 0;
        highlightList(leftPane, -1, curr_idx, ROWS_PER_BOOK);
        refreshLeft(leftPane, curr_idx);
        updateDisplayWin(rightPane, bookPtrs[curr_idx]);
      } else {
        refreshLeft(leftPane, 0);
        wclear(rightPane);
        box(rightPane, 0, 0);
        wrefresh(rightPane);
      }
      drawStatusBar(STATUSBAR_LIST);
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

/* Creates a boxed, centred modal window of the given size with keypad input
 * enabled. Caller draws into it and delwin()s it when done. */
static WINDOW *popup_new(int h, int w) {
  int maxY = getmaxy(stdscr);
  int maxX = getmaxx(stdscr);
  WINDOW *pop = newwin(h, w, (maxY - h) / 2, (maxX - w) / 2);
  wbkgd(pop, COLOR_PAIR(1));
  keypad(pop, TRUE);
  box(pop, 0, 0);
  return pop;
}

/* Yes/No confirmation dialog. Left/right toggle the selection (default No);
 * enter accepts, ESC cancels. Returns 1 for Yes, 0 for No/cancel. */
static int popup_confirm(const char *prompt) {
  int h = 7, w = 44;
  WINDOW *pop = popup_new(h, w);
  mvwprintw(pop, 2, (w - (int)strlen(prompt)) / 2, "%s", prompt);
  mvwprintw(pop, 4, w / 2 - 9, "[ Yes ]");
  mvwprintw(pop, 4, w / 2 + 2, "[ No  ]");
  int sel = 0;
  mvwchgat(pop, 4, w / 2 + 2, 7, A_REVERSE, 1, NULL);
  wrefresh(pop);
  int input;
  while ((input = wgetch(pop)) != '\n' && input != '\r' && input != KEY_ENTER) {
    if (input == 27) {
      sel = 0;
      break;
    }
    if (input == KEY_LEFT || input == KEY_RIGHT) {
      sel = !sel;
      mvwchgat(pop, 4, w / 2 - 9, 7, sel ? A_REVERSE : A_NORMAL, 1, NULL);
      mvwchgat(pop, 4, w / 2 + 2, 7, sel ? A_NORMAL : A_REVERSE, 1, NULL);
      wrefresh(pop);
    }
  }
  delwin(pop);
  return sel;
}

/* Single-line text editor dialog, pre-filled with current. Supports printable
 * input, backspace, and horizontal scrolling when the text exceeds the field.
 * Returns a newly allocated string (caller frees) on enter, or NULL on ESC. */
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

/* Vertical selection menu over n options. Up/down move the highlight, enter
 * selects. Returns the chosen index, or -1 on ESC. */
static int popup_menu(const char **options, int n, const char *title) {
  int h = n + 4, w = 36;
  WINDOW *pop = popup_new(h, w);
  mvwprintw(pop, 1, (w - (int)strlen(title)) / 2, "%s", title);
  for (int i = 0; i < n; i++)
    mvwprintw(pop, i + 3, 2, "%s", options[i]);
  int sel = 0;
  mvwchgat(pop, sel + 3, 1, w - 2, A_REVERSE, 1, NULL);
  wrefresh(pop);
  int input = wgetch(pop);
  while (input != 27) {
    if (input == '\n' || input == '\r' || input == KEY_ENTER) {
      delwin(pop);
      return sel;
    }
    if (input == KEY_UP && sel > 0) {
      mvwchgat(pop, sel + 3, 1, w - 2, A_NORMAL, 1, NULL);
      mvwchgat(pop, --sel + 3, 1, w - 2, A_REVERSE, 1, NULL);
      wrefresh(pop);
    } else if (input == KEY_DOWN && sel < n - 1) {
      mvwchgat(pop, sel + 3, 1, w - 2, A_NORMAL, 1, NULL);
      mvwchgat(pop, ++sel + 3, 1, w - 2, A_REVERSE, 1, NULL);
      wrefresh(pop);
    }
    input = wgetch(pop);
  }
  delwin(pop);
  return -1;
}

/* Presents the filter menu and, on a choice, prompts for a value and reloads
 * the book list through it (or reads all books for "Clear filter"). Replaces
 * the caller's book array, length, and list pane in place on success. Returns
 * 1 if the list was changed, 0 if the user backed out. ESC on the value
 * prompt returns to the menu; ESC on the menu exits. The labels/types order
 * must stay aligned: indices 0-2 map to types[], index 3 is "Clear filter". */
static int filterLoop(sqlite3 *db, Book ***bookPtrs, int *listLength, WINDOW **leftPane) {
  static const char *labels[] = {"Genre", "Rating (min)", "Length (min)", "Clear filter"};
  static const enum filterType types[] = {FILTER_GENRE, FILTER_RATING, FILTER_LENGTH};
  while (1) {
    int sel = popup_menu(labels, 4, "Filter");
    if (sel == -1) return 0;
    char *val = NULL;
    if (sel < 3) {
      val = popup_input(labels[sel], "");
      if (val == NULL) continue; /* ESC on value prompt -> back to menu */
    }
    int new_length = 0;
    Book **new_books = (sel == 3)
      ? readDB(db, &new_length)
      : filterDB(db, &new_length, types[sel], val);
    free(val);
    if (new_books != NULL) {
      for (int i = 0; i < *listLength; i++) freeBook((*bookPtrs)[i]);
      free(*bookPtrs);
      *bookPtrs = new_books;
      *listLength = new_length;
      delwin(*leftPane);
      *leftPane = listPane(getmaxx(stdscr), getmaxy(stdscr) - STATUS_BAR_H,
                           *bookPtrs, *listLength);
    }
    return 1;
  }
}

/* Moves the reverse-video highlight in the details pane from prev_field to
 * curr_field. Single-line fields highlight one row; the review field highlights
 * every row from REVIEW_ROW down. Pass -1 to skip either field. */
static void highlightDetails(WINDOW *win, int prev_field, int curr_field) {
  int inner_width = getmaxx(win) - 2;
  int max_row = getmaxy(win) - BORDER_CROP;

  if (prev_field >= 0) {
    if (prev_field < REVIEW_FIELD)
      mvwchgat(win, FIELD_ROWS[prev_field], 1, inner_width, A_NORMAL, 1, NULL);
    else
      for (int r = REVIEW_ROW; r <= max_row; r++)
        mvwchgat(win, r, 1, inner_width, A_NORMAL, 1, NULL);
  }
  if (curr_field >= 0) {
    if (curr_field < REVIEW_FIELD)
      mvwchgat(win, FIELD_ROWS[curr_field], 1, inner_width, A_REVERSE, 1, NULL);
    else
      for (int r = REVIEW_ROW; r <= max_row; r++)
        mvwchgat(win, r, 1, inner_width, A_REVERSE, 1, NULL);
  }
}

/* Describes every editable field in the details pane. Order must match the
 * rows drawn by displayContents (title, author, genres, length, rating,
 * completion, review). offset locates the member inside Book; for integer
 * fields min/max clamp user input. */
typedef struct {
  const char *name;
  int is_int;
  size_t offset;
  int min;
  int max;
} FieldDesc;

static const FieldDesc FIELDS[] = {
    {"title",      0, offsetof(Book, title),      0, 0},
    {"author",     0, offsetof(Book, author),     0, 0},
    {"genres",     0, offsetof(Book, genres),     0, 0},
    {"length",     1, offsetof(Book, length),     0, 100000},
    {"rating",     1, offsetof(Book, rating),     0, 10},
    {"completion", 1, offsetof(Book, completion), 0, 100},
    {"review",     0, offsetof(Book, review),     0, 0},
};
#define NUM_FIELDS ((int)(sizeof(FIELDS) / sizeof(FIELDS[0])))

void detailsLoop(WINDOW *win, sqlite3 *db, Book *book, WINDOW *list_pane, int list_idx) {
  int curr_field = 0;

  drawStatusBar(STATUSBAR_DETAILS);
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
      if (curr_field < NUM_FIELDS - 1) {
        highlightDetails(win, curr_field, curr_field + 1);
        curr_field++;
        wrefresh(win);
      }
      break;
    case KEY_RIGHT:
    case '\n':
    case '\r':
    case KEY_ENTER: {
      /* Locate the selected member inside book via its table offset, then read
       * its current value as text to seed the editor. */
      const FieldDesc *fd = &FIELDS[curr_field];
      void *slot = (char *)book + fd->offset;
      char int_buf[32];
      const char *cur;
      if (fd->is_int) {
        snprintf(int_buf, sizeof(int_buf), "%d", *(int *)slot);
        cur = int_buf;
      } else {
        cur = *(char **)slot;
      }
      char *new_val = popup_input(fd->name, cur);
      if (new_val != NULL) {
        if (fd->is_int) {
          int v = atoi(new_val);
          if (v < fd->min) v = fd->min;
          if (v > fd->max) v = fd->max;
          *(int *)slot = v;
          free(new_val);
        } else {
          /* String field takes ownership of the editor's allocation. */
          free(*(char **)slot);
          *(char **)slot = new_val;
        }
        updateDB(db, book);
        listElement(list_pane, list_idx * ROWS_PER_BOOK + 1, getmaxx(list_pane), book);
        highlightList(list_pane, -1, list_idx, ROWS_PER_BOOK);
      }
      updateDisplayWin(win, book);
      highlightDetails(win, -1, curr_field);
      wrefresh(win);
      touchwin(list_pane);
      refreshLeft(list_pane, list_idx);
      break;
    }
    }
    input = getch();
  }
  highlightDetails(win, curr_field, -1);
  wrefresh(win);
}
