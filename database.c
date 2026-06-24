#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_PATH_LENGTH 50

int create_tableDB(sqlite3 **db) {
  char *errmsg;
  int result = sqlite3_exec(*db,
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
  // try to create the directory in the user's home folder
  char path[MAX_PATH_LENGTH];
  snprintf(path, MAX_PATH_LENGTH, "%s/Tome_Bin", getenv("HOME"));
  if (mkdir(path, 0755) == -1) {
    if (errno != EEXIST) {
      return 1;
    }
  }
  // now we know the directory exists
  char dbPath[MAX_PATH_LENGTH + 8];
  snprintf(dbPath, MAX_PATH_LENGTH + 8, "%s/tome.db", path);
  int result = sqlite3_open(dbPath, db);
  if (result != SQLITE_OK) {
    return 1;
  }

  if (create_tableDB(db) != 0) {
  	return 1;
  }

  return 0;
}

int closeDB(sqlite3* database) {
	if (sqlite3_close(database) != SQLITE_OK) {
		return 1;
	}
	return 0;
}
