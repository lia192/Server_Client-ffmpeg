#include <stdio.h>

#define UX_SOCKET_PATH "/tmp/server.sock"
#define BUFFER_SIZE 4096
#define ADMIN_BUFFER_SIZE 1024
#define FILE_SIZE 256

typedef enum { kEncode = 1, kCut } OperationType;

typedef struct {
    OperationType operation;
    char encoder[10];                // Encoder
    char input_filename[FILE_SIZE];  // Filename for input
    char output_filename[FILE_SIZE]; // Filename for output
    long long length;                // Size of file
} RequestHeader;

typedef struct {
    char output_filename[FILE_SIZE];
    long length;
} ResponseHeader;

void send_all(int sockfd, const char *data, size_t total_bytes) {
    size_t bytes_sent = 0;
    while (bytes_sent < total_bytes)
    {
        bytes_sent += write(sockfd, data + bytes_sent, total_bytes - bytes_sent);
    }
}

void send_file(int socket_fd, FILE *file) {
    char buffer[BUFFER_SIZE];
    size_t bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send_all(socket_fd, buffer, bytes_read);
    }
}

void receive_all(int socket_fd, char *buffer, size_t bytes_total) {
    size_t bytes_received = 0UL;
    while (bytes_received < bytes_total)
    {
        bytes_received += read(socket_fd, buffer + bytes_received, bytes_total - bytes_received);
    }
}

void receive_file(int socket_fd, FILE *input_file, size_t file_length) {
    char buffer[BUFFER_SIZE];
    size_t bytes_received = 0UL;
    while (bytes_received < file_length)
    {
        // read buffer size but maybe it's file end, so need less
        size_t to_read = BUFFER_SIZE;
        if (file_length - bytes_received < BUFFER_SIZE)
        {
            to_read = file_length - bytes_received;
        }

        size_t bytes_read = read(socket_fd, buffer, to_read);
        printf("Chunk %d: %zu bytes read\n", chunk, bytes_read);
        bytes_received += bytes_read;
        printf("Chunk %d: %zu bytes received\n", chunk, bytes_received);
        fwrite(buffer, 1, bytes_read, input_file);
    }
}