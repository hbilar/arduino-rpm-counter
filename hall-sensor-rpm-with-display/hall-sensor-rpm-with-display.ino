/*  Hall sensor RPM counter 
 *  =======================
 *  
 *  Idea of operation:
 *  Use interrupt handler to get the time when the hall sensor input
 *  switches (falling edge)
 *  Store the current time into a 5 element long array.
 *
 *  In the main loop, calculate the average RPM every from the values
 *  in the array. If the values in the array are too old, discount them.
 *
 *  The RPM is displayed on the serial port, and also through the 7 
 *  segment LED display. The display is one of those circuits with 
 *  2 x 74HC595 ICs (usual cheapies off ebay).
 */

#include <string.h>
#include <stdio.h>


/* bitmap for the 7 segment display
 *      11111
 *    32     2
 *    32     2  
 *      64 64
 *    16     4
 *    16     4
 *      88888   128
 */ 
/* This is the list of characters we have to play with. Each character is
   represented as a full byte using the method above.  */
unsigned char CharBitMap[] =    
  {
    '0', 0xC0, 
    '1', 0xF9, 
    '2', 0xA4, 
    '3', 0xB0, 
    '4', 0x99, 
    '5', 0x92, 
    '6', 0x82, 
    '7', 0xF8, 
    '8', 0x80, 
    '9', 0x90, 
    'A', 0x88, 
    'b', 0x83, 
    'C', 0xC6, 
    'd', 0xA1, 
    'E', 0x86, 
    'F', 0x8e, 
    'P', 0x8c, 
    '-', 0xbf, 
    'c', 0xa7, 
    ' ', 0xff, 
    0, 0        // end of sequence marker
  };

unsigned char DisplayBM[4];
int SCLK = 8;
int RCLK = 7;
int DIO = 4; 


const unsigned long MAX_STAMP_AGE = 6000; // ms max to keep time stamps
const int MAX_STAMPS = 5; /* RPM moving average number of elements */
volatile unsigned long timestamps[MAX_STAMPS];
volatile short int curTimestampPos = 0;
volatile unsigned long lastStamp = 0;


/* interrupt to save the current time */
void rpmInterrupt(void)
{
  timestamps[curTimestampPos++] = millis();
  if (curTimestampPos >= MAX_STAMPS) {
    curTimestampPos = 0;
  }
  lastStamp = millis();
}


void setup() 
{
  /* trigger interrupt when the rpm interrupt pin goes low */
  attachInterrupt(0, rpmInterrupt, FALLING);
  Serial.begin(9600);      

  /* for display */
  pinMode(SCLK,OUTPUT);
  pinMode(RCLK,OUTPUT);
  pinMode(DIO,OUTPUT); 

  /* Clear display initially */
  SetupDigits("----");
}


int calculateRPM()
{
  unsigned long curTime = millis();
  short numIntervals = 0;

  /* make a copy of relevant data */
  noInterrupts();  // disable interrupts
  short p = curTimestampPos;
  unsigned long ts[MAX_STAMPS];
  memset(ts, 0, sizeof(ts));
  for (int i = 0; i < MAX_STAMPS; i++) {
    ts[i] = timestamps[i];   
  }
  
  /* find applicable timestamps and the top and bottom number */
  unsigned long low = 0;
  unsigned long high = 0;
  short datapoints = 0;
  for (int i = 0; i < MAX_STAMPS; i++) {
      unsigned long tsDiff = abs((long)curTime - (long)(ts[i]));
      if (tsDiff < MAX_STAMP_AGE) {
        datapoints ++;
        if ((low == 0) || ts[i] <= low) {
          low = ts[i];
        } 
        if (ts[i] > high) {
          high = ts[i];
        }    
    }
  }
  interrupts();    // enable interrupts

  int rpm = 0;  
  if (datapoints > 1) {
    unsigned long timeDiff = abs(high - low);
    int revs = datapoints - 1;
        
    float rps = (revs / (timeDiff / 1000.0));
    rpm = (int)(rps * 60.0);       
  }
  return rpm;
}


int oldRpm = 0;  /* keep track of the old RPM so we only need to update 
		    the bitmaps when the RPM actually changes */
void loop() {
  Serial.println("Hall sensor with RPM output");

  while (1) {
    int rpm = calculateRPM();
    if (rpm != oldRpm) {
      /* only recalculate the display bitmap if the rpm has changed */
      oldRpm = rpm;
      Serial.print("rpm: ");
      Serial.println(rpm);
      
      char displayString[5];
      sprintf(displayString, "%.4d", rpm);
      SetupDigits(displayString);
    }
    DisplayDigits();
  }
}


/******************************************************************/
/*    DISPLAY CODE  */
/******************************************************************/

/* Display digit bm at pos 0-4.
   The display takes two bytes - 1st is the data for displaying
   the character, 2nd for the position. 
   Pull RCLK low, write data and then pull it high. */
void DisplayDigitBitmap(unsigned char bm, int pos)
{
  digitalWrite(RCLK,LOW);

  ShiftToDisplay(bm);

  switch(pos) {
  case 1:
    ShiftToDisplay(0x01);
    break;
  case 2:
    ShiftToDisplay(0x02);
    break;
  case 3:
    ShiftToDisplay(0x04);
    break;
  case 4:
    ShiftToDisplay(0x08);
    break;
  default:
    ShiftToDisplay(0x01);
  }
  digitalWrite(RCLK,HIGH);
}


/* Look up bitmaps for the string s in the CharBitMap array and
   save the bitmaps to DisplayBM */
void SetupDigits(char *s)
{
  /* find bitmaps */
  char *p = s;
  unsigned char *dispBM = DisplayBM;
  while (*p) {
    unsigned char i = 0;

    /* Find character in bitmap */
    unsigned char *bm = CharBitMap;
    while (*bm) {
      if (*bm == *p) {
	// found char - find the bitmap
	i = *(bm + 1);
	break;
      }
      bm += 2;
    }
    *dispBM++ = i;
    p++;
  }
}


/* Display the characters on the display.
   Displays one digit (out of four) per call - needs to be called frequently
   to look good. Having it only display one digit and then immediately return
   makes the function quick enough to not interfere unduly with other 
   time sensitive in the main loop. 
*/
byte __curCharacter = 0;
void DisplayDigits ()
{
  DisplayDigitBitmap(DisplayBM[__curCharacter % 4], 4 - __curCharacter % 4);
  __curCharacter++;
}


/* Write a byte to the display by shifting the bits out (MSB first). 
   Basically, set the DIO pin either high or low depending
   on the value of the current bit, and then toggle SCLK 
   low, then high. Rinse and repeat for the whole byte. */    
void ShiftToDisplay(unsigned char c)
{
  for(int i = 8; i >= 1; i--) {
    if (c & 0x80)  {
      digitalWrite(DIO,HIGH);
    } else {
      digitalWrite(DIO,LOW);
    }
    c <<= 1;
    digitalWrite(SCLK,LOW);
    digitalWrite(SCLK,HIGH);
  }
}
    
