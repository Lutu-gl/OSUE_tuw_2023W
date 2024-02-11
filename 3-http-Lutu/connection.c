/**
 * @file connection.c
 * @author Luca, xxxxxxxx (exxxxxxxx@student.tuwien.ac.at)
 * @brief Functions for managing HTTP connections.
 * @details This file contains functions to open a connection to a server, send an HTTP request, and read the response
 * @version 0.1
 * @date 2023-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <zlib.h>
#include "connection.h"


int setUpServer(char *port, char *doc_root){
    int sockfd;
    //struct addrinfo hints, *servinfo, *p;
    struct addrinfo hints, *servinfo;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop throu results, connect to the first one available
    //for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) <= -1) {   // Possible to but a loop around if 
        //if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {    
            perror("server: socket");
            return -1;
            //continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) <= -1) {
            perror("setsockopt");
            return -1;
        }

        //if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) <= -1) {
            perror("server: bind");
            return -1;
            //continue;
        }

        //break;
    //}


    if (servinfo == NULL)  {
        freeaddrinfo(servinfo); // no longer needed
        close(sockfd);
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }
    freeaddrinfo(servinfo); // no longer needed

    if (listen(sockfd, 10) == -1) {
        close(sockfd);
        perror("listen");
        return -1;
    }

    return sockfd;
}
// url z.b> neverssl.com
// ret fd
int openConnection(URLComponents* components){
    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int res = getaddrinfo(components->hostname, components->port, &hints, &ai);
    if(res != 0){
        fprintf(stderr, "GetaddrInfo Error\n");
        return -1;
    }

    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sockfd < 0){
        fprintf(stderr, "socket Error\n");
        return -1;
    }

    if(connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0){
        fprintf(stderr, "connect Error\n");
        return -1;
    }
    
    freeaddrinfo(ai);   // because of getaddrinfo
    return sockfd;
}

int sendRequestClient(char* header, FILE* sockfile){
    // Request
    if(fputs(header, sockfile) == EOF){     // similar to fprintf, without formatting
        fprintf(stderr, "fputs Error\n");
        fclose(sockfile);
        return EXIT_FAILURE;
    }
    if(fflush(sockfile) == EOF){            // has to be flushed to make sure it gets send!
        fprintf(stderr, "fflush Error\n");
        fclose(sockfile);
        return EXIT_FAILURE;
    }

    return 0;
}

int readResponseClient(FILE* sockfile, FILE* out){
    char buf[1024];
    int line = 0;
    int useComp = 0;
    //printf("reading\n");
    // header
    while (fgets(buf, sizeof(buf), sockfile) != NULL){
        //fputs(buf, out);
        if(line == 0){ 
            if(strncmp(buf, "HTTP/1.1", 8) != 0){
                fprintf(stderr, "Protocol error!\n");
                return 2;
            }
            // check status 
            char* status = strtok(buf, " ");
            if(status == NULL){
                fprintf(stderr, "Protocol error!\n");
                return 2;
            }
            status = strtok(NULL, " ");
            if(status == NULL){
                fprintf(stderr, "Protocol error!\n");
                return 2;
            }
            char* endptr = NULL;
            errno = 0;
            int statusInt = strtol(status, &endptr, 10);
            if ((errno == ERANGE && (statusInt == LONG_MAX || statusInt == LONG_MIN)) || (errno != 0 && statusInt == 0)) {
                fprintf(stderr, "Protocol error!\n");
                return 2;
            }
            if (endptr == status) {
                fprintf(stderr, "Protocol error!\n");
                return 2;
            }
        
            char* statusDes = strtok(NULL, "\r\n\r\n");
            if(statusDes == NULL){
                fprintf(stderr, "Protocol error!\n");
                return 2;
            }

            if(statusInt != 200){
                fprintf(stderr, "%d %s\n", statusInt, statusDes);
                return 3;
            }
        } else if(strcmp(buf, "\r\n") == 0){    // header finish
            break;
        } else if(strncmp(buf, "Content-Encoding: gzip", strlen("Content-Encoding: gzip")) == 0){
            useComp = 1;
        }
        
        line++;
    }

    
    //easy code to read non-binary body
    //while (fgets(buf, sizeof(buf), sockfile) != NULL){
    //    fputs(buf, out);
    //}
    
    // Read body!
    if(useComp == 0){   // no compression used!
        size_t bytesRead, bytesWritten;
        char buffer[4096];

        while ((bytesRead = fread(buffer, 1, sizeof(buffer), sockfile)) > 0) {
            char *ptr = buffer;
            while(bytesRead > 0){
                bytesWritten = fwrite(ptr, 1, bytesRead, out);
                if(bytesWritten < bytesRead){
                    if(ferror(out)) {
                        fprintf(stderr, "Error: readResponseClient while fwrite\n");
                        return EXIT_FAILURE;
                    }
                    ptr += bytesWritten;
                    bytesRead -= bytesWritten;
                }else{
                    // all bytes written!
                    break;
                }
            }
        }
        

        if(ferror(sockfile)){
            fprintf(stderr, "Error: Reading from socket in readResponseClient\n");
            return EXIT_FAILURE;
        }

        if(!feof(sockfile)){
            fprintf(stderr, "Error: No eof encountered in readResponseClient\n");
            return EXIT_FAILURE;
        }
    } else {        // Compression is used: Decompression neccessary
        // write to temp File
        FILE* tempF = fopen("tempF.gz", "wb");        // read binary
        if(tempF == NULL){
            fprintf(stderr, "Error: Creating of tempory file for decompression!\n");
            return EXIT_FAILURE;
        }        
        char c;
        size_t bytesRead, bytesWritten;
        while ((bytesRead = fread(&c, 1, 1, sockfile)) > 0) {
            bytesWritten = fwrite(&c, 1, bytesRead, tempF);
            if(bytesWritten < bytesRead){
                fprintf(stderr, "Error: readResponseClient while fwrite to tempF\n");
                return EXIT_FAILURE;
            }
        }
        fclose(tempF);

        // Decompress temp File and write to out stream!
        gzFile gzF = gzopen("tempF.gz", "rb");
        if(gzF == NULL){
            fprintf(stderr, "Error: Reading of tempory file for decompression!\n");
            return EXIT_FAILURE;
        }
        
        char buffer[4096];
        while ((bytesRead = gzread(gzF, buffer, sizeof(buffer))) > 0) {
            char *ptr = buffer;
            while(bytesRead > 0){
                bytesWritten = fwrite(ptr, 1, bytesRead, out);
                if(bytesWritten < bytesRead){
                    if(ferror(out)) {
                        fprintf(stderr, "Error: readResponseClient while fwrite\n");
                        return EXIT_FAILURE;
                    }
                    ptr += bytesWritten;
                    bytesRead -= bytesWritten;
                }else{
                    // all bytes written!
                    break;
                }
            }
        }
        gzclose(gzF);

        // Delete temporary file
        if (remove("tempF.gz") != 0) {
            fprintf(stderr, "Warning: Could not delete temp file!\n");  // no EXIT because its not a fatal error
        }
    }

    return EXIT_SUCCESS;
}

int readHeaderServer(int connfd, char* header){
    int index = 0;
    char current_char;
    int end_sequence_matched = 0;
    char* headerEnd = "\r\n\r\n";
    int numRead=0;

    while (index < 1024 - 1) {
        numRead = read(connfd, &current_char, 1);  // returned num read bytes. 0 when End of Stream or Socket closed. neg num equals error!
        if (numRead == 0){  // End of stream reached
            snprintf(header, 50, "Missing \\r\\n\\r\\n!\n");        // leads to an error in checkHeader! >)
            return EXIT_SUCCESS;
        }
        if (numRead != 1) {
            if (errno == EINTR) { // If during read an interruption happens it returns to reading until it reaches end of connection
                continue;
            }
            perror("read");
            return EXIT_FAILURE;
        }
        
        header[index] = current_char;
        index++;

        // Check if header end is reached
        if (index >= 4) {
            end_sequence_matched = 1;
            for (int i = 0; i < 4; i++) {
                if (header[index - 4 + i] != headerEnd[i]) {
                    end_sequence_matched = 0;
                    break;
                }
            }
        }

        if (end_sequence_matched) {
            break; 
        }
    }

    if (!end_sequence_matched) {
        fprintf(stderr, "Error in readHeaderServer: Header end sequence not found\n");
        return -1;
    }

    header[index] = '\0';
    return EXIT_SUCCESS;
}

int sendHeaderServer(FILE* sockfile, char* respHeader){
    if(sockfile == NULL || respHeader == NULL){
        fprintf(stderr, "Error in params of sendHeaderServer");
        return EXIT_FAILURE;
    }

    if(fputs(respHeader, sockfile) == EOF){    
        fprintf(stderr, "fputs Error\n");
        fclose(sockfile);
        return EXIT_FAILURE;
    }
    if(fflush(sockfile) == EOF){            // has to be flushed to make sure the data is sent
        fprintf(stderr, "fflush Error\n");
        fclose(sockfile);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int sendContentServer(FILE* sockfile, FILE* file){
    size_t bytesRead, bytesWritten;
    char buffer[4096]; 
    if (sockfile == NULL || file == NULL) {
        fprintf(stderr, "Error: Wrong params in sendContentServer\n");
        return EXIT_FAILURE;
    }

    while (!feof(file)) {
        bytesRead = fread(buffer, 1, sizeof(buffer), file);
        if(bytesRead <= 0){
            break;
        }

        char *ptr = buffer;
        while (bytesRead > 0) {
            bytesWritten = fwrite(ptr, 1, bytesRead, sockfile);
            if (bytesWritten < bytesRead) {
                if (ferror(sockfile)) {
                    fprintf(stderr, "Error: Writing to socket file in sendContentServer\n");
                    clearerr(sockfile);
                    return EXIT_FAILURE;
                }

                bytesRead -= bytesWritten;
                ptr += bytesWritten;
            } else {
                // all bytes written!
                break;
            }
        }

        if(fflush(sockfile) == EOF){
            fprintf(stderr, "Error in fflush in sendContentServer\n");
            fclose(sockfile);
            return EXIT_FAILURE;
        }
    }
    if (ferror(file)) {
        fprintf(stderr, "Error: Reading from file in sendContentServer\n");
        return EXIT_FAILURE;
    }

    if (!feof(file)) {
        fprintf(stderr, "Error: Error in fread, no eof\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
int sendContentServer2(FILE* sockfile, FILE* file){
    char buf[1024];
    if (sockfile == NULL || file == NULL) {
        fprintf(stderr, "Error: Wrong params in sendContentServer\n");
        return EXIT_FAILURE;
    }

    while(fgets(buf, sizeof(buf), file) != NULL){
        if(fputs(buf, sockfile) == EOF){
            fprintf(stderr, "Error in sendContentServer while fputs to server\n");
            return EXIT_FAILURE;
        }
    }
    if(ferror(file)){
        fprintf(stderr, "Error in sendContentServer while fgets content of file\n");
        return EXIT_FAILURE;  
    }
    if(fflush(sockfile) == EOF){
        fprintf(stderr, "Error in fflush in sendContentServer\n");
        fclose(sockfile);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}*/