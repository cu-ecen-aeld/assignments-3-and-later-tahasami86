#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <syslog.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <sys/queue.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
#define FILE_TO_WRITE "/dev/aesdchar"
#else
#define FILE_TO_WRITE "/var/tmp/aesdsocketdata"
#endif

#define PORT 9000
#define CONNECTION 3
#define BUFFER_SIZE 1024

int server_socket_fd;
pthread_mutex_t file_mutex;
volatile sig_atomic_t stop_flag = 0;

// Structure for thread node
struct thread_info_st {
    pthread_t thread_id;
    int client_fd;
    TAILQ_ENTRY(thread_info_st) entries;
};

TAILQ_HEAD(tailqhead, thread_info_st) queue_head;

void close_everything(void) {
    syslog(LOG_ERR, "Caught signal, exiting");
    close(server_socket_fd);
#ifndef USE_AESD_CHAR_DEVICE
    remove(FILE_TO_WRITE);
#endif
    pthread_mutex_destroy(&file_mutex);
    closelog();
}

void signal_handler(int signo) {
    if ((signo == SIGINT) || (signo == SIGTERM)) {
        stop_flag = 1;
        close_everything();

        struct thread_info_st *temp;
        TAILQ_FOREACH(temp, &queue_head, entries) {
            pthread_join(temp->thread_id, NULL);
            free(temp);
        }

        exit(EXIT_SUCCESS);
    }
}

/*
void *client_handler(void *args) {
    struct thread_info_st *thread_data = (struct thread_info_st *)args;
    int client_fd = thread_data->client_fd;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    // Open file for writing/reading
    pthread_mutex_lock(&file_mutex);
    int file_fd = open(FILE_TO_WRITE, O_RDWR | O_CREAT | O_APPEND, 0644);
    pthread_mutex_unlock(&file_mutex);

    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_fd);
        pthread_exit(NULL);
    }

    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';

        // Check for IOCTL command
        if (strncmp(buffer, "AESDCHAR_IOCSEEKTO:", 19) == 0) {
            unsigned int write_cmd, write_cmd_offset;

            if (sscanf(buffer + 19, "%u,%u", &write_cmd, &write_cmd_offset) == 2) {
                syslog(LOG_INFO, "Parsed IOCTL command: write_cmd=%u, write_cmd_offset=%u", write_cmd, write_cmd_offset);

                struct aesd_seekto seekto_cmd = {
                    .write_cmd = write_cmd,
                    .write_cmd_offset = write_cmd_offset
                };

                pthread_mutex_lock(&file_mutex);
                if (ioctl(file_fd, AESDCHAR_IOCSEEKTO, &seekto_cmd) < 0) {
                    syslog(LOG_ERR, "IOCTL command failed: %s", strerror(errno));
                } else {
                    syslog(LOG_INFO, "IOCTL command succeeded");

                    // Send back data starting from the seek position
                    char read_buffer[BUFFER_SIZE];
                    ssize_t bytes_read;
                    while ((bytes_read = read(file_fd, read_buffer, BUFFER_SIZE)) > 0) {
                        if (send(client_fd, read_buffer, bytes_read, 0) < 0) {
                            syslog(LOG_ERR, "Failed to send data: %s", strerror(errno));
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&file_mutex);

                // Skip writing IOCTL commands to the file
                continue;
            } else {
                syslog(LOG_ERR, "Failed to parse IOCTL command");
            }
        }

        // Write the received data to the file
        pthread_mutex_lock(&file_mutex);
        if (write(file_fd, buffer, bytes_received) < 0) {
            syslog(LOG_ERR, "Write to file failed: %s", strerror(errno));
        }
        lseek(file_fd, 0, SEEK_SET);

        // Read file content and send it back to the client
        char send_buffer[BUFFER_SIZE];
        ssize_t bytes_to_send;
        while ((bytes_to_send = read(file_fd, send_buffer, BUFFER_SIZE)) > 0) {
            if (send(client_fd, send_buffer, bytes_to_send, 0) < 0) {
                syslog(LOG_ERR, "Send error: %s", strerror(errno));
                break;
            }
        }
        lseek(file_fd, 0, SEEK_END);
        pthread_mutex_unlock(&file_mutex);
    }

    if (bytes_received < 0) {
        syslog(LOG_ERR, "Receive error: %s", strerror(errno));
    }

    // Clean up resources
    pthread_mutex_lock(&file_mutex);
    close(file_fd);
    pthread_mutex_unlock(&file_mutex);

    close(client_fd);
    pthread_exit(NULL);
}
*/


void *client_handler(void *args) {
    
    struct thread_info_st *thread_data = (struct thread_info_st *)args;
    int client_fd = thread_data->client_fd;
    int bytes_read;
    int used_size = 0;
    char *recv_buffer;

    recv_buffer = calloc(1,  BUFFER_SIZE);

    if(recv_buffer == NULL){
        syslog(LOG_ERR, "Failed to allocate memory");
        pthread_exit(NULL);
    }

    while((bytes_read = recv(client_fd,recv_buffer,BUFFER_SIZE,0)) > 0){

        used_size += bytes_read;
        if (strchr(recv_buffer, '\n')) break;

        char * recv_buffer = realloc(recv_buffer,BUFFER_SIZE + used_size);
            if (recv_buffer == NULL) {
            syslog(LOG_ERR, "Realloc failed");
            free(recv_buffer);
            pthread_exit(NULL);
        }
    }

    pthread_mutex_lock(&file_mutex);
    int file_fd = open(FILE_TO_WRITE, O_RDWR | O_CREAT | O_APPEND, 0644);
    pthread_mutex_unlock(&file_mutex);      
    
    
    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_fd);
        pthread_exit(NULL);
    }
    
    if(strncmp(recv_buffer, "AESDCHAR_IOCSEEKTO:",19) == 0){
        unsigned int write_cmd, write_cmd_offset;

        if(sscanf(recv_buffer + 19,  "%u,%u", &write_cmd, &write_cmd_offset) == 2){

            struct aesd_seekto seekto_cmd = {
                .write_cmd = write_cmd,
                .write_cmd_offset = write_cmd_offset
            };

                pthread_mutex_lock(&file_mutex);

                if(ioctl(file_fd,AESDCHAR_IOCSEEKTO, &seekto_cmd) < 0){
                    syslog(LOG_ERR, "IOCTL command failed: %s", strerror(errno));
                } else{
                        syslog(LOG_INFO, "IOCTL command succeeded");

                    char read_buffer[BUFFER_SIZE];
                    ssize_t bytes_read;

                    while ((bytes_read = read(file_fd, read_buffer, BUFFER_SIZE) ) > 0)
                    {
                        if (send(client_fd,read_buffer,bytes_read,0) < 0){
                            syslog(LOG_ERR, "Failed to send data: %s", strerror(errno));
                            break;
                        }   
                    }
                    

                }

                pthread_mutex_unlock(&file_mutex);      
        } else {
                syslog(LOG_ERR, "Failed to parse IOCTL command");
            }

    } else{

            pthread_mutex_lock(&file_mutex);

            if (write(file_fd, recv_buffer, used_size) < 0) 
            {
            syslog(LOG_ERR, "Write to file failed: %s", strerror(errno));
            }

        lseek(file_fd, 0, SEEK_SET);

        char send_buffer[BUFFER_SIZE];
        ssize_t bytes_to_send;
        while ((bytes_to_send = read(file_fd, send_buffer, BUFFER_SIZE)) > 0) 
        {
            if (send(client_fd, send_buffer, bytes_to_send, 0) < 0) 
                {
                    syslog(LOG_ERR, "Send error: %s", strerror(errno));
                }
        }

        lseek(file_fd, 0, SEEK_END);
        pthread_mutex_unlock(&file_mutex);

    }
    
    
    pthread_mutex_lock(&file_mutex);
    close(file_fd);
    pthread_mutex_unlock(&file_mutex);

    
    close(client_fd);
    pthread_exit(NULL);
}

#ifndef USE_AESD_CHAR_DEVICE
// Thread function to periodically append timestamp
void *timestamp_thread(void *arg) {
    while (!stop_flag) {
        pthread_mutex_lock(&file_mutex);

        // Get current time
        time_t now = time(NULL);
        struct tm *time_info = localtime(&now);
        char time_str[BUFFER_SIZE];
        strftime(time_str, sizeof(time_str), "timestamp:%a, %d %b %Y %H:%M:%S\n", time_info);

        // Write timestamp to file
        lseek(file_fd, 0, SEEK_END);
        write(file_fd, time_str, strlen(time_str));

        pthread_mutex_unlock(&file_mutex);

        // Sleep for 10 seconds, checking stop_flag during sleep
        for (int i = 0; i < 10 && !stop_flag; i++) {
            sleep(1);
        }
    }
    pthread_exit(NULL);
}
#endif

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};
    char *ipv4_addr;

    pthread_mutex_init(&file_mutex, NULL);
    TAILQ_INIT(&queue_head);

    openlog("aesdsocket", LOG_PID | LOG_PERROR, LOG_USER);

    if ((server_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "Failed to open socket");
        close_everything();
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int opt = 1;
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "setsockopt error");
        close_everything();
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Binding failed");
        close_everything();
        exit(EXIT_FAILURE);
    }

    if ((argc == 2 && (strcmp(argv[1],"-d"))) == 0)
    {
        pid_t pid;
        pid = fork();

        if (pid < 0){
            syslog(LOG_ERR,"Fork Failed \n");
            close_everything();
            exit(EXIT_FAILURE);
        }

        if(pid > 0){
            exit(EXIT_SUCCESS);
        }

        if (setsid() == -1) {
            syslog(LOG_ERR, "setsid error");
            close_everything();
            exit(EXIT_FAILURE);
        }
        if (chdir("/") == -1) {
            syslog(LOG_ERR, "chdir error");
            close_everything();
            exit(EXIT_FAILURE);
        }
        open ("/dev/null", O_RDWR); 
		dup (0); 
		dup (0);
    }

    if (listen(server_socket_fd, CONNECTION) < 0) {
        syslog(LOG_ERR, "Listening failed");
        close_everything();
        exit(EXIT_FAILURE);
    }


#ifndef USE_AESD_CHAR_DEVICE

    // Start timestamp thread
    pthread_t timestamp_tid;
    pthread_create(&timestamp_tid, NULL, timestamp_thread, NULL);
#endif


    while (1) {
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "Failed to accept connection");
            continue;
        }

        ipv4_addr = inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", ipv4_addr);

        struct thread_info_st *thread_new_entry = malloc(sizeof(struct thread_info_st));
        if (!thread_new_entry) {
            syslog(LOG_ERR, "Failed to allocate thread info");
            close(client_fd);
            continue;
        }

        thread_new_entry->client_fd = client_fd;
        TAILQ_INSERT_HEAD(&queue_head, thread_new_entry, entries);

        pthread_create(&(thread_new_entry->thread_id), NULL, client_handler, (void *)thread_new_entry);

        struct thread_info_st *temp;
        TAILQ_FOREACH(temp, &queue_head, entries) {
            pthread_join(temp->thread_id, NULL);
            TAILQ_REMOVE(&queue_head, temp, entries);
            free(temp);
        }
    }

    pthread_mutex_destroy(&file_mutex);
    close_everything();
    return 0;
}
