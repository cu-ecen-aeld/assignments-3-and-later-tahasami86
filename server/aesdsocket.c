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
#include <unistd.h>
#include <fcntl.h>

#define PORT 9000
#define CONNECTION 3
#define FILE_TO_WRITE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024


int server_socket_fd,client_fd,file_fd;

void close_everything(void){
        syslog(LOG_ERR,"Caught signal, exiting");
        close(server_socket_fd);
        close(client_fd);
        remove(FILE_TO_WRITE);
        closelog();
}
void signal_handler(int signo){

    if((signo == SIGINT) || (signo == SIGTERM)){
        close_everything();
        exit(EXIT_SUCCESS);
    }
}

int main (int argc, char *argv[]){
    //FILE *file;
    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};
    int bytes_read;
    char *ipv4_addr;
    int bytes_read_to_send;


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

    file_fd = open(FILE_TO_WRITE, O_CREAT | O_RDWR, 0644);

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

   
    while (1)
    {
          socklen_t client_addr_len = sizeof(client_addr);
          if ((client_fd = accept(server_socket_fd,(struct sockaddr *)&client_addr,&client_addr_len)) < 0)
          {
            syslog(LOG_ERR,"Failed to accept connection \n");
            close_everything();
            exit(EXIT_FAILURE);
          }
        
        ipv4_addr =inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO,"Accepted connection from %s \n",ipv4_addr);

        // int buffer_size = BUFFER_SIZE;
        int used_size = 0 ;
        char *recv_buffer = calloc(1, BUFFER_SIZE);
    
        if(recv_buffer == NULL){

            syslog(LOG_ERR,"Failed to alocate memory \n");
            close_everything();
            exit(EXIT_FAILURE);
        }
        

        while ((bytes_read = recv(client_fd,recv_buffer + used_size, BUFFER_SIZE, 0)) > 0)
        {

            used_size += bytes_read ; 

            recv_buffer = realloc(recv_buffer, BUFFER_SIZE + used_size);

            if (recv_buffer == NULL)
            {
                syslog(LOG_ERR, "Realloc Failed Exiting");
                close_everything();
                free(recv_buffer);
                exit(EXIT_FAILURE);
            }

            if (strchr(recv_buffer, '\n'))
                break;
        }   

        if (write(file_fd,recv_buffer,used_size) < 0){
            syslog(LOG_ERR, "writing to file failed \n");
            free(recv_buffer);
            close_everything();
            exit(EXIT_FAILURE);
        }
            fsync(file_fd);
            lseek(file_fd, 0, SEEK_SET);
        

            char *send_buffer = calloc(1,used_size);

            if(send_buffer == NULL){

                syslog(LOG_ERR , "Failed file sending buffer \n");
                free(recv_buffer);
                close_everything();
            }

            while ((bytes_read_to_send =read(file_fd,send_buffer,used_size)) > 0)
            {
                bytes_read_to_send = send(client_fd,send_buffer,bytes_read_to_send,0);
                if (bytes_read_to_send < 0) {
                syslog(LOG_ERR, "Send error");
                close_everything();
                exit(EXIT_FAILURE);
            }
            }   

            syslog(LOG_INFO, "CLosed connection from %s \n",ipv4_addr);

            free(send_buffer);
            free(recv_buffer);
    }
    return 0;
}