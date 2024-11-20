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

#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
#define FILE_TO_WRITE "/dev/aesdchar"

#else
#define FILE_TO_WRITE "/var/tmp/aesdsocketdata"
#endif


#define PORT 9000
#define CONNECTION 3
#define BUFFER_SIZE 1024
#define BEGIN 0


int server_socket_fd,file_fd;

pthread_mutex_t file_mutex;
volatile sig_atomic_t stop_flag = 0;


// Structure for thread node
struct thread_info_st{
    pthread_t thread_id;
    int client_fd;
    int fd;
    TAILQ_ENTRY(thread_info_st) entries;
};

TAILQ_HEAD(tailqhead, thread_info_st) queue_head;

void close_everything(void){
        syslog(LOG_ERR,"Caught signal, exiting");
        close(server_socket_fd);
#ifndef  USE_AESD_CHAR_DEVICE
        remove(FILE_TO_WRITE);
#endif
        pthread_mutex_destroy(&file_mutex); // Destroy the mutex
        closelog();
}
void signal_handler(int signo){

    if((signo == SIGINT) || (signo == SIGTERM)){
        stop_flag = 1;
        close_everything();
        struct thread_info_st *temp;

        TAILQ_FOREACH(temp,&queue_head,entries){
            pthread_join(temp->thread_id,NULL);
            free(temp);
        }

        exit(EXIT_SUCCESS);
    }
}

/*
void *client_handler(void *args)
{
    struct thread_info_st *thread_data=(struct thread_info_st *)args;
    int client_fd = thread_data->client_fd;
    // free(args);
    int bytes_read;
    int bytes_read_to_send;
    int used_size = 0 ;
    
    char *recv_buffer = calloc(1, BUFFER_SIZE);
    
    if(recv_buffer == NULL){

            syslog(LOG_ERR,"Failed to alocate memory \n");
            pthread_exit(NULL);
        }
        
         while ((!stop_flag) && ((bytes_read = recv(client_fd,recv_buffer + used_size, BUFFER_SIZE, 0)) > 0))
         {
            if(bytes_read == -1)
		    {
			perror("recv error");
			close_everything();
			exit(-1);
		    }
            used_size += bytes_read ; 

            if( bytes_read <= 0) 
                break;

            if (strchr(recv_buffer, '\n')) break;


            recv_buffer = realloc(recv_buffer, BUFFER_SIZE + used_size);

            if (recv_buffer == NULL)
            {
                syslog(LOG_ERR, "Realloc Failed Exiting");
                close_everything();
                free(recv_buffer);
                exit(EXIT_FAILURE);
            }

         }


            pthread_mutex_lock(&file_mutex);

            lseek(file_fd, 0, SEEK_END);
            if (write(thread_data->fd,recv_buffer,used_size) < 0){
            syslog(LOG_ERR, "writing to file failed \n");
            free(recv_buffer);
            close_everything();
            exit(EXIT_FAILURE);
            }

            pthread_mutex_unlock(&file_mutex);
            //fsync(file_fd);


            int file_length =  lseek(thread_data->fd, BEGIN,SEEK_END);
            char *send_buffer = calloc(1,file_length);


            if(send_buffer == NULL){

                syslog(LOG_ERR , "Failed file sending buffer \n");
                free(recv_buffer);
                close_everything();
            }

            lseek(thread_data->fd,BEGIN,SEEK_SET);

            pthread_mutex_lock(&file_mutex);

            while (!stop_flag && (bytes_read_to_send = read(file_fd, send_buffer, used_size)) > 0) 
            {
                bytes_read_to_send = send(client_fd, send_buffer, bytes_read_to_send, 0);
                if (bytes_read_to_send < 0) {
                syslog(LOG_ERR, "Send error");
                close_everything();
                exit(EXIT_FAILURE);
                }

            }

            pthread_mutex_unlock(&file_mutex);



            free(recv_buffer);
            free(send_buffer);
            close(client_fd);
           // TAILQ_REMOVE(&queue_head, thread_data, entries);
    
            pthread_exit(NULL);

}

*/


void *client_handler(void *args)
{
    struct thread_info_st *thread_data = (struct thread_info_st *)args;
    int client_fd = thread_data->client_fd;
    int file_fd; // Local file descriptor
    int bytes_read, bytes_read_to_send;
    int used_size = 0;

    char *recv_buffer = calloc(1, BUFFER_SIZE);
    if (recv_buffer == NULL) {
        syslog(LOG_ERR, "Failed to allocate memory");
        close(client_fd);
        pthread_exit(NULL);
    }

    // Receive data from the client
    while ((!stop_flag) && ((bytes_read = recv(client_fd, recv_buffer + used_size, BUFFER_SIZE, 0)) > 0)) {
        used_size += bytes_read;
        if (strchr(recv_buffer, '\n')) break; // End of a packet

        recv_buffer = realloc(recv_buffer, used_size + BUFFER_SIZE);
        if (recv_buffer == NULL) {
            syslog(LOG_ERR, "Realloc failed");
            free(recv_buffer);
            close(client_fd);
            pthread_exit(NULL);
        }
    }

    if (bytes_read < 0) {
        syslog(LOG_ERR, "Error receiving data");
        free(recv_buffer);
        close(client_fd);
        pthread_exit(NULL);
    }

    // Open the file for writing
    file_fd = open(FILE_TO_WRITE, O_RDWR | O_APPEND);
    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        free(recv_buffer);
        close(client_fd);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&file_mutex);
    if (write(file_fd, recv_buffer, used_size) < 0) {
        syslog(LOG_ERR, "Failed to write to file");
        pthread_mutex_unlock(&file_mutex);
        close(file_fd);
        free(recv_buffer);
        close(client_fd);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&file_mutex);

    // Read the file and send it back to the client
    lseek(file_fd, 0, SEEK_SET);
    char *send_buffer = malloc(BUFFER_SIZE);
    if (send_buffer == NULL) {
        syslog(LOG_ERR, "Failed to allocate send buffer");
        close(file_fd);
        free(recv_buffer);
        close(client_fd);
        pthread_exit(NULL);
    }

    while ((bytes_read_to_send = read(file_fd, send_buffer, BUFFER_SIZE)) > 0) {
        if (send(client_fd, send_buffer, bytes_read_to_send, 0) < 0) {
            syslog(LOG_ERR, "Failed to send data");
            break;
        }
    }

    free(recv_buffer);
    free(send_buffer);
    close(file_fd); // Close file descriptor after use
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


int main (int argc, char *argv[]){
    //FILE *file;
    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};
//    struct thread_info_st *temp_node = NULL;
    //int bytes_read;
    char *ipv4_addr;
   // int bytes_read_to_send;

    pthread_mutex_init (&file_mutex,NULL);
    TAILQ_INIT(&queue_head);

    openlog ("aesdsocket",LOG_PID | LOG_PERROR,LOG_USER);
    if ( (server_socket_fd = socket(AF_INET,SOCK_STREAM,0)) == -1 ){
        syslog(LOG_ERR,"Failed to open socket \n Exiting \n");
        close_everything();
        exit(EXIT_FAILURE);
    }

    
    fflush(stdout);
    signal(SIGINT,signal_handler);
    signal(SIGTERM,signal_handler);
    
    int opt=1;
    if(setsockopt(server_socket_fd,SOL_SOCKET,SO_REUSEADDR ,&opt,sizeof(opt)) < 0){

        syslog(LOG_ERR,"set socket error");
        close_everything();
        exit(EXIT_FAILURE);

    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0)
    {
        syslog(LOG_ERR,"Binding Failed \n");
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

    file_fd = open(FILE_TO_WRITE, O_CREAT | O_RDWR , 0644);

        if(file_fd < 0){
            syslog(LOG_ERR,"Failed to open file");
            close_everything();
            exit(EXIT_FAILURE);

        }
        
    if (listen(server_socket_fd,CONNECTION)< 0){

        syslog(LOG_ERR, "Listening Failed \n");
        close_everything();
        closelog();
        return -1;
    }

#ifndef USE_AESD_CHAR_DEVICE

    // Start timestamp thread
    pthread_t timestamp_tid;
    pthread_create(&timestamp_tid, NULL, timestamp_thread, NULL);
#endif

    while (1)
    {
        socklen_t client_addr_len = sizeof(client_addr);
         int client_fd = accept(server_socket_fd,(struct sockaddr *)&client_addr,&client_addr_len);
          if (client_fd < 0)
          {
            syslog(LOG_ERR,"Failed to accept connection \n");
            continue;
          }
        
        ipv4_addr =inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO,"Accepted connection from %s \n",ipv4_addr);

        // int buffer_size = BUFFER_SIZE;
         // Create a new thread_info structure
        struct thread_info_st *thread_new_entry = NULL;
        thread_new_entry = (struct thread_info_st *) malloc(sizeof(struct thread_info_st));
        TAILQ_INSERT_HEAD(&queue_head,thread_new_entry,entries);
        
        thread_new_entry->client_fd = client_fd;
        thread_new_entry->fd = file_fd;


        pthread_create(&(thread_new_entry->thread_id),NULL,client_handler,(void *)thread_new_entry);

        TAILQ_FOREACH(thread_new_entry,&queue_head,entries){
            pthread_join(thread_new_entry->thread_id,NULL);
            if(stop_flag){
                TAILQ_REMOVE(&queue_head,thread_new_entry,entries);
                free(thread_new_entry);
                thread_new_entry = NULL;

            }
        }
     }

#ifndef USE_AESD_CHAR_DEVICE
     // Join timestamp thread on exit
    pthread_join(timestamp_tid, NULL);
#endif

    pthread_mutex_destroy(&file_mutex);
    close_everything();
    return 0;
}