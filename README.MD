# OS_image_tool

OS_image_tool is a client-server software for easily downloading OS images into any location

## Installation

Simply use the makefile by typing:

```bash
make
```

## Usage - client
Simply run the program like you would any other:
```code
./client
```
keep in mind that the client will need a user/password (depending on the authentication server)

## Usage - server
The servers are composed of 3 services: server_main, server_file and server_auth, which can be run using:

```code
./server_auth
./server_file
./server_main
```

### server_auth
Handles the authentication of the users, read from a raw textfile (_USER_DDBB_)

### server_file
Handles the listing and transfer of the files in the _images_ folder

### server_main
Acts as middleware between the client and server_auth or server_file


### Important notes:
server_main MUST be run before the other two servers, since it needs to setup the message queue used for IPC.
