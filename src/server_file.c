/*
 ============================================================================
 Name        : server_file.c
 Author      : Seba Murillo
 Version     : 1.0
 Copyright   : GPL
 Description : file service
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <global.h>
#include <message.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <stdint.h>

#define FILES_FOLDER "images" ///< directory in which .iso images are stored

#define verbose ///< verbose mode

int FD_client;
int FD_transfer;

char* get_current_dir();
char* get_MD5(const char* target);
int get_message_queue();
int get_filename(int file_id, char* filename);
void transfer_file(int file_id, const char* device);
void get_file_list(char* file_list);
void await_message();
void list_files();

/// file server program entrypoint
/// @returns 1 on error, 0 on success
int main(void){
	printf("> Launching [SERVER_FILE]\n\n");
	get_current_dir();
	// get message queue
	if(get_message_queue() < 0){
		fprintf(stderr, "ERROR: systemV queue not initiated\n"
				"> launch [SERVER_MAIN] first\n");
		exit(EXIT_FAILURE);
	}
	list_files();
	await_message();
	printf("> Closing [SERVER_FILE]\n");
	return EXIT_SUCCESS;
}

/// prints the list of all files in the FILES_FOLDER directory
void list_files(){
	char string[PATH_MAX];
	get_file_list(string);
	printf("%s", string);
}

/// reads the FILES_FOLDER directory and stores it's information into 'file_list'
/// @param file_list the buffer in which to store the file list, **must be at least 4096 bytes long**
/// @returns a pointer to the provided buffer
void get_file_list(char* file_list){
	printf("[SERVER_FILE]: getting file list...\n");
	DIR* directory;
	struct dirent* dir_entity;
	strcpy(file_list, "> image list\n");
	char tmp[MESSAGE_SIZE];
	sprintf(tmp, TAB "%s  %-35s%-15s%-32s\n", "ID", "name", "size (B)", "MD5");
	strcat(file_list, tmp);
	char dir_path[PATH_MAX];
	sprintf(dir_path, "%s/%s", get_current_dir(), FILES_FOLDER);
	if((directory = opendir(dir_path)) == NULL){
		fprintf(stderr, "ERROR: opening %s (%s)\n", dir_path, strerror(errno));
		return;
	}
	int ID = 1;
	while((dir_entity = readdir(directory)) != NULL){
		if(strcmp(".", dir_entity->d_name) == 0) continue;
		if(strcmp("..", dir_entity->d_name) == 0) continue;
		FILE* file_ptr;
		char file_path[PATH_MAX];
		sprintf(file_path, "%s/%s", dir_path, dir_entity->d_name);
		if((file_ptr = fopen(file_path, "r")) == NULL){ // file does not exist
			fprintf(stderr, "ERROR: listing image [%s] (%s)\n", dir_entity->d_name, strerror(errno));
			return;
		}
		// https://www.systutorials.com/how-to-get-the-size-of-a-file-in-c/
		// stat_struct.st_size is in BYTES
		// get file size
		struct stat stat_struct;
		if(stat(file_path, &stat_struct) != 0){
			fprintf(stderr, "ERROR: reading file size (%s)\n", strerror(errno));
			fclose(file_ptr);
			return;
		}
		char* MD5 = get_MD5(file_path);
		sprintf(tmp, TAB "%d)  %-35s%-15d%s\n", ID++, dir_entity->d_name, (unsigned int) stat_struct.st_size, MD5);
		strcat(file_list, tmp);
		fclose(file_ptr);
	}
	strcat(file_list, "\n");
	closedir(directory);
}

/// obtains current working directory
/// @returns a pointer to a string containing current working directory
char* get_current_dir(){
	static char path[PATH_MAX];
	if(getcwd(path, sizeof(path)) == NULL){
		fprintf(stderr, "ERROR: getting current path (%s)\n", strerror(errno));
		return NULL;
	}
	//printf("[SEVER_FILE]: current directory is: %s\n", path);
	return path;
}

/// calculates MD5 hash for *target* file
/// @param target the file or device to be hashed
/// @returns a pointer to the MD5 hash
char* get_MD5(const char* target){
	int FD = open(target, O_RDONLY);
	if(FD < 0){
		fprintf(stderr, "ERROR: opening %s for MD5 hash (%s)\n", target, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// get size
	struct stat stat_struct;
	if(stat(target, &stat_struct) != 0){
		fprintf(stderr, "ERROR: reading file size (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// init MD5
	unsigned char MD5[MD5_DIGEST_LENGTH];
	char buffer[512];
	MD5_CTX CTX;
	long int reads = stat_struct.st_size / 512;
	long int R;
	MD5_Init(&CTX);
	// calculate MD5
	for(int i = 0;i < reads;i++){
		R = read(FD, buffer, sizeof(buffer));
		if(R < 0){
			fprintf(stderr, "ERROR: reading %s for MD5 hash (%s)\n", target, strerror(errno));
			return NULL;
		}
		MD5_Update(&CTX, buffer, (unsigned long) R);
	}
	if(stat_struct.st_size % 512 != 0){
		R = read(FD, buffer, (long unsigned) (stat_struct.st_size % 512));
		if(R < 0){
			fprintf(stderr, "ERROR: reading %s for MD5 hash (%s)\n", target, strerror(errno));
			return NULL;
		}
		MD5_Update(&CTX, buffer, (unsigned long) R);
	}
	MD5_Final(MD5, &CTX);
	close(FD);
	// transform to string
	char* result = calloc(2 * MD5_DIGEST_LENGTH + 1, sizeof(char));
	char tmp[10];
	for(int i = 0;i < MD5_DIGEST_LENGTH;i++){
		sprintf(tmp, "%02x", MD5[i]);
		strcat(result, tmp);
	}
	return result;
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
 "FILE LS"
 "FILE DOWN ????"
 "FILE KILL"
 */
/// enters the program into a loop where it processes incoming messages
///
/// the program reads the messages destined to it, process them, and responds to SERVER_MAIN in the same queue

void await_message(){
	printf("[SERVER_FILE]: awating messages...\n");
	char message[MESSAGE_SIZE];
	while(TRUE){
		get_msg(SERVER_FILE_MSG_TYPE, message); // get MAIN request
		printf("[SERVER_FILE]: processing message: [%s]\n", message);
		char* arg = strtok(message, " ");
		if(strcmp("FILE", arg) != 0){ // should NEVER happen
			fprintf(stderr, "ERROR: something went wrong with SERVER_FILE message queue\n");
			send_msg(SERVER_MAIN_MSG_TYPE, "[SERVER_AUTH] who was THAT for???"); // send response to MAIN
			return;
		}
		arg = strtok(NULL, " ");
		if(strcmp("LS", arg) == 0){
			get_file_list(message);
			printf("[SERVER_FILE]: sending file list to [SERVER_MAIN]\n");
			send_msg(SERVER_MAIN_MSG_TYPE, message); // send response to MAIN
		}else if(strcmp("DOWN", arg) == 0){
			int file_id;
			char* id = strtok(NULL, " "); // file_id
			char* dev = strtok(NULL, " "); // file_id
			if(id == NULL || dev == NULL){
				send_msg(SERVER_MAIN_MSG_TYPE, "[SERVER_FILE]: incorrect syntax, use: file down <file_id> <device>\n"); // send response to MAIN
				continue;
			}
			if((file_id = (int) strtol(id, NULL, 10)) == 0){
				send_msg(SERVER_MAIN_MSG_TYPE, "[SERVER_FILE]: incorrect syntax, use: file down <file_id> <device>\n"); // send response to MAIN
				continue;
			}
			transfer_file(file_id, dev);
		}else if(strcmp("KILL", message) == 0){
			printf("[SERVER_FILE] exiting...\n");
			exit(EXIT_SUCCESS);
		}
	}
}

/// obtains the name of a file given it's ID, and stores it in 'filename'
/// @param file_id the ID of the file, see list_files()
/// @param filename the buffer in which the filename will be stored, **must be at least MAX_FILENAME_SIZE bytes long**
/// @returns 1 on success, 0 on failure
int get_filename(int file_id, char* filename){
	DIR* directory;
	struct dirent* dir_entity;
	char dir_path[MAX_FILENAME_SIZE];
	sprintf(dir_path, "%s/%s", get_current_dir(), FILES_FOLDER);
	if((directory = opendir(dir_path)) == NULL){
		fprintf(stderr, "ERROR: opnening %s (%s)\n", dir_path, strerror(errno));
		return FALSE;
	}
	int ID = 0;
	while((dir_entity = readdir(directory)) != NULL){
		if(strcmp(".", dir_entity->d_name) == 0) continue;
		if(strcmp("..", dir_entity->d_name) == 0) continue;
		char file_path[PATH_MAX];
		sprintf(file_path, "%s/%s", dir_path, dir_entity->d_name);
		if(++ID == file_id){
			strcpy(filename, dir_entity->d_name);
			return TRUE;
		}
	}
	closedir(directory);
	// invalid image ID
	printf("[SERVER_FILE]: could not find file name for file_id: [%d]\n", file_id);
	return FALSE;
}

/// transfer the file pointed by file_id to the client
/// @param file_id the ID of the file, see list_files()
/// @param device the target in which file will be saved on the client's side (done so for simplicity of comms)
void transfer_file(int file_id, const char* device){
	printf("[SERVER_FILE]: setting up transfer for file ID: %d\n", file_id);
	FD_client = socket(AF_INET, SOCK_STREAM, 0); // TCP
	if(FD_client == INEX){
		fprintf(stderr, "ERROR: creating FD_client socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// SO_ZEROCOPY -> evita tantos cambios de contexto
	// SO_REUSEADDR -> permite el reuso del puerto inmediatamente despues de cerrar programa
	//if(setsockopt(FD_client, SOL_SOCKET, SO_REUSEADDR | SO_ZEROCOPY, &(int) {1}, sizeof(int)) < 0){
	if(setsockopt(FD_client, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0){
		fprintf(stderr, "ERROR: setsockopt() in socket FD_client (%s)\n", strerror(errno));
	}
	int FD_transfer;
	// setup server_address
	struct sockaddr_in server_address;
	memset((char*) &server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(SERVER_FILE_PORT);

	// bind to socket
	if(bind(FD_client, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
		fprintf(stderr, "ERROR: binding socket (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// setup client_address and start listening
	struct sockaddr_in client_address;
	int client_length = sizeof(client_address);
	printf("[SERVER_FILE]: waiting for client for transfer...\n");
	listen(FD_client, 1);
	send_msg(SERVER_MAIN_MSG_TYPE, START_FILE_TRANSFER_MSG);// send msg to SERVER_MAIN -> CLIENT to get ready for download
	FD_transfer = accept(FD_client, (struct sockaddr*) &client_address, (socklen_t*) &client_length);
	if(FD_transfer < 0){
		fprintf(stderr, "ERROR: accept incoming conenction (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("[SERVER_FILE]: accepted connection from [CLIENT]\n");
	char filename[MAX_FILENAME_SIZE];
	if(get_filename(file_id, filename) == FALSE){ // invalid file_id, close connection
		close(FD_client);
		close(FD_transfer);
		return;
	}
/*
	printf("[SERVER_FILE]: transfering file [%s]...\n", filename);
	// let client know the filename
	if(send(FD_transfer, filename, MAX_FILENAME_SIZE, 0) < 0){ // SEND filename
		fprintf(stderr, "ERROR: sending filename to [CLIENT] (%s)\n", strerror(errno));
		close(FD_client);
		close(FD_transfer);
		return;
	}
#ifdef verbose
	printf("[SERVER_FILE]: sent filename [%s]...\n", filename);
#endif
*/
	// see if client has permission to write
	if(send(FD_transfer, device, strlen(device), 0) < 0){ // SEND device
		fprintf(stderr, "ERROR: sending device to [CLIENT] (%s)\n", strerror(errno));
		close(FD_client);
		close(FD_transfer);
		return;
	}
#ifdef verbose
	printf("[SERVER_FILE]: sent device [%s]...\n", device);
#endif
	char buffer[BUFFER_SIZE];
	memset(buffer, '\0', sizeof(buffer)); // reset buffer
	if(recv(FD_transfer, buffer, BUFFER_SIZE, 0) < 0){ // GET OK
		fprintf(stderr, "ERROR: receiving OK from [CLIENT] (%s)\n", strerror(errno));
		close(FD_client);
		close(FD_transfer);
		return;
	}
#ifdef verbose
	printf("[SERVER_FILE]: receiving confirmation\n");
#endif
	if(strcmp(buffer, OK) != 0){
		printf("[SERVER_FILE]: [CLIENT] unable to write on [%s]\n", device);
		close(FD_client);
		close(FD_transfer);
		return;
	}
#ifdef verbose
	printf("[SERVER_FILE]: received OK\n");
	printf("[SERVER_FILE]: opening file for transfer\n");
#endif
	FILE* file_ptr;
	char file_path[BUFFER_SIZE];
	sprintf(file_path, "%s/%s/%s", get_current_dir(), FILES_FOLDER, filename);
	if((file_ptr = fopen(file_path, "rb")) == NULL){ // file does not exist
		fprintf(stderr, "ERROR: opening transfer file [%s] (%s)\n", file_path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	size_t read = 0;
	memset(buffer, '0', sizeof(buffer)); // reset buffer
#ifdef verbose
	printf("[SERVER_FILE]: starting transfer\n");
#endif
	// read file in chunks of BUFFER_SIZE and send
	while((read = fread(buffer, 1, sizeof(buffer) - 1, file_ptr)) > 0){
		// send chunck of BUFFER_SIZE to client
		//if(send(FD_transfer, buffer, read, MSG_ZEROCOPY) < 0){
		if(send(FD_transfer, buffer, read, 0) < 0){
			fprintf(stderr, "ERROR: transfering file to [CLIENT] (%s)\n", strerror(errno));
			fclose(file_ptr);
			close(FD_client);
			close(FD_transfer);
			return;
		}
		memset(buffer, '\0', sizeof(buffer));
	}
	fclose(file_ptr);
	close(FD_client);
	close(FD_transfer);
	printf("[SERVER_FILE]: file transfer complete\n");
}
