/*
 * model.c - Lifecycle helpers for the Book record.
 */

#include <stdlib.h>
#include <string.h>

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
  book->title = strdup("");
  book->genres = strdup("");
  book->author = strdup("");
  book->review = strdup("");
  book->id = 0;
  book->rating = 0;
  book->length = 0;
  book->completion = 0;
  return book;
}
