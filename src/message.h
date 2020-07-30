/*
 * message.h
 *
 *  Created on: Apr 7, 2020
 *      Author: seba
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/msg.h>

#define MESSAGE_QUEUE_KEY "/bin/ls" ///< file used for message queue key generation with ftok()
#define SERVER_MAIN_MSG_TYPE 1 ///< message type of messages read by SERVER_MAIN
#define SERVER_AUTH_MSG_TYPE 5 ///< message type of messages read by SERVER_AUTH
#define SERVER_FILE_MSG_TYPE 7 ///< message type of messages read by SERVER_FILE
#define MESSAGE_SIZE 1024 ///< maximum size of message

int Q_ID = -1;

struct message_struct{
	long type;
	char string[MESSAGE_SIZE];
};

/// sends message of type *type* containing *message*
/// @param type message type ID
/// @param message message to be sent
/// @returns the length of the message sent
int send_msg(const int type, const char* message){
	if(Q_ID < 0){
		fprintf(stderr, "ERROR: systemV queue not initiated\n");
		return 0;
	}
	struct message_struct msg;
	msg.type = type;
	strcpy(msg.string, message);
	int sent = 0;
	if((sent = msgsnd(Q_ID, &msg, MESSAGE_SIZE, 0)) < 0){
		if(errno == EIDRM){
			fprintf(stderr, "ERROR: [SERVER_MAIN] is offline\n");
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "ERROR: sending systemV message (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return sent;
}

/// get the first message of type *type* on the message queue and stores it in *message*
/// @param type message type ID
/// @param message buffer to store the message in (**must be MESSAGE_SIZE bytes long**)
/// @returns a pointer to the buffer *message*
char* get_msg(const int type, char* message){
	if(Q_ID < 0){
		fprintf(stderr, "ERROR: systemV queue not initiated\n");
		return NULL;
	}
	struct message_struct msg;
	memset(message, '\0', MESSAGE_SIZE);
	if(msgrcv(Q_ID, &msg, MESSAGE_SIZE, type, 0) < 0){
		if(errno == EIDRM){
			fprintf(stderr, "ERROR: [SERVER_MAN] is offline\n");
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "ERROR: sending systemV message (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	strcpy(message, msg.string);
	return message;
}
#endif /* MESSAGE_H_ */
