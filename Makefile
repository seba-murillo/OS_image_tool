# config
COMPILER 	= gcc
FLAGS 		= -Wall -Wextra -Wconversion -pedantic -pedantic-errors -O3
INCLUDE		= -Isrc
EXT 		= c
################################################################################
.PHONY = compile clean
all : client server

client : src/client.c src/ipcheck.h src/global.h src/MBR.h 
	@echo -n "- compiling $@... "
	@$(COMPILER) $(FLAGS) $(INCLUDE) src/client.c src/MBR.c -o $@ -lcrypto
	@echo "done"
	@echo "> client compiled"

server : server_auth server_file server_main	
	@echo "> all servers compiled"

server_auth : src/server_auth.c src/global.h src/message.h
	@echo -n "- compiling $@... "
	@$(COMPILER) $(FLAGS) $(INCLUDE) src/server_auth.c -o server_auth
	@echo "done"

server_file : src/server_file.c src/global.h src/message.h
	@echo -n "- compiling $@... "
	@$(COMPILER) $(FLAGS) $(INCLUDE) src/server_file.c -o server_file -lcrypto
	@echo "done"

server_main : src/server_main.c src/global.h src/message.h
	@echo -n "- compiling $@... "
	@$(COMPILER) $(FLAGS) $(INCLUDE) src/server_main.c -o server_main
	@echo "done"

clean:
	@echo -n "> cleaning... "
	@rm -rf client
	@rm -rf server_auth
	@rm -rf server_file
	@rm -rf server_main
	@echo "done"