#ifndef LAB4PHOTO_C
#define LAB4PHOTO_C

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define Strobe     (26) // IO8
#define GP_4       (28) // IO4
#define GP_5       (17) // IO5
#define GP_6       (24) // IO6
#define GP_7       (27) // IO7
#define GP_9       (19) // IO9
#define GPIO_DIRECTION_IN      (1)
#define GPIO_DIRECTION_OUT     (0)
#define ERROR                  (-1)

int BUS_A;
int BUS_B;
int BUS_C;
int BUS_D;
int BUS_STROBE;

int BUS_DEBUG;

int openGPIO(int gpio, int direction);
int closeGPIO(int gpio, int fileHandle);
void writeGPIO(int fHandle, int value);
int readGPIO(int fHandle);
void write_msg(char msg);
char read_msg();
void set_write();
void set_read();
void delay(int milis);

int initPhotores();
int getPhotores();
int pingPhotores();
int resetPhotores();

const char MSG_RESET = 0x00;
const char MSG_PING = 0x01;
const char MSG_GET = 0x02;

const char RET_ACK = 0x0E;

const int CLCK_TIME = 10;

//busyloop delay, returns after milis miliseconds
void delay(int milis){
        usleep(milis*1000);
}

//open GPIO and set the direction
int openGPIO(int gpio, int direction )
{
        /*
        1.set the GPIO
        2.set the direction
        3.set the voltage
        */

        char buffer[256];
        int fileHandle;
        int fileMode;

  //Export GPIO
        fileHandle = open("/sys/class/gpio/export", O_WRONLY);
        if(ERROR == fileHandle)
        {
               puts("Error: Unable to opening /sys/class/gpio/export");
               return(-1);
        }
        sprintf(buffer, "%d", gpio);
        write(fileHandle, buffer, strlen(buffer));
        close(fileHandle);

   //Direction GPIO
        sprintf(buffer, "/sys/class/gpio/gpio%d/direction", gpio);
        fileHandle = open(buffer, O_WRONLY);
        if(ERROR == fileHandle)
        {
               puts("Unable to open file:");
               puts(buffer);
               return(-1);
        }

        if (direction == GPIO_DIRECTION_OUT)
        {
               // Set out direction
               write(fileHandle, "out", 3);
               fileMode = O_WRONLY;
        }
        else
        {
               // Set in direction
               write(fileHandle, "in", 2);
               fileMode = O_RDONLY;
        }
        close(fileHandle);


   //Open GPIO for Read / Write
        sprintf(buffer, "/sys/class/gpio/gpio%d/value", gpio);
        fileHandle = open(buffer, fileMode);
        if(ERROR == fileHandle)
        {
               puts("Unable to open file:");
               puts(buffer);
               return(-1);
        }

        return(fileHandle);  //This file handle will be used in read/write and cl
}

int closeGPIO(int gpio, int fileHandle)
{
        char buffer[256];

        close(fileHandle); //This is the file handle of opened GPIO for Read / Wr

        fileHandle = open("/sys/class/gpio/unexport", O_WRONLY);
        if(ERROR == fileHandle)
        {
               puts("Unable to open file:");
               puts(buffer);
               return(-1);
        }
        sprintf(buffer, "%d", gpio);
        write(fileHandle, buffer, strlen(buffer));
        close(fileHandle);

        return(0);
}

//write value
void writeGPIO(int fHandle, int val)
{
        if(val == 0){
                write(fHandle, "0", 1);
        }else{
                write(fHandle, "1", 1);
        }
}

//read value
int readGPIO(int fHandle){
        int i;
        char rval;
        read(fHandle, &rval, 1);
        (rval == '0') ? (i=0) : (i=1);
        return i;
}

//write bottom nibble of char to bus
void write_msg(char msg){
        set_write();

        //for safety, keep bus idle for minimum 1 CLCK_TIME
        writeGPIO(BUS_STROBE, 0);
        delay(CLCK_TIME);

        //pull strobe high to announce upcoming write
        writeGPIO(BUS_STROBE, 1);
        delay(CLCK_TIME);

        //pull strobe low while putting data on the bus
        writeGPIO(BUS_STROBE, 0);
        //write data to bus
        writeGPIO(BUS_A, msg & 0x01);
        writeGPIO(BUS_B, msg & 0x02);
        writeGPIO(BUS_C, msg & 0x04);
        writeGPIO(BUS_D, msg & 0x08);
        //hold down the fort
        delay(CLCK_TIME);

        //High Cycle
        //pull strobe high and wait for pic to read
        writeGPIO(BUS_STROBE, 1);
        delay(CLCK_TIME);

        //write is over! time to turn everything off
        writeGPIO(BUS_STROBE, 0);

        writeGPIO(BUS_A, 0);
        writeGPIO(BUS_B, 0);
        writeGPIO(BUS_C, 0);
        writeGPIO(BUS_D, 0);

        set_read();
}

char read_msg(){
        char rval = 0;

        set_read();
        //Hold down for CLCK_TIME

        //we enter while bus is idle (strobe low), this is just for safety
        writeGPIO(BUS_STROBE, 0);
        delay(CLCK_TIME*2);

        //Bring clock high to announce upcoming galileo read cycle
        writeGPIO(BUS_STROBE, 1);
        delay(CLCK_TIME*2);

        //pull it low again, wait for the pic to write to the bus
        writeGPIO(BUS_STROBE, 0);
        delay(CLCK_TIME*2);

        //pull strobe high, read data from the bus
        writeGPIO(BUS_STROBE, 1);

        readGPIO(BUS_A) != 0 ? (rval |= 0x01) : (rval &= 0x0e);
        readGPIO(BUS_B) != 0 ? (rval |= 0x02) : (rval &= 0x0d);
        readGPIO(BUS_C) != 0 ? (rval |= 0x04) : (rval &= 0x0b);
        readGPIO(BUS_D) != 0 ? (rval |= 0x08) : (rval &= 0x07);

        //Make sure to delay for CLCK_TIME
        delay(CLCK_TIME*2);

        //Write bus low before leaving
        writeGPIO(BUS_STROBE, 0);
        return rval;
}

//this function assumes that there are open filehandles it needs to close before
void set_write(){
        closeGPIO(GP_4, BUS_D);
        closeGPIO(GP_5, BUS_C);
        closeGPIO(GP_6, BUS_B);
        closeGPIO(GP_7, BUS_A);

        BUS_D = openGPIO(GP_4, GPIO_DIRECTION_OUT);
        BUS_C = openGPIO(GP_5, GPIO_DIRECTION_OUT);
        BUS_B = openGPIO(GP_6, GPIO_DIRECTION_OUT);
        BUS_A = openGPIO(GP_7, GPIO_DIRECTION_OUT);
}

//this function assumes that there are open filehandles it needs to close before
void set_read(){
        closeGPIO(GP_4, BUS_D);
        closeGPIO(GP_5, BUS_C);
        closeGPIO(GP_6, BUS_B);
        closeGPIO(GP_7, BUS_A);

        BUS_D = openGPIO(GP_4, GPIO_DIRECTION_IN);
        BUS_C = openGPIO(GP_5, GPIO_DIRECTION_IN);
        BUS_B = openGPIO(GP_6, GPIO_DIRECTION_IN);
        BUS_A = openGPIO(GP_7, GPIO_DIRECTION_IN);
}

int initPhotores(){
	//gotta give the set_ methods something to safely replace
	BUS_D = openGPIO(GP_4, GPIO_DIRECTION_OUT);
	BUS_C = openGPIO(GP_5, GPIO_DIRECTION_OUT);
	BUS_B = openGPIO(GP_6, GPIO_DIRECTION_OUT);
	BUS_A = openGPIO(GP_7, GPIO_DIRECTION_OUT);

	//open the strobe line
	BUS_STROBE = openGPIO(Strobe, GPIO_DIRECTION_OUT);

	set_write();
	int i = 0;
	char busValue;

	//an idle bus is a low strobe
	writeGPIO(BUS_STROBE, 0);
}

int getPhotores(){
	
	int return_msg;
	
	initPhotores();
	
	write_msg(MSG_GET);

	return_msg = (int) read_msg();
	return_msg = (return_msg << 4) | (int)read_msg();
	return_msg = (return_msg << 4) | (int)read_msg();
	//Read ACK
	read_msg();
	return return_msg;
}

int pingPhotores(){
	initPhotores();
	write_msg(MSG_PING);
	if ((int) read_msg() == RET_ACK){
		return 1;
	} else {
		return 0;
	}
}

int resetPhotores(){
	initPhotores();
	write_msg(MSG_RESET);
	return (int) read_msg();
}

/*int main(void)
{
        //gotta give the set_ methods something to safely replace
        BUS_D = openGPIO(GP_4, GPIO_DIRECTION_OUT);
        BUS_C = openGPIO(GP_5, GPIO_DIRECTION_OUT);
        BUS_B = openGPIO(GP_6, GPIO_DIRECTION_OUT);
        BUS_A = openGPIO(GP_7, GPIO_DIRECTION_OUT);

        //open the strobe line
        BUS_STROBE = openGPIO(Strobe, GPIO_DIRECTION_OUT);

        set_write();
        int i = 0;
        char busValue;

        //an idle bus is a low strobe
        writeGPIO(BUS_STROBE, 0);

        struct timespec start, end;
        int delta;

        char cmd; //user command
        int return_msg; //message returned from PIC
        while(1)
        {
                printf("command: ");
                scanf("%c", &cmd);
                int c;
                while ((c = getchar()) != '\n' && c != EOF);

                return_msg = 0;
                switch(cmd){
                        case 'r':
                                write_msg(MSG_RESET);
                                return_msg = (int) read_msg();
                                //Read ACK
                                //read_msg();
                                break;
                        case 'p':
                                write_msg(MSG_PING);
                                return_msg = (int) read_msg();
                                //Read ACK
                                //read_msg();
                                break;
                        case 'g':
                                write_msg(MSG_GET);

                                return_msg = (int) read_msg();
                                return_msg = (return_msg << 4) | (int)read_msg();
                                return_msg = (return_msg << 4) | (int)read_msg();
                                //Read ACK
                                read_msg();
                                break;
                        case 't':
                                while(1){
                                        return_msg = (int) read_msg();
                                        printf("%d \n", return_msg);
                                }
                        case 'q':
                                exit(0);
                                break;
                        default:
                                printf("ERROR: incompatable command \n");
                                return_msg = (int) 0xF;
                                break;
                }
                printf("return_msg= 0x%x %d \n", return_msg, return_msg);
        }
}
*/

#endif