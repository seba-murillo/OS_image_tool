/*
 ============================================================================
 Name        : server_auth.c
 Author      : Seba Murillo
 Version     : 1.0
 Copyright   : GPL
 Description : authentication service
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <global.h>
#include <message.h>

// SERVER_AUTH
#define MAX_USERNAME_SIZE 30 ///< maximum username length
#define MAX_PASSWORD_SIZE 15 ///< maximum user password length
#define USER_DDBB_FILE "USER_DDBB" ///< user database filename
#define USER_DDBB_TMP ".tmp" ///< temporary filename used int update_current_user()

#define verbose ///< verbose mode

int user_auth(const char* user, const char* password);
int user_change_password(const char* new_password);
int update_current_user();
int get_message_queue();
char* get_user_list(char* user_list);
void create_user_database();
void list_users();
void await_message();

typedef struct current_user{
	char name[MAX_USERNAME_SIZE];
	char pass[MAX_PASSWORD_SIZE];
	int strikes;
	int ban;
} user;

user current_user;

/// auth server program entrypoint
/// @returns 1 on error, 0 on success
int main(void){
	printf("> Launching [SERVER_AUTH]\n\n");
	// get message queue
	if(get_message_queue() < 1){
		fprintf(stderr, "ERROR: systemV queue not initiated\n"
				"> launch [SERVER_MAIN] first\n");
		exit(EXIT_FAILURE);
	}
	memset(current_user.name, '\0', MAX_USERNAME_SIZE);
#if debug > 0
	create_user_database();
	list_users();
	user_auth("FAKE_USER", "123");
	user_auth("usuario", "fake_pass");
	user_auth("usuario", "fake_pass");
	user_auth("usuario", "fake_pass");
	user_auth("usuario", "1234");
	user_auth("admin", "1234");
	user_auth("admin", "admin");
	user_change_password("hey_there");
	list_users();
#endif
	list_users();
	await_message();
	printf("> Closing [SERVER_AUTH]\n");
	return EXIT_SUCCESS;
}

/// creates user database for testing purposes **DO NO USE**
void create_user_database(){
	FILE* file_ptr;
	if((file_ptr = fopen(USER_DDBB_FILE, "w")) == NULL){
		fprintf(stderr, "ERROR: creating databse (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fprintf(file_ptr, "%s %s 0 0\n", "usuario", "1234");
	fprintf(file_ptr, "%s %s 0 0\n", "admin", "admin");
	fprintf(file_ptr, "%s %s 0 0\n", "alumno", "alu1234");
	fprintf(file_ptr, "%s %s 0 0\n", "seba", "1234");
	fprintf(file_ptr, "%s %s 0 0\n", "client", "client");
	fclose(file_ptr);
}

/// auth server program entrypoint
int user_auth(const char* user, const char* password){
	if(user == NULL || password == NULL) return FALSE;
	FILE* file_ptr;
	if((file_ptr = fopen(USER_DDBB_FILE, "r")) == NULL){ // file does not exist
		fprintf(stderr, "ERROR: opening user database (%s)\n", strerror(errno));
		return FALSE;
	}
	char read_user[MAX_USERNAME_SIZE];
	char read_pass[MAX_PASSWORD_SIZE];
	int read_strikes;
	int read_ban;
	int io_chars;
	while((io_chars = fscanf(file_ptr, "%s %s %d %d", read_user, read_pass, &read_strikes, &read_ban)) != EOF){
		if(io_chars < 0){
			fprintf(stderr, "ERROR: reading user database [out of format] (%s)\n", strerror(errno));
			fclose(file_ptr);
			return FALSE;
		}
		if(strcmp(user, read_user) == 0){ // line is user
			strcpy(current_user.name, read_user);
			strcpy(current_user.pass, read_pass);
			current_user.strikes = read_strikes;
			current_user.ban = read_ban;
			if(read_ban == TRUE){
				printf("[SERVER_AUTH]: '%s' tried to login, but is banned\n", user);
				fclose(file_ptr);
				return FALSE; // user is banned
			}
			if(strcmp(password, current_user.pass) == 0){ // login OK
				current_user.strikes = 0;
				printf("[SERVER_AUTH]: '%s' just logged in\n", current_user.name);
				fclose(file_ptr);
				update_current_user();
				return TRUE;
			}
			// wrong password...
			printf("[SERVER_AUTH]: '%s' entered a wrong password\n", current_user.name);
			/*
			 current_user.strikes++;
			 printf("[SERVER_AUTH] '%s': wrong password\n", current_user.name, current_user.strikes);
			 if(current_user.strikes > 2){
			 current_user.ban = TRUE;
			 printf("[SERVER_AUTH] '%s' is now BANNED\n", current_user.name);
			 }
			 fclose(file_ptr);
			 update_current_user(); // register new strikes and/or bans
			 */
			fclose(file_ptr);
			return FALSE; // wrong password
		}
	}
	// user does not exist
	printf("[SERVER_AUTH] '%s' is not part of the user database\n", user);
	fclose(file_ptr);
	return FALSE;
}

/// changes the current user's password to new_password
///
/// changes the current user's password to new_password, which must be MAX_PASSWORD_SIZE characters long
/// at most, also updates the user database after the change
/// @returns the result of the update_current_user(), after changing the password
int user_change_password(const char* new_password){
	if(new_password == NULL) return FALSE;
	if(strlen(new_password) > MAX_PASSWORD_SIZE){
		perror("ERROR: new password is too long");
		fprintf(stderr, "ERROR: new password is too long (max: %d chars)\n", MAX_PASSWORD_SIZE);
		return FALSE;
	}
	printf("[SERVER_AUTH] %s just changed his password from [%s] to [%s]\n", current_user.name, current_user.pass, new_password);
	strcpy(current_user.pass, new_password);
	return update_current_user();
}

/// updates the current user, saving it's information into the database
int update_current_user(){
	printf("[SERVER_AUTH]: updating user '%s'...", current_user.name);
	FILE* file_read;
	FILE* file_write;
	if((file_read = fopen(USER_DDBB_FILE, "r")) == NULL){ // file does not exist
		fprintf(stderr, "ERROR: opening user database (%s)\n", strerror(errno));
		return FALSE;
	}
	if((file_write = fopen(USER_DDBB_TMP, "w")) == NULL){ // file does not exist
		fprintf(stderr, "ERROR: writing user database (%s)\n", strerror(errno));
		fclose(file_read);
		return FALSE;
	}
	char read_user[MAX_USERNAME_SIZE], read_pas[MAX_PASSWORD_SIZE];
	int read_strikes, read_ban, io_chars;
	int result = FALSE;
	while((io_chars = fscanf(file_read, "%s %s %d %d\n", read_user, read_pas, &read_strikes, &read_ban)) != EOF){
		if(io_chars < 0){
			fprintf(stderr, "ERROR: reading user database [out of format] (%s)\n", strerror(errno));
			fclose(file_read);
			fclose(file_write);
			return FALSE;
		}
		if(strcmp(current_user.name, read_user) == 0){ // line is current userprintf(" ((%s %s %d %d)) ", current_user.name, current_user.pass, current_user.strikes, current_user.ban);
			// replace line
			fprintf(file_write, "%s %s %d %d\n", current_user.name, current_user.pass, current_user.strikes, current_user.ban);
			result = TRUE; // user existed (it always should)
		}else{
			fprintf(file_write, "%s %s %d %d\n", read_user, read_pas, read_strikes, read_ban); // copy old line
		}
	}
	fclose(file_read);
	fclose(file_write);
	remove(USER_DDBB_FILE);
	rename(USER_DDBB_TMP, USER_DDBB_FILE);
	printf("done\n");
	return result; // if(OK == FALSE) user does not exist
}

/// prints the list of all users in the database
void list_users(){
	char string[MESSAGE_SIZE];
	get_user_list(string);
	printf("%s", string);
}

/// reads the user database and stores it's information into 'user_list'
/// @param user_list the buffer in which to store the user list, **must be at least 1024 bytes long**
/// @returns a pointer to the provided buffer
char* get_user_list(char* user_list){
	printf("[SERVER_AUTH]: getting user list...\n");
	FILE* file_ptr;
	if((file_ptr = fopen(USER_DDBB_FILE, "r")) == NULL){ // file does not exist
		fprintf(stderr, "ERROR: opening user database (%s)\n", strerror(errno));
		return NULL;
	}
	char read_user[MAX_USERNAME_SIZE];
	char read_pass[MAX_PASSWORD_SIZE];
	int read_strikes;
	int read_ban;
	char tmp[MESSAGE_SIZE];
	strcpy(user_list, "> user list:\n");
	sprintf(tmp, "%s%-30s %-10s %-10s\n", TAB, "name", "strikes", "banned");
	strcat(user_list, tmp);
	while(fscanf(file_ptr, "%s %s %d %d", read_user, read_pass, &read_strikes, &read_ban) != EOF){
		sprintf(tmp, "%s%-30s %-10d %-10d\n", TAB, read_user, read_strikes, read_ban);
		strcat(user_list, tmp);
	}
	strcat(user_list, "\n");
	fclose(file_ptr);
	return user_list;
}

/// obtains the systemV message queue ID for communication with SERVER_MAIN
/// @returns 0 on error, queue_id on success
int get_message_queue(){
	key_t key;
	if((key = ftok(MESSAGE_QUEUE_KEY, 66)) < 0){
		fprintf(stderr, "ERROR: getting systemV queue key (%s)\n", strerror(errno));
		return FALSE;
	}
	Q_ID = msgget(key, 0666); // ASK: flags necessary?
	return Q_ID;
}

/*
 "AUTH LOG %s %s"
 "AUTH LS"
 "AUTH PASS %s"
 "AUTH KILL"
 */

/// enters the program into a loop where it processes incoming messages
///
/// the program reads the messages destined to it, process them, and responds to SERVER_MAIN in the same queue
void await_message(){
	printf("[SERVER_AUTH]: awating messages...\n");
	char message[MESSAGE_SIZE];
	while(TRUE){
		get_msg(SERVER_AUTH_MSG_TYPE, message); // get MAIN request
		printf("[SERVER_AUTH]: processing message: [%s]\n", message);
		char* arg = strtok(message, " ");
		if(strcmp("AUTH", message) != 0){ // should NEVER happen
			fprintf(stderr, "ERROR: something went wrong with SERVER_AUTH message queue\n");
			send_msg(SERVER_MAIN_MSG_TYPE, "[SERVER_AUTH] who was THAT for???"); // send response to MAIN
			return;
		}
		arg = strtok(NULL, " ");
		if(strcmp("LOG", arg) == 0){
			char* user = strtok(NULL, " ");
			char* pass = strtok(NULL, " ");
			if(user_auth(user, pass) == TRUE){
				sprintf(message, "[SERVER_AUTH]: welcome back %s!\n", current_user.name);
			}else{
				sprintf(message, "[SERVER_AUTH]: incorrect user and/or password, try again\n");
			}
			send_msg(SERVER_MAIN_MSG_TYPE, message); // send response to MAIN
		}else if(strcmp("LS", arg) == 0){
			get_user_list(message);
			printf("[SERVER_AUTH]: sending user list to [SERVER_MAIN]\n");
			send_msg(SERVER_MAIN_MSG_TYPE, message); // send response to MAIN
		}else if(strcmp("PASS", arg) == 0){
			char* new_pass = strtok(NULL, " ");
			if(user_change_password(new_pass) != TRUE){
				send_msg(SERVER_MAIN_MSG_TYPE, "[SERVER_AUTH]: ERROR changing password (maybe too long)\n"); // send response to MAIN
				continue;
			}
			send_msg(SERVER_MAIN_MSG_TYPE, "[SERVER_AUTH]: password successfully changed\n"); // send response to MAIN
		}else if(strcmp("KILL", message) == 0){
			printf("[SERVER_AUTH] exiting...\n");
			exit(EXIT_SUCCESS);
		}
	}
}

