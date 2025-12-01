CC = gcc
CFLAGS = -Wall -O2

# --- Include paths ---
INCLUDES = -Icommon -Ihelper -Iserver -Iclient

# --- Common source files (shared across binaries) ---
COMMON_OBJ = common/frameq.o common/countq.o common/strq.o common/socket_util.o

# --- Helper specific ---
HELPER_OBJ = helper/thread_encdec.o helper/helper_pipeline.o helper/helper_util.o

# --- Server source files ---
SERVER_OBJ = server/vowel_pipeline.o

# --- Build rules ---
all: server_main client_main helper_service

server_main: server/server_main.o $(COMMON_OBJ) $(HELPER_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

client_main: client/client_main.o $(COMMON_OBJ) $(HELPER_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

helper_service: helper/helper_service.o $(COMMON_OBJ) $(HELPER_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f server/*.o client/*.o helper/*.o common/*.o
	rm -f server_main client_main helper_service