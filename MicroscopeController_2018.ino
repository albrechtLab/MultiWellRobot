/** ------------------------------
Control valves at specific frames during a streming acquisition
Input trigger from camera comes from pin 2&3 wired together
Output trigger mirrors input for controlling the illumination
Valves are triggered to open and close at specific frames according to the serial command:
e.g. 50,150 turns valve 1 on at frame 50 and off and frame 150
     10,20,30,40,50,60,70,80,90,100 turns valves on and off every 10 frames for 5 cycles
Valves can be controlled specifically with the serial commands:
   v1on, v1off , v2on, v2off

   list of intensity levels (0-255) can be specified by prefacing with "i"
   i10,i20,i30,i255,i0 specifies  intensity levels that switch at the frames determined above

   can also interlace:
   e.g. 50,i100,100,i0 means turn on to 100/255 power at frame 50, then off at frame 100

https://codebender.cc/sketch:272121

**/

/***************************************************
  Uses touchscreen Adafruit ILI9341 captouch shield
  ----> http://www.adafruit.com/products/1947

  Check out the links above for our tutorials and wiring diagrams

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
****************************************************/


#include <Adafruit_GFX.h>     // Core graphics library
#include <SPI.h>            // this is needed for display
#include <Adafruit_ILI9341.h> // TFT display
#include <Wire.h>           // this is needed for FT6206
#include <Adafruit_FT6206.h>  // Touchscreen

// The FT6206 uses hardware I2C (SCL/SDA)
Adafruit_FT6206 ctp = Adafruit_FT6206();


// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define TRIGGERIN  2  // for interrupt 0
#define TRIGGEROUT 7  // Digital out for exitation

#define LED1 5  //  PWM for adjustable LED intensity
#define LED2 6  //  PWM for adjustable LED intensity

#define VALVE1 A0 // TTL for valves, high = open
#define VALVE2 A1
#define VALVE3 A2

#define INPUT_SIZE 128 // max input serial string size
#define MAX_SWITCHES 32 // max number of switch frames

const int BF_LEDON = 210; // Intensity control for BF; <80 = max; >220 = off; ~linear in between, 150 = 50% current
const int BF_LEDOFF = 255;

//#define FL_ON LOW     // should be HIGH for Mightex; LOW for Lumencor
//#define FL_OFF HIGH
//#define INPUT_ON LOW  // depends on and should match Micromanager settings
//#define INPUT_OFF HIGH

#define xdiv 2      // display x-scaling 

byte FL_ON = LOW;
byte FL_OFF = HIGH;
byte INPUT_ON = LOW;
byte INPUT_OFF = HIGH;

const int brightfieldInterval = 2;  // how oftten to pulse brightfield illumination
byte testHz = 10;         // test signal frequency
long int BFmicros = 5;        // brightfield illumination pulse duration (us)

// Initialize variables
int valve1state = LOW;         // default valve 1 OFF
int valve2state = LOW;        // default valve 2 OFF
int valve3state = LOW;        // default valve 3 OFF
int outputstate = FL_OFF;      // default value for Fluorescence (FL) output
int LEDlevel = 0;          // default value for LED1

volatile long pulseCount = 0;
int valveSwitchCount = 0;

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
boolean getTouchPoint = false;
boolean updateDisplay = true;
boolean manualEntry = false;
boolean switchToNextValve = false;

//int FL = 1;
boolean FL = true;
boolean BF = false;
boolean testMode = false;
boolean editSettings = false;

int OnOffFrames[MAX_SWITCHES];
int Intensity[MAX_SWITCHES];
int V1states[MAX_SWITCHES];

int xos = 5; 
int yos = 255;

int prevx = 0;
int prevy = 0;
long int ms = millis();
long int testms = ms;
float fps;

char input[INPUT_SIZE + 1];

const char keypad[] = "123456789i0,";

void setup()
{
  for (int i=0; i<MAX_SWITCHES; i++) {
    OnOffFrames[i] = -1;
    Intensity[i] = -1;
  }
  
  pinMode(TRIGGERIN, INPUT);
  pinMode(TRIGGEROUT, OUTPUT);
  pinMode(VALVE1, OUTPUT);
  pinMode(VALVE2, OUTPUT);
  pinMode(VALVE3, OUTPUT);
  pinMode(LED1, OUTPUT);

  digitalWrite(TRIGGEROUT, outputstate);
  digitalWrite(VALVE1, valve1state);
  digitalWrite(VALVE2, valve2state);
  digitalWrite(VALVE3, valve3state);

  digitalWrite(LED1, LOW);

  attachInterrupt(0, triggerChange, CHANGE);
  attachInterrupt(1, screenTouched, LOW);
  
  // initialize LCD display
  Serial.begin(9600);
  Serial.println(F("Biovision Microscope Controller v0.1"));
  
    tft.begin();

    if (! ctp.begin(40)) {  // pass in 'sensitivity' coefficient
      Serial.println(F("Couldn't start FT6206 touchscreen controller"));
      while (1);
    }

    Serial.println(F("Capacitive touchscreen started"));
  
  if (!BF) analogWrite(LED2, BF_LEDOFF);
  resetScreen();
  
}

void loop()
{
  //digitalWrite(VALVE3, LOW);

  // Check for serial command to define and start the pulse counting and valve control.
  int i = 0;
  int j = 0;

  while (Serial.available() > 0 | manualEntry)
  {

    if (Serial.available()) {
      // Get next command from Serial (add 1 for termination character)
      byte size = Serial.readBytes(input, INPUT_SIZE);
      // Add the final 0 to end the string
      input[size] = 0;
      inputString = String(input);
    } else resetScreen();
    manualEntry = false;
    
    Serial.print(F("Input: ")); Serial.print(inputString);
    boolean timingEntry = true;
    
    // Check for manual valve command: v1on/v1off/v2on/v2off, etc
    char* ch = strchr(input, 'v');
    if (ch != 0)
    {
      ++ch;
      int valvenum = atoi(ch);
      ch += 2;

      if (valvenum == 1)
      {
        valve1state = (strchr(input, 'on') != 0);
        digitalWrite(VALVE1, valve1state);
        Serial.print("Valve 1 switch to: ");
        Serial.println(valve1state);
      }
      if (valvenum == 2)
      {
        valve2state = (strchr(input, 'on') != 0);
        digitalWrite(VALVE2, valve2state);
        Serial.print("Valve 2 switch to: ");
        Serial.println(valve2state);
      }
      if (valvenum == 3)
      {
        valve3state = (strchr(input, 'on') != 0);
        digitalWrite(VALVE3, valve3state);
        Serial.print("Valve 3 switch to: ");
        Serial.println(valve3state);
      }
      timingEntry = false;
    }
    
    // added 8/13/15 dra
    //char* 
    ch = strchr(input, '_');  // _F = fluor, _B = brightfield, _BF or _BF = both
    if (ch != 0)
    {
      ++ch;

      BF = (strchr(input, 'B') != 0);
      FL = (strchr(input, 'F') != 0);
      
      drawButton(3,5,"BF",ILI9341_GREEN, BF);
      drawButton(2,5,"FL",ILI9341_BLUE, FL);
      
      timingEntry = false;
    }

    if (timingEntry)
    ///
    
    {

      Serial.print(F("Parse:"));
      // Parse timing and LED intensity commands
      char* command = strtok(input, " ,");
      while (command != 0)
      {
        Serial.print(" "); Serial.print(command); 

        // Check if intensity value
        char* separator = strchr(command, 'i');
        if (separator != 0)
        {
          *separator = 0;
          ++separator;
          Intensity[j] = atoi(separator);
          j++;
        }
        else
        {
          OnOffFrames[i] = atoi(command);
          V1states[i] = (i+1) % 2;  // alternate valve states
          i++;
        }

        // Find the next command in input string
        command = strtok(0, " ,");
      }

      Serial.print(F("Frames ")); Serial.print(i);
      Serial.print(F(", Intensity ")); Serial.println(j);
      
      for (int z = i; z < MAX_SWITCHES; z++) OnOffFrames[z] = -1;  // blank out any previous frame switch settings
      for (int z = j; z < MAX_SWITCHES; z++) Intensity[z] = -1;  // blank out any previous intensity settings
    
      if (i > 0)                  // if timing data, then:
      {
        pulseCount = -2;                    // Initialize pulse count
        valveSwitchCount = 0;             // Initialize valve switch count
        digitalWrite(VALVE1, LOW);        // Initialize valve 1 off
        digitalWrite(VALVE2, HIGH);          // Initialize valve 2 off
        digitalWrite(VALVE3, LOW);          // Initialize valve 3 off
        //digitalWrite(LED1, LOW);        // Initialize LED off
      
        tft.fillRect(1,45,tft.width(),4,ILI9341_BLACK);
        tft.fillRect(xos,yos,64,48,ILI9341_BLACK);
      
        displayGraph(30,65,"V1",OnOffFrames,V1states,1,10.0);
        //displayGraph(30,90,"V2",OnOffFrames,V1states,-1000,-0.01);
        //displayGraph(30,115,"V3",OnOffFrames,V1states,-1000,-0.01);
        //displayGraph(30,140,"V4",OnOffFrames,V1states,-1000,-0.01);
        //displayGraph(30,180,"LED1",OnOffFrames,Intensity,255,0.1);
        //displayGraph(30,220,"LED2",OnOffFrames,V1states,-255,-0.1);
        displayGraph(30,130,"LED1",OnOffFrames,Intensity,255,0.1);
        
        if (~BF) analogWrite(LED2, BF_LEDOFF);
        drawButton(3,5,"BF",ILI9341_GREEN, BF);
        drawButton(2,5,"FL",ILI9341_BLUE, FL);

      }
      else if (j == 1) {
        LEDlevel = Intensity[0];
        Serial.println(LEDlevel);
        if (LEDlevel >= 0) {
          analogWrite(LED1, LEDlevel);
          Serial.println(LEDlevel);
        }
      }

      Serial.read();   //
    }
  }

  if (testMode) {    // establish a 1ms pulse every 100ms test input
    //long delayms = testms + 60000 - millis();
    int delayms = testms + (1000/testHz) - millis();
    if (delayms > 0) delay(delayms);
    
    //testms = millis();
    
    //for slow led pulsing
    //analogWrite(LED1,255);
    //delay(5000);
    //analogWrite(LED1,0);
    
    digitalWrite(TRIGGERIN, INPUT_ON);
    delayMicroseconds(1000);
    digitalWrite(TRIGGERIN, INPUT_OFF);
    //Serial.print('.');
    Serial.println(testms);
  }

  // Update valves if pulseCount equals the next switch frame
  if (switchToNextValve)
  {

    LEDlevel = Intensity[valveSwitchCount];
    if (LEDlevel >= 0) analogWrite(LED1, LEDlevel);

    valveSwitchCount++;
    valve1state = valveSwitchCount % 2;
    digitalWrite(VALVE1, valve1state);
    
    switchToNextValve = false;

  }

  if (updateDisplay) {
  
    //tft.fillRect(xos,yos,64,48,ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setTextSize(1);

    if (pulseCount <= 0) {
      tft.setCursor(xos+ 0 * 6, yos+ 0 * 8);
      tft.print("frame next");
      tft.setCursor(xos+ 6 * 6, yos+ 3 * 8);
      tft.print("LED");
      tft.setCursor(xos+ 0 * 6, yos+ 3 * 8);
      tft.print("v1:");
      tft.setCursor(xos+ 0 * 6, yos+ 4 * 8);
      tft.print("v2:");
    }
    tft.setCursor(xos+ 0 * 6, yos+ 1 * 8);
    tft.print(pulseCount); tft.print(" ");
    tft.setCursor(xos+ 6 * 6, yos+ 1 * 8);
    tft.print(OnOffFrames[valveSwitchCount]); tft.print("  ");
    tft.setCursor(xos+ 3 * 6, yos+ 3 * 8);
    tft.print(valve1state);
    tft.setCursor(xos+ 3 * 6, yos+ 4 * 8);
    tft.print(valve2state);
      tft.setCursor(xos+ 3 * 6, yos+ 5 * 8);
    tft.print(valve3state);
    tft.setCursor(xos+ 6 * 6, yos+ 4 * 8);
    tft.print(LEDlevel); tft.print("  ");
    if (pulseCount % (int) fps == 0) {      // display fps every ~1s
      tft.setCursor(xos+ 0 * 6, yos+ 6 * 8);
      tft.print(fps); tft.print(" Hz ");
    }
      //tft.drawLine( 30 + pulseCount/2 - 1, 65-10-5, 30 + pulseCount/2 - 1, 65-10-10, ILI9341_BLACK);
      if (pulseCount % xdiv == 1) tft.drawLine( 30 + pulseCount/xdiv, 48, 30 + pulseCount/xdiv, 45, ILI9341_GREEN);
  
    updateDisplay = false;
  }

  if (getTouchPoint) {

    TS_Point p = ctp.getPoint();
    int x = map(p.x,0,240,240,0);
    int y = map(p.y,0,320,320,0);
      
    if ( (p.x * p.y > 0) && (abs(prevx-x)>5) && (abs(prevy-y)>5) ) {

      Serial.print(x); Serial.print(","); Serial.print(y);
      
      byte bx = byte(x/48);
      byte by = byte(y/48);
      
      if (bx == 2 && by == 5) {
        FL = !FL;
        drawButton(2,5,"FL", ILI9341_BLUE, FL);
      }
      
      if (bx == 3 && by >= 5) {
        if (by > 5) {
          BFmicros *= 2;
          if (BFmicros > 1000) BFmicros = 1;
        } else BF = !BF;
        String label = "BF| |"; label += BFmicros; label += "us";
        drawButton(3,5,label,ILI9341_GREEN, BF);
      } 
  
      if (bx == 4 && by >= 5) {
        if (by > 5) {
          testHz *= 2;
          if (testHz > 100) testHz = 1; 
        } else testMode = !testMode;
        String label = "test| |"; label += testHz; label += "Hz";
        drawButton(4,5,label,ILI9341_RED, testMode);
        if (testMode) {
          pinMode(TRIGGERIN, OUTPUT);
        } else {
          pinMode(TRIGGERIN, INPUT);
        }
      }
      if (bx == 4 && by == 4) {
        Serial.print("Reset");
        //Serial.println(inputString);
        manualEntry = true;
        inputString.toCharArray(input,INPUT_SIZE+1);
      }
      if (bx == 3 && by == 4) {
        Serial.print("Preset");
        manualEntry = true;
        inputString = "50,100\n";
        inputString.toCharArray(input,INPUT_SIZE+1);
      }
      if (bx == 2 && by == 4) {
        Serial.print("Enter program");
        //getSettings();
        manualEntry = true;
        inputString = getSettings(inputString,"Enter new command:");
        inputString.toCharArray(input,INPUT_SIZE+1);
      }
      if (bx == 1 && by == 4) {
        Serial.print("Settings");
        editSettings = !editSettings;
        drawButton(1,4,"Set-|tings", 0x7BEF, editSettings); 

        if (editSettings)
        {
          tft.fillRect(1,45,tft.width(),100,ILI9341_BLACK);
          tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
          tft.setCursor(64,50); tft.print(F("Signal polarity:"));
          tft.setCursor(52,80); tft.print(F("Camera"));
          tft.setCursor(180,80); tft.print(F("Fluor."));
          drawButton(1,2,(INPUT_ON ? "Pos| | +" : "Neg| | -"),ILI9341_WHITE,INPUT_ON);
          drawButton(3,2,(FL_ON ? "Pos| | +" : "Neg| | -"),ILI9341_BLUE,FL_ON);
        } else {
          manualEntry = true;
          inputString.toCharArray(input,INPUT_SIZE+1);
        }
      }
      if (bx == 1 && by == 2 & editSettings) {
        INPUT_ON = !INPUT_ON;
        INPUT_OFF = !INPUT_OFF;
        drawButton(1,2,(INPUT_ON ? "Pos| | +" : "Neg| | -"),ILI9341_WHITE,INPUT_ON);
      }
      if (bx == 3 && by == 2 & editSettings) {
        FL_ON = !FL_ON;
        FL_OFF = !FL_OFF;
        drawButton(3,2,(FL_ON ? "Pos| | +" : "Neg| | -"),ILI9341_BLUE,FL_ON);
      }
      Serial.print(" --> "); Serial.print(bx); Serial.print(","); Serial.println(by);
      
    }
    prevx = x; prevy = y;
    getTouchPoint = false;

    /*if (y < 240) {
      pulseCount++;
    } */
  }
}

void drawButton( int xpos, int ypos, String buttonName, unsigned int buttonColor, boolean pressed)
{
  if (pressed) {
    tft.fillRoundRect(48*xpos+1,48*ypos+1,48-2,48-2,10,buttonColor);
    tft.setTextColor(ILI9341_BLACK, buttonColor);
  } else {
    tft.fillRoundRect(48*xpos+1,48*ypos+1,48-2,48-2,10,ILI9341_BLACK);
    tft.drawRoundRect(48*xpos+1,48*ypos+1,48-2,48-2,10,buttonColor);
    tft.setTextColor(buttonColor, ILI9341_BLACK);
  }
  //tft.setCursor(48*xpos+10,48*ypos+12);
  //tft.print(buttonName);
  
  int buttonLine = 0;
  char buttonChars[INPUT_SIZE + 1];
  buttonName.toCharArray(buttonChars, INPUT_SIZE+1);
  char* buttonText = strtok(buttonChars, "|");
    while (buttonText != 0)
    {
      tft.setCursor(48*xpos+10,48*ypos+12+buttonLine*8);
      tft.print(buttonText);
      buttonText = strtok(0, "|");
      buttonLine++;
    }
}


void displayGraph( int xos, int yos, String graphName, int xlist[], int ylist[], int maxy, float ysc )
{
  tft.fillRect(0,yos-maxy*ysc,tft.width(),ysc*maxy+20,ILI9341_BLACK);
  
  //tft.setCursor(xos-25, yos-25);
  tft.setCursor(xos-25, yos-maxy*ysc);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(1);
  tft.print(graphName);
  
  //int xdiv = 2;
  int i=0;
  int x1 = 0;
  int y1 = 0;
  int x2,y2;
  
    for (i=0; (i<MAX_SWITCHES-1) & (xlist[i] > -1); i++) {
      if (i>0) x1 = xlist[i-1];
      x2 = xlist[i];
      if (i>0) y1 = ylist[i-1];
      y2 = ylist[i];
  
      //Serial.println(x1); Serial.println(y1); Serial.println(x2); Serial.println(y2);
      tft.drawLine( xos+x1/xdiv, yos-y1*ysc, xos+x2/xdiv, yos-y1*ysc, ILI9341_WHITE);
      tft.drawLine( xos+x2/xdiv, yos-y1*ysc, xos+x2/xdiv, yos-y2*ysc, ILI9341_WHITE);
      //Serial.println(i);
    }
    //Serial.print(i); Serial.println(x2);
    tft.drawLine( xos+x2/xdiv, yos-y2*ysc, xos+200, yos-y2*ysc, ILI9341_WHITE);
  
  for (int i=0; i<=200; i+=50/xdiv) {
    tft.drawPixel( xos + i, yos-maxy*ysc-4, ILI9341_RED);
    tft.drawPixel( xos + i, yos+4, ILI9341_RED);
  }
}

String getSettings(String inputString, String prompt)
//void getSettings()
{
  tft.fillRect(0,20,tft.width(),280,ILI9341_BLACK);

  drawButton(1,2,"  1",ILI9341_WHITE, false); 
  drawButton(2,2,"  2",ILI9341_WHITE, false);
  drawButton(3,2,"  3",ILI9341_WHITE, false);
  drawButton(1,3,"  4",ILI9341_WHITE, false);
  drawButton(2,3,"  5",ILI9341_WHITE, false);
  drawButton(3,3,"  6",ILI9341_WHITE, false);
  drawButton(1,4,"  7",ILI9341_WHITE, false);
  drawButton(2,4,"  8",ILI9341_WHITE, false);
  drawButton(3,4,"  9",ILI9341_WHITE, false);
  drawButton(1,5,"  i",ILI9341_WHITE, false);
  drawButton(2,5,"  0",ILI9341_WHITE, false);
  drawButton(3,5,"  ,",ILI9341_WHITE, false);

  drawButton(0,5," Go|Back",ILI9341_RED, false);
  drawButton(4,5," OK!",ILI9341_GREEN, true);
  
  delay(1000);
  
  String newString = "";
  //noInterrupts();
  boolean entryDone = false;
  boolean getTouchPoint = false;
  
  
  tft.setTextColor(0x7BEF, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(20,30);
  tft.print(prompt);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(20,40);
  
  while (!entryDone) {
  
    if (ctp.touched()) {
      TS_Point p = ctp.getPoint();
      byte bx = byte(map(p.x,0,240,240,0)/48);
      byte by = byte(map(p.y,0,320,320,0)/48);
      if (bx == 0 && by == 5) {
        entryDone = true;
        return inputString;
        Serial.println("Cancel");
      }
      if (bx == 4 && by == 5) {
        entryDone = true;
        newString += "\n";
        return newString;
        Serial.println("OK");
      }
      if (bx >= 1 && bx <= 3 && by >= 2) {
        byte val = (by-2)*3+bx;
        newString += keypad[val-1];
        tft.print(keypad[val-1]);
      }
      if (by < 2) {  
        newString = newString.substring(0,newString.length()-1);
        tft.fillRect(0,40,tft.width(),48,ILI9341_BLACK);
        tft.setCursor(20,40);
        tft.print(newString);
      }
    
    }
    delay(150);
    
  }
  getTouchPoint = false;
  //manualEntry = true;
  //interrupts();
}

void resetScreen()
{
  tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(0x7BEF, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5,5);
  tft.print(F("RAMM Controller 2017 v1"));

  drawButton(3,5,"BF",ILI9341_GREEN, BF); 
  drawButton(2,5,"FL",ILI9341_BLUE, FL);
    drawButton(4,5,"test",ILI9341_RED, testMode);
   
    drawButton(1,4,"Set-|tings", 0x7BEF, false); 
    drawButton(2,4,"Enter|Prog.", 0x7BEF, false); 
    drawButton(3,4,"Pre-|set", 0x7BEF, false);
    drawButton(4,4,"Reset", 0x7BEF, false);
}

void triggerChange()
{
  if (digitalRead(TRIGGERIN) == INPUT_ON)  // input signal from camera topulse the light
  {
    pulseCount++;
    if (pulseCount == OnOffFrames[valveSwitchCount]) switchToNextValve = true;
    
    updateDisplay = true;
    fps = 1000.0 / (millis()-testms);
    testms = millis();
    
    if (BF && FL) {
      if (pulseCount % brightfieldInterval) {
        analogWrite(LED2, 0);
        delayMicroseconds(BFmicros);
        analogWrite(LED2, BF_LEDOFF);
      } else {
        digitalWrite(TRIGGEROUT, FL_ON); 
      }
    } else {
      if (BF) {
        analogWrite(LED2, 0); delayMicroseconds(BFmicros);
        analogWrite(LED2, BF_LEDOFF);
      }
      if (FL) digitalWrite(TRIGGEROUT, FL_ON);
    }
  } 
  else 
  {
    analogWrite(LED2, BF_LEDOFF);
    digitalWrite(TRIGGEROUT, FL_OFF);
  }
}

void screenTouched()
{
  if (millis()-ms > 100)
  {
    getTouchPoint = true;
    ms = millis();
  }
}

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}