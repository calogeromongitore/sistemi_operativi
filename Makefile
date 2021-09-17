
.prevent_execution:
	exit 0

CC = gcc
CC_FLAGS = -g

CLIENT_NAME = client
SERVER_NAME = server

MAIN_DIR = .
INCLUDE_DIRS = -I $(MAIN_DIR)/include
SRCS_SRV = $(shell find $(MAIN_DIR)/src/ -name 'srv_*.c' -o -name 'com_*.c')
SRCS_CLN = $(shell find $(MAIN_DIR)/src/ -name 'cln_*.c' -o -name 'com_*.c')

MAKE_CMD_SERVER = $(CC) $(CC_FLAGS) $(SERVER_NAME).c $(SRCS_SRV) -o $(SERVER_NAME) $(INCLUDE_DIRS) 
MAKE_CMD_CLIENT = $(CC) $(CC_FLAGS) $(CLIENT_NAME).c $(SRCS_CLN) -o $(CLIENT_NAME) $(INCLUDE_DIRS) 

all:
	@$(MAKE_CMD_SERVER)
	@$(MAKE_CMD_CLIENT)

client:
	@$(MAKE_CMD_CLIENT)

server:
	@$(MAKE_CMD_SERVER)

clean:
	@rm -f $(MAIN_DIR)/$(SERVER_NAME)
	@rm -f $(MAIN_DIR)/$(CLIENT_NAME)


