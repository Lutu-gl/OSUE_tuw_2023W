/**
 * @file cbuffer.c
 * @author Luca, xxxxxxx (exxxxxxx@student.tuwien.ac.at)
 * @brief Implementation of circular buffer functions for inter-process communication.
 * @details This file contains functions to create, open, and manipulate a circular buffer for inter-process communication. Generator.c and supervisor.c depend on it
 * @version 0.1
 * @date 2023-12-07
 */

#include "cbuffer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#define SHM_NAME "/xxxxxxx_SHM"
#define SEM_FREE_NAME "/xxxxxxx_SEMFREE"
#define SEM_USED_NAME "/xxxxxx_SEMUSED"
#define SEM_MUTEX_NAME "/xxxxxx_SEMMUTEX"

circularBuffer *create_cbuffer_server(int *shmfd, semaphores* sems){ 
    *shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0600);
    if(*shmfd == -1){
        fprintf(stderr, "ERROR: Creation of Shared mem didn't work!\n");
        return NULL;
    }

    if(ftruncate(*shmfd, sizeof(circularBuffer)) < 0){
        fprintf(stderr, "ERROR: ftruncate of shared mem didn't work!\n");
        return NULL;
    }

    circularBuffer *myshm;
    myshm = mmap(NULL, sizeof(*myshm), PROT_READ | PROT_WRITE, MAP_SHARED, *shmfd, 0);
    if(myshm == MAP_FAILED){
        fprintf(stderr, "ERROR: mmap of shared mem didn't work!\n");
        return NULL;
    }

	myshm->head=0;
    myshm->tail=0;
    myshm->stop=false;
    // create semaphore
    if ((sems->sem_free = sem_open(SEM_FREE_NAME, O_CREAT, 0600, BLOCK_SIZE)) == SEM_FAILED) {
        fprintf(stderr, "ERROR: creation sem free!\n");
        if(close_cbuff_server(*shmfd, myshm, NULL) == -1){
            fprintf(stderr, "ERROR: closing cbuffer!\n");
        }
        return NULL;
    }
    if ((sems->sem_used = sem_open(SEM_USED_NAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
        fprintf(stderr, "ERROR: creation sem used!\n");
        if(sem_close(sems->sem_free) == -1){
            fprintf(stderr, "ERROR: closing sem free\n");
        }
        if(sem_unlink(SEM_FREE_NAME) == -1){
            fprintf(stderr, "ERROR: unlinking sem free\n");
        }
        if(close_cbuff_server(*shmfd, myshm, NULL) == -1){
            fprintf(stderr, "ERROR: closing cbuffer!\n");
        }
        return NULL;
    }
    if ((sems->sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, 0600, 1)) == SEM_FAILED) {
        fprintf(stderr, "ERROR: creation sem mutex!\n");
        if(sem_close(sems->sem_used) == -1){
            fprintf(stderr, "ERROR: closing sem used\n");
        }
        if(sem_unlink(SEM_USED_NAME) == -1){
            fprintf(stderr, "ERROR: unlinking sem used\n");
        }

        if(sem_close(sems->sem_free) == -1){
            fprintf(stderr, "ERROR: closing sem free\n");
        }
        if(sem_unlink(SEM_FREE_NAME) == -1){
            fprintf(stderr, "ERROR: unlinking sem free\n");
        }

        if(close_cbuff_server(*shmfd, myshm, NULL) == -1){
            fprintf(stderr, "ERROR: closing cbuffer!\n");
        }
        return NULL;
    }

    return myshm;
}


circularBuffer *open_cbuffer_client(int *shmfd, semaphores *sems) {
    // open shared buffer
    *shmfd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (*shmfd == -1) {
        perror("shm_open");
        return NULL;
    }

    circularBuffer *myshm;
    myshm = mmap(NULL, sizeof(*myshm), PROT_READ | PROT_WRITE, MAP_SHARED, *shmfd, 0);
    if(myshm == MAP_FAILED){
        fprintf(stderr, "ERROR: mmap of shared mem didn't work!\n");
        return NULL;
    }

    // create semaphore
    if ((sems->sem_free = sem_open(SEM_FREE_NAME, 0)) == SEM_FAILED) {
        fprintf(stderr, "ERROR: creation sem free!\n");

        if(close_cbuff_client(*shmfd, myshm, NULL) == -1){
            fprintf(stderr, "ERROR: mmap of shared mem didn't work!\n");
        }
        return NULL;
    }
    if ((sems->sem_used = sem_open(SEM_USED_NAME, 0)) == SEM_FAILED) {
        fprintf(stderr, "ERROR: creation sem used!\n");
        if(sem_close(sems->sem_free) == -1){
            fprintf(stderr, "ERROR: closing sem free\n");
        }

        if(close_cbuff_client(*shmfd, myshm, NULL) == -1){
            fprintf(stderr, "ERROR: mmap of shared mem didn't work!\n");
        }
        return NULL;
    }
    if ((sems->sem_mutex = sem_open(SEM_MUTEX_NAME, 0)) == SEM_FAILED) {
        fprintf(stderr, "ERROR: creation sem mutex!\n");
        if(sem_close(sems->sem_used) == -1){
            fprintf(stderr, "ERROR: closing sem used\n");
        }
        if(sem_close(sems->sem_free) == -1){
            fprintf(stderr, "ERROR: closing sem free\n");
        }

        if(close_cbuff_client(*shmfd, myshm, NULL) == -1){
            fprintf(stderr, "ERROR: mmap of shared mem didn't work!\n");
        }
        return NULL;
    }   


    return myshm; 
}

int close_cbuff_server(int fd, circularBuffer* cb, semaphores* sems) {

    if (munmap(cb, sizeof(*cb)) == -1) {
        fprintf(stderr, "ERROR: unmapping shared memory\n");
        return -1;        
    }

    if(close(fd) == -1){
        fprintf(stderr, "ERROR: closing of shared mem didn't work!\n");
        return -1;
    }

    if (shm_unlink(SHM_NAME) == -1) {
        perror("unlinkin shared mem");
        return -1;
    }

	if(sems != NULL){
		int ret = 0;
		if (sems->sem_free != SEM_FAILED) {
			if (sem_close(sems->sem_free) == -1) {
				perror("sem_close");
				ret = -1;
			}
            if(sem_unlink(SEM_FREE_NAME) == -1){
                perror("unlinkin semaphore free");
                ret = -1;
            }
		}
		if (sems->sem_used != SEM_FAILED) {
			if (sem_close(sems->sem_used) == -1) {
				perror("sem_close");
				ret = -1;
			}
            if(sem_unlink(SEM_USED_NAME) == -1){
                perror("unlinkin semaphore used");
                ret = -1;
            }
		}
		if (sems->sem_mutex != SEM_FAILED) {
			if (sem_close(sems->sem_mutex) == -1) {
				perror("sem_close");
				ret = -1;
			}
            if(sem_unlink(SEM_MUTEX_NAME) == -1){
                perror("unlinkin semaphore mutex");
                ret = -1;
            }
		}
		if(ret != 0) return -1;
	}
    return 0;
}

int close_cbuff_client(int fd, circularBuffer* cb, semaphores* sems) {
    if (munmap(cb, sizeof(*cb)) == -1) {
        fprintf(stderr, "ERROR: unmapping shared memory \n");
        return -1;        
    }

    if(close(fd) == -1){
        fprintf(stderr, "ERROR: closing of shared mem didn't work!\n");
        return -1;
    }
	if(sems != NULL){
		int ret = 0;
		if (sems->sem_free != SEM_FAILED) {
			if (sem_close(sems->sem_free) == -1) {
				perror("sem_close");
				ret = -1;
			}
		}
		if (sems->sem_used != SEM_FAILED) {
			if (sem_close(sems->sem_used) == -1) {
				perror("sem_close");
				ret = -1;
			}
		}
		if (sems->sem_mutex != SEM_FAILED) {
			if (sem_close(sems->sem_mutex) == -1) {
				perror("sem_close");
				ret = -1;
			}
		}
		if(ret != 0) return -1;
	}
    return 0;
}

int write_to_cbuf(circularBuffer *cbuf, int *removeEdges, semaphores* sems) {
    //locken darf nicht vergessen werden!!!


    //sem_wait(sems->sem_free);
    //printf("vor writen\n");
    
    if (sem_wait(sems->sem_mutex) == -1) {
        perror("Error in sem_wait");
        return EXIT_FAILURE;
    }

	if (cbuf->stop) {
		if(sem_post(sems->sem_mutex) == -1){
            perror("Fehler bei sem_post");
            return -1;   
        }
		return 0;
	}
    int edgeCount = removeEdges[0];
    
	//printf("vor sem_free 0\n");
	


	if (sem_wait(sems->sem_free) == -1) {
        perror("Error in sem_wait");
        if(sem_post(sems->sem_mutex) == -1){
            perror("Fehler bei sem_post");
        }
        return EXIT_FAILURE;
    }
	
	if (cbuf->stop) {
        if(sem_post(sems->sem_mutex) == -1){
            perror("Fehler bei sem_post");
            return -1;
        }
		return 0;
	}
	
	
    cbuf->data[cbuf->head % BLOCK_SIZE] = edgeCount;
    cbuf->head +=1;
    if(sem_post(sems->sem_used) == -1){
        perror("Fehler bei sem_post");
        return -1;
    }

    for(int i = 1; i < edgeCount*2+1; i+=2){
		//printf("vor sem_free 1\n");
        if (sem_wait(sems->sem_free) == -1) {
            perror("Error in sem_wait");
            if(sem_post(sems->sem_mutex) == -1){
                perror("Fehler bei sem_post");
                return -1;
            }
            return EXIT_FAILURE;
        }
		
		if (cbuf->stop) {
            if(sem_post(sems->sem_mutex) == -1){
                perror("Fehler bei sem_post");
                return -1;
            }
			return 0;
		}		
		
        cbuf->data[ (cbuf->head) % BLOCK_SIZE] = removeEdges[i];
        if(sem_post(sems->sem_used) == -1){
            perror("Fehler bei sem_post");
            return -1;
        }

        if (sem_wait(sems->sem_free) == -1) {
            perror("Error in sem_wait");
            if(sem_post(sems->sem_mutex) == -1){
                perror("Fehler bei sem_post");
                return -1;
            }
            return EXIT_FAILURE;
        }

        cbuf->data[ (cbuf->head+1) % BLOCK_SIZE] = removeEdges[i+1];
        cbuf->head += 2;
        
        if(sem_post(sems->sem_used) == -1){
            perror("Fehler bei sem_post");
            return -1;
        }
    }
    //printf("in remove edges steht: %d: %d-%d\n", removeEdges[0], removeEdges[1], removeEdges[2]);
    //printf("in cbuf steht: %d: %d-%d\n", cbuf->data[0], cbuf->data[1], cbuf->data[2]);
    if(sem_post(sems->sem_mutex) == -1){
        perror("Fehler bei sem_post");
        return -1;
    }
	//printf("nach writen\n");
    //sem_post(sems->sem_used);

    return true;
}

int read_from_cbuf(circularBuffer *cbuf, int* bestSolution, semaphores* sems) {
    //sem_wait(sems->sem_used);
    
    if (sem_wait(sems->sem_used) == -1) {
        perror("Error in sem_wait");
        return EXIT_FAILURE;
    }
    int size = cbuf->data[cbuf->tail % BLOCK_SIZE];
    if(sem_post(sems->sem_free) == -1){
        perror("Fehler bei sem_post");
        return -1;
    }
    if(size == 0){
        printf("The graph is 3-colorable!\n");
        *bestSolution = 0;
        //sem_post(sems->sem_free);
        return 0;
    }else{
        if(size < *bestSolution){
            fprintf(stderr, "Solution with %d edges: ", size);
            *bestSolution = size;
        }else{
            cbuf->tail += size*2 + 1;
            
			for(int i=0; i < size; i++){
                if (sem_wait(sems->sem_used) == -1) {
                    perror("Error in sem_wait");
                    return EXIT_FAILURE;
                }
				if (sem_wait(sems->sem_used) == -1) {
                    perror("Error in sem_wait");
                    return EXIT_FAILURE;
                }
				
                if(sem_post(sems->sem_free) == -1){
                    perror("Fehler bei sem_post");
                    return -1;
                }
                if(sem_post(sems->sem_free) == -1){
                    perror("Fehler bei sem_post");
                    return -1;
                }			
            }
			//em_post(sems->sem_free);
            
            return 0;
        }
    }
    cbuf->tail+=1;

    for(int i=0; i < size; i++){
        if (sem_wait(sems->sem_used) == -1) {
            perror("Error in sem_wait");
            return EXIT_FAILURE;
        }
        if (sem_wait(sems->sem_used) == -1) {
            perror("Error in sem_wait");
            return EXIT_FAILURE;
        }

        int tail1 = cbuf->tail % BLOCK_SIZE;
        int tail2 = (cbuf->tail + 1) % BLOCK_SIZE;
        fprintf(stderr, "%d-%d ", cbuf->data[tail1], cbuf->data[tail2]);
        cbuf->tail += 2;
        
        if(sem_post(sems->sem_free) == -1){
            perror("Fehler bei sem_post");
            return -1;
        }        
        if(sem_post(sems->sem_free) == -1){
            perror("Fehler bei sem_post");
            return -1;
        }
    }
    fprintf(stderr, "\n");
    
    //sem_post(sems->sem_free);

    return 0;
}
