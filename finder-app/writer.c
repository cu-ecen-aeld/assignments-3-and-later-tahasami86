#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>


int main(int argc , char *argv[]){

    FILE *file;
openlog("writer",LOG_PID | LOG_PERROR,LOG_USER);

if (argc !=3){
    syslog(LOG_ERR,"Invalid number of arguments. Expected 2 but got %d",argc -1);
    return 1;
}

    const char *file_path=argv[1];
    const char *string_to_write = argv[2];
    file = fopen(file_path,"w");

    if ((file == NULL))
    {
        syslog(LOG_ERR,"Error opening file %s : %s ",file_path,strerror(errno));
    }
    
    fprintf(file,"%s",string_to_write);
    syslog(LOG_DEBUG,"writing the '%s' to the file '%s' ",string_to_write,file_path);

    if(fclose(file) != 0)
    {
        syslog(LOG_ERR,"Error closing the file '%s' %s",file_path,strerror(errno));
    }

    closelog();
    return 0;
}