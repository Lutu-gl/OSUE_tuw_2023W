/**
 * @file mygrep.c
 * @author Luca, xxxxxxxx (exxxxxxxxxx@student.tuwien.ac.at)
 * @brief Implementation of a reduced variation of the Unix-command grep. Reads in several files and prints all lines containing a keyword
 * @details reduced version of the unix-command grep, input is a keyword, inputfiles or stdin, and a argument [i] if search should be case insensitive
 * @version 0.1
 * @date 2023-10-21
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>


static void usage(void);
static char *strcasestr(char *haystack, char *needle);
static int grep_file(int case_insensitive, FILE* fout, char* keyword, char* filein);
static int grep(int case_insensitive, FILE* fout, char* keyword);

static char *prog_name; /**< char pointer to the name of the program. d.h. the name that is in the arguments at pos. 0  (argv[0]). Used for error messages */

/**
 * @brief main function, parses arguments etc
 * @details Start of the program. function does the argument parsing with getopt of the arguments given in argv.
 * @param argc arguments count
 * @param argv arguments (first argument is the program name)
 * @return int
 */
int main(int argc, char *argv[]) {
    int opt;
    int return_status = 0;
    int case_insensitive = 0;
    int use_stdin = 1;
    char *output_file_str = NULL;
    FILE *fout = stdout; //set fout to stdout. If no output file is specified, then stdout is used.
    prog_name = argv[0];

    //Going through options
    while ((opt = getopt(argc, argv, "io:")) != -1) {
        switch (opt) {
            case 'i':
                case_insensitive = 1;
                break;
            case 'o':
                output_file_str = optarg;
                break;
            case '?':
                usage();
                return EXIT_FAILURE;
            default:
                assert(0); //should not happen!
        }
    }

    char *keyword = NULL;
    if (optind < argc) {
        keyword = argv[optind];
        optind++;
    } else {
        fprintf(stderr, "[%s] Error: No keyword specified.\n", argv[0]);
        usage();
        return EXIT_FAILURE;
    }

    if (output_file_str) {
        fout = fopen(output_file_str, "w");

        if(fout == NULL){
            fprintf(stderr, "[%s] Error: [%s] Failed to write to file\n", argv[0], output_file_str);
            return EXIT_FAILURE;
        }
    }

    for (int i = optind; i < argc; i++) {
        use_stdin = 0;
        return_status |= grep_file(case_insensitive, fout, keyword, argv[i]); //for each input file, function is called: If function returns Error, error status is updated!
    }

    if (use_stdin) {
        grep(case_insensitive, fout, keyword);
    }
    fclose(fout);
    if(return_status > 0) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/**
 * @brief prints usage to stdout
 * @details This function prints information about how to use the program, including options and arguments.
 */
static void usage(void) {
    printf("Usage: %s [-i] [-o outfile] keyword [file...]\n", prog_name);
    printf("[-i]: the program shall not differentiate between lower and upper case letters, i.e the search for the keyword in a line is case insensitive.\n");
    printf("[-o outfile] If the option -o is given, the output is written to the specified file (outfile). Otherwise, the output is written to stdout.\n");
    printf("keyword: keyword that the program searches for.\n");
    printf("[file...]: name of input files. If no input file is specified, the program reads from stdin\n");
}

/**
 * @brief core function: logic of grep. prints all lines containing a keyword. Uses stdin
 * @detail This function reads input lines from stdin, searches for the specified keyword, and prints
 *         the lines containing the keyword to the specified output file. It can perform case-sensitive
 *         or case-insensitive searches based on the value of the 'case_insensitive' parameter. Reads global var prog_name
 * @param case_insensitive: should search be case insensitive
 * @param fout stream to write to
 * @param keyword searching this string
 * @return exit status
 */
static int grep(int case_insensitive, FILE* fout, char* keyword){
    char* input = NULL;
    size_t input_size = 0;

    while(getline(&input, &input_size, stdin) != -1){ // Runs till infinity, User can exit by pressing STRG C or STRG D to mark EOF
        char *result = strstr(input, keyword);
        if (case_insensitive) result = strcasestr(input, keyword);

        if (result != NULL) {
            fprintf(fout, "%s", input);
        }
    }
    free(input); // With getline you have to free the dynamically allocated memory
    return 0;
}


/**
 * @brief core function: logic of grep. prints all lines containing a keyword. Uses input file.
 * @detail This function reads input lines from the specified input file, searches for the specified keyword, and prints
 *         the lines containing the keyword to the specified output file. It can perform case-sensitive or case-insensitive
 *         searches based on the value of the 'case_insensitive' parameter. Reads global var prog_name
 * @param case_insensitive: should search be case insensitive
 * @param fout stream to write to
 * @param keyword searching this string
 * @param filein string filename
 * @return exit status
 */
static int grep_file(int case_insensitive, FILE* fout, char* keyword, char* filein){
    FILE *fin;
    char* input = NULL;
    size_t input_size = 0;

    fin = fopen(filein, "r");

    if(fin == NULL){
        fprintf(stderr, "[%s] Error: [%s] No such file or directory\n", prog_name, filein);
        return 1;
    }
    while(getline(&input, &input_size, fin) != -1){ // Runs till infinity, User can exit by pressing STRG C or STRG D to mark EOF
        char *result = strstr(input, keyword);
        if (case_insensitive) result = strcasestr(input, keyword);
        if (result != NULL) {
            fprintf(fout, "%s", input);
        }
    }
    fclose(fin);
    free(input);
    return 0;
}


/**
 * @brief Function to search for a keyword inside another string case insensitive
 * @detail This function searches for the first occurrence of the specified substring (needle) within
 *         the given string (haystack) in a case-insensitive manner. It returns a pointer to the first
 *         occurrence of the substring in the string, or NULL if the substring is not found.
 * @param haystack string
 * @param needle keyword
 * @return return pointer to the beginning of the substring. NULL if not found.
 */
static char *strcasestr(char *haystack, char *needle) {
    if(*needle == '\0') return (char *) haystack;
    char* res = NULL;
    char lowerH = 0, lowerN = 0;

    int found = 0;
    while(*haystack != '\0' && !found){
        lowerH = tolower(*haystack);
        lowerN = tolower(*needle);
        if(lowerH == lowerN){
            char *continueN = needle, *continueH = haystack;
            while(*continueN != '\0'){
                lowerH = tolower(*continueH);
                lowerN = tolower(*continueN);
                if(lowerH != lowerN){
                    break;
                }
                continueH++;
                continueN++;
            }
            if(*continueN == '\0'){
              found = 1;
              break;
            }
        }
        haystack++;
    }

    if(found){
        res = haystack;
    }
    return res;
}
