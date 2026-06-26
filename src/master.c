/*
 * master.c - Program entry point. Opens the database, hands control to the
 * TUI event loop, and tears the database down on exit.
 */

#include <sqlite3.h>
#include <stdio.h>

#include "../include/database.h"
#include "../include/tui.h"

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	sqlite3* db;
	if (initDB(&db) != 0) {
		fprintf(stderr, "Failed to initialise database.\n");
		return 1;
	}
	listLoop(db);
	closeDB(db);
	return 0;
}
