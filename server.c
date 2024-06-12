#include <arpa/inet.h> // Include for inet_addr
#include <microhttpd.h>
#include <netinet/in.h> // Include for INET sockets
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <unistd.h>
#include <wait.h>

#include "common.h"

#define PORT 8080 // Define the port number for the server

#define PORT 8080 // Define the port number for the server

#define MAX_SOCKET_CONNECTIONS 128

typedef struct {
    int socket;
    struct sockaddr addr;
} client_thread_arg_t;

typedef struct {
    int server_socket;
} connection_thread_arg_t;

typedef struct {
    int client_socket;
    char filename[FILE_SIZE];
    char out_filename[FILE_SIZE];
    char encoder[10];
} file_processing_arg_t;

void *encode_handler(void *arg);
void send_response_file(int socket, char *filename, char *out_filename);

// Thread function to handle each client
void *python_client_handler(void *arg) {
    client_thread_arg_t *client_data = (client_thread_arg_t *)arg;
    int client_fd = client_data->socket;
    free(client_data);

    FILE *input_file = NULL;
    char unique_filename[FILE_SIZE];
    int unique_id = 1;

    while (1) {
        // Read header
        printf("READING HEADER\n");
        size_t headerSize = sizeof(RequestHeader);
        char header_buffer[headerSize];
        receive_all(client_fd, header_buffer, headerSize);

        for (int i = 0; i < 2; ++i) {
            putchar(header_buffer[i]);
        }
        printf("\n");

        // Cast the buffer to struct
        RequestHeader req;
        memcpy(&req, header_buffer, headerSize);
        printf("GOT HEADER\n");

        printf("%s\n%s\n", req.input_filename, req.output_filename);

        // Create local file and receive from client
        snprintf(unique_filename, sizeof(unique_filename), "%d_%d_%s",
                 client_fd, unique_id, req.input_filename);
        input_file = fopen(unique_filename, "wb");
        receive_file(client_fd, input_file, req.length);
        printf("Recived file\n");
        fclose(input_file);
        input_file = NULL;
        unique_id++;

        // Start a new thread for processing
        printf("File received, starting processing...\n");
        printf("operation = %d\n", req.operation);
        pthread_t processing_thread;
        file_processing_arg_t *processing_arg =
            malloc(sizeof(file_processing_arg_t));
        processing_arg->client_socket = client_fd;
        strcpy(processing_arg->filename, unique_filename);
        strcpy(processing_arg->out_filename, req.output_filename);
        strcpy(processing_arg->encoder, req.encoder);

        switch (req.operation) {
        case kEncode:
            printf("RIGHT OPERATION\n");
            pthread_create(&processing_thread, NULL, encode_handler,
                           processing_arg);
            return NULL;

        case kCut:
            // pthread_create(&processing_thread, NULL, cut_handler,
            // processing_arg);
            break;

        default:
            printf("Unknown operation\n");
            continue;
        }

        pthread_detach(processing_thread);
    }

    if (input_file)
        fclose(input_file);
    close(client_fd);
    return NULL;
}

// Thread function to handle each client
void *client_handler(void *arg) {
    client_thread_arg_t *client_data = (client_thread_arg_t *)arg;
    int client_fd = client_data->socket;
    free(client_data);

    FILE *input_file = NULL;
    char unique_filename[FILE_SIZE + 30];
    int unique_id = 1;

    while (1) {
        // Read header
        printf("READING HEADER\n");
        size_t headerSize = sizeof(RequestHeader);
        char header_buffer[headerSize];
        receive_all(client_fd, header_buffer, headerSize);

        for (int i = 0; i < 2; ++i) {
            putchar(header_buffer[i]);
        }
        printf("\n");

        // Cast the buffer to struct
        RequestHeader req;
        memcpy(&req, header_buffer, headerSize);
        printf("GOT HEADER\n");

        // printf("%s\n%s\n%s\n%d\n", req.encoder, req.input_filename,
        // req.output_filename, req.length);

        // Create local file and receive from client
        snprintf(unique_filename, sizeof(unique_filename), "%d_%d_%s",
                 client_fd, unique_id, req.input_filename);
        input_file = fopen(unique_filename, "wb");
        receive_file(client_fd, input_file, req.length);
        printf("Recived file\n");
        fclose(input_file);
        input_file = NULL;
        unique_id++;

        // Start a new thread for processing
        printf("File received, starting processing...\n");
        printf("operation = %d\n", req.operation);
        pthread_t processing_thread;
        file_processing_arg_t *processing_arg =
            malloc(sizeof(file_processing_arg_t));
        processing_arg->client_socket = client_fd;
        strcpy(processing_arg->filename, unique_filename);
        strcpy(processing_arg->out_filename, req.output_filename);
        strcpy(processing_arg->encoder, req.encoder);

        switch (req.operation) {
        case kEncode:
            pthread_create(&processing_thread, NULL, encode_handler,
                           processing_arg);
            break;

        case kCut:
            // pthread_create(&processing_thread, NULL, cut_handler,
            // processing_arg);
            break;

        default:
            printf("Unknown operation\n");
            continue;
        }

        pthread_detach(processing_thread);
    }

    if (input_file)
        fclose(input_file);
    close(client_fd);
    return NULL;
}

void *ux_connection_handler(void *arg) {
    connection_thread_arg_t *connection_arg = (connection_thread_arg_t *)arg;
    int server_fd = connection_arg->server_socket;

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1)
            continue;

        printf("Got new ux connection\n");

        pthread_t client_thread;
        client_thread_arg_t *arg = malloc(sizeof(client_thread_arg_t));
        arg->socket = client_fd;
        pthread_create(&client_thread, NULL, client_handler, arg);
        pthread_detach(client_thread); // Do not wait for thread termination
    }
}

void shutdown_handler(int sig) { printf("Server shutting down...\n"); }

void *encode_handler(void *arg) {
    file_processing_arg_t *data = (file_processing_arg_t *)arg;
    int socket = data->client_socket;
    char filename[FILE_SIZE];
    strcpy(filename, data->filename);
    char out_filename[FILE_SIZE];
    strcpy(out_filename, data->out_filename);
    char encoder[10];
    strcpy(encoder, data->encoder);

    printf("Starting encoding process for file %s...\n", data->filename);
    char output_filename[FILE_SIZE];
    snprintf(output_filename, sizeof(output_filename), "encoded_%s", filename);

    // Construct FFmpeg command to encode the video
    char command[1024];
    snprintf(command, sizeof(command),
             "ffmpeg -i \"%s\" -c:v %s -c:a copy \"%s\"", filename, encoder,
             output_filename);

    // Execute FFmpeg command
    system(command);

    printf("Encoding completed for file %s, output in %s\n", filename,
           output_filename);

    send_response_file(socket, output_filename, out_filename);
}

void send_response_file(int socket, char *filename, char *out_filename) {
    // Open file to determine the size and to send
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    ResponseHeader resp;
    strcpy(resp.output_filename, out_filename);

    // Determine file size and number of chunks
    fseek(file, 0, SEEK_END);
    resp.length = ftell(file);
    rewind(file);

    send_all(socket, (char *)&resp, sizeof(ResponseHeader));
    send_file(socket, file);
    fclose(file);
}

struct connection_info_struct {
    FILE *fp;
    int operation;
    char *encoder;
    char *filename;
    char *output_filename;
    struct MHD_PostProcessor *postprocessor;
};

enum MHD_Result send_ws_response(struct MHD_Connection *connection,
                                 char *message, int success) {
    char error_json[1024];
    snprintf(error_json, sizeof(error_json),
             "{\"status\": \"%s\", \"message\": \"%s.\"}",
             success ? "success" : "error", message);
    printf("%s\n", error_json);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(error_json), (void *)error_json, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    int ret = MHD_queue_response(
        connection, success ? MHD_HTTP_OK : MHD_HTTP_BAD_REQUEST, response);
    MHD_destroy_response(response);

    return ret;
}

static int send_ws_file_response(struct MHD_Connection *connection,
                                 const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return send_ws_response(connection, "Server couldn't open file", 0);
    }

    // Obtain file size:
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    // Create the response

    struct MHD_Response *response =
        MHD_create_response_from_fd_at_offset64(size, fileno(file), 0);
    MHD_add_response_header(response, "Content-Type", "video/mp4");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

int ws_unique_id = 0;

enum MHD_Result iterate_post(void *coninfo_cls, enum MHD_ValueKind kind,
                             const char *key, const char *filename,
                             const char *content_type,
                             const char *transfer_encoding, const char *data,
                             uint64_t off, size_t size) {
    struct connection_info_struct *con_info = coninfo_cls;

    if (0 == strcmp(key, "file")) {
        if (0 == off) {
            // Open a file for writing; use filename provided by client
            ws_unique_id++;
            char unique_filename[FILE_SIZE + 15];
            snprintf(unique_filename, sizeof(unique_filename), "ws_%d_%s",
                     ws_unique_id, filename);
            con_info->filename = strdup(unique_filename);
            con_info->fp = fopen(unique_filename, "wb");
            if (!con_info->fp)
                return MHD_NO;
        }
        if (size > 0) {
            fwrite(data, size, 1, con_info->fp);
        }
        return MHD_YES;
    }
    if (0 == strcmp(key, "output_filename")) {
        con_info->output_filename = strdup(data);
    }
    if (0 == strcmp(key, "encoder")) {
        con_info->encoder = strdup(data);
    }
    if (0 == strcmp(key, "operation")) {
        con_info->operation = atoi(data);
    }
    return MHD_YES;
}

void request_completed(void *cls, struct MHD_Connection *connection,
                       void **con_cls, enum MHD_RequestTerminationCode toe) {
    struct connection_info_struct *con_info = *con_cls;

    if (NULL == con_info)
        return;

    if (con_info->fp) {
        fclose(con_info->fp);
        con_info->fp = NULL;
    }
    if (con_info->filename)
        free(con_info->filename);

    MHD_destroy_post_processor(con_info->postprocessor);
    free(con_info);
    *con_cls = NULL;
}

enum MHD_Result answer_to_connection(void *cls,
                                     struct MHD_Connection *connection,
                                     const char *url, const char *method,
                                     const char *version,
                                     const char *upload_data,
                                     size_t *upload_data_size, void **con_cls) {
    if (NULL == *con_cls) {
        struct connection_info_struct *con_info =
            malloc(sizeof(struct connection_info_struct));
        if (NULL == con_info)
            return MHD_NO;

        if (0 == strcmp(method, "POST")) {
            con_info->postprocessor = MHD_create_post_processor(
                connection, 1024, iterate_post, (void *)con_info);
            if (NULL == con_info->postprocessor) {
                free(con_info);
                return MHD_NO;
            }

            *con_cls = (void *)con_info;
            return MHD_YES;
        }
    }

    if (0 == strcmp(method, "POST")) {
        struct connection_info_struct *con_info = *con_cls;

        if (*upload_data_size != 0) {
            MHD_post_process(con_info->postprocessor, upload_data,
                             *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        } else if (NULL != con_info->fp) {
            fclose(con_info->fp);
            con_info->fp = NULL;

            char *output_filename;
            switch (con_info->operation) {
            case kEncode:
                ffmpeg_encode(con_info->filename, con_info->encoder,
                              &output_filename);
                break;

            case kCut:
                // ffmpeg_cut(con_info->filename, con_info->encoder,
                // output_filename);
                break;

            default:
                printf("Unknown operation from client\n");
                return send_ws_response(connection, "Invalid operation", 0);
            }

            return send_ws_file_response(connection, output_filename);
        }
    }

    return send_ws_response(connection, "Invalid method", 0);
}

int main() {
    int ux_server_fd;
    struct sockaddr_un server_addr_ux;

    // Setup socket
    ux_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&server_addr_ux, 0, sizeof(struct sockaddr_un));
    server_addr_ux.sun_family = AF_UNIX;
    strcpy(server_addr_ux.sun_path, UX_SOCKET_PATH);
    unlink(UX_SOCKET_PATH);
    bind(ux_server_fd, (struct sockaddr *)&server_addr_ux,
         sizeof(server_addr_ux));
    listen(ux_server_fd, MAX_SOCKET_CONNECTIONS);

    pthread_t ux_connection_thread;
    connection_thread_arg_t *ux_connection_arg =
        malloc(sizeof(connection_thread_arg_t));
    ux_connection_arg->server_socket = ux_server_fd;
    pthread_create(&ux_connection_thread, NULL, client_connection_handler,
                   ux_connection_arg);
    pthread_detach(ux_connection_thread); // The thread can run independently

    int admin_ux_server_fd;
    struct sockaddr_un admin_server_addr_ux;

    // Setup admin socket
    admin_ux_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&admin_server_addr_ux, 0, sizeof(struct sockaddr_un));
    admin_server_addr_ux.sun_family = AF_UNIX;
    strcpy(admin_server_addr_ux.sun_path, ADMIN_SOCKET_PATH);
    unlink(ADMIN_SOCKET_PATH);
    bind(admin_ux_server_fd, (struct sockaddr *)&admin_server_addr_ux,
         sizeof(admin_server_addr_ux));
    listen(admin_ux_server_fd, 1);

    pthread_t admin_ux_connection_thread;
    connection_thread_arg_t *admin_ux_connection_arg =
        malloc(sizeof(connection_thread_arg_t));
    admin_ux_connection_arg->server_socket = admin_ux_server_fd;
    pthread_create(&admin_ux_connection_thread, NULL, admin_connection_handler,
                   admin_ux_connection_arg);
    pthread_detach(
        admin_ux_connection_thread); // The thread can run independently

    // WS part
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, WS_PORT, NULL,
                              NULL, &answer_to_connection, NULL,
                              MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                              NULL, MHD_OPTION_END);
    if (NULL == daemon)
        return 1;

    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);

    printf("Server start\n");

    pause();

    close(server_fd);
    close(ux_server_fd);
    MHD_stop_daemon(daemon);

    return 0;
}
