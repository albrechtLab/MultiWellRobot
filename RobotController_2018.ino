/*
This is the final WORKING code for the servo-multiWell plate controller
	
For use with the Adafruit Motor Shield v2
---->	http://www.adafruit.com/products/1438
*/


#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "Adafruit_PWMServoDriver.h"
#include <Servo.h>

#define INPUT_SIZE 128 // max input serial string size

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield();

// Connect a stepper motor with 200 steps per revolution (1.8 degree)
Adafruit_StepperMotor *yMotor = AFMS.getStepper(200, 1);
Adafruit_StepperMotor *xMotor = AFMS.getStepper(200, 2);

Servo servo;

const int stepSize = 45;  // 20 tooth gear --> 0.2 mm/step, 9 mm well-to-well spacing on 96-well plate
const int servoDelay = 300; // time for servo to move up <--> down in ms

const int xMinLimitPin = 4;
const int xMaxLimitPin = 5;
const int yMinLimitPin = 6;
const int yMaxLimitPin = 7;

const int zPosPin = 3;

int i = 0;
int val = 0;

const boolean showStatus = false; // print status info to serial port

boolean limitReached;
int limitState = 0;
int priorState = 0;
int ypos = 0;
int xpos = 0;
int newypos = 0;
int newxpos = 0;
int dy = 0;
int dx = 0;
int zdown = 0;
int newzdown = 0;
int A1x = 90; // 32 x-steps from home to A1 well position
int A1y = -345; // y-steps from home to A1 well position

char input[INPUT_SIZE + 1];  // max characters for input

void setup()
{
	Serial.begin(9600);           // set up Serial library at 9600 bps

	AFMS.begin();  // create with the default frequency 1.6KHz

	xMotor->setSpeed(100);  // rpm
	yMotor->setSpeed(100);  // rpm

	servo.attach(zPosPin);

	pinMode(xMinLimitPin, INPUT_PULLUP);
	pinMode(xMaxLimitPin, INPUT_PULLUP);
	pinMode(yMinLimitPin, INPUT_PULLUP);
	pinMode(yMaxLimitPin, INPUT_PULLUP);
}

void loop()
{

	while (Serial.available() > 0)
	{

		// Read serial input line
		if (Serial.available()) {
			// Get next command from Serial (add 1 for termination character)
			byte size = Serial.readBytes(input, INPUT_SIZE);
			// Add the final 0 to end the string
			input[size] = 0;
			//inputString = String(input);
		}

		// Parse serial input (well list, comma separated, e.g. "A1-,B1-")
		char* command = strtok(input, " ,");
		while (command != 0)
		{				
			//Serial.print(" "); Serial.print(command); 
			
			moveToWell(command);
			
			// Find the next command in input string
			command = strtok(0, " ,");
		}
		Serial.println("OK");
		
	}

	if (showStatus)
	{
		priorState = limitState;
		limitState = readLimitSwitches();
		if (limitState != priorState) Serial.println(limitState);
		delay(50);
	}

}

void moveToWell (String command)
{
	
		char row = command[0];  // read row character A...H
		char col = command[1];   // read column integer 1 - 12
		if (col == '1') 
		{
			int col2 = command[2];
			if ((col2 >= '0') && (col2 <= '2'))
			{
				col = 10 + col2 - '0';
			} else {
				col = col - '0';
			}
		} else {
			col = col - '0';
		}
		char z = command[2 + (col>=10)]; // read z-position "+"=up, "-"=down

		newypos = row - 'A' + 1;
		newxpos = col;
		newzdown = (z == '-');

		// move to up position
		// *make 50 and move servo bar to position 2
		// *make 80 and move servo bar to position 1
		servo.write(50); 	// *was 60=up but made 80 to get to bottom of 2mL well, allows 40mm tube length into 43mm deep well
		if (zdown) 
		{
			delay(servoDelay);
			zdown = 0;
		}
	
		if (newypos == -16)    // '0' to home and move to well A1
		{

			// home the stage
			homeStage();

			// Adjust to well A1 position
			limitReached = MoveStepsAndCheckLimits(A1x, A1y);
			if (limitReached && showStatus)
			{
				Serial.println("Warning!! Limit Reached");
			}

			ypos = 1;
			xpos = 1;
			newzdown = 0;   // home with z-position up

		}
		else
		{

			// handle y-position
			if (newypos < 1) newypos = ypos;
			if (newypos > 8) newypos = ypos;
			dy = newypos - ypos;

			// handle x-position
			if (newxpos < 1) newxpos = xpos;
			if (newxpos > 12) newxpos = xpos;
			dx = newxpos - xpos;

			// move steppers
			limitReached = MoveStepsAndCheckLimits(dx * stepSize, 0);
			limitReached = MoveStepsAndCheckLimits(0, dy * stepSize);

			if (limitReached && showStatus)
			{
				Serial.println("Warning!! Limit Reached");
			}

			// Update positions
			ypos = newypos;
			xpos = newxpos;
		}

		// Release power to motors
		xMotor->release();
		yMotor->release();

		// Handle z-position
		// only move if needed
		
		if (newzdown)
		{
			servo.write(0);
			delay(servoDelay);
			zdown = newzdown;
		}	

		if (showStatus) Serial.print(z);

		//delay(500);
		
		// Send serial command to confirm new position and motor completion
		if (showStatus) {
			Serial.print(row);
			Serial.print(col);
			Serial.println(z);
		}
	
}

void homeStage()
{
	// home y-axis
	boolean yHome = false;
	while (!yHome)
	{
		yHome = MoveStepsAndCheckLimits(0, 1);
	}
	if (showStatus) Serial.println("Home on y-axis");

	delay(100);

	// home x-axis
	boolean xHome = false;
	while (!xHome)
	{
		xHome = MoveStepsAndCheckLimits(-1, 0);
	}
	if (showStatus) Serial.println("Home on x-axis");
}

int readLimitSwitches()
{
	int limitState  = !digitalRead(xMinLimitPin) * 8;
	limitState += !digitalRead(xMaxLimitPin) * 4;
	limitState += !digitalRead(yMinLimitPin) * 2;
	limitState += !digitalRead(yMaxLimitPin) * 1;

	return limitState;
}

boolean MoveStepsAndCheckLimits(int stepx, int stepy)
{

	//boolean limitReached = false;
	boolean xfwd = (stepx > 0);
	boolean yfwd = (stepy > 0);

	int limitState = readLimitSwitches();
	boolean limitReached = (limitState > 0);

	stepx = abs(stepx);
	stepy = abs(stepy);

	while (((stepx > 0) || (stepy > 0)) && !limitReached)
	{

		if (stepx > 0)
		{
			if (xfwd)
			{
				xMotor->onestep(FORWARD, DOUBLE);
			}
			else
			{
				xMotor->onestep(BACKWARD, DOUBLE);
			}
			stepx--;
		}

		if (stepy > 0)
		{
			if (yfwd)
			{
				yMotor->onestep(FORWARD, DOUBLE);
			}
			else
			{
				yMotor->onestep(BACKWARD, DOUBLE);
			}
			stepy--;
		}

		limitState = readLimitSwitches();
		limitReached = (limitState > 0);
	}

	// delay(10);
	// Handle Limits: back off
	while ((limitState & B1000) > 0)
	{
		xMotor->onestep(FORWARD, DOUBLE);
		limitState = readLimitSwitches();
	}
	while ((limitState & B0100) > 0)
	{
		xMotor->onestep(BACKWARD, DOUBLE);
		limitState = readLimitSwitches();
	}
	while ((limitState & B0010) > 0)
	{
		yMotor->onestep(FORWARD, DOUBLE);
		limitState = readLimitSwitches();
	}
	while ((limitState & B0001) > 0)
	{
		yMotor->onestep(BACKWARD, DOUBLE);
		limitState = readLimitSwitches();
	}

	return limitReached;
}