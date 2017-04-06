/*
 * Data logger using a Fluke ScopeMeter
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
#include <time.h>

#define COMMS_TIMEOUT 5

int timeout;

/*************************************************************/
void catch_alarm(int dummy)
{
	timeout = !timeout;	/* Toggle flag to say signal occurred */
}

/*************************************************************/
void usage(char *s)
{
	fprintf(stderr,"Fluke ScopeMeter data logger\n");
	fprintf(stderr,"(c) 2002-2017 by Steven J. Merrifield\n\n");
	fprintf(stderr,"Usage: %s <device> <channels> <delay>\n",s);
	fprintf(stderr,"   device: serial port to connect to\n");
	fprintf(stderr,"   channels: 1 = A main, 2 = A main+sub, 3 = A+B main, 4 = A+B main+sub\n");
	fprintf(stderr,"   delay: usleep after a command (in  microseconds), typically 300000\n");
	fprintf(stderr,"   eg: %s /dev/ttyUSB0 1 300000\n",s);
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
void send_command(int fd, char *command, int delay)
{
	if (write(fd,command,strlen(command)) == -1)
	{
		fprintf(stderr,"write failed: %s\n",strerror(errno));
		exit(-1);
	}
	usleep(delay);		/* wait for serial transfer, both tx & rx */
}

/*************************************************************/
void print_response(int fd)
{

	char buffer[64];
	int nbytes;
	int i;

	if ((nbytes = read(fd,buffer,sizeof(buffer))) == -1)	
	{
		fprintf(stderr,"read failed: %s\n",strerror(errno));
		exit(-1);
	}
	check_acknowledge(buffer[0]);
	for (i=2;i<nbytes-1;i++)	/* skip ACK and both CR's */
	{
		printf("%c",buffer[i]);
	}
}

/*************************************************************/
int main(int argc, char **argv)
{
	struct termios options;
	int fd;
	int channels;
	int delay;
	time_t current_time;
	struct tm *time_info;
	char time_buffer[64];

	if (argc != 4)
		usage(argv[0]);

	if ((fd = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY)) == -1)
	{
		fprintf(stderr,"open failed: %s\n",strerror(errno));
		exit(-1);
	}

	channels = atoi(argv[2]);

	delay = atoi(argv[3]);
	if (delay == 0) delay = 300000;

	fcntl(fd,F_SETFL,FNDELAY);
	//fcntl(fd,F_SETFL,0);	/* 0 = blocking */
	tcgetattr(fd,&options);
	options.c_cflag = CLOCAL | CREAD | CS8 | B1200;
	options.c_iflag = IXON | IXOFF | IXANY;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 10;	/* in tenths of a second */
	tcflush(fd,TCIFLUSH);
	tcsetattr(fd,TCSANOW,&options);

	signal(SIGALRM, catch_alarm);	/* Install the signal handler */
	alarm(COMMS_TIMEOUT);		/* Start the timer */

	while (!timeout)
	{

		time(&current_time);
		time_info = localtime(&current_time);
		strftime(time_buffer,sizeof(time_buffer),"%H:%M:%S",time_info);
		printf("%s   ",time_buffer);

		printf("A: ");
		send_command(fd,"QM 11\r",delay);	/* Ch A main reading */
		print_response(fd);

		if ((channels == 2) || (channels == 4))
		{
			printf("   ");
			printf("A sub: ");
			send_command(fd,"QM 12\r",delay);	/* Ch A sub reading */
			print_response(fd);
		}

		if ((channels == 3) || (channels == 4))
		{
			printf("   ");
			printf("B: ");
			send_command(fd,"QM 21\r",delay);	/* Ch B main reading */
			print_response(fd);
		}

		if (channels == 4)
		{
			printf("   ");
			printf("B sub: ");
			send_command(fd,"QM 22\r",delay);	/* Ch B sub reading */
			print_response(fd);
		}

		printf("\n");
		alarm(COMMS_TIMEOUT);	/* reset timer */
	}


	close(fd);
	return(0);
}







