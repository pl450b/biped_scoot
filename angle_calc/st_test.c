#include <stdio.h>
#include <stdlib.h>


typedef struct {
    int id;
    int value;
    char letter;
} fun_data_t;

int main(int argc, char* argv[]) {
    
    fun_data_t fun_data;
    fun_data.id = 33;
    fun_data.value = 55;
    fun_data.letter = 'a';

    char* tptr = (char*) &fun_data;

    for(int i = 0; i < 13; i++) {
        printf("Data at byte %i: %c\n", i, (&fun_data + i*8));  
    }
    
    return 0;
}
