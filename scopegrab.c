/*
 * Capture a screendump from a Fluke ScopeMeter.
 *
 * Copyright (c) 2002-2017 by Steven J. Merrifield <info at stevenmerrifield.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */      

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define COMMS_TIMEOUT 2

int timeout;

/*************************************************************/
void catch_alarm(int dummy)
{
	timeout = !timeout;	/* Toggle flag to say signal occurred */
}

/*************************************************************/
void usage(char *s)
{
	fprintf(stderr,"Capture a screendump from a Fluke ScopeMeter\n");
	fprintf(stderr,"(c) 2002-2017 by Steven J. Merrifield\n\n");
	fprintf(stderr,"Usage: %s <device> <p|b> <filename>\n\n",s);
	fprintf(stderr,"   device:   serial port to connect to\n");
	fprintf(stderr,"   p:        save image in Postscript format\n");
	fprintf(stderr,"   b:        save image in X11 bitmap format\n");
	fprintf(stderr,"   filename: name of file to save as\n\n");
	exit(-1);
}

/*************************************************************/
void check_acknowledge(char ack)
{
        if (ack != '0')
        {
                switch(ack)
                {
                        case '1': 	fprintf(stderr,"Syntax error\n");
                                	break;
                        case '2': 	fprintf(stderr,"Execution error\n");
                                	break;
                        case '3': 	fprintf(stderr,"Synchronization error\n");
                                	break;
                        case '4': 	fprintf(stderr,"Communications error\n");
                                	break;
                        default:        fprintf(stderr,"Unknown acknowledge\n");
                }
                fprintf(stderr,"Program aborted\n");
                exit(-1);
        }
}
                  
                                                                                
/*************************************************************/
void send_command(int fd, char *command)
{
	if (write(fd,command,strlen(command)) == -1)
	{
		fprintf(stderr,"write failed: %s\n",strerror(errno));
		exit(-1);
	}
	sleep(3);	/* wait for meter to execute */
}


/*************************************************************/
/* Take a single 4 bit char, rotate left 2 bits and invert */
char remap(char ch)
{
	switch(toupper(ch))
	{
		case '0': return('F'); 
		case '1': return('B'); 
		case '2': return('7');
		case '3': return('3');
		case '4': return('E');
		case '5': return('A');
		case '6': return('6');
		case '7': return('2');
		case '8': return('D');
		case '9': return('9');
		case 'A': return('5');
		case 'B': return('1');
		case 'C': return('C');
		case 'D': return('8');
		case 'E': return('4');
		case 'F': return('0');
	}
	return(ch);
}


/*************************************************************/
/* The ScopeMeter defaults to 1200 baud, so we need to start comms */
/* at 1200, then change the baudrate to 19200 when we transfer the */
/* data. Once the data transfer is complete, we change the ScopeMeter */
/* baudrate back to the default, so we can continue to communicate */
/* with it, without cycling the power */

int main(int argc, char **argv)
{
	struct termios options;
	char buffer[16384];
	char orig_image[65535];
	char new_image[65535];
	int nbytes;
	int i,j,m;
	int old_start;	/* start position of data in original array */
	int new_start;	/* start position of data in new array */
	int fd;
	FILE *outfile;
	char c1,c2;
	char filename[255];
	int filetype;

	if (argc != 4)
		usage(argv[0]);

	if ((strcmp(argv[2],"p")) == 0)
		filetype = 0;	/* Postscript */
	else
		filetype = 1;	/* X11 bitmap */

	strcpy(filename,argv[3]);
        if ((outfile = fopen(filename,"w")) == NULL)
        {
                fprintf(stderr,"fopen failed: %s\n",strerror(errno));
                exit(-1);
        }

	if ((fd = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY)) == -1)
	{
		fprintf(stderr,"open failed: %s\n",strerror(errno));
		exit(-1);
	}

	printf("Setting PC baudrate to 1200...\n");
	fcntl(fd,F_SETFL,FNDELAY);
	tcgetattr(fd,&options);
	options.c_cflag = CLOCAL | CREAD | CS8 | B1200;
	options.c_iflag = IXON | IXOFF | IXANY;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 10;
	tcflush(fd,TCIFLUSH);
	tcsetattr(fd,TCSANOW,&options);

	printf("Setting ScopeMeter baudrate to 19200...\n");
	send_command(fd,"PC 19200\r");
	if ((nbytes = read(fd,buffer,2)) == -1)	/* ack, cr */
	{
		fprintf(stderr,"read failed: %s\n",strerror(errno));
		exit(-1);
	}
	check_acknowledge(buffer[0]);

	close(fd); /* finished with 1200 baud comms */

	if ((fd = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY)) == -1)
	{
		fprintf(stderr,"open failed: %s\n",strerror(errno));
		exit(-1);
	}

	printf("Setting PC baudrate to 19200...\n");
	fcntl(fd,F_SETFL,FNDELAY);	// (fd,F_SETFL,0); 0=Blocking
	tcgetattr(fd,&options);
	options.c_cflag = CLOCAL | CREAD | CS8 | B19200;
	options.c_iflag = IXON | IXOFF | IXANY;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 10;
	tcflush(fd,TCIFLUSH);
	tcsetattr(fd,TCSANOW,&options);

	printf("Sending command to get screendump...\n");
	send_command(fd,"QP 0,3\r");

	if ((nbytes = read(fd,buffer,2)) == -1)	/* ack, cr */
	{
		fprintf(stderr,"read failed: %s\n",strerror(errno));
		exit(-1);
	}
	check_acknowledge(buffer[0]);

	signal(SIGALRM, catch_alarm);	/* Install the signal handler */
	alarm(COMMS_TIMEOUT);		/* Start the timer */
	timeout = 0;
	printf("Reading data...\n");

	j=0;
	while (!timeout)
	{
		nbytes = read(fd,buffer,sizeof(buffer));
		while (nbytes != -1)
		{
			for (i=0;i<nbytes;i++)
			{
				orig_image[j] = buffer[i];
				j++;
				if ((j % 1024) == 0)
				{
					printf("*");
					fflush(stdout);
				}
			}
			nbytes = read(fd,buffer,sizeof(buffer));
			alarm(COMMS_TIMEOUT);	/* reset timer */
		}
	}

	printf("\nSetting ScopeMeter baudrate to 1200...\n");
	send_command(fd,"PC 1200\r");
	if ((nbytes = read(fd,buffer,2)) == -1)	/* ack, cr */
	{
		fprintf(stderr,"read failed: %s\n",strerror(errno));
		exit(-1);
	}
	check_acknowledge(buffer[0]);
	close(fd);	/* Finished serial comms */

	printf("Processing data...\n");

	if (filetype == 0)
	{

	        /* Add my header to the file */
     	   strcpy(new_image,"%!PS-Adobe-3.0\n%%Creator: scopegrab by Steven J. Merrifield\n%%BoundingBox: 72 72 552 552\n\n72 72 translate\n480 480 scale\n");
 
      		/* Work out where we need to insert the data */
	        new_start = strlen(new_image);
		/* j = num of bytes in original buffer read from scopemeter */
     		for (i=0; i<j; i++)
        	{
	                if ((orig_image[i] == '4') && (orig_image[i+1] == '8') &&
	                        (orig_image[i+2] == '0') && (orig_image[i+3] == ' '))
	                {
	                        old_start = i;
	                        m = new_start;
	                        for (i=old_start; i<j; i++,m++)
	                                new_image[m]=orig_image[i];
	                }
	        }
 
		printf("Writing Postscript file...\n");

	        for (i=0; i < (j-old_start+new_start-1); i++)
	                fprintf(outfile,"%c",new_image[i]);
 
	} /* filetype == 0 */
	else
	{

		/* strip out data */
		for (i=0; i<j; i++)
		{
			if ((orig_image[i] == 'i') && (orig_image[i+1] == 'm') &&
				(orig_image[i+2] == 'a') && (orig_image[i+3] == 'g'))
			{
       		                 old_start = i+7;
       		                 m = 0;
       		                 for (i=old_start; i<j; i++,m++)
       		                         new_image[m]=orig_image[i];
         
			}
		}

		printf("Writing X11 bitmap file...\n");

		fprintf(outfile,"#define %s_width 480\n",filename);
		fprintf(outfile,"#define %s_height 480\n",filename);
		fprintf(outfile,"static char %s_bits[] = {",filename);
	
		j=0;	/* Number of hex bytes per line */
		for (i=0; i < (m-11); i+=2)	/* m-11 to remove showpage command */
		{
			if ((j % 10) == 0)
				fprintf(outfile,"\n\t");

			c1 = new_image[i];
			c2 = new_image[i+1];

			if (!((c1 == 0x0d) && (c2 == 0x0a)))
			{
				c1 = remap(c1);
				c2 = remap(c2);
				fprintf(outfile,"0x%c%c",c2,c1);
				if (j < 28799)
					fprintf(outfile,", ");
				j++;
			}
		}
		fprintf(outfile,"};\n");
	} /* else */

	fclose(outfile);

	return(0);
}

