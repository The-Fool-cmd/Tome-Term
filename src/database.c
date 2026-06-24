#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/database.h"

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

static char *col_strdup(sqlite3_stmt *stmt, int col) {
  const char *text = (const char *)sqlite3_column_text(stmt, col);
  return text ? strdup(text) : NULL;
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

  Book **bookList = (Book **)calloc(*listLength, sizeof(Book *));
  if (bookList == NULL) {
    return NULL;
  }

  sqlite3_stmt *list_statement;
  result = sqlite3_prepare_v2(database, "SELECT * FROM books", -1,
                              &list_statement, NULL);
  if (result != SQLITE_OK) {
    free(bookList);
    return NULL;
  }

  int i = 0;
  while (sqlite3_step(list_statement) == SQLITE_ROW) {
    bookList[i] = (Book *)calloc(1, sizeof(Book));
    bookList[i]->title  = col_strdup(list_statement, 1);
    bookList[i]->author = col_strdup(list_statement, 2);
    bookList[i]->length = sqlite3_column_int(list_statement, 3);
    bookList[i]->completion = sqlite3_column_int(list_statement, 4);
    bookList[i]->rating = sqlite3_column_int(list_statement, 5);
    bookList[i]->review = col_strdup(list_statement, 6);
    bookList[i]->genres = col_strdup(list_statement, 7);
    i++;
  }
  result = sqlite3_finalize(list_statement);
  if (result != SQLITE_OK || i != *listLength) {
    for (int j = 0; j < i; j++) {
      freeBook(bookList[j]);
    }
    free(bookList);
    return NULL;
  }
  return bookList;
}
