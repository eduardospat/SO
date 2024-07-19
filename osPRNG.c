/*
	osPRNG() - A pseudo-random library call
*/

#include "osPRNG.h"

#define MAX_LEN 20

/* We will be implimenting a circular linked list
which performs the random function. Start at the
beginning and if you read the end you start back at
the beginning. */

typedef struct linkedList linkedList, *linkedListPtr;
struct linkedList {
	int value;
	linkedListPtr next;
};

/* This function inits the osPRNG library.
It takes the memory address of the "head"
variable. This is actually our current 
variable, since we are implimenting a circular
linked list there is no need for a head. */

int  testChar(char input);
int  getInt(char* buf, FILE* infile);
void init_osPRNG(linkedList** head);


/* Here is our current position in the circular
linked list. We make this global static as we
treat it as the inited variable for this library
function. The library needs to init itself if 
this is NULL. */

static linkedListPtr current = NULL;

int osPRNG(void) {

	int random = -1;

	if( current == NULL )
		init_osPRNG(&current);

	if(current != NULL) {

		random = current->value;
		current = current->next;
	}

	if( random == -1) {
		fprintf(stderr,"There was an error in the osPRNG() function.\n");
		fprintf(stderr,"It's possible you did not put any numbers in the file \"random.txt\".\n");
		exit(1);
	}

return random;

}

void init_osPRNG(linkedList** head) {

	int  err = 0;
	FILE* infile;
	char buf[MAX_LEN];
	linkedListPtr next = NULL, current = NULL;

	infile = fopen("random.txt", "r");

	if( infile == NULL ){
		err = 1;
		fprintf(stderr,"Unable to open the file \"random.txt\".\nPlease maker sure it is in your directory and run again.\n");
	}

	/*Get an int from the file stream while 
	they are there to be read. When the function
	returns a non-zero value then it has reached
	end-of file and whatever is in the buffer is
	invalid.                                    */
	if(err == 0)
		err = getInt(buf, infile);

	if(err == 0) {
		current = malloc(sizeof(linkedList));
		if(current == NULL)
			err = 1;
		else {
			if(*head == NULL)
				*head = current;

			sscanf(buf, "%d", &current->value);
		}		
	}
	
	while( err == 0 && getInt(buf, infile) == 0) {

		current->next = malloc(sizeof(linkedList));
		if(current->next == NULL)
			err = 1;
		else {
			current = current->next;
			sscanf(buf, "%d", &current->value);
		}
	}

	if(err == 0)
		current->next = *head;

}

int testChar(char input) {

int accepted;

	switch(input) {
	
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			accepted = 1;
			break;
		default:
			accepted = 0;
			break;
	}

	return accepted;			

}

int getInt(char* buf, FILE* infile) {

	int count, err = 0;
	char mychar;

	/* Skip all characters until the first number and the
	   continue until the first non-number. */
	mychar = fgetc(infile);

	/* Take out all the chars that are not numerical      */
	while( testChar(mychar) == 0 && err == 0) {
		mychar = fgetc(infile);
		err = feof(infile);
	}

	/* The variable mychar is now the start of our number 
	start placing that in our buffer until there no
	longer is a number to stuff.                       */
	count = 0;
	while( testChar(mychar) && feof(infile) == 0){

		buf[count++] = mychar;
		mychar = fgetc(infile);

	}

//	buf[count] = NULL;
	buf[count] = '\0';

	return err;
}
