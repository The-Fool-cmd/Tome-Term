#ifndef MODEL_H
#define MODEL_H

typedef struct {
	char* title;
	char* genres;
	int length;
	char* author;
	int completion;
	int rating;
	char* review;
} Book;

void freeBook(Book* book);

#endif
