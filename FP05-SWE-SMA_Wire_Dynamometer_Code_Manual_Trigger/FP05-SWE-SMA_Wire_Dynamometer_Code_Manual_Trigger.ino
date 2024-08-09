#include <ESP32TimerInterrupt.h>
#include <ESP32TimerInterrupt.hpp>
#define ACTIVE_DATA_INTERVAL_US 1000 // Amount of time in microseconds between data points taken when the MOSFET is on
#define PASSIVE_DATA_INTERVAL_US 500000 // Amount of time in microseconds between data points taken when the MOSFET is off

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
// int time2[1000];   //DEBUG CODE
// int A_read[1000];  //DEBUG CODE
// int B_read[1000];  //DEBUG CODE
int LC_read[1000];
int SMA_read[1000];
int Shunt_read[1000];
int MOS_trig_arr[1000];
int ctr = 0; // Counter for printing data to Serial monitor
String Print_str; // String for printing data
String Serialinput; // String for reading inputs from user
int prev_time = 0; // Variable for noting when the last data point was taken
bool MOS_triggered = false; // For keeping track of when the MOSFET is triggered

void readAnalogs() {
  // Put values for encoder position, Load Cell reading, SMA reading, and Shunt resistor reading into arrays.
  enc_pos_arr[pulse_count] = enc_pos;
  // A_read[pulse_count] = Asig;    // DEBUG CODE
  // B_read[pulse_count] = Bsig;    // DEBUG CODE
  LC_read[pulse_count] = analogRead(LC_pin);
  SMA_read[pulse_count] = analogRead(SMA_pin);
  Shunt_read[pulse_count] = analogRead(Shunt_pin);
  MOS_trig_arr[pulse_count] = MOS_triggered;
}

bool IRAM_ATTR TimerCallbackFunction(void* timerNum) { // Timer interrupt function
  if (pulse_count == 0) { // Notes the time that the previous data point was taken at
    prev_time = time1[999];
  } else {
    prev_time = time1[pulse_count - 1];
  }
  time1[pulse_count] = micros();  // Get the current time
  if ((((time1[pulse_count] - prev_time) >= PASSIVE_DATA_INTERVAL_US) && MOS_triggered == false) || (MOS_triggered == true && ((time1[pulse_count] - prev_time) >= (ACTIVE_DATA_INTERVAL_US - 1)))) {
    // IF The MOSFET is off and the amount of time since the last data point was collected is longer than the specified amount of time
    // OR IF the MOSFET is on and the amount of time since the last data point was collected is longer than the specified amount of time
    readAnalogs(); // take and store data
    // time2[pulse_count] = micros();   // DEBUG CODE
    pulse_count++;  // update the number of data points collected
    if (pulse_count >= 1000) { // Only 1000 data points are stored in the ESP32's memory. 
      pulse_count -= 1000;
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
    // A_read[pulse_count] = Asig;    // DEBUG CODE
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
    readAnalogs(); // Collect data, keep track of what the A signal is and update the number of data points collected
    // time2[pulse_count] = micros();
    Aprev = Asig;
    pulse_count++;
    if (pulse_count >= 1000) {
      pulse_count -= 1000;
    }
  }
}

void IRAM_ATTR Bchange() { // Basically the same as the Encoder A pin interrupt
  time1[pulse_count] = micros();
  Bsig = digitalRead(B_pin);
  if (Bsig != Bprev) {
    // B_read[pulse_count] = Bsig;
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
    readAnalogs();
    // time2[pulse_count] = micros();
    Bprev = Bsig;
    pulse_count++;
    if (pulse_count >= 1000) {
      pulse_count -= 1000;
    }
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
  while (!Serial)
    Serial.println("Starting");


  Serialinput = Serial.readString();
  Serial.println("Input something into the serial monitor to start");
  // Wait to finish setup until the user is ready. Allows the user to double check before everything is initialized.
  while (Serialinput == "") {
    Serialinput = Serial.readString();
    delay(100);
  }
  // Set up interrupts for Encoder A and B pins and the timer
  attachInterrupt(A_pin, Achange, CHANGE);
  attachInterrupt(B_pin, Bchange, CHANGE);
  Timer.attachInterruptInterval(ACTIVE_DATA_INTERVAL_US, TimerCallbackFunction);
  // Getting initial readings
  Asig = digitalRead(A_pin);
  Bsig = digitalRead(B_pin);
  time1[pulse_count] = micros();
  readAnalogs();
  // time2[pulse_count] = micros();   // DEBUG CODE
  pulse_count++;
  Aprev = Asig;
  Bprev = Bsig;
  // Finish setup, give user instructions
  Serial.println("End of setup");
  Serial.println("\nInput the number of ms that you want the MOSFET to be triggered for");
}

void loop() {
  Serialinput = Serial.readString();
  if (Serialinput != "") {
    // check if the user has input anything into the serial monitor.
    time_on = Serialinput.toInt();
    Serialinput = "";
    Serial.print("You have input ");
    Serial.print(time_on);
    Serial.println("ms. Hit enter in the serial monitor again to continue.");
    // if the user has input something into the serial monitor check with the user that it's correct. Gives the user time to shut things down if they input the wrong number.
    while (Serialinput == "") {
      Serialinput = Serial.readString();
    }
    // Let the user know that it's working, turn the MOSFET on for the right amount of time, and then let the user know that they can input another number and do it again.
    Serial.println("MOSFET Triggered");
    digitalWrite(MOS_Trig_pin, HIGH);
    MOS_triggered = true;
    delay(time_on);
    digitalWrite(MOS_Trig_pin, LOW);
    MOS_triggered = false;
    Serial.println("MOSFET Untriggered");
    Serial.println("\nInput the number of ms that you want the MOSFET to be triggered for");
  }

  while (ctr != pulse_count) {  // Print data to serial monitor
  // WHILE the counter for the number of data points is not equal to the counter for the printing of the data 
    // (there is data that has been collected but not yet included in the printing)

    // Each new line is a single data point (data collected at one time)
    // Each measurement/reading is separated by a tab character to make things easier to read
    // I found that putting everything into one string is faster than printing everything as it comes

    Print_str.concat(time1[ctr]); // Add the time and encoder position to the string
    Print_str.concat("\t");
    Print_str.concat(enc_pos_arr[ctr]);
    Print_str.concat("\t");
    // Print_str.concat(A_read[ctr]);
    // Print_str.concat("\t");
    // Print_str.concat(B_read[ctr]);
    // Print_str.concat("\t");
    Print_str.concat(LC_read[ctr]);
    Print_str.concat("\t");
Print_str.concat(Shunt_read[ctr]);
    Print_str.concat("\t");
 // Add tehe force, current, and voltage readings to thes  string    Print_str.concat(SMA_read[ctr]);
    Print_str.concat("\t");
    // Print_str.concat(time2[ctr]);
    // Print_str.concat("\t");
    // Print_str.concat(ctr

    // DEBUG CODE);
    Print_str.concat("\t");
    Print_str.concat(MOS_trig_arr[ctr]);
    Print_str.concat("\n");
        ctr++;
    if (ctr >= 1000) {
  // Add whether or not the MOSFET is triggered      ctr -= 1000;
    }

    // Increment the counter for the printing string  }
  Serial.print(Print_str);
  Print_st =0
  delay(1000);
}
  Pr// Prionnt the printing string and reset it  // Wait 1 seoccond before starting the printing process again/.