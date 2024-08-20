// DeepPressureWearable.cpp
// Written by: Sreela Kodali, kodali@stanford.edu

#include "Arduino.h"
#include "pins_arduino.h"
#include "DeepPressureWearableArr.h"


//Constructor
DeepPressureWearableArr::DeepPressureWearableArr(INPUT_TYPE input, bool serial, bool c) {

	// settings
	inputType = input;
	serialON = serial;
  sdWriteON = !(serialON); // if outputting data via serial, no need to write to SD card

  //N_ACTUATORS = n;

	// DEFAULT Calibration settings
	user_position_MIN = 60;//sPOSITION_MIN;
	user_position_MAX = 110;//POSITION_MAX;
	user_flex_MIN = FLEX_MIN;
	user_flex_MAX = FLEX_MAX;

  int i;
  for (i=0; i < N_ACTUATORS; ++i) {
    position_CommandArr[i] = user_position_MIN;
    zeroForceArr[i] = 0;
    forceData[i] = 0;
  }

  //if (!(actuatorType)) Servo actuatorArr[N_ACT];
  if (actuatorType) {
    MightyZap m_zapObj(&Serial4, mightyZapWen_OUT);
	  m_zap = &m_zapObj;
  }

  writeOut = false;
	cycleCount = 0; // cycleCount
	powerOn = 0; // powerOn

	// flex sensor and angle
	ADS  capacitiveFlexSensor;
	flexSensor = 0.0;

	// Pushbutton & LED
	buttonState = 0; // button state
	oldButtonState = 0; // old button state
	buttonCount = 0; // button count

	initializeSystem(c);
}

static void DeepPressureWearableArr::ISR(void* obj) {
    DeepPressureWearableArr* THIS = (DeepPressureWearableArr*)obj;

    for (int i=0; i < THIS->N_ACTUATORS; ++i) {
      // Serial.print(THIS->readDataFromSensor(THIS->I2C_ADDRArr[i]));
      // Serial.print(" ");
      THIS->forceData[i] = THIS->readDataFromSensor(THIS->I2C_ADDRArr[i]);
    }
    // Serial.println();
    // Serial.print(millis());
    // Serial.print(",");
    // Serial.print(analogRead(THIS->position_INArr[0]));
    // Serial.print(",");
    // Serial.println(THIS->forceData[0]);
    // myTime = millis();
    //data[n] = forceData[n];
        // position_Measured = analogRead(THIS->position_INArr[n]);        
        // if (position_Measured < minValue) minValue = position_Measured;    
    // if (serialON) Serial.println((String(millis()) + "," + String(counter) + "," + String(position_Measured) + "," + THIS->forceData[n]));
    THIS->writeOut = true;
    //Serial.println(THIS->forceData[0]);
  }


void DeepPressureWearableArr::beginTimer() {
    ForceSampleSerialWriteTimer.begin(ISR, this, T_SAMPLING);
}


// Public methods
void DeepPressureWearableArr::testLed () {
  //digitalWrite(led_OUT, HIGH);   // turn the LED on (HIGH is the voltage level)
  analogWrite(led_OUT, 200);
  delay(1000);               // wait for a second
  //digitalWrite(led_OUT, LOW);    // turn the LED off by making the voltage LOW
  analogWrite(led_OUT, 10);
  delay(1000);               // wait for a second
}

void DeepPressureWearableArr::testPushbutton () {
  digitalWrite(led_OUT, LOW);
  if (risingEdgeButton()) {
    if (serialON) Serial.println("Pushed!");
    digitalWrite(led_OUT, HIGH);
    delay(50);
    digitalWrite(led_OUT, LOW);
  }
}

void DeepPressureWearableArr::blink_T (int t_d) {
  digitalWrite(led_OUT, LOW);
  delay(t_d);
  digitalWrite(led_OUT, HIGH);
  delay(t_d);
}

void DeepPressureWearableArr::writeActuator(int idx, int pos) {
  if (actuatorType) { // if mightyZap

    (*m_zap).GoalPosition(idx+1,pos);
  }

  else { // if actuonix
    actuatorArr[idx].write(pos);
  }
}



void DeepPressureWearableArr::safety() {
      // Safety withdraw actuator and stop
    if (risingEdgeButton()) {

      int i;

      for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);

      if (serialON) Serial.println("Device off. Turn off power.");
      if (sdWriteON) {
        File dataFile = SD.open("raw_data.csv", FILE_WRITE);
        if (dataFile) {
            //dataFile.print(buf);
            dataFile.close();
        }
      }
      while (1);
    }
}

void DeepPressureWearableArr::runtime(void (*mapping)(int)) {
    short data[N_ACTUATORS];
    int position_MeasuredArr[N_ACTUATORS];
    unsigned long myTime;
    int i;
    bool localWriteOut;

    myTime = millis(); // time for beginning of the loop

    // read flex sensor and map angle to actuator command. keep values in the bounds
    if (inputType == FLEX_INPUT) {
      if (capacitiveFlexSensor.available() == true) flexSensor = abs(capacitiveFlexSensor.getX());
    }
    else if (inputType == KEYBOARD_INPUT) {
      if (Serial.available() > 0) flexSensor = (Serial.read() - '0')*100 + (Serial.read() - '0')*10 + (Serial.read() - '0')*1 ;
    }
    mapping(int(flexSensor));
    //position1_Command = map(int(flexSensor), int(user_flex_MIN), int(user_flex_MAX), user_position_MIN, user_position_MAX);
    
    for (i=0; i < N_ACTUATORS; ++i) {
        if(position_CommandArr[i] > user_position_MAX) position_CommandArr[i] = user_position_MAX;
        else if(position_CommandArr[i] < user_position_MIN) position_CommandArr[i] = user_position_MIN;
     }

    // Send command to actuator and measure actuator position

    if (!(buttonCount % 2)) {
      for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, position_CommandArr[i]);
    }
    else {
      for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);
      //actuator1.write(POSITION_MIN);
    }

    for (i=0; i < N_ACTUATORS; ++i) position_MeasuredArr[i] = analogRead(position_INArr[i]); // could put this in the interrupt too

    noInterrupts();
    localWriteOut = writeOut;
    // read force data and write out data at lower frequency. writeOut is activated by IntervalTimer, interrupt based
    
    if (localWriteOut) {
      for (i=0; i < N_ACTUATORS; ++i) data[i] = forceData[i];
      writeOutData(N_ACTUATORS, myTime, flexSensor, position_CommandArr, position_MeasuredArr, data);
      writeOut = false;
    }
    interrupts(); 

    risingEdgeButton();
    if (T_CYCLE > 0) delay(T_CYCLE);    
}

// n is how many actuators to control at the same time
void DeepPressureWearableArr::directActuatorControl(int n) {
    short data[N_ACTUATORS];
    int position_MeasuredArr[N_ACTUATORS];
    unsigned long myTime;
    int i;
    bool localWriteOut;

    myTime = millis(); // time for beginning of the loop

    //position_CommandArr[0] = 900;
    // parse serial input for command value
    if (inputType == KEYBOARD_INPUT) {
      if (Serial.available() > 0) {
        position_CommandArr[0] = Serial.parseInt();
        Serial.parseInt();
        //Serial.println(position_CommandArr[0]);
        //bound the command
        if(position_CommandArr[0] > user_position_MAX) position_CommandArr[0] = user_position_MAX;
        else if(position_CommandArr[0] < user_position_MIN) position_CommandArr[0] = user_position_MIN;

        // pass the command to other actuators
        if (n > 1) {
            for (i=1; i < n; ++i) position_CommandArr[i] = position_CommandArr[0];
        }
      }
    }

    // Send command to actuator and measure actuator position
    if (!(buttonCount % 2)) {
       for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, position_CommandArr[i]);
    } else {
       for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);
    }
    for (i=0; i < n; ++i) position_MeasuredArr[i] = analogRead(position_INArr[i]);

    // read force data and write out data at lower frequency

    noInterrupts();
    localWriteOut = writeOut;
    // read force data and write out data at lower frequency. writeOut is activated by IntervalTimer, interrupt based
    
    if (localWriteOut) {
      for (i=0; i < N_ACTUATORS; ++i) data[i] = forceData[i];
      writeOutData(N_ACTUATORS, myTime, flexSensor, position_CommandArr, position_MeasuredArr, data);
      writeOut = false;
    }
    interrupts(); 

  //   cycleCount = cycleCount + 1;
  // //if ((cycleCount == WRITE_COUNT)) {
  //   if ((cycleCount == 1)) { // formerly was 10
  //     for (i=0; i < 2; ++i) data[i] = readDataFromSensor(I2C_ADDRArr[i]);
  //     // powerOn = (data >= 150);
  //     // if (powerOn) analogWrite(led_OUT, 255);
  //     // else analogWrite(led_OUT, 30);

  //     writeOutData(2, myTime, flexSensor, position_CommandArr, position_MeasuredArr, data);
  //     cycleCount = 0;
  //   }
    risingEdgeButton();
    if (T_CYCLE > 0) delay(T_CYCLE);    
}

// sweeping actuator position, increasing and decreasing. infinite loop.
// t_d is time between actuator steps. n is which actuator
int DeepPressureWearableArr::sweep(int t_d, int n) {
    unsigned long myTime;
    short data;
    bool retracting = false;
    int position_Measured;
    int counter = user_position_MIN-1;
    int inc = 1;
    int minValue = 1000;
    bool localWriteOut;
    unsigned long startTime = millis();
    String dataString;

    while(1) {
      myTime = millis();

      // update counter
      if (int(myTime - startTime) > t_d) {
        counter = counter + inc;
        
        //bound the command
        if(counter > user_position_MAX) counter = user_position_MAX;
        else if(counter < user_position_MIN) counter = user_position_MIN;
        //Serial.println(counter);

        writeActuator(n, counter);
        //Serial.println(counter);
        startTime = millis();
      }


      noInterrupts();
      localWriteOut = writeOut;
      // read force data and write out data. writeOut is activated by IntervalTimer, interrupt based
    
      if (localWriteOut) {
        dataString = "";
        data = forceData[n];
        position_Measured = analogRead(position_INArr[n]);        
        if (position_Measured < minValue) minValue = position_Measured;
        dataString += (String(myTime) + "," + String(counter) + "," + String(position_Measured) + "," + data);
        if (serialON) Serial.println(dataString);
        //writeOutData(N_ACTUATORS, myTime, flexSensor, position_CommandArr, position_MeasuredArr, data);
        writeOut = false;
      }
      interrupts(); 

      if (counter >= user_position_MAX) {
        inc = -1 * inc;
        retracting = true;
      }
      else if (counter <= (user_position_MIN) && retracting) break;

    }
    
    
    // while (counter <= user_position_MAX) {
    //   String dataString = "";

    //   actuatorArr[n].write(counter);
    //   Serial.println(counter);
      // position_Measured = analogRead(position_INArr[n]);
      // myTime = millis();
      // if (position_Measured < minValue) minValue = position_Measured;
      //data[n] = readDataFromSensor(I2C_ADDRArr[n]);

      // if (serialON) {
      //   // writeOutData(1, myTime, 0, counter, position_MeasuredArr[n], data);
      //   dataString += (String(myTime) + "," + String(counter) + "," \
      //   + String(position_Measured) + "," + data);
      //   Serial.println(dataString);
      // }



    //   if (extending == 1) {
    //     if (counter == (user_position_MAX)) {
    //       counter = counter - 1;
    //       extending = 0;
    //     } else counter = counter + 1;
    //   }
    //   else if (extending == 0) {
    //     if (counter == user_position_MIN) {
    //       counter = counter + 1;
    //       extending = 1;
    //       break; // comment if I want infinite sweeping
    //     } else counter = counter - 1;
    //   }
    //   delay(t_d);
    // }
    return minValue;    
}

void DeepPressureWearableArr::miniPilot_patternsCommand() {
      int w;
      int n_valid = 0;
      int i;
      int input[N_ACTUATORS];
      int patterns[3] = {user_position_MIN, user_position_MIN + (user_position_MAX-user_position_MIN)/2, user_position_MAX};

      //for (i=0; i < N_ACTUATORS; ++i) actuatorArr[i].write(POSITION_MIN);


      if (Serial.available() > 0) {

        // read input
        for (i=0; i <= N_ACTUATORS; ++i) {
          //Serial.println(i);
          w = (Serial.read() - '0');
          //Serial.println(w);

          if ((i == N_ACTUATORS) and (w < 0)) break; // right input length

          else if ((w < 0 or w > 2)) {
            n_valid = 0;
            Serial.println("Error: invalid input. Try again.");
            break;
          }
          else {
            input[i] = w;
            n_valid = n_valid + 1;
          }
        }

        // if input is valid, print combo and move actuators
        if (n_valid == N_ACTUATORS) {
          //Serial.println("valid input!");
          for (i=0; i < N_ACTUATORS; ++i) {
            Serial.print(input[i]);
            Serial.print(" ");
            writeActuator(i, patterns[input[i]]);
          }
          Serial.println();
        }

      }
}

// 8-2-24 : FYI this hasn't been updated to the interrupt based data output
// Note: this is a fixed mapping for two tactor and 9 combos A-I
void DeepPressureWearableArr::miniPilot_patternsCommandbyLetter() {
      int x;
      int y;
      int z;
      int i;
      int patterns[3] = {user_position_MIN, user_position_MIN + (user_position_MAX-user_position_MIN)/2, user_position_MAX};
      unsigned long myTime;
      short data[N_ACTUATORS];
      int position_MeasuredArr[N_ACTUATORS];

      myTime = millis(); // time for beginning of the loop

      if (N_ACTUATORS != 2) {
         Serial.println("Incorrect number of actuators.");
         while(1);
      }

      // if command sent, move actuators
      if (Serial.available() > 0) {
        x = (Serial.read());
        (Serial.read());
        
        // If less than A or greater than I
        if ((x < 65) or (x > 73)) {
          Serial.println("Error: invalid input. Try again.");
        }
        else {
          Serial.println((char) x);
          z = (x < 68) + (x >= 68 and x < 71)*2 + (x >= 71 and x <74)*3 - 1;
          y = (x+1) % 3;

          position_CommandArr[0] = patterns[z];
          position_CommandArr[1] = patterns[y];

          //temp
          if (position_CommandArr[0] == user_position_MAX) position_CommandArr[0] = position_CommandArr[0] +10;
          if (position_CommandArr[1] == user_position_MAX) position_CommandArr[1] = position_CommandArr[1] -20;


          if (!(buttonCount % 2)) {
              for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, position_CommandArr[i]);
          } else {
              for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);
          }

          // actuatorArr[0].write(patterns[z]);
          // actuatorArr[1].write(patterns[y]);
          // delay(2000); // new 
          // actuatorArr[0].write(user_position_MIN);
          // actuatorArr[1].write(user_position_MIN);
        }
      }
      for (i=0; i < N_ACTUATORS; ++i) position_MeasuredArr[i] = analogRead(position_INArr[i]);
      cycleCount = cycleCount + 1;
      // Serial.println(WRITE_COUNT);
      if ((cycleCount == WRITE_COUNT)) {
        // Serial.println(myTime);
        for (i=0; i < N_ACTUATORS; ++i) data[i] = readDataFromSensor(I2C_ADDRArr[i]);
        // powerOn = (data >= 150);
        // if (powerOn) analogWrite(led_OUT, 255);
        // else analogWrite(led_OUT, 30);
        writeOutData(N_ACTUATORS, myTime, 0, position_CommandArr, position_MeasuredArr, data);
        cycleCount = 0;
      }
      // risingEdgeButton();
      if (T_CYCLE > 0) delay(T_CYCLE);
}

// sweeping actuator position, increasing and decreasing. infinite loop.
// t_d is time between actuator steps. goes through all the actuators
void DeepPressureWearableArr::miniPilot_sweep(int t_d) {
    unsigned long myTime;
    short data[N_ACTUATORS];
    int position_MeasuredArr[N_ACTUATORS];
    int counter = user_position_MIN; // user_position_MIN + (user_position_MAX-user_position_MIN)/2
    int i;
    
    for (i=0; i < N_ACTUATORS; ++i) {
      writeActuator(i, counter);
      position_MeasuredArr[i] = 0;
      data[i] = 0;
    }
  
    for (i=0; i < N_ACTUATORS; ++i) {

      while (counter <= user_position_MAX) {
        writeActuator(i, counter);
        position_MeasuredArr[i] = analogRead(position_INArr[i]);
        myTime = millis();
        data[i] = readDataFromSensor(I2C_ADDRArr[i]);

        if (serialON) {
          //writeOutData(1, myTime, 0, counter, position_MeasuredArr[i], data[i]);
        }
        counter = counter + 1;
        delay(t_d);
      }
      counter = user_position_MIN; // user_position_MIN + (user_position_MAX-user_position_MIN)/2
      writeActuator(i, counter);
    }
}

// using the servo writeMicroseconds() command intentionally
void DeepPressureWearableArr::sweep_uS(int t_d, int n) {
    unsigned long myTime;
    short data;
    int counter = 900;
    int position_Measured;
    
    actuatorArr[n].writeMicroseconds(counter);
    position_Measured = 0;
    data = 0;
  

    while (counter <= 2100) {
        String dataString = "";
        actuatorArr[n].writeMicroseconds(counter);
        position_Measured = analogRead(position_INArr[n]);
        myTime = millis();
        data = readDataFromSensor(I2C_ADDRArr[n]);
        
        if (serialON) {
          dataString += (String(myTime) + "," + String(counter) + "," + String(position_Measured) + "," + String(data));        
            Serial.println(dataString);
        }
        counter = counter + 1;
        delay(t_d);
    }
      counter = 2100; // user_position_MIN + (user_position_MAX-user_position_MIN)/2
      actuatorArr[n].writeMicroseconds(counter);
}


// sweeping across a wider range of values. as increasing value, switching to different actuators
void DeepPressureWearableArr::miniPilot_sweepKeyboard() {
    //unsigned long myTime;
    //int position_MeasuredArr[N_ACTUATORS];
    //short data;
    int x;
    int i;
    int counter = user_position_MIN; // user_position_MIN + (user_position_MAX-user_position_MIN)/2
    int max = user_position_MIN + (user_position_MAX-user_position_MIN + 1)*(N_ACTUATORS-1) + (user_position_MAX-user_position_MIN);
    int bound1[N_ACTUATORS];
    int bound2[N_ACTUATORS];

    for (i=0; i < N_ACTUATORS; ++i) {
      writeActuator(i, counter);
      //position_MeasuredArr[i] = 0;
      //data[i] = 0;
      bound1[i] = user_position_MIN + (user_position_MAX-user_position_MIN + 1)*(i);
      bound2[i] = bound1[i] + (user_position_MAX-user_position_MIN);
    }

    while (1) {

      if (Serial.available() > 0) {
        x = (Serial.read());
         //Serial.println(x);

      if ( (x < 52) or (x > 53)) {
        Serial.println("Error: invalid input. Try again.");
      }

      else if (x == 52) {
        counter = counter - 1;
        Serial.println(counter);
      }
      else if (x == 53) {
        counter = counter + 1;
        Serial.println(counter);
      }

      if(counter < user_position_MIN) counter = user_position_MIN;
      else if(counter > max) counter = max;

      for (i=0; i < N_ACTUATORS; ++i) {
        if ((counter >= bound1[i]) and (counter <= bound2[i])) {
          writeActuator(i, counter - ((user_position_MAX-user_position_MIN + 1)*(i)));
        } else {
          writeActuator(i, user_position_MIN);
        }

       }

      }      
    }
}

// Calibration
void DeepPressureWearableArr::calibrationActuatorFeedback(int n) {
  int i;
  int ACTUATOR_FEEDBACK_MAX = 500;
  int ACTUATOR_FEEDBACK_MIN = 500;

  // Calibration: Sweep and record the actuator position feedback positions
  for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);

  
  delay(500);
  ACTUATOR_FEEDBACK_MAX = analogRead(position_INArr[n]);
  ACTUATOR_FEEDBACK_MIN = sweep(300, n);
  Serial.println(ACTUATOR_FEEDBACK_MAX);
  Serial.println(ACTUATOR_FEEDBACK_MIN);
}

void DeepPressureWearableArr::calibrationZeroForce() {
  int i;
  for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);

  // Calibration Stage: Get the zero force of the device
  for (i=0; i < N_ACTUATORS; ++i) {
    zeroForceArr[i] = readDataFromSensor(I2C_ADDRArr[i]);
    Serial.println(( zeroForceArr[i] - 255) * (45.0)/512);
  }
}

void DeepPressureWearableArr::calibrationFlexSensor(unsigned long timeLength) {
  unsigned long startTime;
  unsigned long endTime;
  int i;
  //unsigned long timeLength = 50000;

  for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);

  // use the initial value as baseline. otherwise user_flex is set at 0 180 default
   if (capacitiveFlexSensor.available() == true) user_flex_MIN = capacitiveFlexSensor.getX();
   user_flex_MAX = user_flex_MIN;
   Serial.println(user_flex_MIN);
   delay(2*T_CYCLE);
 
  // record flex values for x seconds
  startTime = millis();
  endTime = millis();

  while((endTime - startTime) < timeLength) {
    if (capacitiveFlexSensor.available() == true) flexSensor = capacitiveFlexSensor.getX();
    if (flexSensor < user_flex_MIN) user_flex_MIN = flexSensor;
    else if (flexSensor > user_flex_MAX) user_flex_MAX = flexSensor;
    Serial.println(flexSensor);
    endTime = millis();
    delay(2*T_CYCLE);
  }

  startTime = millis();
  endTime = millis();
  while((endTime - startTime) < timeLength) {
    if (capacitiveFlexSensor.available() == true) flexSensor = capacitiveFlexSensor.getX();
    Serial.println(flexSensor);
    endTime = millis();
    delay(2*T_CYCLE);
  }
//  Serial.println(user_flex_MIN);
//  Serial.println(user_flex_MAX);
}


// Calibration Stage: Get the detection and pain thresholds
int DeepPressureWearableArr::calibrationMaxDeepPressure(int n) {
   int counter = POSITION_MIN;
   int x;
   short data;
   int position_Measured;
   String dataString;
   short maxForce = 0;
   short minForce = 0; // detection threshold
   int nClicks = 0;
   int i;

   for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, counter);
   blinkN(5,1000);
   for (i=0; i < N_ACTUATORS; ++i) zeroForceArr[i] = readDataFromSensor(I2C_ADDRArr[i]);

    while (counter <= POSITION_MAX) {
       // Measure force and actuator position
      dataString = "";
      data = readDataFromSensor(I2C_ADDRArr[n]);
      position_Measured = analogRead(position_INArr[n]);
      // Send command to actuator
      writeActuator(n, counter);
      dataString += (String(counter) + "," + String(position_Measured) + "," + String((data - 255) * (45.0)/512));
      Serial.println(dataString);
        
      //if (serialON) Serial.println((data - 255) * (45.0)/512);
      if (Serial.available() > 0) x = (Serial.read() - '0');
//      Serial.println(x);
      if (risingEdgeButton() || (x == 5)) {
        nClicks = nClicks + 1;
        x = 0;
      }
      if (nClicks == 1) {
          //Serial.println("min detected!");
          user_position_MIN = counter;
          delay(3000);
          minForce = readDataFromSensor(I2C_ADDRArr[n]);
          nClicks = nClicks + 1;
      }
      if (nClicks == 3) { // NOTE: nClicks = 3 is deliebrate. rising edge of click
          user_position_MAX = counter - 2; // NOTE: the -2 is arbitrary
          maxForce = readDataFromSensor(I2C_ADDRArr[n]);
          delay(3000);
          writeActuator(n, POSITION_MIN);
          break; 
      }
      delay(250);
      counter = counter + 1;
  }

  for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);

  Serial.println(zeroForceArr[n]);
  Serial.println(minForce); // new
  Serial.println(maxForce);
  Serial.println(user_position_MIN); // new
  Serial.println(user_position_MAX);

  return user_position_MAX;
}

/* Calibration: Interfaces with python gui */
void DeepPressureWearableArr::calibration() {
  CALIBRATION_OPTIONS mode;
  bool calibrationComplete = false;
  int nMaxPressure = 0;
  int sum = 0;
  int sum1 = 0;
  int lenUserArr = 10; // User can do max of 10 attempts for deep pressure calibration
  int user_position_MAX_arr[lenUserArr];
  int user_position_MIN_arr[lenUserArr];
  int d1 = 0;
  int d2 = 0;
  int d3 = 0;
  int i;

  // initialize blank array
  for (int j = 0; j< lenUserArr; j++) {
     user_position_MAX_arr[j] = 0;
     user_position_MIN_arr[j] = 0;
  }
          
  // Reset position of actuator
  for (i=0; i < N_ACTUATORS; ++i) writeActuator(i, POSITION_MIN);

  while(!(calibrationComplete)){
    if (Serial.available() > 0) {
      mode = CALIBRATION_OPTIONS(Serial.read() - '0');
      switch (mode) {

        // THIS ISN'T ACTUALLY ZERO_FORCE. If experiment got interrupted and I don't want to redo
        // calibration, and i know the calibration values, I can pass in min + max directly.
        // FIX: should rename to LOAD_MIN_MAX 
        case ZERO_FORCE: // test for passing min AND max
          while (!(Serial.available() > 0));
          user_position_MIN = (Serial.read() - '0')*10 + (Serial.read() - '0')*1;
          d1 = (Serial.read() - '0');
          d2 = (Serial.read() - '0');
          user_position_MAX = d1*10 + d2*1;
          if (d1 == 1) {
            d3 = (Serial.read() - '0');
            user_position_MAX = user_position_MAX * 10 + d3*1;
          }

          // I don't necessarily need this, but in case I get interrupted mid-calibration
          // and I want to load in previous values and continue calibration.
          user_position_MAX_arr[nMaxPressure] = user_position_MAX;
          user_position_MIN_arr[nMaxPressure] = user_position_MIN;
          nMaxPressure = nMaxPressure + 1;

          Serial.println(user_position_MIN);
          Serial.println(user_position_MAX);
          break;
        case FLEX:
          calibrationFlexSensor(50000);
          break;
        case MAX_PRESSURE:
          user_position_MAX_arr[nMaxPressure] = calibrationMaxDeepPressure(0); // FIX
          user_position_MIN_arr[nMaxPressure] = user_position_MIN;
          nMaxPressure = nMaxPressure + 1;
          sum = 0;
          sum1 = 0;
          for (int i = 0; i< nMaxPressure; i++) {
            sum = sum + user_position_MAX_arr[i];
            sum1 = sum1 + user_position_MIN_arr[i];
          }
          user_position_MAX = sum/nMaxPressure;
          user_position_MIN = sum1/nMaxPressure;
          Serial.println(user_position_MIN);
          Serial.println(user_position_MAX);
          break;
        case ACTUATOR:
          calibrationActuatorFeedback(0); // FIX
          break;
        case NONE:
          calibrationComplete = true;
        default:
          break;
      }
      delay(50); 
    }
  }
}


// Private methods

void DeepPressureWearableArr::blinkN (int n, int t_d) {
  //noInterrupts();
  for(int i=0; i < n; i++) {
    analogWrite(led_OUT, 10);
    delay(t_d/2);
    analogWrite(led_OUT, 200);
    delay(t_d/2);
  }
  //interrupts();
}

void DeepPressureWearableArr::initializeSystem(bool c) {
  Wire.begin(); // join i2c bus
  initializeSerial(); // start serial for output
  initializeSDCard(); // initialize sd card
  initializeActuator(); // initialize actuator and set in min position
  
  if (inputType == FLEX_INPUT) initializeFlexSensor(); // initialize flex sensor
  else {
    //WRITE_COUNT = 30; // why did I set the default for 30....so slow
    flexSensor = 180;
  }
  initializeIO(); // initialize IO pins, i.e. button and led
  analogWrite(led_OUT, 10);
  if (c) {
    Serial.println("Entering calibration...");
    calibration();
  }
}

bool DeepPressureWearableArr::initializeSerial() {
    Serial.begin(4608000);  
    Serial.flush();
    while (!Serial);
    return (true); 
    Serial.println("serialIn");
}

void DeepPressureWearableArr::initializeSDCard() {
  if (sdWriteON) {
    // See if the card is present and can be initialized:
    if (!SD.begin(CHIP_SELECT)) {
      Serial.println("Card failed, or not present");
      while (1);
    }
    Serial.println("card initialized.");
    // fix test code
    //    SD.remove("raw_data.csv");
    //    Serial.println("Removed old data");
  }
}

bool DeepPressureWearableArr::initializeActuator() {
  int i;

  for (i=0; i < N_ACTUATORS; ++i) {
    if (!(actuatorType)) actuatorArr[i].attach(position_OUTArr[i]); // attach servo 
    writeActuator(i, POSITION_MIN);
  }
  return (true);
}

bool DeepPressureWearableArr::initializeFlexSensor() {
    if (capacitiveFlexSensor.begin() == false) {
      Serial.println(("No sensor detected. Check wiring. Freezing..."));
      while (1);
  }
  capacitiveFlexSensor.enableStretching(true);
  return (true);
}

bool DeepPressureWearableArr::initializeIO() {
  pinMode(button_IN, INPUT); // set button and led
  pinMode(led_OUT, OUTPUT);
  return (true); 
}

bool DeepPressureWearableArr::risingEdgeButton() {
  buttonState = digitalRead(button_IN);
  //Serial.println(buttonState);
  if (buttonState != oldButtonState) {
    if (buttonState == HIGH) {
      buttonCount = buttonCount + 1;
      oldButtonState = buttonState;
      delay(300);
      return true;
    }
  }
  oldButtonState = buttonState; 
  return false;
}

void DeepPressureWearableArr::writeOutData(int l, unsigned long t, float f, int *c, int *m, short *d) {
  int i;
  String dataString = "";
  if (sdWriteON || serialON) {
    dataString += (String(t) + "," + String(f));

    for (i=0; i < l; ++i) {
      dataString += ( "," + String(c[i]) + "," + String(m[i]) + "," + String(d[i]));
    }
      
    if (sdWriteON) {
      File dataFile = SD.open("raw_data.csv", FILE_WRITE);
      if (dataFile) {
        dataFile.println(dataString);
        dataFile.close();
      } else if (serialON) Serial.println("error opening datalog");
    }
    if (serialON) Serial.println(dataString);
  }  
}

short DeepPressureWearableArr::readDataFromSensor(short address) {
  byte i2cPacketLength = 6;//i2c packet length. Just need 6 bytes from each peripheral
  byte outgoingI2CBuffer[3];//outgoing array buffer
  byte incomingI2CBuffer[6];//incoming array buffer
  bool debug;

  debug = false;

  outgoingI2CBuffer[0] = 0x01;//I2c read command
  outgoingI2CBuffer[1] = 128;//peripheral data offset
  outgoingI2CBuffer[2] = i2cPacketLength;//require 6 bytes

  if (debug) Serial.println("Transmit address");  
  Wire.beginTransmission(address); // transmit to device 
  Wire.write(outgoingI2CBuffer, 3);// send out command
  if (debug) Serial.println("Check sensor status");
  byte error = Wire.endTransmission(); // stop transmitting and check peripheral status
  if (debug) Serial.println("bloop");
  if (error != 0) return -1; //if peripheral not exists or has error, return -1
  Wire.requestFrom((uint8_t)address, i2cPacketLength);//require 6 bytes from peripheral
  if (debug) Serial.println("Request bytes from sensor");
  
  byte incomeCount = 0;
  while (incomeCount < i2cPacketLength)    // peripheral may send less than requested
  {
    if (Wire.available())
    {
      incomingI2CBuffer[incomeCount] = Wire.read(); // receive a byte as character
      incomeCount++;
      if (debug) Serial.println("Read byte from sensor");
    }
    else
    {
      delayMicroseconds(10); //Wait 10us 
      if (debug) Serial.println("Waiting from sensor");
    }
  }

  short rawData = (incomingI2CBuffer[4] << 8) + incomingI2CBuffer[5]; //get the raw data

  return rawData;
}


