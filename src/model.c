#include <stdlib.h>

#include "model.h"

void freeBook(Book* book) {
	free(book->title);
	free(book->genres);
	free(book->author);
	free(book->review);
	free(book);
	return;
}
