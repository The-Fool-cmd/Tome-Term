#ifndef DATABASE_H
#define DATABASE_H

/*
 * database.h - SQLite persistence layer for Book records.
 *
 * The database lives at $HOME/Tome/tome.db and is created on first run. Every
 * function returns 0 on success and non-zero on failure unless noted; the
 * read functions instead return an allocated array or NULL on error. All SQL
 * is parameterised, so caller-supplied strings are never interpolated.
 */

#include <sqlite3.h>
#include "model.h"

/* Filter dimensions supported by filterDB. */
enum filterType {
  FILTER_GENRE,   /* substring match against the genres column */
  FILTER_RATING,  /* rating >= value */
  FILTER_LENGTH,  /* length >= value */
};

/* Creates the `books` table if it does not already exist. Called by initDB. */
int create_tableDB(sqlite3 **db);

/* Opens (creating if needed) $HOME/Tome/tome.db and ensures the schema exists.
 * On success *db holds an open handle the caller must release with closeDB. */
int initDB(sqlite3 **db);

/* Closes a handle opened by initDB. */
int closeDB(sqlite3 *database);

/* Inserts book as a new row. The id member is ignored (assigned by SQLite). */
int insertDB(sqlite3 *database, Book *book);

/* Persists every field of book to the row identified by book->id. */
int updateDB(sqlite3 *database, Book *book);

/* Deletes the row with the given id. */
int removeDB(sqlite3 *database, int id);

/* Reads every book ordered by insertion. On success returns a heap array of
 * *listLength Book pointers (each owned by the caller; free with freeBook then
 * free the array) and sets *listLength. An empty table yields a valid length-0
 * array, not NULL; NULL is returned only on error. */
Book **readDB(sqlite3 *database, int *listLength);

/* Like readDB but returns only rows matching filter against filter_value.
 * Same ownership and empty-vs-NULL contract as readDB. */
Book **filterDB(sqlite3 *database, int *listLength, enum filterType filter, char *filter_value);

#endif
