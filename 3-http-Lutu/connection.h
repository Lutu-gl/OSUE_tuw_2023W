/**
 * @file connection.h
 * @author Luca, xxxxxxxx (exxxxxxxx@student.tuwien.ac.at)
 * @brief Functions for managing HTTP connections.
 * @details This file contains functions to open a connection to a server, send an HTTP request, and read the response
 * @version 0.1
 * @date 2023-12-26
 */

#ifndef CONNECTION_H
#define CONNECTION_H

/**
 * @brief Stores individual components of a URL.
 * @details This structure is used to hold the different parts of a URL after parsing, including the hostname, port, and path.
 */
typedef struct {
    char* hostname; /**< The hostname component of the URL. */
    char* port;  /**< The port component of the URL. */
    char* path; /**< The path component of the URL. */
    char* qString; /**< Query String component of the URL. */
} URLComponents;


/**
 * @brief Sets up the server socket and binds it to the specified port.
 * @details This function initializes a server socket, sets socket options, and binds it to the given port.
 *          It uses getaddrinfo to support both IPv4 and IPv6. The function iterates through the results of getaddrinfo
 *          and tries to set up a socket and bind it until successful.
 * @param port The port number as a string on which the server will listen.
 * @param doc_root The document root directory from which the server will serve files. Currently not used in this function.
 * @return The socket file descriptor on success, -1 if getaddrinfo fails, or -2 if binding fails.
 */
int setUpServer(char *port, char *doc_root);

/**
 * @brief Opens a connection to the server specified in URL components.
 * @details This function creates a socket and establishes a connection to the server based on the hostname and port in the URL components.
 * @param components A pointer to the URLComponents structure containing the details (hostname, port) needed to establish the connection.
 * @return The file descriptor for the opened socket or -1 on error.
 */
int openConnection(URLComponents* components);

/**
 * @brief Sends an HTTP request over the socket file stream.
 * @details This function writes the HTTP header to the socket file stream and flushes it to ensure it's sent immediately.
 * @param header A string containing the prepared HTTP request header.
 * @param sockfile A FILE pointer to the socket file stream where the request will be sent.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int sendRequestClient(char* header, FILE* sockfile);

/**
 * @brief Reads the HTTP response from the socket file stream.
 * @details This function reads the response line by line, checks the HTTP status code, and writes the response body to the output file.
 * @param sockfile A FILE pointer to the socket file stream to read the response from.
 * @param out A FILE pointer to the output file where the response body will be written.
 * @return EXIT_SUCCESS on success, an error code (2 or 3) on failure.
 */
int readResponseClient(FILE* sockfile, FILE* out);

/**
 * @brief Reads the HTTP header from a connection socket.
 * @details This function reads the header from the socket one character at a time,
 *          accumulating characters until the end of the header is reached, marked by "\r\n\r\n".
 *          It handles partial reads and signal interruptions.
 * @param connfd File descriptor for the connection socket.
 * @param header Pointer to the buffer where the header will be stored.
 * @return EXIT_SUCCESS on successful read of the entire header, EXIT_FAILURE on error.
 */
int readHeaderServer(int connfd, char* header);

/**
 * @brief Sends the HTTP response header to the client.
 * @details This function sends the prepared HTTP response header to the client using the socket file stream.
 *          It ensures that the header is flushed from the buffer and sent to the client.
 * @param sockfile FILE pointer to the socket file stream.
 * @param respHeader Pointer to the buffer containing the response header to be sent.
 * @return EXIT_SUCCESS on successful sending, EXIT_FAILURE on error.
 */
int sendHeaderServer(FILE* sockfile, char* respHeader);

/**
 * @brief Sends the content of the requested file to the client.
 * @details This function reads the content of the requested file and sends it to the client.
 *          It handles the file and socket file streams to ensure the content is correctly transmitted.
 * @param sockfile FILE pointer to the socket file stream for sending data to the client.
 * @param file FILE pointer to the file to be sent.
 * @return EXIT_SUCCESS on successful sending of the content, EXIT_FAILURE on error.
 */
int sendContentServer(FILE* sockfile, FILE* file);

#endif 