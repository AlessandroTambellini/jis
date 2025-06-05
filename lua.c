#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "tokenizer.h"
#include "parser.h"
#include "array.h"

void interpret(char *source_code);
void compile(char *source_code);

static char *read_program_file(char *path) 
{
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char* buffer = (char*)malloc(fileSize + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}
	
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(74);
	}
	
	buffer[bytesRead] = '\0';

	fclose(file);
	return buffer;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(1);
    }

    char *source_code = read_program_file(argv[1]);
    interpret(source_code);
    free(source_code);

    return 0;
}

void interpret(char *source_code)
{
	// 1 - Tokenization Phase
	init_tokenizer(source_code);
    
	TokenArr ta;
	ARR_INIT(&ta);

	bool tokenization_err = false;
	collect_tokens(&ta, &tokenization_err);

#ifdef TDEBUG
    printf("TOKENS:\n");
    for (int i = 0; i < ta.size; i++)
    {
        print_token(ta.data[i]);
        printf("\n");
    }
#endif // DEBUG

	// 2 - Parsing Phase
	if (tokenization_err) {
		printf("Parsing aborted.\n");
	} else {
		parse_tokens(ta);
	}

	ARR_FREE(&ta);
}

