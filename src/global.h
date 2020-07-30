/*
 * global.h
 *
 *  Created on: Mar 29, 2020
 *      Author: seba
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_

// GLOBAL
#define TRUE 1
#define FALSE 0
#define INEX -1
#define TAB "    "
#define BUFFER_SIZE 1024 ///< default buffer size for socket communications
#define OK "OK" ///< 'OK' message
#define NO "NO" ///< 'NO' message

// SERVER_MAIN
#define SERVER_MAIN_PORT 37777 ///< default port for communication with client

// SERVER_FILE
#define MAX_FILENAME_SIZE 256 ///< maximum filename size
#define START_FILE_TRANSFER_MSG "SETUP_FILETRANSFER" ///< 'signal' used to informe client to start preparations for file transfer
#define SERVER_FILE_PORT 37778 ///< default port for file-transfering

extern int make_iso_compilers_happy; // evita warning

#endif /* GLOBAL_H_ */
