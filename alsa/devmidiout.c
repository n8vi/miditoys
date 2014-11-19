//
// Programmer:    Craig Stuart Sapp [craig@ccrma.stanford.edu]
// Creation Date: Mon Dec 21 18:00:42 PST 1998
// Last Modified: Mon Dec 21 18:00:42 PST 1998
// Filename:      ...linuxmidi/output/method1.c
// Syntax:        C 
// $Smake:        gcc -O -o devmidiout devmidiout.c && strip devmidiout
//

#include <linux/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>


int main(void) {
   char* device =  "/dev/midi" ;
   unsigned char data[3] = {0x90, 60, 127};

   // step 1: open the OSS device for writing
   int fd = open(device, O_WRONLY, 0);
   if (fd < 0) {
      printf("Error: cannot open %s\n", device);
      exit(1);
   }

   // step 2: write the MIDI information to the OSS device
   write(fd, data, sizeof(data));

   // step 3: (optional) close the OSS device
   close(fd);

   return 0;
}
