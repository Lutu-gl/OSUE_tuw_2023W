/**
 * @file client.c
 * @author Luca, xxxxxxxx (exxxxxxxx@student.tuwien.ac.at)
 * @brief Simple HTTP client implementation.
 * @details This file contains the implementation of a simple HTTP client 
 *          that can send requests to a specified server and receive responses. 
 *          It handles basic URL parsing, header generation, and manages 
 *          the connection to the server. The client supports specifying 
 *          the port, output file, or directory for response storage, 
 *          and the URL of the server.
 * @version 0.1
 * @date 2023-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h> // For basename func
#include <zlib.h>
#include "connection.h"


static char* prog_name; /**< a char pointer to the name of the program. The name that is in the arguments at pos. 0  (argv[0]). Used for error messages */


/**
 * @brief Parses a URL into its components.
 * @details This function splits the given URL into hostname, path, and port. It expects URLs in the format 'http://hostname/path'.
 * @param url A string containing the URL to be parsed.
 * @param components A pointer to the URLComponents structure where the parsed components will be stored.
 * @return EXIT_SUCCESS on successful parsing, EXIT_FAILURE otherwise.
 */
static int urlParse(char* url, URLComponents *components){
    if(url == NULL || components == NULL){
        return EXIT_FAILURE;
    }

    char* urlCopy = strdup(url);
    char *endP = strstr(urlCopy, "http://");
    if(endP != NULL){
        endP += 7;
    }else{ //fehlerhaft
        return EXIT_FAILURE;
        endP = urlCopy;
    }
    char* hostnameStart = endP;
    char* endP2 = endP;

    endP2 = strpbrk(endP, ";/?:@=&"); // hostname bevor these vals
    endP = strchr(endP, '/'); //first / and path starts!
    if(endP != NULL){
        char savedChar = *endP2;
        *endP2 = '\0';
        components->hostname = strdup(hostnameStart);
        *endP2 = savedChar;
        // so that ? QueryString  is split by the path!
        char* trav = endP;
        while(*trav != '\0'){ 
            if(*trav == '?'){ 
                components->qString = strdup(trav);
                *trav = '\0';
                break;
            } 
            trav++;
        }
        components->path = strdup(endP);
    }else{
        fprintf(stderr, "Error in UrlParse. No / in url\n");
        return EXIT_FAILURE;
    }

    free(urlCopy);
    return EXIT_SUCCESS;
}

/**
 * @brief Frees the memory allocated for URL components.
 * @details This function frees the memory allocated to the hostname and path components of the URL. The port is not freed as it references an argv element.
 * @param components A pointer to the URLComponents structure to be freed.
 */
static void freeURLComponents(URLComponents *components) {
    free(components->hostname);
    free(components->path);
    //free(components->port);   //not needed, because port points to an argv[] which we do not free
    if(components->qString != NULL) free(components->qString);
}

/**
 * @brief Generates an HTTP request header.
 * @details This function creates an HTTP request header using the provided URL components.
 * @param header A char pointer where the generated header will be stored.
 * @param components A pointer to the URLComponents structure containing the URL details.
 * @return EXIT_SUCCESS on successful generation, EXIT_FAILURE otherwise.
 */
static int generateHeader(char *header, URLComponents *components){
    //"GET / HTTP/1.1\r\nHost: neverssl.com\r\nConnection: close\r\n\r\n"
    int written = 0;
    if(components->qString != NULL)
        written = sprintf(header, "GET %s%s HTTP/1.1\r\nHost: %s:%s\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n",components->path, components->qString, components->hostname, components->port); // oder strcpy wueder auch gehen
    else
        written = sprintf(header, "GET %s HTTP/1.1\r\nHost: %s:%s\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n",components->path, components->hostname, components->port); // oder strcpy wueder auch gehen

    if(written < 0){
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Displays the program usage information.
 */
static void usage(void){
    printf("Usage: %s [-p PORT] [-o FILE | -d DIR] URL\n", prog_name);
    printf("[-p PORT]: Specify a port number for the connection [0, 65353]\n");
    printf("[-o FILE]: Output the response to a specified file.\n");
    printf("[-d DIR ]: Save the response to a specified directory.\n");
    printf("URL: The URL of the server to connect to.\n");
    printf("Note: -o and -d are mutually exclusive.\n");
}

/**
 * @brief Main entry point of the program.
 * @details This function processes command line arguments and initiates the process of sending an HTTP request.
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE or other return code otherwise.
 */
int main(int argc, char** argv){
    prog_name = argv[0];
    int opt=0;
    int argP=0;
    char* port="80";
    char* filename=NULL;
    char* dir=NULL;
    int argO=0;
    int argD=0;
    char* url = NULL;
    FILE* out = stdout;

    while((opt = getopt(argc, argv, "p:o:d:")) != -1){
        switch(opt){
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
                // Check if really finished
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
            case 'o':
                argO++;
                filename = optarg;
                //printf("[%s]\n", filename);
                break;
            case 'd':
                argD++;
                dir = optarg;
                //printf("[%s]\n", dir);
                break;
            case '?':
                usage();
                return EXIT_FAILURE;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }
    if(argP > 1 || (argO + argD > 1)){
        fprintf(stderr, "Error: Double Arguments given!\n");
        usage();
        return EXIT_FAILURE;
    }
    if(argc != optind + 1){
        usage();
        return EXIT_FAILURE;
    }
    url = argv[optind];

    URLComponents urlC;
    urlC.hostname = NULL;
    urlC.path = NULL;
    urlC.qString = NULL;
    urlC.port = port;
    int ret = urlParse(url, &urlC);
    if(ret != 0){
        fprintf(stderr, "Error parsing url\n");
        freeURLComponents(&urlC);
        return EXIT_FAILURE;
    }
    // Build header [GET / HTTP/1.1\r\nHost:\r\nConnection: close\r\n\r\n] = 44chars. 
    char* header = (char*) malloc(sizeof(char) * (100 + strlen(urlC.hostname) + strlen(urlC.path)));
    if(header == NULL){
        fprintf(stderr, "Error malloc'ing header\n");
        freeURLComponents(&urlC);
        return EXIT_FAILURE;
    }

    int val = generateHeader(header, &urlC);
    if(val != 0){
        fprintf(stderr, "Error generating header\n");
        freeURLComponents(&urlC);
        free(header);
        return EXIT_FAILURE;
    }
    
    //printf("TO: [%s] [%s] [%s]\n", urlC.hostname, urlC.path, urlC.port);

    // socket connection
    int sockfd = 0;
    sockfd = openConnection(&urlC);
    if (sockfd < 0){
        fprintf(stderr, "Error in openConnection\n");
        freeURLComponents(&urlC);
        free(header);
        return EXIT_FAILURE;
    }

    FILE *sockfile = fdopen(sockfd, "r+");
    if (sockfile == NULL){
        fprintf(stderr, "fdopen Error\n");
        freeURLComponents(&urlC);
        free(header);
        return EXIT_FAILURE;
    }

    // send request / get response 
    ret = sendRequestClient(header, sockfile);
    if(ret != 0){
        fprintf(stderr, "error in sendRequestClient\n");
        freeURLComponents(&urlC);
        free(header);
        fclose(sockfile);
        return EXIT_FAILURE;
    }

    if(argO == 1){
        out = fopen(filename, "w");
    }else if(argD == 1){
        //if(strcmp("/", urlC.path) == 0){
        size_t len = strlen(urlC.path);
        if (len > 0 && urlC.path[len - 1] == '/') {
            out = fopen(strcat(dir, "/index.html"), "w");
        }else{
            // name after the last '/' /
            char *fileName = basename(urlC.path);
            out = fopen(strcat(strcat(dir, "/"), fileName), "w");
        }
    }
    if(argO == 1 || argD == 1){
        if (out == NULL) {
            perror("Error when opening file");
            freeURLComponents(&urlC);
            free(header);
            fclose(sockfile);
            return EXIT_FAILURE;
        }
    }

    ret = readResponseClient(sockfile, out);
    if(ret != 0){
        if(ret == -1) fprintf(stderr, "error in readResponseClient\n");
        freeURLComponents(&urlC);
        free(header);
        fclose(sockfile);
        if(argO == 1 || argD == 1)
            fclose(out);
        return ret;
    }
    
    freeURLComponents(&urlC);
    free(header);
    fclose(sockfile);
    if(argO == 1 || argD == 1)
        fclose(out);
    return EXIT_SUCCESS;
}