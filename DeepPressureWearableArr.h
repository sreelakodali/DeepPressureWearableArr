// DeepPressureWearableArr.h
// Written by: Sreela Kodali, kodali@stanford.edu

#ifndef DeepPressureWearableArr_h
#define DeepPressureWearableArr_h

# define N_ACT 2

#include "Arduino.h"
// #else
// #include "WProgram.h"
// #endif

#include "SparkFun_Displacement_Sensor_Arduino_Library.h"
#include <Servo.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>


// Notes: haven't added SD card stuff yet, single actuator

typedef enum {
	FLEX_MIN = 0,
	FLEX_MAX = 180
} FLEX_SENSOR_LIMITS;

typedef enum {
	POSITION_MIN = 47,
	POSITION_MAX = 139
} ACTUATOR_LIMITS;

// Calibration states
typedef enum { NONE, ZERO_FORCE, FLEX, MAX_PRESSURE, ACTUATOR  
} CALIBRATION_OPTIONS;

typedef enum {
	NO_INPUT,
	FLEX_INPUT,
	KEYBOARD_INPUT
} INPUT_TYPE;

class DeepPressureWearableArr {
	public:
	// methods
	DeepPressureWearableArr(INPUT_TYPE input, bool serial, bool c);

	int  N_ACTUATORS = N_ACT;

	// Calibration 
	int  user_position_MIN;
	int  user_position_MAX;
	float  user_flex_MIN;
	float  user_flex_MAX;
	int position_CommandArr[N_ACT];
	void calibration();
	
	void runtime(void (*mapping)(int));
	void directActuatorControl(int n);
	void sweep_uS(int t_d, int n);

	//void miniPilot_patternsSequence(int t_d);
	void miniPilot_patternsCommand();
	void miniPilot_patternsCommandbyLetter();
	void miniPilot_sweep(int t_d);
	void miniPilot_sweepKeyboard();

	void testLed();
	void testPushbutton();
	void blink_T(int t_d);
	void safety();
	int sweep(int t_d, int n);
	void blinkN(int n, int t_d);

	private:
	

	// variables

	// Settings
	INPUT_TYPE inputType;
	bool serialON;
	bool sdWriteON;
	int WRITE_COUNT = 8;
	int T_CYCLE = 15; // minimum delay to ensure not sampling at too high a rate for sensors

	// Array version for multiple actuators
	Servo actuatorArr[N_ACT];
	int position_MeasuredArr[N_ACT]; // this could be local
	short zeroForceArr[N_ACT]; // should this be local?

	const  int position_INArr[4] = {21, 20, 22, 23};
	const  int position_OUTArr[4] = {7, 6, 8, 9};
	const  byte I2C_ADDRArr[4] = {0x06, 0x08, 0x10, 0x12};
	const int  button_IN = 4;
	const int  led_OUT = 5;	
	
	int  cycleCount; // cycleCount
	bool  powerOn; // powerOn
	const int CHIP_SELECT = 10;

	// flex sensor and angle
	ADS  capacitiveFlexSensor;
	float  flexSensor; // could be local(?)

	// Pushbutton & LED
	int  buttonState; // button state
	int  oldButtonState; // old button state
	int  buttonCount; // button count. global!
	

	// methods
	
	void initializeSystem(bool c);
	bool initializeSerial();
	bool initializeSDCard();
	bool initializeActuator();
	bool initializeFlexSensor();
	bool initializeIO();
	bool risingEdgeButton();
	void writeOutData(int l, unsigned long t, float f, int *c, int *m, short *d);
	short readDataFromSensor(short address);

	void calibrationActuatorFeedback(int n);
	void calibrationZeroForce();
	void calibrationFlexSensor(unsigned long timeLength);
	int calibrationMaxDeepPressure(int n);

};
#endif