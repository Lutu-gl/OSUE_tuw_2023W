/**
 * @file server.c
 * @author Luca, xxxxxxxx (exxxxxxxx@student.tuwien.ac.at)
 * @brief HTTP Server
 * @details This server program implements a part of the HTTP 1.1 protocol. 
 *          It waits for incoming connections from clients and serves requested files from the document root.
 * @version 0.1
 * @date 2023-12-26
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <zlib.h>
#include "connection.h"



static char* prog_name; /**< A char pointer to the name of the program. The name that is in the arguments at pos. 0 (argv[0]). Used for error messages and usage information. */

//volatile = to tell the compiler var can change unexpectatly
// sig_atomic_t = uint datatyp that is atomic!
volatile sig_atomic_t stop = 0; /**< atomic volatile that changes value when a signal call is received.  */


/**
 * @brief Signal handler to gracefully stop the server.
 * @details This function sets the stop variable when SIGTERM or SIGINT is received, allowing the server to shutdown gracefully.
 * @param signal The signal number.
 */
void handle_signal(int signal){
    stop=1;
}

/**
 * @brief Gets the size of a file.
 * @details This function returns the size of the file specified by the filename. It uses the stat system call to obtain file size.
 * @param filename The name of the file.
 * @return The size of the file in bytes, or -1 if an error occurs.
 */
static int getFileSize(const char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0) {
        return st.st_size; // Die Größe der Datei in Bytes
    } 
    perror("Fehler beim Abrufen der Dateiinformation");
    return -1;
}

/**
 * @brief Determines the MIME type based on the file extension.
 * @details This function takes a file path as input and extracts the file extension. 
 *          It then compares the file extension against known types to determine the MIME type.
 *          If the file extension is recognized, the corresponding MIME type is returned. 
 *          If the extension is not recognized, NULL is returned, indicating that the MIME 
 *          type could not be determined.
 * @param path Pointer to a string containing the file path.
 * @return The MIME type for known extensions or NULL for unknown extensions.
 */
static char* getMimeType(char* path) {
    char* ext = strrchr(path, '.');
    if (!ext) {
        return NULL;
    }
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "text/html";
    } else if (strcmp(ext, ".css") == 0) {
        return "text/css";
    } else if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    }
    return NULL;
}

// return = 400 (Bad Request)
// return = 501 (Not implemented)
// return = 404 (Not Found)
// return = 200 (ok)

/**
 * @brief Checks the HTTP header received from the client.
 * @details This function processes the HTTP header received from the client, checks the validity of the request, and prepares a response header (param respHeader). It also opens the requested file if it exists. 
 * @param connfd File descriptor for the connection socket.
 * @param header Pointer to the buffer containing the received HTTP header.
 * @param doc_root Root directory for document serving.
 * @param indexFilename Default index filename.
 * @param respHeader Buffer to store the response header.
 * @param file Pointer to a FILE pointer for the requested file. Has to only be closed if code of 200 is returned!
 * @param useComp Does the client accept Compression with gzip. useComp is set to 1 if true. If 1 is returned, then it is expected to delete the created temp file: tempFServer.gz to which the pointer file now points to!
 * @return HTTP status code indicating the result of the request processing. -1 if error with snprintf
 */
static int checkHeaderServer(int connfd, char* header, char* doc_root, char* indexFilename, char* respHeader, FILE** file, int* useComp){
    char method[256]; //  GET  
    char path[1024]; // Path
    char protocol[256]; // HTTP/1.1
    int len = 0;
    
    if (sscanf(header, "%s %s %s", method, path, protocol) < 3) {
        len = snprintf(respHeader, 1024, "HTTP/1.1 400 (Bad Request)\r\nConnection: close\r\n\r\n"); //[HTTP/1.1 200 OK\r\n\r\n]
        if (len < 0)
            return EXIT_FAILURE;
        return 400;
    }

    if (strcmp(protocol, "HTTP/1.1") != 0) {
        len = snprintf(respHeader, 1024, "HTTP/1.1 400 (Bad Request)\r\nConnection: close\r\n\r\n"); //[HTTP/1.1 200 OK\r\n\r\n]
        if (len < 0)
            return EXIT_FAILURE;
        return 400;
    }

    if (strcmp(method, "GET") != 0) {
        len = snprintf(respHeader, 1024, "HTTP/1.1 501 (Not implemented)\r\nConnection: close\r\n\r\n"); //[HTTP/1.1 200 OK\r\n\r\n]
        if (len < 0)
            return EXIT_FAILURE;        
        return 501;
    }

    if (path[strlen(path) - 1] == '/') {    // if end with / filename einfuegen
        strcat(path, indexFilename);
    }

    char fullPath[512];
    if(snprintf(fullPath, sizeof(fullPath), "%s%s", doc_root, path) < 0){
        return EXIT_FAILURE;
    }

    // open file, so that it cant be modified when length is calculated!
    *file = fopen(fullPath, "r");  
    if (*file == NULL) {
        snprintf(respHeader, 1024, "HTTP/1.1 404 (Not Found)\r\nConnection: close\r\n\r\n"); //[HTTP/1.1 200 OK\r\n\r\n]
        return 404;
    }
    long size = getFileSize(fullPath);
    if (size < 0) {
        snprintf(respHeader, 1024, "HTTP/1.1 404 (Not Found)\r\nConnection: close\r\n\r\n"); //[HTTP/1.1 200 OK\r\n\r\n]
        return 404;    
    }

    char timeBuffer[100];
    time_t now = time(0);
    struct tm *tm = gmtime(&now);
    strftime(timeBuffer, sizeof(timeBuffer), "%a, %d %b %y %T %z", tm); // in example of strftime speciefied R822 compliant date!
    
    // Check if the client accepts compression
    char* ptr;
    char find[4] = "gzip";
    // suchen ob Accept-Encoding exisitert und darin gzip suchen. 
    if ((ptr = strstr(header, "Accept-Encoding:")) != NULL) {
        while(*ptr != '\n'){
            if(strncmp(ptr, find, 4) == 0){
                *useComp = 1;
            }
            ptr++;
        }        
    }

    if(*useComp == 1){
        FILE *destFile = fopen("tempFServer.gz", "w+b"); // opening temp file for reading and writing binary
        if (destFile == NULL) {
            fprintf(stderr, "Error: Creating temporary file for compression\n");
            return EXIT_FAILURE;
        }
        gzFile destGzFile = gzdopen(fileno(destFile), "wb");
        if (destGzFile == NULL) {
            fprintf(stderr, "Error: Compressing\n");
            return EXIT_FAILURE;
        }

        char buffer[4096];
        size_t bytesRead;
        do {
            bytesRead = fread(buffer, 1, 4096, *file);
            if (gzwrite(destGzFile, buffer, bytesRead) != bytesRead) {  // da fehler. Es schreibt nix rein
                fprintf(stderr, "Error: Compressing\n");
                return EXIT_FAILURE;
            }
        } while (bytesRead == 4096);


        // close file
        gzclose(destGzFile);
        fclose(*file);
        fclose(destFile);

        // reopen file
        if ((*file = fopen("tempFServer.gz", "rb")) == NULL) {
            fprintf(stderr, "Error: Opening Compressed file, couldnt open tempFServer.gz\n");
            return EXIT_FAILURE;
        }
        
        size = getFileSize("tempFServer.gz"); // update size
        if (size < 0) {
            fprintf(stderr, "Error: Compressing, couldnt getFileSize of tempFServer.gz\n");
            return EXIT_FAILURE;    
        }
    }

    // Creation of Response-Headers
    const char* mimeType = getMimeType(fullPath);
    if (mimeType == NULL) { // MimeType not found
        if(*useComp == 1){
            len = snprintf(respHeader, 1024,
                    "HTTP/1.1 200 OK\r\n"
                    "Date: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "Content-Encoding: gzip\r\n"
                    "Connection: close\r\n\r\n",
                    timeBuffer, size);
        } else {
            len = snprintf(respHeader, 1024,
                "HTTP/1.1 200 OK\r\n"
                "Date: %s\r\n"
                "Content-Length: %ld\r\n"
                "Connection: close\r\n\r\n",
                timeBuffer, size);
        }
    } else {
        if(*useComp == 1){
            len = snprintf(respHeader, 1024,
                    "HTTP/1.1 200 OK\r\n"
                    "Date: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Encoding: gzip\r\n"
                    "Connection: close\r\n\r\n",
                    timeBuffer, size, mimeType);
        } else {
            len = snprintf(respHeader, 1024,
                    "HTTP/1.1 200 OK\r\n"
                    "Date: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "Content-Type: %s\r\n"
                    "Connection: close\r\n\r\n",
                    timeBuffer, size, mimeType);
        }
    }

    if(len < 0)
        return EXIT_FAILURE;

    return 200;
}

/**
 * @brief Displays the program usage information.
 */
static void usage(void){
    printf("Usage: %s [-p PORT] [-i INDEX] DOC_ROOT\n", prog_name);
    printf("[-p PORT]: Specify the port number on which the server will listen for incoming connections. If not used, the default port is 8080.\n");
    printf("[-i INDEX]: Specify the index filename to be served when the request path is a directory. If not used, the default filename is 'index.html'\n");
    printf("DOC_ROOT: Specify the document root directory path. It is the directory from which the server will serve the files.\n");
}


/**
 * @brief The main function of the HTTP server.
 * @details This function sets up the server, handles incoming connections, processes requests, and sends responses. It uses a loop to accept and handle connections until a termination signal is received.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE otherwise.
 */
int main(int argc, char** argv){
    prog_name = argv[0];    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    int getopt_result = 0;
    int argP = 0;
    int argI = 0;
    char* port = "8080"; // Default port value.
    char* indexFilename = "index.html"; // Default index file.
    char* doc_root = NULL;

    while((getopt_result = getopt(argc, argv, "p:i:")) != -1){
        switch(getopt_result){
            case 'p':
                argP++;
                port = optarg;
                char* endptr = NULL;
                errno = 0;
                int portInt = strtol(port, &endptr, 10);
                if ((errno == ERANGE && (portInt == LONG_MAX || portInt == LONG_MIN)) || (errno != 0 && portInt == 0)) {
                    fprintf(stderr, "Wrong Port given port = [0, 65353]\n");
                    usage();
                    return EXIT_FAILURE;
                }
                if (endptr == port) {
                    fprintf(stderr, "Wrong Port given port = [0, 65353]\n");
                    usage();
                    return EXIT_FAILURE;
                }

                if (*endptr != '\0') {
                    fprintf(stderr, "Wrong Port given port = [0, 65353]\n");
                    usage();
                    return EXIT_FAILURE;
                }
                if(portInt < 0 || portInt > 65353){   //to big ports
                    fprintf(stderr, "Wrong Port given port = [0, 65353]\n");
                    usage();
                    return EXIT_FAILURE;
                } 
                break;
            case 'i':
                argI++;
                indexFilename = optarg;
                //printf("[%s]\n", indexFilename);
                break;
            case '?':
                usage();
                return EXIT_FAILURE;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    if (optind == argc-1) {
        doc_root = argv[optind];
    } else {
        usage();
        return EXIT_FAILURE;
    }

    if(argP > 1 || !doc_root || argI > 1){
        fprintf(stderr, "Error: Invalid usage.\n");
        usage();
        return EXIT_FAILURE;
    }
    int sockfd = setUpServer(port, doc_root);
    if (sockfd < 0) {
        fprintf(stderr, "Error: Setting up Server failed\n");
        return EXIT_FAILURE;
    }


    while(!stop) {
        // New Connection
        int connfd=0;
        if ((connfd = accept(sockfd, NULL, NULL))<0) {
            if(stop) return EXIT_SUCCESS;
            perror("accept");
            return EXIT_FAILURE;
        }

        // Read Header 
        char header[1024];
        if(readHeaderServer(connfd, header) != 0){
            fprintf(stderr, "Error in readHeaderServer\n");
            close(connfd);
            return EXIT_FAILURE;
        }

        // Check header
        char respHeader[1024];
        FILE* file = NULL;
        int useComp = 0;
        int status = checkHeaderServer(connfd, header, doc_root, indexFilename, respHeader, &file, &useComp); // write to respHeader 
        if(status < 0){
            fprintf(stderr, "Error in checkHeaderServer (snprintf)\n");
            close(connfd);
            return EXIT_FAILURE;
        }

        //  Send Header
        FILE *sockfile = fdopen(connfd, "r+");
        if (sockfile == NULL){
            fprintf(stderr, "fdopen Error\n");
            close(connfd);
            if(status == 200){
                fclose(file);
                if(useComp == 1){
                    if (remove("tempFServer.gz") != 0) {
                        fprintf(stderr, "Warning: Could not delete temp file!\n");  // no EXIT because its not a fatal error
                    }
                }
            }

            return EXIT_FAILURE;
        }
        if(sendHeaderServer(sockfile, respHeader) != 0) {//respHeader senden
            fprintf(stderr, "Error in sendHeaderServer\n");
            close(connfd);
            fclose(sockfile);
            if(status == 200){
                fclose(file);
                if(useComp == 1){
                    if (remove("tempFServer.gz") != 0) {
                        fprintf(stderr, "Warning: Could not delete temp file!\n");  // no EXIT because its not a fatal error
                    }
                }
            }
            
            return EXIT_FAILURE;
        } 
        // Send Content
        if(status == 200){
            if(sendContentServer(sockfile, file) != 0){ // file_content  FILE* 
                fprintf(stderr, "Error in sendContentServer\n");
                close(connfd);
                if(file != NULL) fclose(file);
                if(sockfile != NULL) fclose(sockfile);
                if(useComp == 1){
                    if (remove("tempFServer.gz") != 0) {
                        fprintf(stderr, "Warning: Could not delete temp file!\n");  // no EXIT because its not a fatal error
                    }
                }
                return EXIT_FAILURE;
            }
            fclose(file);
            if(useComp == 1){
                if (remove("tempFServer.gz") != 0) {
                   fprintf(stderr, "Warning: Could not delete temp file!\n");  // no EXIT because its not a fatal error
                }
            }
        }
        // Close Connection
        close(connfd);
        fclose(sockfile);

        // Go back to Accepting
        //printf("Connection to Client closed, everything send? :)\n");
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
