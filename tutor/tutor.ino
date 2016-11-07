// === Game tutorial machine - LEGEND TO PLAY
// --- Usage - Keypad actions ------------------------------------------------------
// press * and then press A..D to choose game legend set
// press # and then press 0..9 to set sound volume level
// press repeatedly 0..9 to enter up to 3-digit number folowed by # to choose legend
// ---------------------------------------------------------------------------------

// libraries
#include <Keypad.h>              //(included with Arduino IDE)
#include <Timer.h>               //http://playground.arduino.cc/Code/Timer
#include <SoftwareSerial.h>      //http://arduino.cc/en/Reference/SoftwareSerial (included with Arduino IDE)
#include <DFPlayer_Mini_Mp3.h>   //http://github.com/DFRobot/DFPlayer-Mini-mp3

#define debug_mode false

// =========================== HW peripherals ========================

// --- LED - Integrated LED diode
#define STATUS_LED LED_BUILTIN

// --- Keypad 4x4
// Keypad rows and columns
const byte rows = 4;
const byte columns = 4;
// Keypad char map
char keys[rows][columns] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// Keypad digital pins
uint8_t RowPins[rows] = {2, 3, 4, 5};
uint8_t ColumnPins[columns] =  {6, 7, 8, 9};

// Keypad object
Keypad keyboard = Keypad( makeKeymap(keys), RowPins, ColumnPins, rows, columns);

// --- RTC - real time clock module DS3231
// RTC external object from DS3232RTC

// --- MP3 - DFPlayer mini mp3 module
SoftwareSerial mp3Serial(10, 11); // RX, TX
#define DEFAULT_SOUND_VOLUME 20   // 0..30

// =========================== SW modules ========================
// --- Action timer
Timer t;

// --- State machines
// (SM: STATE: 1-Start,2-SelectMode,3-SelectSound,4-SetVolume TRANSITION: 1-2,2-1,1-3,3-3,3-1,1-4,4-1)
#define SM_START		1
#define SM_SELECTMODE	2
#define SM_SELECTSOUND	3 
#define SM_SETVOLUME	4

// (IDLE: STATE: 1-Start,2-Waiting,3-Nervous,4-Forgotten TRANSITION: 1-2,2-1,2-3,3-1,3-4,4-1)
#define IDLE_START		1
#define IDLE_WAITING	2
#define IDLE_NERVOUS	3
#define IDLE_FORGOTTEN	4

struct StateMachine{
  uint8_t state;
  uint8_t last_state;
} sm, idlesm;

// --- Core
#define MAX_KEYPAD_BUFFER_LENGTH 3

String keypadbuffer = ""; // input string buffer to read up to 3-digit numbers
uint16_t glsb = 1000;     // game legend set base - mp3 file index base - A-1000, B-2000, C-3000, D-4000

// =================================== SETUP ====================
void setup() {
  // Serial communication 38400 baud
  Serial.begin(38400);
  Serial.println("------ Speaking game tutorial, Firmware version 0.1");

  Serial.println(">> Status LED...");
  // Pin 13 has an LED connected on most Arduino boards:
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
  Serial.println(">> Status LED Initialized");

  Serial.println(">> MP3 Player...");
  // MP3 serial communication setup
    mp3Serial.begin(9600);
    mp3_set_serial (mp3Serial); //set Serial for DFPlayer-mini mp3 module
    delay(1);  //wait 1ms for mp3 module to set volume
    mp3_set_volume (DEFAULT_SOUND_VOLUME);
  Serial.println(">> MP3 Player Initialized");

  Serial.println(">> Core...");
    sm.state = SM_START;
    idlesm.state = IDLE_START;
  Serial.println(">> Core Initialized");

  digitalWrite(STATUS_LED, LOW);
}


// =================================== LOOP ====================

void loop() {

  // mandatory timer call
  t.update();

  // read pressed key from keypad
  char _key = keyboard.getKey();

  // pressed key processing
  if (_key){
    if (debug_mode){
      Serial.print("Pressed key: ");
      Serial.println(_key);
      Serial.print(sm.state);
      Serial.print("->");
    }

  // restart idle timers 
  _restartidletimer();

  // Main event-based decision routine
  switch (sm.state){
    case 1: // Initial
      if (CNs1t2(_key)) { sm.last_state = SM_START;  TNs1t2(); }  // *
      else if (CNs1t4(_key)) { sm.last_state = SM_START; TNs1t4(); }  // #
      else if (CNs1t3(_key)) { sm.last_state = SM_START; TNs1t3(_key); }  // 0..9
      break;
    case 2: // reading game legend set base
      if (CNs2t1(_key)) { sm.last_state = 2; TNs2t1(_key); } // A..D
      else { sm.last_state = 2; _clear(); } // * # 0..9
      break;
    case 3: // reading sound index
      if (CNs3t3(_key)) { sm.last_state = 3; TNs3t3(_key); } // 0..9
      else if (CNs3t1(_key)) { sm.last_state = 3; TNs3t1(); } // #
      else { sm.last_state = 3; _clear(); } // * A..D
      break;
    case 4: // reading volume 
      if (CNs4t1(_key)) { sm.last_state = 4;  TNs4t1(_key); }  // 0..9
      else { sm.last_state = 4; _clear(); } // * # A..D
      break;
    }

    if (debug_mode){ 
      Serial.println(sm.state);
    }
  }
}


bool CNs1t2 (char c) {
  return c == '*';
}

bool CNs1t3 (char c) {
  return (c >= '0') && (c <= '9');
}

bool CNs1t4 (char c) {
  return c == '#';
}

bool CNs2t1 (char c) {
  return (c >= 'A') && (c <= 'D');
}

bool CNs3t1 (char c) {
  return c == '#';
}

bool CNs3t3 (char c) {
  return (c >= '0') && (c <= '9');
}

bool CNs4t1 (char c) {
  return (c >= '0') && (c <= '9');
}

void TNs1t2 () {
  sm.state = SM_SELECTMODE;
  digitalWrite(STATUS_LED, HIGH);
}

void TNs1t3 (char c) {
  keypadbuffer += c;
  sm.state = SM_SELECTSOUND;
  digitalWrite(STATUS_LED, HIGH);
}

void TNs1t4 () {
  sm.state = SM_SETVOLUME;
  digitalWrite(STATUS_LED, HIGH);
}

void TNs2t1 (char c) {
  switch (c) {
    case 'A': { glsb = 1000; mp3_play(glsb);}
    break;
    case 'B': { glsb = 2000; mp3_play(glsb);}
    break;
    case 'C': { glsb = 3000; mp3_play(glsb);}
    break;
    case 'D': { glsb = 4000; mp3_play(glsb);}
    break;
  }

  _clear();
}

void TNs3t1 () {
  // play sound
  if (debug_mode){ 
    Serial.print(glsb + keypadbuffer.toInt());
    Serial.print(":");
  }

  mp3_play(glsb + keypadbuffer.toInt());
  _clear();
}

void TNs3t3 (char c) {
  if (keypadbuffer.length() < MAX_KEYPAD_BUFFER_LENGTH) keypadbuffer += c;
}

void TNs4t1 (char c) {
  // set volume
  if (keypadbuffer.length() < MAX_KEYPAD_BUFFER_LENGTH) keypadbuffer += c;  
  
  if (debug_mode){ 
    Serial.print(keypadbuffer.toInt());
    Serial.print(":");
    Serial.println(map(keypadbuffer.toInt(),0,9,0,30));
  };

  mp3_set_volume (map(keypadbuffer.toInt(),0,9,0,30));
  _clear();
}

void _clear ()
{
  sm.state = SM_START;
  keypadbuffer = "";
  digitalWrite(STATUS_LED, LOW);
}

void _restartidletimer ()
{
  if (idlesm.state > 1) {mp3_play(10+idlesm.state);};
  idlesm.state = IDLE_START;
  for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++){t.stop(i);};
  t.after(15*1000L,onWaitTooLongForInput);  // wait 15 sec on input
  t.after(5*60*1000L,onIdleWhile);          // 5 min warning on idle
  t.after(10*60*1000L,onIdleLonger);        // 10 min warning on idle
  t.after(15*60*1000L,onIdleTooLong);       // 15 min warning on idle
}

// ===== Timer: no input for too long ==============
void onWaitTooLongForInput()
{
  if (sm.state > SM_START) {mp3_play(1);};
  _clear ();
}

// ===== Timer: no activity handlers ==============
void onIdleWhile()
{
  mp3_play(2);
  idlesm.state = IDLE_WAITING;
  _clear ();
}

void onIdleLonger()
{
  mp3_play(3);
  idlesm.state = IDLE_NERVOUS;
  _clear ();
}

void onIdleTooLong()
{
  mp3_play(4);
  idlesm.state = IDLE_FORGOTTEN;
  _clear ();
}

/*
  mp3_play (1);
  delay (6000);
  mp3_next ();
  delay (6000);
  mp3_prev ();
  delay (6000);
  mp3_play (4);
  delay (6000);

  mp3_play ();    //start play
  mp3_play (5); //play "mp3/0005.mp3"
  mp3_next ();    //play next
  mp3_prev ();    //play previous
  mp3_set_volume (uint16_t volume); //0~30
  mp3_set_EQ ();  //0~5
  mp3_pause ();
  mp3_stop ();
  void mp3_get_state ();  //send get state command
  void mp3_get_volume ();
  void mp3_get_u_sum ();
  void mp3_get_tf_sum ();
  void mp3_get_flash_sum ();
  void mp3_get_tf_current ();
  void mp3_get_u_current ();
  void mp3_get_flash_current ();
  void mp3_single_loop (boolean state); //set single loop
  void mp3_DAC (boolean state);
  void mp3_random_play ();
*/
