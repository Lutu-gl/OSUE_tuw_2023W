/**
 * @file supervisor.c
 * @author Luca, xxxxxxx (exxxxxxx@student.tuwien.ac.at)
 * @brief Implementation of the generator to generate solution for 3-color problem
 * @details The program implements an algorithm, that reads from generators (see generator.c). 
 * 			The supervisor sets up the shared memory and the semaphores and initializes the circular buffer required
 * 			for the communication with the generators. It then waits for the generators to write solutions to the
 *          circular buffer.
 * @version 0.1
 * @date 2023-12-07
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include "cbuffer.h"
#include <signal.h>
#include <string.h>

static char* prog_name; /**< a char pointer to the name of the program. The name that is in the arguments at pos. 0  (argv[0]). Used for error messages */



//volatile = to tell the compiler var can change unexpectatly
// sig_atomic_t = uint datatyp that is atomic!
volatile sig_atomic_t stop = 0; /**< atomic volatile that changes value when a signal call is received.  */


/**
 * @brief Signal handler to set the stop variable when a signal SIGTERM / SIGINT is received.
 * @param signal The signal number.
 */
void handle_signal(int signal){
    stop=1;
}


/**
 * @brief Displays the program usage information.
 */
static void usage(void){
    printf("Usage: %s [-n limit] [-w delay] [-p]\n", prog_name);
    printf("[-n limit]: The argument limit specifies a limit (integer value) for the number of generated solutions\n");
    printf("[-w delay]: The argument delay specifies a delay (in seconds) before reading the first solution from the buffer\n");
    printf("[-p]: Graph is printed using ASCII-art visualization\n");
}

/*
static void printShmSem(circularBuffer* cb, semaphores* sems, int countAA){
	int semaphoreValue1 = 0;
	int semaphoreValue2 = 0;
	int semaphoreValue3 = 0;

    sem_getvalue(sems->sem_free, &semaphoreValue1);
    sem_getvalue(sems->sem_used, &semaphoreValue2);
    sem_getvalue(sems->sem_mutex, &semaphoreValue3);
	
	
	printf("head=%d, tail=%d, sem_free=%d, sem_used=%d, sem_mutex=%d, count=%d\n",cb->head, cb->tail, semaphoreValue1, semaphoreValue2, semaphoreValue3, countAA);
}*/


/**
 * @brief Supervisor function that reads solutions from the circular buffer.
 * @param limit The maximum number of solutions to read (-1 for infinite).
 * @param delay The delay (in seconds) before reading the first solution.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
static int supervisor(int limit, int delay){
    //error handling
    int shmfd = 0;
    
    semaphores sems;
    circularBuffer* cb = create_cbuffer_server(&shmfd, &sems);
    

    if (cb == NULL) {
        fprintf(stderr, "[%s] Error opening of circular Buffer\n", prog_name);
        if (close_cbuff_server(shmfd, cb, &sems) == -1) {
            fprintf(stderr, "[%s] ERROR: Couldn't close circular buffer\n", prog_name);
        }
        return EXIT_FAILURE;
    }

    sleep(delay);
    int bestSolution=100000;
    int readCount=0;
    int solutionFound=0;
    while(stop == 0){
		//printShmSem(cb, &sems, readCount);
		
            //printf("supervisor liest: ");
        if(read_from_cbuf(cb, &bestSolution, &sems) == -1){
            fprintf(stderr, "[%s] ERROR: Couldn't read circular buffer\n", prog_name);
            if (close_cbuff_server(shmfd, cb, &sems) == -1) {
                fprintf(stderr, "[%s] ERROR: Couldn't close circular buffer\n", prog_name);
            }
            return EXIT_FAILURE;
        }
        readCount++;
        solutionFound=1;
        
        if(bestSolution == 0){
            break;
        }else if(readCount >= limit && limit != -1){
            break;
        }
        //sleep(1);
    }
    if(solutionFound == 0){
        fprintf(stderr, "[%s] No solution found! Please check if you start generators\n", prog_name);
        return EXIT_SUCCESS;
    }
    if(bestSolution != 0){
        printf("The graph might not be 3-colorable, best solution removes %d edges.\n", bestSolution);
    }
    cb->stop = true;
    if (close_cbuff_server(shmfd, cb, &sems) == -1) {
        fprintf(stderr, "[%s] ERROR: Couldn't close circular buffer\n", prog_name);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Main function for the supervisor program.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int main(int argc, char** argv){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    int opt=0;
    int argN=0;
    int argW=0;
    int argP=0;
    char* limitStr;
    char* delayStr;
    int limit=-1;       // -1 = inf
    int delay=0;
    prog_name = argv[0];
    while((opt = getopt(argc, argv, "n:w:p")) != -1){
        switch(opt){
            case 'n':
                argN++;
                limitStr = optarg;
                break;
            case 'w':
                argW++;
                delayStr = optarg;
                break;
            case 'p':
                argP++;
                break;
            case '?':
                usage();
                return EXIT_FAILURE;
            default:
                assert(0);
                return EXIT_FAILURE;
        }
    }
    if(optind < argc){
        usage();
        return EXIT_FAILURE;
    }

    if(argN > 1 || argW > 1 || argP > 1){
        usage();
        return EXIT_FAILURE;
    }
    if(argN == 1){
        char* endptr=NULL;
        errno = 0;
        limit = strtol(limitStr, &endptr, 10);
        if ((errno == ERANGE || (errno != -1 && limit == 0)) || *endptr != '\0') {
            usage();
            perror("strtol");
            return EXIT_FAILURE;
        }
    }
    if(argW == 1){
        char* endptr=NULL;
        errno = 0;
        delay = strtol(delayStr, &endptr, 10);
        if ((errno == ERANGE || (errno != -1 && limit == 0)) || *endptr != '\0') {
            usage();
            perror("strtol");
            return EXIT_FAILURE;
        }
    }

    //printf("limit = %d delay = %d\n", limit, delay);
    
    int ret = supervisor(limit, delay);
    return ret;
}
