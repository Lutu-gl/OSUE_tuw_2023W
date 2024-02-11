/**
 * @file cbuffer.h
 * @author Luca, xxxxxxx (exxxxxxx@student.tuwien.ac.at)
 * @brief Implementation of circular buffer functions for inter-process communication.
 * @details This file contains functions to create, open, and manipulate a circular buffer for inter-process communication. Generator.c and supervisor.c depend on it
 * @version 0.1
 * @date 2023-12-07
 */

#ifndef CBUFFER_H
#define CBUFFER_H

#include <semaphore.h>
#include <stdbool.h>

#define BLOCK_SIZE 300 

/**
 * @brief Structure representing a circular buffer.
 */
typedef struct {
    int data[BLOCK_SIZE]; /**< Array to store data in the circular buffer. */
    int head; /**< Index indicating the writing start position. */
    int tail; /**< Index indicating the reading done position. */
    bool stop; /**< Boolean flag indicating if the circular buffer is marked for stopping. */
} circularBuffer;


/**
 * @brief Structure representing semaphores for synchronization.
 */
typedef struct {
    sem_t* sem_free; /**< Semaphore indicating the number of free spaces in the circular buffer. */
    sem_t* sem_used; /**< Semaphore indicating the number of used spaces in the circular buffer. */
    sem_t* sem_mutex; /**< Mutex semaphore for ensuring mutual exclusion. */
} semaphores;



/**
 * @brief Creates a circular buffer and associated semaphores for the server.
 * @param shmfd Pointer to the shared memory file descriptor.
 * @param sems Pointer to the semaphores structure.
 * @return A pointer to the circular buffer structure on success, or NULL on failure.
 * @details Creates a circular buffer in shared memory, initializes its properties,
 * and creates semaphores for synchronization.
 */
circularBuffer *create_cbuffer_server(int *shmfd, semaphores* sems);

/**
 * @brief Opens an existing circular buffer for a client.
 * @param shmfd Pointer to the shared memory file descriptor.
 * @param sems Pointer to the semaphores structure.
 * @return A pointer to the circular buffer structure on success, or NULL on failure.
 * @details Opens an existing circular buffer in shared memory and associates with it the existing semaphores.
 */
circularBuffer *open_cbuffer_client(int *shmfd, semaphores *sems);

/**
 * @brief Closes a circular buffer and releases associated resources for the server.
 * @param fd File descriptor of the shared memory.
 * @param cb Pointer to the circular buffer structure.
 * @param sems Pointer to the semaphores structure.
 * @return 0 on success, -1 on failure.
 * @details Unmaps the shared memory, closes the file descriptor, unlinks the shared memory,
 * and closes and unlinks the associated semaphores.
 */
int close_cbuff_server(int fd, circularBuffer* cb, semaphores* sems);

/**
 * @brief Closes a circular buffer for a client.
 * @param fd File descriptor of the shared memory.
 * @param cb Pointer to the circular buffer structure.
 * @param sems Pointer to the semaphores structure.
 * @return 0 on success, -1 on failure.
 * @details Unmaps the shared memory and closes the file descriptor.
 * Optionally, closes the associated semaphores if sems is not NULL.
 */
int close_cbuff_client(int fd, circularBuffer* cb, semaphores* sems);

/**
 * @brief Writes the given data to the circular buffer.
 * @param cbuf Pointer to the circular buffer structure.
 * @param removeEdges Array of integers representing edges to be removed.
 * @param sems Pointer to the semaphores structure.
 * @return 1 on success, 0 if the circular buffer has been marked for stopping.
 * @details Writes the number of edges and their details to the circular buffer,
 * updating the head pointer accordingly.
 */
int write_to_cbuf(circularBuffer *cbuf, int *removeEdges, semaphores* sems);

/**
 * @brief Reads the best solution data from the circular buffer.
 * @param cbuf Pointer to the circular buffer structure.
 * @param bestSolution Pointer to the variable holding the best solution edge count.
 * @param sems Pointer to the semaphores structure.
 * @return 0 on success, -1 on failure.
 * @details Reads the size of the best solution and its details from the circular buffer,
 * updating the tail pointer accordingly. Prints the solution details to stderr.
 */
int read_from_cbuf(circularBuffer *cbuf, int* bestSolution, semaphores* sems);

#endif 
