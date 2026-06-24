#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "model.h"

int create_tableDB(sqlite3 **db);
int initDB(sqlite3 **db);
int closeDB(sqlite3 *database);
int insertDB(sqlite3 *database, Book *book);
Book **readDB(sqlite3 *database, int *listLength);

#endif
