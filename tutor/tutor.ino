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

//#define debug_mode

// =========================== HW peripherals ========================

// --- LED - Integrated LED diode
#define STATUS_LED LED_BUILTIN

// --- BUZER - Beep module
#define BUZZER_PIN 12  //Digital pin. HIGH mean sound

// --- Keypad 4x4
#define analog_keypad

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
#ifndef analog_keypad
  uint8_t ColumnPins[columns] =  {6, 7, 8, 9};     // digital pins
#else
  uint8_t ColumnPins[columns] =  {A3, A2, A1, A0};  // analog pins
#endif
// Keypad object
Keypad keyboard = Keypad( makeKeymap(keys), RowPins, ColumnPins, rows, columns);

// --- RTC - real time clock module DS3231
// RTC external object from DS3232RTC

// --- MP3 - DFPlayer mini mp3 module
#define DEFAULT_SOUND_VOLUME 15   // 0..30
#define IDLE_WARNING_VOLUME_LEVEL 25  

#define MP3_STATUS  9  // pin mp3 status flag LOW means Busy/Playing
uint8_t act_volume = DEFAULT_SOUND_VOLUME;
bool idle_volume_active = false;

SoftwareSerial mp3Serial(10, 11); // RX, TX

// =========================== SW modules ========================
// --- Action timer, beep timer
Timer t, b;

// --- State machines
// (SM: STATE: 1-Start,2-SelectMode,3-SelectSound,4-SetVolume TRANSITION: 1-1,1-2,2-1,1-3,3-3,3-1,1-4,4-1)
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
uint16_t glsb = 1;        // game legend set base - mp3 file index base - A-1, B-2, C-3, D-4..99

// =================================== SETUP ====================
void setup() {
  // Serial communication 38400 baud
  Serial.begin(38400);
  Serial.println("------ Speaking game tutorial, Firmware version 0.3");

  Serial.println(">> Status LED...");
  // Pin 13 has an LED connected on most Arduino boards:
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
  Serial.println(">> Status LED Initialized");

  Serial.println(">> Buzzer...");
  // BUZZER_PIN
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
  Serial.println(">> Buzzer Initialized");

  Serial.println(">> MP3 Player...");
    pinMode(MP3_STATUS, INPUT);
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
  b.update();

  // read pressed key from keypad
  char _key = keyboard.getKey();

  // pressed key processing
  if (_key){
    #ifdef debug_mode
      Serial.print("Pressed key: ");
      Serial.println(_key);
      Serial.print(sm.state);
      Serial.print("->");
    #endif

    beep();
    _restartidletimer();     // restart idle timer 

    // Keypressed-based decision routine
    switch (sm.state){
	  case SM_START: // Initial
		  if (CNs1t1(_key)) { sm.last_state = SM_START; TNs1t1(_key); }        // A..D game legend set 1..4 shorcut
          else if (CNs1t2(_key)) { sm.last_state = SM_START;  TNs1t2(); }      // * set game legend
		  else if (CNs1t4(_key)) { sm.last_state = SM_START; TNs1t4(); }       // # set volume
          else if (CNs1t3(_key)) { sm.last_state = SM_START; TNs1t3(_key); }   // 0..9 sound index
        break;
	  case SM_SELECTMODE: // * reading game legend set base
		  if (CNs2t1(_key)) { sm.last_state = SM_SELECTMODE; TNs2t1(_key); }   // A..D/0..9#
		  else { sm.last_state = SM_SELECTMODE; _clear(); }                    // *
        break;
	  case SM_SELECTSOUND: // 0..9 reading sound index
		  if (CNs3t3(_key)) { sm.last_state = SM_SELECTSOUND; TNs3t3(_key); }  // 0..9
		  else if (CNs3t1(_key)) { sm.last_state = SM_SELECTSOUND; TNs3t1(); } // #
		  else { sm.last_state = SM_SELECTSOUND; _clear(); }                   // * A..D
        break;
	  case SM_SETVOLUME: // # reading volume setting 
		  if (CNs4t1(_key)) { sm.last_state = SM_SETVOLUME;  TNs4t1(_key); }  // 0..9
		  else { sm.last_state = SM_SETVOLUME; _clear(); }                    // * # A..D
        break;
    }

    #ifdef debug_mode
      Serial.println(sm.state);
    #endif
  } //if (_key)
}

bool CNs1t1(char c) {
	return (c >= 'A') && (c <= 'D');
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
	return ((c >= 'A') && (c <= 'D')) || ((c >= '0') && (c <= '9')) || (c == '#');
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

void TNs1t1(char c) {
	TNs2t1(c);
}

void TNs2t1 (char c) {
  // choose game set
  if(digitalRead(MP3_STATUS) == HIGH){
    mp3_stop();
    delay(50);
  }
  switch (c) {
  case 'A': { glsb = 1; mp3_play(255, glsb); _clear(); }
    break;
  case 'B': { glsb = 2; mp3_play(255, glsb); _clear(); }
    break;
  case 'C': { glsb = 3; mp3_play(255, glsb); _clear(); }
    break;
  case 'D': { glsb = 4; mp3_play(255, glsb); _clear(); }
    break;
  case '#': { 
				glsb = keypadbuffer.toInt(); 
				if (glsb < 100) {
					mp3_play(255, glsb);
				}
				else {
					mp3_play(5); //err
				}
				_clear();
			 }
	break;
  default: { if (keypadbuffer.length() < MAX_KEYPAD_BUFFER_LENGTH) keypadbuffer += c; }
  } 
}

void TNs3t1 () {
  // play sound
  #ifdef debug_mode 
    Serial.print(glsb);
    Serial.print("/");
    Serial.print(keypadbuffer.toInt());
    Serial.print(":");
  #endif

  if(digitalRead(MP3_STATUS) == HIGH){
    mp3_stop();
    delay(50);
  }
  mp3_play(keypadbuffer.toInt(),glsb);
  _clear();
}

void TNs3t3 (char c) {
  if (keypadbuffer.length() < MAX_KEYPAD_BUFFER_LENGTH) keypadbuffer += c;
}

void TNs4t1 (char c) {
  // set volume
  if (keypadbuffer.length() < MAX_KEYPAD_BUFFER_LENGTH) keypadbuffer += c;  
  
  #ifdef debug_mode 
    Serial.print(keypadbuffer.toInt());
    Serial.print(":");
    Serial.println(map(keypadbuffer.toInt(),0,9,0,30));
  #endif

	act_volume = map(keypadbuffer.toInt(), 0, 9, 0, 30);
	mp3_set_volume(act_volume);
  _clear();
}

void _clear ()
{
  sm.state = SM_START;
  keypadbuffer = "";
  digitalWrite(STATUS_LED, LOW);
}

void beep ()
{
  beep_on(); 
  b.after(100, beep_off);
}

void beep_on()
{
	digitalWrite(BUZZER_PIN, HIGH);
}

void beep_off()
{
  digitalWrite(BUZZER_PIN,LOW);  
}


void _restartidletimer ()
{
  idlesm.state = IDLE_START;
  if (idle_volume_active){
	  idle_volume_active = false;
	  mp3_set_volume(act_volume);
  }
  for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++){t.stop(i);};
  t.after(15*1000L,onWaitTooLongForInput);  // wait 15 sec for input
  t.after(5*60*1000L,onIdleWhile);          // 5 min warning on idle
  t.after(10*60*1000L,onIdleLonger);        // 10 min warning on idle
  t.after(15*60*1000L,onIdleTooLong);       // 15 min warning on idle
}

// ===== Timer: input timeout ==============
void onWaitTooLongForInput()
{
  beep();
  _clear ();
}

// ===== Timer: no activity handlers ==============
void onIdleWhile()
{
  if(digitalRead(MP3_STATUS) == HIGH){
	  mp3_set_volume(IDLE_WARNING_VOLUME_LEVEL);
	  idle_volume_active = true;
	  mp3_play(2);
    idlesm.state = IDLE_WAITING;
    _clear ();
  }
  else {
    _restartidletimer ();
  }
}

void onIdleLonger()
{
  if(digitalRead(MP3_STATUS) == HIGH){
    mp3_play(3);
    idlesm.state = IDLE_NERVOUS;
    _clear ();
  }
  else {
    _restartidletimer ();
  }
}

void onIdleTooLong()
{
  if(digitalRead(MP3_STATUS) == HIGH){
    mp3_play(4);
    idlesm.state = IDLE_FORGOTTEN;
    _clear ();
  }
  else {
    _restartidletimer ();
  }
}

