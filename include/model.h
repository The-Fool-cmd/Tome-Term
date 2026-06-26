#ifndef MODEL_H
#define MODEL_H

/*
 * model.h - The core Book record shared across every layer.
 *
 * A Book mirrors one row of the `books` table. String members are heap
 * allocated and owned by the Book; use createBook/freeBook to manage them
 * rather than allocating fields by hand.
 */

typedef struct {
	int id;          /* database primary key; 0 for a Book not yet inserted */
	char* title;
	char* genres;
	int length;      /* page count */
	char* author;
	int completion;  /* percent read, 0-100 */
	int rating;      /* 0-10 */
	char* review;
} Book;

/* Frees a Book and all of its owned string members. */
void freeBook(Book* book);

/* Allocates a zero-initialised Book whose string members are empty (not NULL)
 * strings, so callers can display or edit them without NULL checks. Returns a
 * heap Book the caller must release with freeBook. */
Book *createBook();

#endif
