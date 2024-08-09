#include <ESP32TimerInterrupt.h>
#include <ESP32TimerInterrupt.hpp>
#define ACTIVE_DATA_INTERVAL_US 1000 // Amount of time in microseconds between data points taken when the MOSFET is on
#define PASSIVE_DATA_INTERVAL_US 500000 // Amount of time in microseconds between data points taken when the MOSFET is off
#define TIME_BETWEEN_PULSES_SECONDS 20 // Amount of time in seconds between when the MOSFET is initially triggered and the next time it is triggered
#define MAX_POWER_TIME_MS 350 // The maximum amount of time in milliseconds that the MOSFET can be on for before the program shuts off as a failsafe
#define FULL_CONTRACTION_POSITION 50 // The encoder position where the MOSFET will be turned off

// Initialize pin numbers
int A_pin = 9;
int B_pin = 11;
int LC_pin = 18;
int SMA_pin = 17;
int Shunt_pin = 14;
int MOS_Trig_pin = 12;
// Initialize variables for reading encoder data
int Asig;
int Bsig;
int Aprev;
int Bprev;
int enc_pos = 0;
int pulse_count = 0; // Variable for keeping track of number of data points collected
// Arrays for storing data
int enc_pos_arr[1000];
int time1[1000];
int LC_read[1000];
int SMA_read[1000];
int Shunt_read[1000];
int MOS_trig_arr[1000];
int ctr = 0; // Counter for printing data to Serial monitor
String Print_str; // String for printing data
String Serialinput; // String for reading inputs from user
int prev_time = 0; // Variable for noting when the last data point was taken
bool MOS_triggered = false; // For keeping track of when the MOSFET is triggered
// converting from seconds/ms to microseconds
int time_btwn_pulses_us = TIME_BETWEEN_PULSES_SECONDS * 1000000;
int max_power_time_us = MAX_POWER_TIME_MS * 1000;
// Variables for shutdown protocol
bool stop = false;
int num_left = 0;

void readAnalogs() { // stores data for the encoder position, force, current, voltage, and if the MOSFET is triggered
  enc_pos_arr[pulse_count] = enc_pos;
  LC_read[pulse_count] = analogRead(LC_pin);
  SMA_read[pulse_count] = analogRead(SMA_pin);
  Shunt_read[pulse_count] = analogRead(Shunt_pin);
  MOS_trig_arr[pulse_count] = MOS_triggered;
}

void increment_pulse_count() { // increments the pulse count variable, for keeping track of the number of data points collected
  pulse_count++;
  if (pulse_count >= 1000) { // Only 1000 data points are stored in the ESP32's memory.
    pulse_count = 0;
  }
}

bool IRAM_ATTR TimerCallbackFunction(void* timerNum) { // Timer interrupt function
  if (stop == false) { // Makes sure that nothing runs if the program has been shut down
    if (pulse_count == 0) { // Notes the time that the previous data point was taken at
      prev_time = time1[999];
    } else {
      prev_time = time1[pulse_count - 1];
    }
    time1[pulse_count] = micros(); // note the current time
    if (MOS_triggered == false && (time1[pulse_count] % time_btwn_pulses_us) <= ACTIVE_DATA_INTERVAL_US) {
      // IF the MOSFET is not triggered and it is supposed to be triggered:

      // check if supposed to be triggered using a modulo function. 
      // If the current time mod the time between pulses is less than the amount of time between timer interrupts, then it is supposed to be triggered.
      readAnalogs(); // take and store data
      increment_pulse_count(); // update the number of data points collected
      MOS_triggered = true; // turn on the MOSFET
      digitalWrite(MOS_Trig_pin, HIGH);
      time1[pulse_count] = micros(); // Note the time so another data point can be collected within the interrupt function
    }
    if (MOS_triggered == true && (time1[pulse_count] % time_btwn_pulses_us) >= max_power_time_us) {
      // IF the MOSFET is on and the amount of time it has been on is longer than the max amount of time the MOSFET is supposed to be on for:

      MOS_triggered = false; // Turn off the MOSFET
      digitalWrite(MOS_Trig_pin, LOW);
      detachInterrupt(A_pin); // Detach the interrupts from the Encoder A and B pins
      detachInterrupt(B_pin);
      readAnalogs(); // Collect data and update the number of data points collected
      increment_pulse_count(); 
      num_left = 3; // Start program shutdown
    }

    if ((((time1[pulse_count] - prev_time) >= PASSIVE_DATA_INTERVAL_US) && MOS_triggered == false) || (MOS_triggered == true && ((time1[pulse_count] - prev_time) >= (ACTIVE_DATA_INTERVAL_US - 1)))) {
      // IF The MOSFET is off and the amount of time since the last data point was collected is longer than the specified amount of time
      // OR IF the MOSFET is on and the amount of time since the last data point was collected is longer than the specified amount of time
      
      readAnalogs(); // Collect data and indicate that a data point has been collected
      increment_pulse_count();
      if (num_left != 0) {
        // If the shutdown protocol has been triggered, takes a few more data points before shutting down completely
        num_left--;
        if (num_left == 1) {
          stop = true; // Stop collecting data
          // Tell the user that the program is shut down
          Serial.println("\nSomething went wrong. Power was being delivered to the SMA for too long. Shutting down the program.");
        }
      }
    }
  }
  return true;
}

ESP32Timer Timer(0);

void IRAM_ATTR Achange() { // Interrupt function for the Encoder A signal
  time1[pulse_count] = micros(); // Note the time
  Asig = digitalRead(A_pin); // Note whether the signal is high or low
  if (Asig != Aprev) { 
    // IF the signal is actually different from the previous one, sometimes the interrupt gets triggered when there is no change in the signal
    if (Asig == 0) { // Increase or decrease the encoder's position depending on the signal change
      if (Bsig == 0) {
        enc_pos--;
      } else if (Bsig == 1) {
        enc_pos++;
      }
    } else if (Asig == 1) {
      if (Bsig == 0) {
        enc_pos++;
      } else if (Bsig == 1) {
        enc_pos--;
      }
    }
    if (enc_pos >= FULL_CONTRACTION_POSITION && MOS_triggered == true) {
      // IF the MOSFET is triggered and the encoder has reached the location to turn off the MOSFET: 
      MOS_triggered = false; // Turn off the MOSFET
      digitalWrite(MOS_Trig_pin, LOW);
    }
    readAnalogs(); // Collect data, keep track of what the A signal is and update the number of data points collected.
    // time2[pulse_count] = micros();
    Aprev = Asig;
    increment_pulse_count();
  }
}

void IRAM_ATTR Bchange() { // Essentially the same as the Encoder A pin interrupt
  time1[pulse_count] = micros();
  Bsig = digitalRead(B_pin);
  if (Bsig != Bprev) {
    if (Bsig == 0) {
      if (Asig == 0) {
        enc_pos++;
      } else if (Asig == 1) {
        enc_pos--;
      }
    } else if (Bsig == 1) {
      if (Asig == 0) {
        enc_pos--;
      } else if (Asig == 1) {
        enc_pos++;
      }
    }
    if (enc_pos >= FULL_CONTRACTION_POSITION && MOS_triggered == true) {
      MOS_triggered = false;
      digitalWrite(MOS_Trig_pin, LOW);
    }
    readAnalogs();
    Bprev = Bsig;
    increment_pulse_count();
  }
}

void setup() {
  // Initialize the pin modes first
  pinMode(A_pin, INPUT_PULLUP);
  pinMode(B_pin, INPUT_PULLUP);
  pinMode(LC_pin, INPUT);
  pinMode(SMA_pin, INPUT);
  pinMode(Shunt_pin, INPUT);
  pinMode(MOS_Trig_pin, OUTPUT);
  digitalWrite(MOS_Trig_pin, LOW); // Make sure that at the start the MOSFET is not triggered
  // Initialize Serial
  Serial.begin(115200);
  while (!Serial) {
    Serial.println("Starting");
  }

  Serialinput = Serial.readString();
  Serial.println("\nInput something into the serial monitor to start");
  // Wait to finish setup until the user is ready. Allows the user to double check before everything is initialized.
  while (Serialinput == "") {
    Serialinput = Serial.readString();
    delay(100);
  }
  // Getting initial readings
  Asig = digitalRead(A_pin);
  Bsig = digitalRead(B_pin);
  time1[pulse_count] = micros();
  readAnalogs();
  pulse_count = 1;
  Aprev = Asig;
  Bprev = Bsig;
  time1[pulse_count] = micros();
  // Set up interrupts for Encoder A and Encoder B
  attachInterrupt(A_pin, Achange, CHANGE);
  attachInterrupt(B_pin, Bchange, CHANGE);
  
  Serialinput = "";
  while (Serialinput == "") {
    // Let the user know when the next time the MOSFET will turn on. 
    // Gives the user time to wait until there is enough time before the next trigger so that they can check to make sure the readings are okay
    Serialinput = Serial.readString();
    Serial.print("The current time is ");
    Serial.print(micros());
    Serial.println(" microseconds");
    Serial.print("Power to the wire will next be triggered at ");
    Serial.print((int(micros()/time_btwn_pulses_us)+1)*time_btwn_pulses_us);
    Serial.println(" microseconds");
    Serial.println("Input something into the serial monitor when you are ready to start");
    delay(1000);
  }
  // Start the timer interrupt
  Timer.attachInterruptInterval(ACTIVE_DATA_INTERVAL_US, TimerCallbackFunction);
  // Let the user know how often the MOSFET will be triggered
  Serial.print("End of setup. Power will be provided to the wire every ");
  Serial.print(TIME_BETWEEN_PULSES_SECONDS);
  Serial.println(" seconds.");
}

void loop() { // All that happens in the loop is that data is printed to the Serial monitor
  while (ctr != pulse_count) {
    // WHILE the counter for the number of data points is not equal to the counter for the printing of the data 
    // (there is data that has been collected but not yet included in the printing)

    // Each new line is a single data point (data collected at one time)
    // Each measurement/reading is separated by a tab character to make things easier to read both visually and when it is time for data analysis
    // I found that putting everything into one string is faster than printing everything as it comes
    
    Print_str.concat(time1[ctr]); // Append to the string: the time and the encoder position
    Print_str.concat("\t");
    Print_str.concat(enc_pos_arr[ctr]);
    Print_str.concat("\t");
    Print_str.concat(LC_read[ctr]); // Add the force, current, and voltage readings to the string
    Print_str.concat("\t");
    Print_str.concat(Shunt_read[ctr]);
    Print_str.concat("\t");
    Print_str.concat(SMA_read[ctr]);
    Print_str.concat("\t");
    Print_str.concat(MOS_trig_arr[ctr]); // Add whether or not the MOSFET is triggered
    Print_str.concat("\n");
    // Increment the counter for the printing string
    ctr++;
    if (ctr >= 1000) {
      ctr -= 1000;
    }
  }
  Serial.print(Print_str); // Print the printing string and reset it
  Print_str = "";
  delay(1000); // Wait 1 second before starting the printing process again.
}
