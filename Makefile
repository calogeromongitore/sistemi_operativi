
.prevent_execution:
	exit 0

define TEST1_CONFIG
workers: 1
totalmb: 128
maxfiles: 10000
maxqueue: 64
maxincomingdata: 2048
endef

export TEST1_CONFIG

TEST1_CONFIGFILE = config_makefile.txt
TEST1_SRVPIDFILE = SERVER.PID

CC = gcc
CC_FLAGS = -g
CC_LIBS = -pthread

CLIENT_NAME = client
SERVER_NAME = server

MAIN_DIR = .
INCLUDE_DIRS = -I $(MAIN_DIR)/include
SRCS_SRV = $(shell find $(MAIN_DIR)/src/ -name 'srv_*.c' -o -name 'com_*.c')
SRCS_CLN = $(shell find $(MAIN_DIR)/src/ -name 'cln_*.c' -o -name 'com_*.c')

MAKE_CMD_SERVER = $(CC) $(CC_LIBS) $(CC_FLAGS) $(SERVER_NAME).c $(SRCS_SRV) -o $(SERVER_NAME) $(INCLUDE_DIRS) 
MAKE_CMD_CLIENT = $(CC) $(CC_LIBS) $(CC_FLAGS) $(CLIENT_NAME).c $(SRCS_CLN) -o $(CLIENT_NAME) $(INCLUDE_DIRS) 

all:
	@$(MAKE_CMD_SERVER)
	@$(MAKE_CMD_CLIENT)

client:
	@echo "Building Client.."
	@$(MAKE_CMD_CLIENT)

server:
	@echo "Building Server.."
	@$(MAKE_CMD_SERVER)

clean:
	@rm -fv $(MAIN_DIR)/$(SERVER_NAME)
	@rm -fv $(MAIN_DIR)/$(CLIENT_NAME)
	@rm -fv $(MAIN_DIR)/$(TEST1_CONFIGFILE)

test1: client server
	@echo "$$TEST1_CONFIG" > $(TEST1_CONFIGFILE)
	@valgrind --leak-check=full ./$(SERVER_NAME) -f /tmp/socketfile.sk -s $(TEST1_CONFIGFILE) & echo $$! > $(TEST1_SRVPIDFILE)
	@sleep 5
	@./$(CLIENT_NAME) -f /tmp/socketfile.sk -w $(SERVER_NAME)& ./$(CLIENT_NAME) -f /tmp/socketfile.sk -w $(TEST1_CONFIGFILE)  

	@if [ -e $(TEST1_SRVPIDFILE) ]; then \
		kill -INT $$(cat $(TEST1_SRVPIDFILE)); \
	fi;

	@rm $(TEST1_SRVPIDFILE)
	@sleep 5