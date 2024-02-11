/**
 * @file forkFFT.c
 * @author Luca, xxxxxxxx (exxxxxxxx@student.tuwien.ac.at)
 * @brief Implementation of the Cooley-Tukey Fast Fourier Tranformation algorithm using fork
 * @details The program implements the Cooley Tukey Fast Fourer Transformation. Input are 2^n real numbers, output are 2^n imaginary numbers.
 *          This program reads input data from stdin, performs FFT using the Cooley-Tukey algorithm on input data using a parallelized approach.
 *          It prints the result to stdout. The FFT is parallelized using fork() to create child processes.
 * @version 0.1
 * @date 2023-11-06
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>


static char* prog_name; /**< a char pointer to the name of the program. The name that is in the arguments at pos. 0  (argv[0]). Used for error messages */
static double const PI = 3.141592654;  /**< Saves the value 3.141592654 to a double const variable PI. Used for calculation of FFT. */

static void usage(void);
static int forkFFT(int argP);
static void printImaginary(double r, double i, FILE* fout, int better_acc);
static int makeChildRun(double* start, int* pipefd1, int*pipefd2, int size);
static double multiplyImaginaryI(double r1, double i1, double r2, double i2);
static double multiplyImaginaryR(double r1, double i1, double r2, double i2);
static int readSolutionFromChild(int* pipefd, double* R);
static double roundToZero(double number, double epsilon);

/**
 * @brief Main function. parses arguments etc
 * @details Start of the program. function does the argument parsing with getopt of the arguments given in argv.
 * @param argc arguments count
 * @param argv  arguments (first arg is prog name)
 * @return int
 */
int main(int argc, char **argv){
    int opt=0;
    int argP=0;
    prog_name = argv[0];

    while((opt = getopt(argc, argv, "p")) != -1){
        switch(opt){
            case 'p':
                argP = 1;
                break;
            case '?':
                usage();
                return EXIT_FAILURE;
            default:
                assert(0);
        }
    }

    int ret = forkFFT(argP);
    return ret;
}

/**
 * @brief prints usage to stdout
 * @details This function prints information about how to use the program, including options and arguments.
 */
static void usage(void){
    printf("Usage: %s [-p]\n", prog_name);
    printf("[-p]: If option is given, the output must use exactly 3 digits after the decimal point\n");
}
/**
 * @brief cThis function reads input data from stdin, performs FFT using the Cooley-Tukey algorithm on input data using a parallelized approach.
 * @details For explanation of algorithm see: https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm makes children, allocates memory, uses prog_name, uses PI, uses prog_name.
 *          It prints the result to stdout. The FFT is parallelized using fork() to create child processes
 *          for computation. The input data is expected to be in the format of real and imaginary parts
 *          interleaved, and the size of the input data should be a power of 2.
 * @param argP is argument p given or not
 * @return integer value/ return status
 */
static int forkFFT(int argP) {
    char *line = NULL;
    size_t len = 0;
    ssize_t string;
    double *input = NULL;
    int size = 0;

    // Read input from stdin
    while ((string = getline(&line, &len, stdin)) != -1) {
        input = (double *) realloc(input, (size + 1) * sizeof(double));
        if(input == NULL){
            free(line);
            fprintf(stderr, "[%s] Error when allocating memory for reading of input\n", prog_name);
            return EXIT_FAILURE;
        }
        size++;
        char *ptr;
        double val = strtod(line, &ptr);

        if(line == ptr || (*ptr != '\0' && *ptr != '\n')){
            free(input);
            free(line);
            fprintf(stderr, "[%s] Error on strtod, received faulty input\n", prog_name);
            return EXIT_FAILURE;
        }

        if (val == (double) -0) {
            val = (double) 0;
        }
        input[size - 1] = val;
    }
    free(line); //line no longer needed!

//freeen bei error
    if(size > 1 && size % 2 != 0){
        free(input);
        fprintf(stderr, "[%s] Error: received faulty input\n", prog_name);
        return EXIT_FAILURE;
    }

    switch(size){
        case 0:
            free(input);
            fprintf(stderr, "[%s] no input given\n", prog_name);
            return EXIT_FAILURE;
        case 1:
            printImaginary(input[0], (double) 0, stdout, argP); // only one input
            free(input);
            break;
        default: ; // multiple inputs ; is used because: a declaration is not a statement after default switch
            //create pipes!
            int pipefd11[2];
            int pipefd12[2];
            int pipefd21[2];
            int pipefd22[2];
            pipe(pipefd11);
            pipe(pipefd12);
            pipe(pipefd21);
            pipe(pipefd22);

            if(makeChildRun(&input[0], pipefd11, pipefd12, size) != 0) {
                free(input);
                return EXIT_FAILURE;
            }

            if(makeChildRun(&input[1], pipefd21, pipefd22, size) != 0) {
                free(input);
                return EXIT_FAILURE;
            }

            // parent tasks..
            //free mem allocated that is no longer used
            free(input);

            // here wait and read result from pipe;
            // wait for children
            wait(NULL);
            wait(NULL);

            double* Re = NULL;
            double* Ro = NULL;
            // allocate mem for result
            Re = (double *) calloc(size, sizeof(double)); // allocate full size, because real and imaginary num is stored!
            if(Re == NULL){
                fprintf(stderr, "[%s] Error when allocating memory for result of children\n", prog_name);
                return EXIT_FAILURE;
            }
            Ro = (double *) calloc(size, sizeof(double));
            if(Ro == NULL){
                free(Re);
                fprintf(stderr, "[%s] Error when allocating memory for result of children\n", prog_name);
                return EXIT_FAILURE;
            }

            if( (readSolutionFromChild(pipefd12, Re) != 0) || (readSolutionFromChild(pipefd22, Ro) != 0)){
                free(Ro);
                free(Re);
                return EXIT_FAILURE;
            }

            // calculate result according to: Cooley-Tukey FFT
            double *R = (double *) calloc(2*size, sizeof(double));
            if(R == NULL){
                free(Re);
                free(Ro);
                fprintf(stderr, "[%s] Error when allocating memory for result computation\n", prog_name);
                return EXIT_FAILURE;
            }

            for (int i = 0; i < size; i+=2) {
                R[i] = Re[i] + multiplyImaginaryR((double) cos(-2.0 * PI / size * (double)i/2), (double) sin(-2.0 * PI / (double) size * (double)i/2), Ro[i], Ro[i+1]);
                R[i+1] = Re[i+1] + multiplyImaginaryI((double) cos(-2.0 * PI / size * (double)i/2), (double) sin(-2.0 * PI /(double) size * (double)i/2), Ro[i], Ro[i+1]);
            }

            for (int i = 0; i < size; i+=2) {
                R[i+size] = Re[i] - multiplyImaginaryR((double) cos(-2.0 * PI / size * i/2), (double) sin(-2.0 * PI / (double) size * i/2), Ro[i], Ro[i+1]);
                R[i+size+1] = Re[i+1] - multiplyImaginaryI((double) cos(-2.0 * PI / size * (double)i/2), (double) sin(-2.0 * PI /(double) size * (double)i/2), Ro[i], Ro[i+1]);
            }

            // print result and fix rounding errors
            for (int i = 0; i < 2*size; i+=2) {
                R[i] = roundToZero(R[i], 1e-3);
                R[i+1] = roundToZero(R[i+1], 1e-3);
                printImaginary(R[i], R[i+1], stdout, argP);
            }

            // close read end of pipes and free mem
            close(pipefd22[0]);
            close(pipefd12[0]);
            free(R);
            free(Re);
            free(Ro);
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Rounds a number to zero if it is within a specified tolerance. Also transforms -0 to 0
 * @detail This function rounds the given double precision number to zero if its absolute value
 *         is within the specified epsilon tolerance. Otherwise, it returns the original number. It is used for better calculation.
 * @param number The number to be checked and possibly rounded to zero.
 * @param epsilon he tolerance for rounding the number to zero.
 * @return transformed number
 */
static double roundToZero(double number, double epsilon) {
    if (number == (double) -0) {
        number = (double) 0.0;
    }
    if (fabs(number) <= epsilon) {
        return 0.0; // Return 0 if it's within the epsilon tolerance
    } else {
        return number; // Otherwise, return the original number
    }
}

/**
 * @brief Reads result from the children.
 * @details This function reads the result from a child process through a pipe and inserts into the
 *          provided array 'R' the real and imaginary parts of the computed values. The data
 *          is expected to be in the format of alternating real and imaginary parts, each followed
 *          by a '*i' character. The function returns EXIT_FAILURE on failure with appropriate error messages. Uses prog_name, allocates memory.
 * @param pipefd pipefds where read end is specified
 * @param R location where doubles are stored
 * @return 0 if success, else error
 */
static int readSolutionFromChild(int* pipefd, double* R){
    int sizePipe = 0;
    if(ioctl(pipefd[0], FIONREAD, &sizePipe) != 0){
        fprintf(stderr, "[%s] Error: size of pipe is 0\n", prog_name);
        return EXIT_FAILURE;
    }
    char *buffer = (char*) calloc(sizePipe+1, sizeof(char));
    if(buffer == NULL){
        fprintf(stderr, "[%s] Error when allocating memory for Result of Children\n", prog_name);
        return EXIT_FAILURE;
    }
    char *endptr = NULL;
    int counter = 0;

    while(read(pipefd[0], buffer, sizePipe) > 0) {
        char *str = buffer;
        while(*str != '\0'){
            R[counter++] = strtod(str, &endptr); // Real part
            if(str == endptr) {
                free(buffer);
                fprintf(stderr, "[%s] Error on strtod, received faulty result\n", prog_name);
                return EXIT_FAILURE;
            }
            str = endptr;
            R[counter++] = strtod(str, &endptr); // Imaginary part

            if(str == endptr || *endptr != '*') {
                free(buffer);
                fprintf(stderr, "[%s] Error on strtod, received faulty result\n", prog_name);
                return EXIT_FAILURE;
            }
            str = endptr + 2; //skip the "*i" from the imaginary number
            while(*str == ' ' || *str == '\n') { //go to next number
                str++;
            }
        }
    }
    free(buffer);
    return 0;
}

/**
 * @brief creates a child with fork and calls the program (recursion) with half the input. See C-T FFT
 * @details This function creates a child process using fork() and redirects the read end of pipe1 to
 *         stdin and the write end of pipe2 to stdout in the child process. It then sends the
 *         input data to the child process + closes the unused ends of the pipes. The function returns
 *         EXIT_FAILURE on failure with appropriate error messages. Uses prog_name, allocates memory.
 * @param start start of the location where the doubles of input are stored
 * @param pipefd1 Parent to Child pipe fd
 * @param pipefd2 Child to Parent pipe fd
 * @param size Size of the inputs
 * @return 0 if success, else if error
 */
static int makeChildRun(double* start, int* pipefd1, int*pipefd2, int size){
    int pid;
    switch (pid = fork()){
        case -1:
            // exit
            fprintf(stderr, "[%s] Error when forking children\n", prog_name);
            return EXIT_FAILURE;
        case 0: // child
            //redirection of pipe1 read end to stdin and pipe2 write end to stdout.
            close(pipefd1[1]);
            dup2(pipefd1[0], STDIN_FILENO);
            close(pipefd1[0]);

            close(pipefd2[0]);
            dup2(pipefd2[1], STDOUT_FILENO);
            close(pipefd2[1]);

            // call programm
            if (execlp("./forkFFT", "forkFFT", NULL) == -1) {
                fprintf(stderr, "[%s] Error when calling execlp. Check if path is right\n", prog_name);
                return EXIT_FAILURE;
            }
        default: // parent
            // close not used ends
            close(pipefd1[0]);
            close(pipefd2[1]);

            char* buffer = NULL;
            int sizeBuffer = 0;
            int bufferOffset=0;
            for (int i = 0; i < size; i+=2) {
                int sizeNum =  snprintf(NULL, 0, "%.10lf\n", start[i]); // snprintf returns the size needed
                buffer = realloc(buffer, (sizeNum + sizeBuffer + 1) * sizeof(char));

                if(buffer == NULL){
                    fprintf(stderr, "[%s] Error when reallocating memory\n", prog_name);
                    return EXIT_FAILURE;
                }
                sizeBuffer += sizeNum;

                int length = sprintf(buffer + bufferOffset, "%.10lf\n", start[i]);
                if (length >= 0) {
                    bufferOffset += length;
                } else {
                    free(buffer);
                    fprintf(stderr, "[%s] Error when converting double to string\n", prog_name);
                    return EXIT_FAILURE;
                }
            }

            write(pipefd1[1], buffer, sizeBuffer);
            close(pipefd1[1]);
            free(buffer);
            return 0;
    }
}
/**
 * @brief Helper function for multiplying two imaginary numbers.
 * @details calculates the real part of a multiplication of two imaginary numbers
 * @param r1 number 1 real part
 * @param i1 number 1 imag part
 * @param r2 number 2 real part
 * @param i2 number 2 real part
 * @return real part as a double
 */
static double multiplyImaginaryR(double r1, double i1, double r2, double i2){
    return r1 * r2 - i1 * i2;
}
/**
 * @brief Helper function for multiplying two imaginary numbers.
 * @details calculates the imaginary part of a multiplication of two imaginary numbers
 * @param r1 number 1 real part (double)
 * @param i1 number 1 imag part (double)
 * @param r2 number 2 real part (double)
 * @param i2 number 2 real part (double)
 * @return imaginary part as a double
 */
static double multiplyImaginaryI(double r1, double i1, double r2, double i2){
    return r1 * i2 + i1 * r2;
}

/**
 * @brief Prints the Imaginary number with format: [real part] [imag part]*i to the specified output file
 * @detail This function prints a complex number with the given real and imaginary parts to the specified
 *         output file. The format of the output is determined by the 'argP' parameter - if 'argP' is non-zero,
 *         the format is "%.3lf %.3lf*i", otherwise "%.6lf %.6lf*i". it is used for the output of the FFT.
 * @param r real number (double)
 * @param i imag number (double)
 * @param fout File Pointer to out file
 * @param argP argument P: if true then 3 decimal places, else 6
 */
static void printImaginary(double r, double i, FILE* fout, int argP){
    if (argP)
        fprintf(fout, "%.3lf %.3lf*i\n", r, i);
    else
        fprintf(fout, "%.6lf %.6lf*i\n", r, i); 
}
