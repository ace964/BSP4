/*
 * Test program for tzm
 * 
 * Tim Tiedemann, Franz Korf
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

 
#define TZM "/dev/tzm"
#define STR "Das ist ein Test \n mit einem newline "

int mode=0;

void help() {
    printf("TZM Linux-Kernel-Driver Test\n");
    printf("Options:\n  -r : Receiver Mode\n");
    printf("  -t : Transmitter Mode\n");
    printf("\n");
}


int main(int argc, char* argv[]) {
 
    int fd;
    int i;

    char buffer[1024];


    if (argc!=2) {
      help();
      exit(-1);
    }

    if (strcmp(argv[1], "-r")==0) mode=1;
    if (strcmp(argv[1], "-t")==0) mode=2;

    if (mode==0) {
      help();
      exit(-1);
    }
 
    switch(mode) {
    
       case 1:       
           for (i=0; i < 1024; i++) buffer[i] = 'X';

           /* Create input file descriptor */
           fd = open (TZM , O_RDONLY);
           if (fd == -1) {
             perror ("open");
             return -2 ;
           }
           printf("successfully opened\n");

           /* read test */
           i = read (fd, buffer, 1024) ;
           printf("number of characters read = %d \n", i);
           printf("read result = >>%s<<\n", buffer);
    
          i = close (fd);
          if (i == -1) {
            perror ("close");
            return -5 ;
          }
          printf("successfully closed\n");
    
           break;
          
       case 2:
          printf("Test %s \n", TZM);

          fd = open (TZM , O_WRONLY);
          if (fd == -1) {
            perror ("open");
            return -3 ;
          }
          printf("successfully opened\n");

          printf("print text >> %s <<\n", STR);

          i = write (fd, STR, sizeof(STR));
          if (i == -1) {
            perror ("write");
            return -4 ;
          }

          printf("number of byte written : %d \n", i);

          i = close (fd);
          if (i == -1) {
            perror ("close");
            return -6 ;
          }
          printf("successfully closed\n");
          
          break;
    }
    
    return 0;
}
