int MOS_Trigger_Pin = 12;
String Serialinput;
int time_on;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
    Serial.println("Starting");
  pinMode(MOS_Trigger_Pin, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:

  // MOSFET Test Code
  Serialinput = Serial.readString();
  if (Serialinput != "") {
    time_on = Serialinput.toInt();
    Serialinput = "";
    Serial.print("You have input ");
    Serial.print(time_on);
    Serial.println("ms. Hit enter in the serial monitor again to continue.");
    while (Serialinput == "") {
      Serialinput = Serial.readString();
    }
    // Serial.println(time_on);
    Serial.println("MOSFET Triggered");
    digitalWrite(MOS_Trigger_Pin, HIGH);
    delay(time_on);
    digitalWrite(MOS_Trigger_Pin, LOW);
    Serial.println("MOSFET Untriggered");
    Serial.println("\nInput the number of ms that you want the MOSFET to be triggered for");
  }
  delay(100);
}
