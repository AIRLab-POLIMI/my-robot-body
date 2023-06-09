// uncomment this for dev mode (this is a trick to save precious memory space)
// #define DEVMODE 1

#include <Arduino.h>
#include <Servo.h>
#include <NewSonar.h>
#include <RGBLed.h>
#include <LedMatrix.h>
#include <LedMatrix_constants.h>
#include <Thermosensor.h>
#include <TriskarBase.h>



// NOTES
// - "println" adds "\n" at the end, so you don't need to add it manually
// - all varialbles declared in the outmost scope are "global"


// ______________________________________________________________________________________________ GLOBALS


#define EMPTY_STRING " "
#define DELIMITER ":"
#define MSG_DELIMITER "_"
#define ARDUINO_OK_FLAG "OK"
#define REQUEST_ARDUINO_RESET "RESET"
#define ARDUINO_READY_FLAG "READY"
#define RASP_DEFAULT "A"

// --- RECEIVED MESSAGES KEYS
// TRISKAR BASE
#define BASE_FORWARD_KEY "BF"
#define BASE_STRIFE_KEY "BS"
#define BASE_ANGULAR_KEY "BB"
// SERVO ARM
#define SERVO_PETALS_KEY "SP"
// LGB LED
#define RGB_LED_KEY "L"
// EYE MATRIX
#define EYE_MATRIX_CENTER_X_KEY "EX"
#define EYE_MATRIX_CENTER_Y_KEY "EY"
// TERMO SENSOR
#define TERMO_SENSOR_KEY "T"


// SETUP
#define MAX_READ_MSGS 5  // max number of messages read per iteration


// FLAGS
bool dirty;

String current_data = EMPTY_STRING;

struct KeyValueMsg {
  String key;
  String value;  
};

KeyValueMsg current_msg = {"", ""}; // global variable with the current received message

// WATCHDOG
unsigned long last_command_time;
unsigned long current_time;


// ______________________________________________________________________________________________ MOTORS
//
// LEGEND
// - MR (Motor Right)
// - ML (Motor Left)
// - MB (Motor Back)

// Maximum speed wanted. It's defined by raspberry and sent to Arduino on setup
float _MAX_SPEED = 80;  //cm/s
float _MAX_ANGULAR = 6.23;  //rad/s

// driver pins
// NB since the NEWPING LIBRARY uses a TIMER connected to PWM PINS 9 and 10, 
//    any motor connected to those PINs will not work: avoid PINs 9 and 10
#define _MR_A 3
#define _MR_B 5
#define _ML_A 7
#define _ML_B 4
#define _MB_A 8
#define _MB_B 6
TriskarBaseMid triskarBase;  // MEDIUM TRISKAR BASE (Siid; Blackwing; ..) 

// #define _MR_A 7
// #define _MR_B 8
// #define _ML_A 5
// #define _ML_B 6
// #define _MB_A 3
// #define _MB_B 4
// TriskarBaseBig triskarBase;  // BIG TRISKAR BASE (Sonoma) 

// ______________________________________________________________________________________________ SERVOS


#define SERVO_PETALS_PIN 44

// servos are 180 DEG servos. 
// POSITIONS: The center position is 90. Leftmost is 0. Rightmost is 180.

#define SERVO_CENTER 90
#define SERVO_LEFT 0
#define SERVO_RIGHT 180

// #define PETAL_ANGLE_CLOSED 108  // servo angle corresponding to petals FULLY CLOSED (arm HIGHEST) - LONG ARMS CASE
// #define PETAL_ANGLE_CLOSED 98  // servo angle corresponding to petals FULLY CLOSED (arm HIGHEST)
#define PETAL_ANGLE_CLOSED 94  
// #define PETAL_ANGLE_OPENED 125  // servo angle corresponding to petals FULLY OPEN (arm LOWEST)
#define PETAL_ANGLE_OPENED 131  // servo angle corresponding to petals FULLY OPEN (arm LOWEST)

// #define PETAL_ANGLE_INIT 113 // (long arms version)
#define PETAL_ANGLE_INIT 103  // servo angle at the beginning. 
                              // It's almost fully closed but not completely, to have a lower current consumption
                              // and allow the system to start 
// #define PETAL_ANGLE_INIT 103 

#define SERVO_m (float)(PETAL_ANGLE_OPENED - PETAL_ANGLE_CLOSED)


// SERVO ARM POS
float petals_pos;

Servo servo_petals;


void reset_all_servos() {

  // by default petals are closed
  petals_pos = PETAL_ANGLE_INIT;
  servo_petals.write(PETAL_ANGLE_INIT);

  // Serial.println("[reset_all_servos] ---- COMPLETE");
}

void set_servo_pos(float pos, Servo servo) {

  // pos arrives in range [-1, 1] - rescale it in [PETAL_ANGLE_OPENED, PETAL_ANGLE_CLOSED]
  pos = (((pos + 1.0f) / 2.0f) * SERVO_m)  + (float)PETAL_ANGLE_CLOSED;

  // Make sure the position is within the limit.
  // it could not be the case if the incoming normalized setpoint was not in [-1, 1]
  if (pos < PETAL_ANGLE_CLOSED) {
    pos = PETAL_ANGLE_CLOSED;
  } else if (pos > PETAL_ANGLE_OPENED) {
    pos = PETAL_ANGLE_OPENED;
  }

  // Set the pos
  servo.write(pos);

  // write_serial("[SET SERVO POS] - target pos: " + String(servo_target_pos) + " - SPEED: " + String(speed));
}

void set_servos_pos() {

  set_servo_pos(petals_pos, servo_petals);
}

void initialize_servos(){
  // attatch pins
  servo_petals.attach(SERVO_PETALS_PIN);

  // brief delay
  delay(200);

  // reset all SERVO positions and AWAIT for them to be reset
  reset_all_servos();
}

void onPetalsValueRcv(float newVal) {
  // pos arrives in range [-1, 1]. Saturate it to those bounds.
  petals_pos = newVal;
  if (petals_pos > 1)
    petals_pos = 1;
  if (petals_pos < -1)
    petals_pos = -1;
}


// -------------------------------------------------------------------------------------- RGB LED

// PINS for the three color channels of the RGB led
const uint8_t GREEN = 11 ;  
const uint8_t BLU = 12;  
const uint8_t RED = 13;  
    
RGBLed led(RED, GREEN, BLU);

// -------------------------------------------------------------------------------------- LED MATRIX

// we define the pin that our NeoPixel panel is plugged into
uint8_t LED_MATRIX_PIN = 52;
int NUM_PIXELS = 8;
uint64_t BRIGHTNESS = 2; // 5;
uint64_t BLINK_EYE_TIME_OUT_MIN = 150;  // the amount of time from a blink and another
uint64_t BLINK_EYE_TIME_OUT_MAX = 250;  // the amount of time from a blink and another
// define constants 
int DELAY_EYE = 30; // in ms

LedMatrix eye(LED_MATRIX_PIN, NUM_PIXELS, BRIGHTNESS, BLINK_EYE_TIME_OUT_MIN, BLINK_EYE_TIME_OUT_MAX);


// ______________________________________________________________________________________________ UTILS


void write_key_value_serial(char * key, float val) {
  Serial.print(key);
  Serial.print(':');
  Serial.println(val);
}

void write_key_value_serial(char * key, unsigned long val) {
  Serial.print(key);
  Serial.print(':');
  Serial.println(val);
}


// ______________________________________________________________________________________________ SERIAL

bool read_serial() {
  
  if (Serial.available() > 0) {
    current_data = Serial.readStringUntil('\n');
    return true;    
  }
  else
    return false;
}

void write_serial(String msg) {
    Serial.println(msg);
}

void message_response(KeyValueMsg keyValueMsg){

  // act based on a message - discriminate using the key

  String key = keyValueMsg.key;
  String value = keyValueMsg.value;

  if(key == BASE_FORWARD_KEY) {
    triskarBase.setForwardTarget(value.toFloat());
    dirty = true;
    last_command_time = millis();
    }
  else if (key == BASE_STRIFE_KEY) {
    triskarBase.setStrafeTarget(value.toFloat());
    dirty = true;
    last_command_time = millis();
    }
  else if (key == BASE_ANGULAR_KEY) {
    triskarBase.setAngularTarget(value.toFloat());
    dirty = true;
    last_command_time = millis();
    }
  else if (key == SERVO_PETALS_KEY) {
    onPetalsValueRcv(value.toFloat());
    dirty = true;
    // Serial.print(" - - PETALS VAL RCV: ");
    // Serial.println(value);
    last_command_time = millis();
    }
  else if (key == RGB_LED_KEY) {
    led.onValueRcv(value.toInt());
    last_command_time = millis();
    }
  else if (key == EYE_MATRIX_CENTER_X_KEY) {
    eye.onPosXRcv(value.toInt());
    last_command_time = millis();
    }
  else if (key == EYE_MATRIX_CENTER_Y_KEY) {
    eye.onPosYRcv(value.toInt());
    last_command_time = millis();
    }
  else {
    write_serial("[ERROR] unsupported message key: " + key);
  }
}

void get_key_value(String msg) {
  // 1 find the position of the delimiter
  // 2 get the first and second substrings: the KEY and VALUE of the message

  int delim_index = msg.indexOf(DELIMITER);

  String key = msg.substring(0, delim_index);
  String value = msg.substring(delim_index + 1);

  // current_msg.key = key;
  // current_msg.value = value;

  KeyValueMsg tempKeyValueMsg = KeyValueMsg();
  tempKeyValueMsg.key = key;
  tempKeyValueMsg.value = value;

  message_response(tempKeyValueMsg);
}


void split_msg(String msg) {

  // split all the messages with the MSG DELIMITER
  // for each, split it into KEY-VALUE pairs and send the corresponding RESPONSE
  // at each point in time we need two indexes: the STARTING msg index, and the TERMINATING msg index
  // - the START index is either 0 or the index of the first available MSG_DELIMITER
  // - the END index is either the index of a MSG_DELIMITER or the end of the message
  // CASES
  // - only one messages: there are no delimiters
  //          * start index = 0
  //          * end index = string len
  // - more than one message: there is at least one deimiter
  //          * start index = 0 for first one; last delimiter index + 1 for all the rest
  //          * end index = string len for the last one, 

  // do nothing if it's the DEFAULT EMPTY MSG
  if (current_data == RASP_DEFAULT)
    return;

  int startDelimiterIndex = -1;
  int endDelimiterIndex = msg.indexOf(MSG_DELIMITER);

  if (endDelimiterIndex == -1) {
    // CASE 1: no delimiters, only one message
    get_key_value(msg);
    return;
  }

  bool firstRound = true;

  while (true) {

    if (firstRound) {
      startDelimiterIndex = -1;
      endDelimiterIndex = msg.indexOf(MSG_DELIMITER);
      firstRound = false;

    } else {
      startDelimiterIndex = endDelimiterIndex;
      endDelimiterIndex = msg.indexOf(MSG_DELIMITER, startDelimiterIndex + 1);
    }
    
    if (endDelimiterIndex == -1) {
      // it's the last message
      get_key_value(msg.substring(startDelimiterIndex + 1));
      break;
    }  

    get_key_value(msg.substring(startDelimiterIndex + 1, endDelimiterIndex));
  }
}


bool read_key_value_serial(){

    if (read_serial()){
      split_msg(current_data);
      return true;
    }
    else {
      return false;  
    }
}

// you can only use this with the NON-BLOCKING raspberry "send_serial" method
// otherwise you'll get rubbish
void resend_data() { 
  if (Serial.available() > 0) {
    String current_data = Serial.readStringUntil('\n');
    write_serial("you sent me: " + current_data);
  }
}

void print_msg_feedback() {
  write_serial("[MESSAGE RECEIVED] - key: " + current_msg.key + " - value: " + current_msg.value + " -> current speeds:: F:" + String(triskarBase.getForwardTarget()) + " S:" + String(triskarBase.getStrafeTarget()) + " A:" + String(triskarBase.getAngularTarget()) + " :: - max longitudinal speed: " + String(_MAX_SPEED) + " :: - max angular speed: " + String(_MAX_ANGULAR));  
}


// ______________________________________________________________________________________________ WATCHDOG

// if after 'MAX_WATCHDOG_ELAPSED_TIME' time in milliseconds there is no input signal from serial, 
// assume central control has died, and stop everything
#define MAX_WATCHDOG_ELAPSED_TIME 7000


void reset_all_target_speeds() {
  // - set all speeds to 0
  // - set the speeds ('set_wheel_speeds' is normally only triggered when new message arrives)
  // - update also 'last_command_time' so that the watchdog_tick method does not keep triggering 
  // (but only will every 'MAX_WATCHDOG_ELAPSED_TIME' ms if nothing happens)
  
  // WHEEL BASE
  triskarBase.setSpeedTargets(0, 0, 0);
  triskarBase.setWheelSpeeds();

  last_command_time = millis();
}

void watchdog_tick() {
  current_time = millis();
  
  if (current_time - last_command_time > MAX_WATCHDOG_ELAPSED_TIME){
    reset_all_target_speeds();
  }
}


// -------------------------------------------------------------------------------------- MAIN

int prevcTime = 0;
int maxtimee = 2000;
bool canWrite = false;
unsigned int lastWriteTime = 0;

void initialize() {

  // watchdog times
  last_command_time = millis();
  current_time = millis();

  // motor speed targets
  petals_pos= SERVO_CENTER;

  // initialization flags
  dirty = false;
}

void final_setup() {
  write_serial("[SETUP] internal setup complete");
}

void setup() {

  // 1 start serial and await a bit for warmup;
  Serial.begin(500000);
  delay(200);

  // 2 initialize variables
  initialize();

  // 3.1 setup wheel base
  triskarBase.setup(_MR_A, _MR_B, _ML_A, _ML_B, _MB_A, _MB_B);

  // 3.2 setup servos
  initialize_servos();

  // 4 setup RGB LED  
  led.setup();

  // 5 setup LED MATRIX
  eye.setup();

  // wrapping up
  final_setup();
  canWrite = false;
  lastWriteTime = millis();

  write_serial(ARDUINO_OK_FLAG);
  
  Serial.setTimeout(5000);

  prevcTime = millis();

  // TEST

  led.writeColor(0, 0, 255);
  delay(50);

}  
  
// how long we wait for a message to arrive before we write anyway
unsigned int maxWriteElapsed = 5000;
unsigned int now;
unsigned int diff;

unsigned int serial_elapsed;
unsigned int min_serial_elapsed = 5;
unsigned int last_serial_time = millis();

bool isMsg = false;


void serial_loop() {

  // a serial loop can only be performed once every SERIAL_ELAPSED time
  now = millis();

  serial_elapsed = now - last_serial_time;
  if (serial_elapsed < min_serial_elapsed)
    return;

  // read everything it can
  while (read_key_value_serial()){
    isMsg = true;
  }

  // if at least a msg was received, use the updated values
  if (isMsg) {
    triskarBase.setWheelSpeeds();

    set_servos_pos();

    led.loop();
    eye.loop();

    canWrite = true;

    last_serial_time = millis();

    isMsg = false;
  }
}


void speeds_loop() {

  int a = 1;

  triskarBase.setSpeedTargets(a, 0, 0);
  Serial.println("FORWARD 1");
  triskarBase.setWheelSpeeds();
  delay(2000);

  triskarBase.setSpeedTargets(0, a, 0);
  Serial.println("STRAFE 1");
  triskarBase.setWheelSpeeds();
  delay(2000);

  triskarBase.setSpeedTargets(0, 0, a);
  Serial.println("ANGULAR 1");
  triskarBase.setWheelSpeeds();
  delay(2000);

  triskarBase.setSpeedTargets(-a, 0, 0);
  Serial.println("FORWARD -1");
  triskarBase.setWheelSpeeds();
  delay(2000);

  triskarBase.setSpeedTargets(0, -a, 0);
  Serial.println("STRAFE -1");
  triskarBase.setWheelSpeeds();
  delay(2000);

  triskarBase.setSpeedTargets(0, 0, -a);
  Serial.println("ANGULAR -1");
  triskarBase.setWheelSpeeds();
  delay(2000);
}


void loop() {  

  // watchdog_tick();

  // // the EYE needs a dedicated step for the BLINK, to be executed at every timestep
  // eye.blinkTick();

  // serial_loop();

  speeds_loop();
} 
