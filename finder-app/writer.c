#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

int main(int argc, char** argv) {
     openlog(NULL, 0, LOG_USER);

     if (argc != 3) {
	     fprintf(stderr, "Invalid number of arguments: %d\n", argc);
	     syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
	     return 1;	
     }
     
     syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
     
     FILE *fptr;
     size_t bytes_written;
     int ret;

     fptr = fopen(argv[1], "a");
     if (fptr == NULL){
	     fprintf(stderr, "Failed to open file %s: %s", argv[1], strerror(errno));
	     syslog(LOG_ERR, "Failed to open file %s: %s", argv[1], strerror(errno));
	     return 1;
     }

     bytes_written = fwrite(argv[2], strlen(argv[2]), 1, fptr);

     ret = fclose(fptr);
     if (ret != 0) {
	     fprintf(stderr, "Failed to close file %s: %s", argv[1], strerror(errno));
	     syslog(LOG_ERR, "Failed to close file %s: %s", argv[1], strerror(errno));
	     return 1;
     }

     closelog();
     
     return 0;
}

