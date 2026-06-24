#include <sqlite3.h>
#include <stdio.h>

#include "../include/database.h"
#include "../include/tui.h"

#define EMPTY '.'
#define SOLID '@'
#define QUIT 'q'
#define STATE_PAUSED 0
#define STATE_PLAYING 1
#define STATE_QUIT 2

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	sqlite3* db;
	if (initDB(&db) != 0) {
		fprintf(stderr, "Failed to initialise database.\n");
		return 1;
	}
	test_function(db);
	closeDB(db);
	return 0;
}
