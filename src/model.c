#include <stdlib.h>

#include "../include/model.h"

void freeBook(Book *book) {
  free(book->title);
  free(book->genres);
  free(book->author);
  free(book->review);
  free(book);
  return;
}

Book *createBook() {
  Book *book = (Book *)calloc(1, sizeof(Book));
  book->title = NULL;
  book->genres = NULL;
  book->author = NULL;
  book->review = NULL;
  book->id = 0;
  book->rating = 0;
  book->length = 0;
  book->completion = 0;
  return book;
}
