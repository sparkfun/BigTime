/*
 12-10-2013
 Marcel Hecko - forked and hacked to be used with one 7-segment display

 7-17-2011
 Spark Fun Electronics 2011
 Nathan Seidle
 
 This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 This is the firmware for BigTime, the wrist watch kit. It is based on an ATmega328 running with internal
 8MHz clock and external 32kHz crystal for keeping the time (aka RTC). The code and system have been tweaked
 to lower the power consumption of the ATmeg328 as much as possible. The watch currently uses about 
 1.2uA in idle (non-display) mode and about 13mA when displaying the time. With a 200mAh regular 
 CR2032 battery you should get 2-3 years of use!
 
 To compile and load this code onto your watch, select "Arduino Pro or Pro Mini 3.3V/8MHz w/ ATmega328" from
 the Boards menu. 
 
 If you're looking to save power in your own project, be sure to read section 9.10 of the ATmega328 
 datasheet to turn off all the bits of hardware you don't need.
 
 BigTime requires the Pro 8MHz bootloader with a few modifications:
 Internal 8MHz
 Clock div 8 cleared
 Brown out detect disabled
 BOOTRST set
 BOOSZ = 1024 
 This is to save power and open up the XTAL pins for use with a 38.786kHz external osc.
 
 So the fuse bits I get using AVR studio:
 HIGH 0xDA
 LOW 0xE2
 Extended 0xFF  
 
 3,600 seconds in an hour
 1 time check per hour, 2 seconds at 13mA
 3,598 seconds @ 1.2uA
 (3598 / 3600) * 0.0012mA + (2 / 3600) * 13mA = 0.0084mA used per hour
 
 200mAh / 0.0084mA = 23,809hr = 992 days = 2.7 years
 
 We can't use the standard Arduino delay() or delaymicrosecond() because we shut down timer0 to save power
 
 We turn off Brown out detect because it alone uses ~16uA.
 
 */
#include "main.h"

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

//Declaring this will enable IR broadcast when you hit the time button twice
//By default, we don't enable this
//#define ENABLE_TVBGONE 

//Set the 12hourMode to false for military/world time. Set it to true for American 12 hour time.
//int TwelveHourMode = true;

//Set this variable to change how long the time is shown on the watch face. In milliseconds so 1677 = 1.677 seconds
int show_time_length = 800;
int show_the_time = false;

//You can set always_on to true and the display will stay on all the time
//This will drain the battery in about 15 hours 
int always_on = false;

long seconds = 55;
int minutes = 22;
int hours = 12;

int display_brightness = 15000; //A larger number makes the display more dim. This is set correctly below.

int segA = 9; //Display pin 14
int segB = A1; //Display pin 16
int segC = 7; //Display pin 13
int segD = 6; //Display pin 3
int segE = 5; //Display pin 5
int segF = 10; //Display pin 11
int segG = A0; //Display pin 15
int colons = 8; //Display pin 4

int theButton2 = 3;
int theButton = 2;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//The very important 32.686kHz interrupt handler
SIGNAL(TIMER2_OVF_vect){
  seconds += 8; //We sleep for 8 seconds instead of 1 to save more power
  //seconds++; //Use this if we are waking up every second

  //Update the minutes and hours variables
  minutes += seconds / 60; //Example: seconds = 2317, minutes = 58 + 38 = 96
  seconds %= 60; //seconds = 37
  hours += minutes / 60; //12 + (96 / 60) = 13
  minutes %= 60; //minutes = 36

  while(hours > 12) hours -= 12;
}

//The interrupt occurs when you push the button
SIGNAL(INT0_vect){
  //When you hit the button, we will display the time
  //if(show_the_time == false) 
  show_the_time = true;
}

void setup() {                
  //To reduce power, setup all pins as inputs with no pullups
  for(int x = 1 ; x < 18 ; x++){
    pinMode(x, INPUT);
    digitalWrite(x, LOW);
  }

  pinMode(theButton, INPUT); //This is the main button, tied to INT0
  digitalWrite(theButton, HIGH); //Enable internal pull up on button

  pinMode(theButton2, INPUT); //This is the main button, tied to INT0
  digitalWrite(theButton2, HIGH); //Enable internal pull up on button

  //These pins are used to control the display
  pinMode(segA, OUTPUT);
  pinMode(segB, OUTPUT);
  pinMode(segC, OUTPUT);
  pinMode(segD, OUTPUT);
  pinMode(segE, OUTPUT);
  pinMode(segF, OUTPUT);
  pinMode(segG, OUTPUT);

  pinMode(colons, OUTPUT);

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_SAVE);
  sleep_enable();

  //Shut off ADC, TWI, SPI, Timer0, Timer1

  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR = (1<<ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); //Disable digital input buffer on AIN1/0

  power_twi_disable();
  power_spi_disable();
  //  power_usart0_disable(); //Needed for serial.print
  //power_timer0_disable(); //Needed for delay and millis()
  power_timer1_disable();
  //power_timer2_disable(); //Needed for asynchronous 32kHz operation

  //Setup TIMER2
  TCCR2A = 0x00;
  //TCCR2B = (1<<CS22)|(1<<CS20); //Set CLK/128 or overflow interrupt every 1s
  TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20); //Set CLK/1024 or overflow interrupt every 8s
  ASSR = (1<<AS2); //Enable asynchronous operation
  TIMSK2 = (1<<TOIE2); //Enable the timer 2 interrupt

  //Setup external INT0 interrupt
  EICRA = (1<<ISC01); //Interrupt on falling edge
  EIMSK = (1<<INT0); //Enable INT0 interrupt

  //System clock futzing
  //CLKPR = (1<<CLKPCE); //Enable clock writing
  //CLKPR = (1<<CLKPS3); //Divid the system clock by 256

  Serial.begin(9600);  
  Serial.println("BigTime Testing:");

  showTime(); //Show the current time for a few seconds

  sei(); //Enable global interrupts
}

void loop() {
  if(always_on == false) {
    Serial.print("Going to bed...");
    delayMicroseconds(10000); // to flush the serial buffer
    sleep_mode(); //Stop everything and go to sleep. Wake up if the Timer2 buffer overflows or if you hit the button
  }

  if(show_the_time == true || always_on == true) {

    //Debounce
    while(digitalRead(theButton) == LOW) ; //Wait for you to remove your finger
    delay(100);
    while(digitalRead(theButton) == LOW) ; //Wait for you to remove your finger

    //Serial.print(hours, DEC);
    //Serial.print(":");
    //Serial.print(minutes, DEC);
    //Serial.print(":");
    // Serial.println(seconds, DEC);

    showTime(); //Show the current time for a few seconds

    //If you are holding the button after the time is shown, then you must want to adjust the time
    if(digitalRead(theButton) == LOW) setTime();

    show_the_time = false; //Reset the show variable
  }
}

void showTime() {

  Serial.println("show time mode");  

  // these values are bogus
  //For the default red display:
  //Let's define a variable called display_brightness that varies from:
  //5000 blindingly bright (15.7mA current draw per digit)
  //2000 shockingly bright (11.4mA current draw per digit)
  //1000 pretty bright (5.9mA)
  //500 normal (3mA)
  //200 dim but readable (1.4mA)
  //50 dim but readable (0.56mA)
  //5 dim but readable (0.31mA)
  //1 dim but readable in dark (0.28mA)

  int combinedTime = (hours * 100) + minutes; //Combine the hours and minutes
  //int combinedTime = (minutes * 100) + seconds; //For testing, combine the minutes and seconds

    boolean buttonPreviouslyHit = false;

  long startTime = millis();
  while( (millis() - startTime) < 700) {
    //Each of these itterations takes about 8ms, display the colon
    displayNumber(hours, true); //Each call takes about 8ms, display the colon
    delayMicroseconds(10000); //Wait before we paint the display again - this makes the dimm effect - waiting
  }

  delayMicroseconds(700000);

  // mili = 1.000
  // micro = 1.000.000

    int min_10 = minutes / 10;
  int min_01 = minutes - (min_10 * 10);

  startTime = millis();
  while( (millis() - startTime) < 500) {
    displayNumber(min_10, false);
    delayMicroseconds(12000); //Wait before we paint the display again - this makes the dimm effect - waiting (and saves battery)

  }

  delayMicroseconds(500000);

  startTime = millis();
  while( (millis() - startTime) < 500) {
    displayNumber(min_01, false); //Each call takes about 8ms, display the colon
    delayMicroseconds(12000); //Wait before we paint the display again - this makes the dimm effect - waiting
  }


}


//This routine occurs when you hold the button down
//The colon blinks indicating we are in this mode
//Holding the button down will increase the time (accelerates)
//Releasing the button for more than 2 seconds will exit this mode
void setTime(void) {

  Serial.println("entered set time mode");

  int idleMiliseconds = 0;
  //This counts the number of times you are holding the button down consecutively
  //Once we notice you're really holding the button down a lot, we will speed up the minute counter
  int exitMode = 0;

  while(exitMode == 0) {

    //cli(); //We don't want the interrupt changing values at the same time we are!

    while(hours > 12) hours -= 12;

    int min_10 = minutes / 10;
    int min_01 = minutes - (min_10 * 10);

    long startTime = millis();
    while( (millis() - startTime) < 800) {
      displayNumber(hours, true); //Each loop call takes about 8ms, display the number for about x*8ms, then continue
      delayMicroseconds(5000); //Wait before we paint the display again - this makes the dimm effect - waiting
    }

    delayMicroseconds(600000);

    startTime = millis();
    while( (millis() - startTime) < 500) {
      displayNumber(min_10, false); //Each call takes about 8ms, turn off the colon for about 100ms
      delayMicroseconds(5000); //Wait before we paint the display again
    }

    delayMicroseconds(400000);

    startTime = millis();
    while( (millis() - startTime) < 500) {
      displayNumber(min_01, false); //Each call takes about 8ms, turn off the colon for about 100ms
      delayMicroseconds(5000); //Wait before we paint the display again
    }

    if(digitalRead(theButton) == LOW) {
      exitMode = 1;
      //sei(); //Resume interrupts
      Serial.println("exit set mode");
    }

    if(digitalRead(theButton2) == LOW) {
      Serial.println("setting - incrementing - time");
      seconds = 60; // we have pressed the button - reset the seconds to the end of the minute
      // also calculate other values
      minutes += seconds / 60; //Example: seconds = 2317, minutes = 58 + 38 = 96
      seconds %= 60; //seconds = 37
      hours += minutes / 60; //12 + (96 / 60) = 13
      minutes %= 60; //minutes = 36
    }
  }

}

//This is a not-so-accurate delay routine
//Calling fake_msdelay(100) will delay for about 100ms
//Assumes 8MHz clock
/*void fake_msdelay(int x){
 for( ; x > 0 ; x--)
 fake_usdelay(1000);
 }*/

//This is a not-so-accurate delay routine
//Calling fake_usdelay(100) will delay for about 100us
//Assumes 8MHz clock
/*void fake_usdelay(int x){
 for( ; x > 0 ; x--) {
 __asm__("nop\n\t"); 
 __asm__("nop\n\t"); 
 __asm__("nop\n\t"); 
 __asm__("nop\n\t"); 
 __asm__("nop\n\t"); 
 __asm__("nop\n\t"); 
 __asm__("nop\n\t"); 
 }
 }*/

void displayNumber(int toDisplay, boolean displayColon) {

#define DIGIT_ON  HIGH
#define DIGIT_OFF  LOW

  //Now display this digit (7 segment)
  lightNumber(toDisplay); //Turn on the right segments for this digit

  if (displayColon == true) {
    lightNumber('.'); 
  }

  delayMicroseconds(2000); //Display this digit for a fraction of a second (between 1us and 5000us, 500-2000 is pretty good)
  //If you set this too long, the display will start to flicker. Set it to 25000 for some fun.

  //Turn off all segments
  lightNumber(13);

}

//Given a number, turns on those segments
//If number == 13, then turn off all segments
void lightNumber(int numberToDisplay) {

#define SEGMENT_ON  LOW
#define SEGMENT_OFF HIGH

  /*
Segments
   -  A
   F / / B
   -  G
   E / / C
   -  D
   */

  switch (numberToDisplay){

  case 0:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    break;

  case 1:
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    break;

  case 2:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 3:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 4:
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 5:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 6:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 7:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    break;

  case 8:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 9:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 10:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case 11:
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    break;

  case 12:
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    break;

    // all segments off
  case 13:
    digitalWrite(segA, SEGMENT_OFF);
    digitalWrite(segB, SEGMENT_OFF);
    digitalWrite(segC, SEGMENT_OFF);
    digitalWrite(segD, SEGMENT_OFF);
    digitalWrite(segE, SEGMENT_OFF);
    digitalWrite(segF, SEGMENT_OFF);
    digitalWrite(segG, SEGMENT_OFF);
    digitalWrite(colons, SEGMENT_OFF);
    break;

    /*
Segments
     -  A
     F / / B
     -    G
     E / / C
     - D
     */

    //Letters
  case 'b': //cdefg
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;
  case 'L': //def
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    break;
  case 'u': //cde
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    break;

  case 'g': //abcdfg
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;
  case 'r': //eg
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;
  case 'n': //ceg
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

    //case r
  case 'e': //adefg
    digitalWrite(segA, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;
  case 'd': //bcdeg
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;
  case ' ': //None
    digitalWrite(segA, SEGMENT_OFF);
    digitalWrite(segB, SEGMENT_OFF);
    digitalWrite(segC, SEGMENT_OFF);
    digitalWrite(segD, SEGMENT_OFF);
    digitalWrite(segE, SEGMENT_OFF);
    digitalWrite(segF, SEGMENT_OFF);
    digitalWrite(segG, SEGMENT_OFF);
    break;

  case 'y': //bcdfg
    digitalWrite(segB, SEGMENT_ON);
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segF, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;
    //case e 
    //case L
  case 'o': //cdeg
    digitalWrite(segC, SEGMENT_ON);
    digitalWrite(segD, SEGMENT_ON);
    digitalWrite(segE, SEGMENT_ON);
    digitalWrite(segG, SEGMENT_ON);
    break;

  case '.':
    digitalWrite(colons, SEGMENT_ON);
    break;

  }
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
