/**
 * @file generator.c
 * @author Luca, xxxxxxx (exxxxxxx@student.tuwien.ac.at)
 * @brief Implementation of the generator to generate solution for 3-color problem
 * @details The program implements an algorithm, that reads a number of edges, forms a graph and tries to find a solution for the 3-color problem
 * 			It colors the vertixes randomly, and calculates the min. edges that have to be removed to make the graph 3-color "able".
 *          It writes its solutions to the supervisor.c program. They depend on each other.
 * 			The generator program takes a graph as input. The program repeatedly generates a random solution 
 * 			to the problem as described on the first page and writes its result to the circular buffer. It repeats this
 * 			procedure until it is notified by the supervisor to terminate.
 * 			The generator program takes as arguments the set of edges of the graph:
 * @version 0.1
 * @date 2023-12-07
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
#include <time.h>
#include "cbuffer.h"

static char* prog_name; /**< a char pointer to the name of the program. The name that is in the arguments at pos. 0  (argv[0]). Used for error messages */
#define MAXREMOVEDEDGES (8)

/**
 * @brief Structure representing a graph.
 */
typedef struct Graph {
    int V; /**< Number of vertices in the graph. */
    int E; /**< Number of edges in the graph. */
    int** matrix; /**< Adjacency matrix of the graph. */
    
    // colors
    int sizeRed; /**< Number of vertices in the 'red' color class. */
    int sizeGreen; /**< Number of vertices in the 'green' color class. */
    int sizeBlue; /**< Number of vertices in the 'blue' color class. */
    
    int* red; /**< Array storing vertices in the 'red' color class. */
    int* green; /**< Array storing vertices in the 'green' color class. */
    int* blue; /**< Array storing vertices in the 'blue' color class. */
} Graph;

/**
 * @brief Displays the usage information for the program.
 */
static void usage(void){
    printf("Usage: %s [edges]\n", prog_name);
    printf("[edges]: vertex1-vertex2 vertex2-vertex3 vertex1-vertex3...\n");
}

/**
 * @brief Creates a new graph with the given number of vertices.
 * @param V Number of vertices in the graph.
 * @return Pointer to the newly created graph.
 * @details Allocates memory for a new graph with V vertices, initializes the adjacency matrix, and returns a pointer to the graph.
 * The adjacency matrix is initially filled with zeros.
 */
struct Graph* createGraph(int V) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    graph->V = V;

    graph->matrix = (int**)malloc(V * sizeof(int*));
    for (int i = 0; i < V; ++i) {
        graph->matrix[i] = (int*)malloc(V * sizeof(int));
        for (int j = 0; j < V; ++j) {
            graph->matrix[i][j] = 0;
        }
    }

    return graph;
}

/**
 * @brief Adds an undirected edge between two vertices in the graph.
 * @param graph Pointer to the graph.
 * @param src Source vertex.
 * @param dest Destination vertex.
 * @details Modifies the adjacency matrix to represent an edge between the source and destination vertices.
 */
void addEdge(Graph* graph, int src, int dest) {
    graph->matrix[src][dest] = 1;
    graph->matrix[dest][src] = 1;
}


/**
 * @brief Removes edges in a color set that violate the graph's structure.
 * @param graph Pointer to the graph.
 * @param color Array representing the color set.
 * @param colorSize Size of the color set.
 * @details Iterates over pairs of vertices in the given color set, identifies edges in the graph, and removes them if they exist.
 * Updates the number of edges in the graph accordingly.
 */
void removeWrongEdgesColor(Graph* graph, int* color, int colorSize){
    for (size_t i = 0; i < colorSize; i++){
        for (size_t j = i+1; j < colorSize; j++){
            int node1 = color[i];
            int node2 = color[j];
            if(graph->matrix[node1][node2] == 1){
                printf("Wrong edge detected: %d-%d\n", node1, node2);
                graph->matrix[node1][node2] = 0;
                graph->matrix[node2][node1] = 0;
                graph->E--;
            }
        }
    }
}

/**
 * @brief Removes edges in all color sets that violate the graph's structure.
 * @param graph Pointer to the graph.
 * @details Calls removeWrongEdgesColor for each color set in the graph.
 */
void removeWrongEdges(Graph* graph){
    removeWrongEdgesColor(graph, graph->red, graph->sizeRed);
    removeWrongEdgesColor(graph, graph->green, graph->sizeGreen);
    removeWrongEdgesColor(graph, graph->blue, graph->sizeBlue);
}
/**
 * @brief Populates a buffer with vertices forming edges that need to be removed.
 * @param graph Pointer to the graph.
 * @param color Array representing the color set.
 * @param colorSize Size of the color set.
 * @param buffer Array to store vertices forming edges to be removed.
 * @param count Pointer to the count of vertices in the buffer.
 * @return 0 on success, -1 on error.
 * @details Iterates over pairs of vertices in the given color set, identifies edges in the graph, and adds the vertices to the buffer.
 * The buffer is formatted as follows: size|int1|int2|int3|int4. The number of integers is 2 * size.
 */
int getRemoveEdgesColor(Graph* graph, int* color, int colorSize, int* buffer, int* count){
    for (size_t i = 0; i < colorSize; i++){
        for (size_t j = i+1; j < colorSize; j++){
            int node1 = color[i];
            int node2 = color[j];
            if(graph->matrix[node1][node2] == 1){
                //printf("Wrong edge detected: %d-%d\n", node1, node2);
                buffer[(*count)++] = node1;
                buffer[(*count)++] = node2;
            }
        }
    }
    return 0;
}
/**
 * @brief Populates a buffer with vertices forming edges that need to be removed from all color sets.
 * @param graph Pointer to the graph.
 * @param buffer Array to store vertices forming edges to be removed.
 * @return 0 on success, -1 on error.
 * @details Calls getRemoveEdgesColor for each color set in the graph.
 */
// buffer structure: size|int1|int2|int3|int4. #ints = 2*size
int getRemoveEdges(Graph* graph, int* buffer){
    int size=0;
    getRemoveEdgesColor(graph, graph->red, graph->sizeRed, buffer+1, &size);
    getRemoveEdgesColor(graph, graph->green, graph->sizeGreen, buffer+1, &size);
    getRemoveEdgesColor(graph, graph->blue, graph->sizeBlue, buffer+1, &size);
    buffer[0] = size/2;
    return 0;
}

/**
 * @brief Checks if the graph is 3-colorable based on a color set.
 * @param graph Pointer to the graph.
 * @param color Array representing the color set.
 * @param colorSize Size of the color set.
 * @return Number of violations found.
 * @details Iterates over pairs of vertices in the given color set, identifies edges in the graph, and prints an error message for each violation.
 */
int check3ColorableColor(Graph* graph, int*color, int colorSize){
    int ret=0;
    for (size_t i = 0; i < colorSize; i++){
        for (size_t j = i+1; j < colorSize; j++){
            int node1 = color[i];
            int node2 = color[j];
            if(graph->matrix[node1][node2] == 1){
                fprintf(stderr, "Graph is not 3-colorable - Edge found: %d-%d\n", node1, node2);
                ret++;
            }
        }
    }
    return ret;
}

/**
 * @brief Checks if the graph is 3-colorable.
 * @param graph Pointer to the graph.
 * @return 0 if 3-colorable, 1 if violations found.
 * @details Calls check3ColorableColor for each color set in the graph.
 * Prints an error message if the graph is not 3-colorable.
 */
int check3Colorable(Graph* graph){
    int i = 0;
    i += check3ColorableColor(graph, graph->red, graph->sizeRed);
    i += check3ColorableColor(graph, graph->green, graph->sizeGreen);
    i += check3ColorableColor(graph, graph->blue, graph->sizeBlue);
    if(i != 0){
        fprintf(stderr, "Graph is not 3 colorable!\n");
        return 1;
    }
    return 0;
}

/**
 * @brief Prints information about the graph.
 * @param graph Pointer to the graph.
 * @details Prints the number of vertices, the number of edges, the size of each color set, and the adjacency matrix.
 */
void printGraph(Graph* graph){
    printf("V = %d\nE = %d\n", graph->V, graph->E);
    printf("%d Red: ", graph->sizeRed);
    for (size_t i = 0; i < graph->sizeRed; i++){
        printf("%d ", graph->red[i]);
    }
    printf("\n%d Green: ", graph->sizeGreen);
    for (size_t i = 0; i < graph->sizeGreen; i++){
        printf("%d ", graph->green[i]);
    }
    printf("\n%d Blue: ", graph->sizeBlue);
    for (size_t i = 0; i < graph->sizeBlue; i++){
        printf("%d ", graph->blue[i]);
    }
    printf("\n");
    printf("Adjazenzmatrix:\n");
        for (int i = 0; i < graph->V; ++i) {
            for (int j = 0; j < graph->V; ++j) {
                printf("%d ", graph->matrix[i][j]);
            }
        printf("\n");
    }
}
/**
 * @brief Frees memory allocated for the graph.
 * @param graph Pointer to the graph.
 * @details Frees memory allocated for the adjacency matrix and each color set.
 */
void freeGraph(Graph* graph) {
    for (int i = 0; i < graph->V; ++i) {
        free(graph->matrix[i]);
    }
    free(graph->matrix);
    free(graph->red);
    free(graph->green);
    free(graph->blue);
    free(graph);
}

/**
 * @brief Colors the vertices of the graph randomly into three color sets.
 * @param graph Pointer to the graph.
 * @details Randomly assigns each vertex to one of the three color sets: red, green, or blue.
 * The size of each color set is updated accordingly.
 */
void colorGraph(Graph* graph){
    graph->sizeRed=0;
    graph->sizeGreen=0;
    graph->sizeBlue=0;

    for (size_t i = 0; i < graph->V; i++){
        int col = rand() % 3;
        switch (col){
            case 0: graph->red = realloc(graph->red, (graph->sizeRed + 1) * sizeof(int)); graph->red[graph->sizeRed++] = i; break;
            case 1: graph->green = realloc(graph->green, (graph->sizeGreen + 1) * sizeof(int)); graph->green[graph->sizeGreen++] = i; break;
            case 2: graph->blue = realloc(graph->blue, (graph->sizeBlue + 1) * sizeof(int)); graph->blue[graph->sizeBlue++] = i; break;
            default: assert(0);
        }
    }
}

/*
static void printIntArray(const int *buffer, size_t size) {
    if (buffer == NULL) {
        // Optional: Behandle den Fall eines NULL-Zeigers
        printf("Ungültiger Zeiger.\n");
        return;
    }

    printf("Inhalt des Integer-Arrays:\n");
    for (size_t i = 0; i < size; ++i) {
        printf("%d ", buffer[i]);
    }
    printf("\n");
}*/

/**
 * @brief Main function of the program.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return 0 on successful execution, EXIT_FAILURE on error.
 * @details Parses command-line arguments, creates a graph based on the provided edges, opens a circular buffer as a client,
 * generates random solutions until the supervisor program halts, and closes the circular buffer and frees the graph.
 */

// Expect that when edges are given, the edges are unique + in the right format!
int main(int argc, char** argv){
    prog_name = argv[0];
	srand(time(NULL)); // seed for rgen!
    int opt = 0;

    while ((opt = getopt(argc, argv, "")) != -1) {

        switch(opt){
            case '?':
                usage();
                return EXIT_FAILURE;
            default:
                assert(0);
        }
    }

    int edgeCount = argc-1;
    int num_vertex = 0;

    /** Get count of vertixes */
    for (int i = optind; i < argc; ++i) {
        if (argv[i][0] != '-') {
            int src, dest;
            int scanRes = sscanf(argv[i], "%d-%d", &src, &dest);
            if(scanRes != 2){
                usage();
                return EXIT_FAILURE;
            }
            //printf("egdes: %d-%d\n",src,dest);
            if(src > num_vertex )
                num_vertex = src;
            if(dest > num_vertex)
                num_vertex = dest;
        }
    }


    /** Create graph */
    Graph* originalGraph = createGraph(++num_vertex);
    originalGraph->E = edgeCount;
    originalGraph->sizeRed = 0;
    originalGraph->sizeGreen = 0;
    originalGraph->sizeBlue = 0;
    originalGraph->red = NULL;
    originalGraph->green = NULL;
    originalGraph->blue = NULL;

    for (int i = optind; i < argc; ++i) {
        if (argv[i][0] != '-') {
            int src, dest;
            int scanRes = sscanf(argv[i], "%d-%d", &src, &dest);
            if(scanRes != 2){
                usage();
                return EXIT_FAILURE;
            }
            addEdge(originalGraph, src, dest);
        }
    }
    /** Open circular buffer as client */
	semaphores sems;
    int shmfd = 0;
    circularBuffer *cb = open_cbuffer_client(&shmfd, &sems);
    if (cb == NULL) {
        fprintf(stderr, "[%s] Error opening of circular Buffer\n", prog_name);
        freeGraph(originalGraph);
        exit(EXIT_FAILURE);
    }

    /** Generate random solutions until supervisor program halts */
    int* buffer = (int*) realloc(NULL, (2*originalGraph->E+1) * sizeof(int));
    if(buffer == NULL){
        fprintf(stderr, "[%s] Buffer konnte nicht allociert werden\n", prog_name);
        freeGraph(originalGraph);
        if(close_cbuff_client(shmfd, cb, &sems) == -1){
            fprintf(stderr, "[%s] Error when closing cbuf\n", prog_name);
        }
        exit(EXIT_FAILURE);
    }
    
    int count = 0;	
    while (!cb->stop) {
        colorGraph(originalGraph);
        //printGraph(originalGraph);
        if(getRemoveEdges(originalGraph, buffer) == -1){
            fprintf(stderr, "[%s] Error when deleting edges\n", prog_name);
            free(buffer);
            freeGraph(originalGraph);
            if(close_cbuff_client(shmfd, cb, &sems) == -1){
                fprintf(stderr, "[%s] Error when closing cbuf\n", prog_name);
            }
            exit(EXIT_FAILURE);	
        }
        count = buffer[0];
        if (count > MAXREMOVEDEDGES) continue;
        if(write_to_cbuf(cb, buffer, &sems) == -1){
            fprintf(stderr, "[%s] Error when writing to cbuf\n", prog_name);
            freeGraph(originalGraph);
            free(buffer);
            if(close_cbuff_client(shmfd, cb, &sems) == -1){
                fprintf(stderr, "[%s] Error when closing cbuf\n", prog_name);
            }
			exit(EXIT_FAILURE);
		}
        //printIntArray(buffer, count*2 + 1);
        //printf("---------------------------------------\n");
        //sleep(1);
    }
    
	/** Close circular buffer and delete graph */
    free(buffer);
    freeGraph(originalGraph);
    if(close_cbuff_client(shmfd, cb, &sems) == -1){
        fprintf(stderr, "[%s] Error when closing cbuf\n", prog_name);
    }
    return EXIT_SUCCESS;
}
