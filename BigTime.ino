/*
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
 
 7-2-2011: Currently at 1.13uA
 
 7-4-2011: Let's wake up every 8 seconds instead of every 1 to save even more power
 Since we don't display seconds, this should be fine
 We are now at ~1.05uA on average
 
 7-17-2011: 1.09uA, portable with coincell power
 Jumps to 1.47uA every 8 seconds with system clock lowered to 1MHz
 Jumps to 1.37uA every 8 seconds with system clock at 8MHz
 Let's keep the internal clock at 8MHz since it doesn't seem to help to lower the internal clock.
 
 8-11-2011: Adding display color so that production can more easily know what code is on the 
 pre-programmed ATmega. 
 
 8-19-2011: Now we can print things like "red, gren, blue, yelo".
 
 7-12-2012: Added TV-B-Gone off codes. TV-B-Gone by Mitch Altman and Limor Fried. This project allows BigTime to
 turn off TVs by soldering an IR LED to Arduino pin 3 and GND. I updated the enclosure design files as well to
 allow the 5MM LED to fit inside the circumference of the enclosure.
 
 */
#include "main.h"

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

//Declaring this will enable IR broadcast when you hit the time button twice
//By default, we don't enable this
//#define ENABLE_TVBGONE 

//Set the 12hourMode to false for military/world time. Set it to true for American 12 hour time.
int TwelveHourMode = true;

//Set this variable to change how long the time is shown on the watch face. In milliseconds so 1677 = 1.677 seconds
int show_time_length = 2000;
int show_the_time = false;

//You can set always_on to true and the display will stay on all the time
//This will drain the battery in about 15 hours 
int always_on = false;

long seconds = 55;
int minutes = 12;
int hours = 8;

//Careful messing with the system color, you can damage the display if
//you assign the wrong color. If you're in doubt, set it to red and load the code,
//then see what the color is.
#define RED  1
#define GREEN 2
#define BLUE  3
#define YELLOW  4
int systemColor = BLUE;
int display_brightness = 15000; //A larger number makes the display more dim. This is set correctly below.

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//Pin definitions
int digit1 = 9; //Display pin 1
int digit2 = 10; //Display pin 2
int digit3 = A0; //Display pin 6
int digit4 = A1; //Display pin 8

int segA = 6; //Display pin 14
int segB = 8; //Display pin 16
int segC = 5; //Display pin 13
int segD = 11; //Display pin 3
int segE = 13; //Display pin 5
int segF = 4; //Display pin 11
int segG = 7; //Display pin 15

int colons = 12; //Display pin 4
int ampm = 3; //Display pin 10

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

  //Do we display 12 hour or 24 hour time?
  if(TwelveHourMode == true) {
    //In 12 hour mode, hours go from 12 to 1 to 12.
    while(hours > 12) hours -= 12;
  }
  else {
    //In 24 hour mode, hours go from 0 to 23 to 0.
    while(hours > 23) hours -= 24;
  }
}

//The interrupt occurs when you push the button
SIGNAL(INT0_vect){
  //When you hit the button, we will need to display the time
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

  //These pins are used to control the display
  pinMode(segA, OUTPUT);
  pinMode(segB, OUTPUT);
  pinMode(segC, OUTPUT);
  pinMode(segD, OUTPUT);
  pinMode(segE, OUTPUT);
  pinMode(segF, OUTPUT);
  pinMode(segG, OUTPUT);

  pinMode(digit1, OUTPUT);
  pinMode(digit2, OUTPUT);
  pinMode(digit3, OUTPUT);
  pinMode(digit4, OUTPUT);
  pinMode(colons, OUTPUT);
  pinMode(ampm, OUTPUT);

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

  //Now display the system color - this is mostly for production to verify
  //that the correct code is loaded onto the ATmega
  //Display brightness changes based on color
  if(systemColor == RED) {
    display_brightness = 1500; //The higher the number, the lower the brightness
    showColor("red ");
  }
  else if(systemColor == GREEN) {
    display_brightness = 1;
    showColor("gren");
  }
  else if(systemColor == BLUE) {
    display_brightness = 1; //Works well for blue displays, uses about 5.5mA
    showColor("bLue");
  }
  else if(systemColor == YELLOW) {
    //display_brightness = 1500;
    display_brightness = 1;
    showColor("yeLo");
  }

  //  showTime(); //Show the current time for a few seconds

  sei(); //Enable global interrupts
}

void loop() {
  if(always_on == false)
    sleep_mode(); //Stop everything and go to sleep. Wake up if the Timer2 buffer overflows or if you hit the button

  if(show_the_time == true || always_on == true) {
    
    //Debounce
    while(digitalRead(theButton) == LOW) ; //Wait for you to remove your finger
    delay(100);
    while(digitalRead(theButton) == LOW) ; //Wait for you to remove your finger

    /*Serial.print(hours, DEC);
     Serial.print(":");
     Serial.print(minutes, DEC);
     Serial.print(":");
     Serial.println(seconds, DEC);*/

    showTime(); //Show the current time for a few seconds

    //If you are STILL holding the button, then you must want to adjust the time
    if(digitalRead(theButton) == LOW) setTime();

    show_the_time = false; //Reset the button variable
  }
}

void showTime() {

  //Here is where we display the time and PWM the segments
  //Display brightness changes based on color, red is too bright, blue is very dull
  //By painting the time, then turning off for a number of microseconds, we can control how bright the display is

  //Each digit is on for a certain amount of microseconds
  //Then it is turned off and we go to the next digit
  //We loop through each digit until we reach the show_time_length (usually about 2 seconds)

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

  //Now show the time for a certain length of time
  long startTime = millis();
  while( (millis() - startTime) < show_time_length) {
    displayNumber(combinedTime, true); //Each call takes about 8ms, display the colon

    //After the time is displayed, the segments are turned off
    //We control the brightness by modifying how long we wait between re-paints of the display
    //delayMicroseconds(display_brightness);

    //If you have hit and released the button while the display is on, start the IR off sequence
    if(digitalRead(theButton) == LOW) {
      buttonPreviouslyHit = true;
    }
    else if( (buttonPreviouslyHit == true) && (digitalRead(theButton) == HIGH) ) {

#ifdef ENABLE_TVBGONE
      //Disable TIMER2 for IR control
      TCCR2A = 0x00;
      TCCR2B = 0;
      ASSR = 0;
      TIMSK2 = 0; //Enable the timer 2 interrupt

      //Turn off all the things!
      sendAllCodes();

      //Setup TIMER2
      TCCR2A = 0x00;
      //TCCR2B = (1<<CS22)|(1<<CS20); //Set CLK/128 or overflow interrupt every 1s
      TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20); //Set CLK/1024 or overflow interrupt every 8s
      ASSR = (1<<AS2); //Enable asynchronous operation
      TIMSK2 = (1<<TOIE2); //Enable the timer 2 interrupt

      quickflashLEDx(2); //Blink Colons twice letting us know it's done
#endif

      return;
    }      
  }

}

//Displays a string on the display
//Used to indicate which code is loaded onto the watch
void showColor(char *colorName) {

  //Now show the letters for a certain length of time
  long startTime = millis();
  while( (millis() - startTime) < show_time_length) {
    displayLetters(colorName); //Each call takes about 8ms, display the colon

    //After the letters are displayed, the segments are turned off
    //We control the brightness by modifying how long we wait between re-paints of the display
    delayMicroseconds(display_brightness);
  }
}

//This routine occurs when you hold the button down
//The colon blinks indicating we are in this mode
//Holding the button down will increase the time (accelerates)
//Releasing the button for more than 2 seconds will exit this mode
void setTime(void) {

  int idleMiliseconds = 0;
  //This is the timeout counter. Once we get to ~2 seconds of inactivity, the watch
  //will exit the setTime function and return to normal operation

  int buttonHold = 0; 
  //This counts the number of times you are holding the button down consecutively
  //Once we notice you're really holding the button down a lot, we will speed up the minute counter

  while(idleMiliseconds < 2000) {

    cli(); //We don't want the interrupt changing values at the same time we are!

    //Update the minutes and hours variables
    minutes += seconds / 60; //Example: seconds = 2317, minutes = 58 + 38 = 96
    seconds %= 60; //seconds = 37
    hours += minutes / 60; //12 + (96 / 60) = 13
    minutes %= 60; //minutes = 36

    //Do we display 12 hour or 24 hour time?
    if(TwelveHourMode == true) {
      //In 12 hour mode, hours go from 12 to 1 to 12.
      while(hours > 12) hours -= 12;
    }
    else {
      //In 24 hour mode, hours go from 0 to 23 to 0.
      while(hours > 23) hours -= 24;
    }

    sei(); //Resume interrupts

    int combinedTime = (hours * 100) + minutes; //Combine the hours and minutes

      for(int x = 0 ; x < 10 ; x++) {
      displayNumber(combinedTime, true); //Each call takes about 8ms, display the colon for about 100ms
      delayMicroseconds(display_brightness); //Wait before we paint the display again
    }
    for(int x = 0 ; x < 10 ; x++) {
      displayNumber(combinedTime, false); //Each call takes about 8ms, turn off the colon for about 100ms
      delayMicroseconds(display_brightness); //Wait before we paint the display again
    }

    //If you're still hitting the button, then increase the time and reset the idleMili timeout variable
    if(digitalRead(theButton) == LOW) {
      idleMiliseconds = 0;

      buttonHold++;
      if(buttonHold < 10) //10 = 2 seconds
        minutes++; //Advance the minutes
      else {
        //Advance the minutes faster because you're holding the button for 10 seconds
        //Start advancing on the tens digit. Floor the single minute digit.
        minutes /= 10; //minutes = 46 / 10 = 4
        minutes *= 10; //minutes = 4 * 10 = 40
        minutes += 10;  //minutes = 40 + 10 = 50
      }
    }
    else
      buttonHold = 0;

    idleMiliseconds += 200;
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

//Given 1022, we display "10:22"
//Each digit is displayed for ~2000us, and cycles through the 4 digits
//After running through the 4 numbers, the display is turned off
void displayNumber(int toDisplay, boolean displayColon) {

#define DIGIT_ON  HIGH
#define DIGIT_OFF  LOW

  for(int digit = 4 ; digit > 0 ; digit--) {

    //Turn on a digit for a short amount of time
    switch(digit) {
    case 1:
      digitalWrite(digit1, DIGIT_ON);
      break;
    case 2:
      digitalWrite(digit2, DIGIT_ON);
      if(displayColon == true) digitalWrite(colons, DIGIT_ON); //When we update digit 2, let's turn on colons as well
      break;
    case 3:
      digitalWrite(digit3, DIGIT_ON);
      break;
    case 4:
      digitalWrite(digit4, DIGIT_ON);
      break;
    }

    //Now display this digit
    if(digit > 1)
      lightNumber(toDisplay % 10); //Turn on the right segments for this digit
    else if(digit == 1) { //Special case on first digit, don't display 02:11, display 2:11
      if( (toDisplay % 10) != 0) //If we are on the first digit, and it's not zero
        lightNumber(toDisplay % 10); //Turn on the right segments for this digit
    }

    toDisplay /= 10;

    delayMicroseconds(2000); //Display this digit for a fraction of a second (between 1us and 5000us, 500-2000 is pretty good)
    //If you set this too long, the display will start to flicker. Set it to 25000 for some fun.

    //Turn off all segments
    lightNumber(10);

    //Turn off all digits
    digitalWrite(digit1, DIGIT_OFF);
    digitalWrite(digit2, DIGIT_OFF);
    digitalWrite(digit3, DIGIT_OFF);
    digitalWrite(digit4, DIGIT_OFF);
    digitalWrite(colons, DIGIT_OFF);
    digitalWrite(ampm, DIGIT_OFF);
  }

}

//Takes a string like "gren" and displays it, left justified
//We don't use the colons, or AMPM dot, so they are turned off
void displayLetters(char *colorName) {
#define DIGIT_ON  HIGH
#define DIGIT_OFF  LOW

  digitalWrite(digit4, DIGIT_OFF);
  digitalWrite(colons, DIGIT_OFF);
  digitalWrite(ampm, DIGIT_OFF);

  for(int digit = 0 ; digit < 4 ; digit++) {
    //Turn on a digit for a short amount of time
    switch(digit) {
    case 0:
      digitalWrite(digit1, DIGIT_ON);
      break;
    case 1:
      digitalWrite(digit2, DIGIT_ON);
      break;
    case 2:
      digitalWrite(digit3, DIGIT_ON);
      break;
    case 3:
      digitalWrite(digit4, DIGIT_ON);
      break;
    }

    //Now display this letter
    lightNumber(colorName[digit]); //Turn on the right segments for this letter

    delayMicroseconds(2000); //Display this digit for a fraction of a second (between 1us and 5000us, 500-2000 is pretty good)
    //If you set this too long, the display will start to flicker. Set it to 25000 for some fun.

    //Turn off all segments
    lightNumber(10);

    //Turn off all digits
    digitalWrite(digit1, DIGIT_OFF);
    digitalWrite(digit2, DIGIT_OFF);
    digitalWrite(digit3, DIGIT_OFF);
  }
}

//Given a number, turns on those segments
//If number == 10, then turn off all segments
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
    digitalWrite(segA, SEGMENT_OFF);
    digitalWrite(segB, SEGMENT_OFF);
    digitalWrite(segC, SEGMENT_OFF);
    digitalWrite(segD, SEGMENT_OFF);
    digitalWrite(segE, SEGMENT_OFF);
    digitalWrite(segF, SEGMENT_OFF);
    digitalWrite(segG, SEGMENT_OFF);
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

  }
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=





