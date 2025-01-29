// DeepPressureWearableArr.h
// Written by: Sreela Kodali, kodali@stanford.edu

#ifndef DeepPressureWearableArr_h
#define DeepPressureWearableArr_h

# define N_ACT 2
# define T_SAMPLING 1000000

#include "Arduino.h"
// #else
// #include "WProgram.h"
// #endif

// #include <MightyZap.h>
#include "SparkFun_Displacement_Sensor_Arduino_Library.h"
#include "skFilter.h"
#include "skFilter_terminate.h"
#include <SD.h>
#include <SPI.h>

#include <Servo.h>
#include <Wire.h>
#include "IntervalTimerEx.h"

// Notes: haven't added SD card stuff yet, single actuator

typedef enum {
	FLEX_MIN = 0,
	FLEX_MAX = 180
} FLEX_SENSOR_LIMITS;

// actuonix command limits
typedef enum {
	POSITION_MIN = 47, // 900us mightyZap
	POSITION_MAX = 139 // 2100us mightyZap
} ACTUATOR_LIMITS;

// // MightyZap actuator limits
// typedef enum {
// 	POSITION_MIN = 0, 
// 	POSITION_MAX = 4095
// } ACTUATOR_LIMITS;


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
    int  buttonCount; // button count. global!
    const  int position_INArr[4] = {21, 20, 22, 23}; // analog adc pins
    volatile short forceData[N_ACT];
    volatile int position_MeasuredArrGlobal[N_ACT];
	volatile bool writeOut;

	//MightyZap* m_zap;

    void beginTimer();
	void calibration();
	void runtime(void (*mapping)(int));
	void directActuatorControl(int n);
	short readDataFromSensor(short address);
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
	bool risingEdgeButton();

	void writeActuator(int idx, int pos);
	int readFeedback(int idx);


	private:
	

	// variables

	// Settings
	INPUT_TYPE inputType;
	bool serialON;
	bool sdWriteON;
	const  byte I2C_ADDRArr[4] = {0x06, 0x08, 0x0A, 0x0C};

	// FIX: don't forget to change these
	const bool actuatorType = 0; // NEW. 0 = actuonix and 1 = MightyZap. CHANGE THIS for new actuator!
	const int mightyZapWen_OUT = 12; // FIX THIS: temporary write enable output signal for buffer
	Servo actuatorArr[N_ACT]; // Array version for multiple actuators
	
	
	int WRITE_COUNT = 8;
	int T_CYCLE = 15; // minimum delay to ensure not sampling at too high a rate for sensors
	short zeroForceArr[N_ACT]; // should this be local?

	IntervalTimerEx ForceSampleSerialWriteTimer;
	//IntervalTimerEx t2;
	
	const  int position_OUTArr[4] = {7, 6, 8, 9}; // pwm output
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
	

	// methods
	
	void initializeSystem(bool c);
	bool initializeSerial();
	void initializeSDCard();
	bool initializeActuator();
	bool initializeFlexSensor();
	bool initializeIO();
	static void ISR(void* obj);
	void writeOutData(int l, unsigned long t, float f, int *c, int *m, short *d);
	

	void calibrationActuatorFeedback(int n);
	void calibrationZeroForce();
	void calibrationFlexSensor(unsigned long timeLength);
	int calibrationMaxDeepPressure(int n);

};
#endif