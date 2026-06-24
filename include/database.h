#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "model.h"

enum filterType {
  FILTER_GENRE,
  FILTER_RATING,
  FILTER_LENGTH,
};

int create_tableDB(sqlite3 **db);
int initDB(sqlite3 **db);
int closeDB(sqlite3 *database);
int insertDB(sqlite3 *database, Book *book);
int updateDB(sqlite3 *database, Book *book);
int removeDB(sqlite3 *database, int id);
Book **readDB(sqlite3 *database, int *listLength);
Book **filterDB(sqlite3 *database, int *listLength, enum filterType filter, char *filter_value);

#endif
