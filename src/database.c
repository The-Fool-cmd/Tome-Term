/*
 * database.c - SQLite persistence layer.
 *
 * Each write follows the prepare -> bind -> step -> finalize lifecycle. The
 * read paths first run a COUNT(*) to size the result array, then a SELECT to
 * populate it; collect_books owns the SELECT statement and finalizes it.
 */

#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/database.h"

/* Upper bound on a LIKE pattern built for genre filtering. */
#define MAX_PATTERN_LENGTH 256

int create_tableDB(sqlite3 **db) {
  char *errmsg;
  int result = sqlite3_exec(
      *db,
      "CREATE TABLE IF NOT EXISTS books ( id INTEGER PRIMARY KEY, "
      "title TEXT, author TEXT, length INTEGER, completion INTEGER, "
      "rating INTEGER, review TEXT, genres TEXT )",
      NULL, NULL, &errmsg);
  if (result != SQLITE_OK) {
    sqlite3_free(errmsg);
    return 1;
  }
  sqlite3_free(errmsg);
  return 0;
}

int initDB(sqlite3 **db) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    return 1;
  }

  char path[PATH_MAX];
  snprintf(path, PATH_MAX, "%s/Tome", home);
  if (mkdir(path, 0755) == -1) {
    if (errno != EEXIST) {
      return 1;
    }
  }

  char dbPath[PATH_MAX + 16];
  snprintf(dbPath, sizeof(dbPath), "%s/tome.db", path);
  int result = sqlite3_open(dbPath, db);
  if (result != SQLITE_OK) {
    return 1;
  }

  if (create_tableDB(db) != 0) {
    return 1;
  }

  return 0;
}

int closeDB(sqlite3 *database) {
  if (sqlite3_close(database) != SQLITE_OK) {
    return 1;
  }
  return 0;
}

/* prepare -> bind -> step -> finalize */
int insertDB(sqlite3 *database, Book *book) {
  sqlite3_stmt *statement;
  int result = sqlite3_prepare_v2(
      database,
      "INSERT INTO books (title, author, length, completion, "
      "rating, review, genres) VALUES (?, ?, ?, ?, ?, ?, ?)",
      -1, &statement, NULL);
  if (result != SQLITE_OK) {
    return 1;
  }
  sqlite3_bind_text(statement, 1, book->title, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 2, book->author, -1, SQLITE_STATIC);
  sqlite3_bind_int(statement, 3, book->length);
  sqlite3_bind_int(statement, 4, book->completion);
  sqlite3_bind_int(statement, 5, book->rating);
  sqlite3_bind_text(statement, 6, book->review, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 7, book->genres, -1, SQLITE_STATIC);

  result = sqlite3_step(statement);
  sqlite3_finalize(statement);
  if (result != SQLITE_DONE) {
    return 1;
  }
  return 0;
}

int updateDB(sqlite3 *database, Book *book) {
  sqlite3_stmt *statement;
  int result = sqlite3_prepare_v2(
      database,
      "UPDATE books SET title = ?, author = ?, length = ?, completion = ?, "
      "rating = ?, review = ?, genres = ? WHERE id = ?",
      -1, &statement, NULL);
  if (result != SQLITE_OK) {
    return 1;
  }
  sqlite3_bind_text(statement, 1, book->title, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 2, book->author, -1, SQLITE_STATIC);
  sqlite3_bind_int(statement, 3, book->length);
  sqlite3_bind_int(statement, 4, book->completion);
  sqlite3_bind_int(statement, 5, book->rating);
  sqlite3_bind_text(statement, 6, book->review, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 7, book->genres, -1, SQLITE_STATIC);
  sqlite3_bind_int(statement, 8, book->id);

  result = sqlite3_step(statement);
  sqlite3_finalize(statement);
  if (result != SQLITE_DONE) {
    return 1;
  }
  return 0;
}

int removeDB(sqlite3 *database, int id) {
  sqlite3_stmt *statement;
  int result = sqlite3_prepare_v2(database, "DELETE FROM books WHERE id = ?",
                                  -1, &statement, NULL);
  if (result != SQLITE_OK) {
    return 1;
  }
  sqlite3_bind_int(statement, 1, id);
  result = sqlite3_step(statement);
  sqlite3_finalize(statement);
  if (result != SQLITE_DONE) {
    return 1;
  }
  return 0;
}

/* Duplicates a text column into a freshly owned string (NULL if the column is
 * NULL), so a Book never aliases SQLite's internal row buffers. */
static char *col_strdup(sqlite3_stmt *stmt, int col) {
  const char *text = (const char *)sqlite3_column_text(stmt, col);
  return text ? strdup(text) : NULL;
}

/* Owns stmt: steps through every row, populates a Book** array, then finalizes.
 * Returns NULL and frees on any error. */
static Book **collect_books(sqlite3_stmt *stmt, int listLength) {
  Book **bookList = (Book **)calloc(listLength, sizeof(Book *));
  if (bookList == NULL) {
    sqlite3_finalize(stmt);
    return NULL;
  }
  int i = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    bookList[i] = (Book *)calloc(1, sizeof(Book));
    if (bookList[i] == NULL) {
      for (int j = 0; j < i; j++) {
        freeBook(bookList[j]);
      }
      free(bookList);
      sqlite3_finalize(stmt);
      return NULL;
    }
    bookList[i]->id         = sqlite3_column_int(stmt, 0);
    bookList[i]->title      = col_strdup(stmt, 1);
    bookList[i]->author     = col_strdup(stmt, 2);
    bookList[i]->length     = sqlite3_column_int(stmt, 3);
    bookList[i]->completion = sqlite3_column_int(stmt, 4);
    bookList[i]->rating     = sqlite3_column_int(stmt, 5);
    bookList[i]->review     = col_strdup(stmt, 6);
    bookList[i]->genres     = col_strdup(stmt, 7);
    i++;
  }
  int result = sqlite3_finalize(stmt);
  if (result != SQLITE_OK || i != listLength) {
    for (int j = 0; j < i; j++) {
      freeBook(bookList[j]);
    }
    free(bookList);
    return NULL;
  }
  return bookList;
}

Book **readDB(sqlite3 *database, int *listLength) {
  sqlite3_stmt *count_statement;
  int result = sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM books", -1,
                                  &count_statement, NULL);
  if (result != SQLITE_OK) {
    return NULL;
  }
  sqlite3_step(count_statement);
  *listLength = sqlite3_column_int(count_statement, 0);
  sqlite3_finalize(count_statement);

  if (*listLength == 0) {
    return (Book **)calloc(1, sizeof(Book *));
  }

  sqlite3_stmt *list_statement;
  result = sqlite3_prepare_v2(database, "SELECT * FROM books", -1,
                              &list_statement, NULL);
  if (result != SQLITE_OK) {
    return NULL;
  }
  return collect_books(list_statement, *listLength);
}

/* Binds the single placeholder of a filter query: a wrapped %value% pattern
 * for genre LIKE matching, or the integer threshold for rating/length. */
static void bind_filter(sqlite3_stmt *stmt, enum filterType filter,
                        char *filter_value) {
  char pattern[MAX_PATTERN_LENGTH];
  switch (filter) {
  case FILTER_GENRE:
    snprintf(pattern, sizeof(pattern), "%%%s%%", filter_value);
    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
    break;
  case FILTER_LENGTH:
  case FILTER_RATING:
    sqlite3_bind_int(stmt, 1, atoi(filter_value));
    break;
  }
}

Book **filterDB(sqlite3 *database, int *listLength, enum filterType filter,
                char *filter_value) {
  const char *count_sql;
  const char *select_sql;
  switch (filter) {
  case FILTER_LENGTH:
    count_sql  = "SELECT COUNT(*) FROM books WHERE length >= ?";
    select_sql = "SELECT * FROM books WHERE length >= ?";
    break;
  case FILTER_GENRE:
    count_sql  = "SELECT COUNT(*) FROM books WHERE genres LIKE ?";
    select_sql = "SELECT * FROM books WHERE genres LIKE ?";
    break;
  case FILTER_RATING:
    count_sql  = "SELECT COUNT(*) FROM books WHERE rating >= ?";
    select_sql = "SELECT * FROM books WHERE rating >= ?";
    break;
  default:
    return NULL;
  }

  sqlite3_stmt *count_statement;
  int result = sqlite3_prepare_v2(database, count_sql, -1, &count_statement, NULL);
  if (result != SQLITE_OK) {
    return NULL;
  }
  bind_filter(count_statement, filter, filter_value);
  sqlite3_step(count_statement);
  *listLength = sqlite3_column_int(count_statement, 0);
  sqlite3_finalize(count_statement);

  if (*listLength == 0) {
    return (Book **)calloc(1, sizeof(Book *));
  }

  sqlite3_stmt *list_statement;
  result = sqlite3_prepare_v2(database, select_sql, -1, &list_statement, NULL);
  if (result != SQLITE_OK) {
    return NULL;
  }
  bind_filter(list_statement, filter, filter_value);
  return collect_books(list_statement, *listLength);
}
