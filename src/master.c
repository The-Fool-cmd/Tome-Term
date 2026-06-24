#include <sqlite3.h>

#include "database.h"
#include "tui.h"

#define EMPTY '.'
#define SOLID '@'
#define QUIT 'q'
#define STATE_PAUSED 0
#define STATE_PLAYING 1
#define STATE_QUIT 2

int main(int argc, char **argv) {
	sqlite3* db;
	initDB(&db);
	test_function(db);
	return 0;
}
