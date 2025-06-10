#include "utils.h"
#include "tokenizer.h"
#include "parser.h"

static void interpret(char *source_code);
static char *read_program_file(char *path);

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: jis <path>\n");
        exit(EXIT_FAILURE);
    }

    char *source_code = read_program_file(argv[1]);
    interpret(source_code);
    
	free(source_code);

    return 0;
}

static char *read_program_file(char *path) 
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Unable to open file '%s'.\n", path);
		exit(EXIT_FAILURE);
	}

	fseek(file, 0L, SEEK_END);
	size_t f_size = ftell(file);
	rewind(file);

	char *buffer = (char *)malloc(f_size + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory to read '%s'.\n", path);
		exit(EXIT_FAILURE);
	}
	
	size_t bytes = fread(buffer, sizeof(char), f_size, file);
	if (bytes < f_size) {
		fprintf(stderr, "Unable to read file '%s'.\n", path);
		exit(EXIT_FAILURE);
	}
	
	buffer[bytes] = '\0';
	fclose(file);

	return buffer;
}

static void interpret(char *source_code)
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
#endif // TDEBUG

	// 2 - Parsing and interpretation phase
	if (!tokenization_err) {
		init_parser(ta);
		parse_tokens();
	}

	ARR_FREE(&ta);
}

