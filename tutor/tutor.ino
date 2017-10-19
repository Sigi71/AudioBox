// === Game SFX machine - SOUNDS TO PLAY
// --- Usage - Keypad actions ------------------------------------------------------
// press A..D to quickly choose sound set
// press * and then repeatedly 0..9 to enter up to 2-digit number (99 max) followed by # to choose sound set
// press # and then press 0..9 to set sound volume level
// press repeatedly 0..9 to enter up to 3-digit number (255 max) followed by # to choose legend
// ---------------------------------------------------------------------------------

// libraries
#include <Keypad.h>              //(included with Arduino IDE)
#include <Timer.h>               //http://playground.arduino.cc/Code/Timer
#include <SoftwareSerial.h>      //http://arduino.cc/en/Reference/SoftwareSerial (included with Arduino IDE)
#include <DFPlayer_Mini_Mp3.h>   //http://github.com/DFRobot/DFPlayer-Mini-mp3
#include <EEPROM.h>              //(included with Arduino IDE)

//#define debug_mode

// ===============================================================
// ==                      HW peripherals                       ==
// ===============================================================

// --- LED - Integrated LED diode
#define STATUS_LED_PIN LED_BUILTIN

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
#define MP3_STATUS_PIN  9  // pin mp3 status flag LOW means Busy/Playing
SoftwareSerial mp3Serial(10, 11); // RX, TX

// --- Vin analog voltmeter (divider 100k/10k)
#define VOLTMETER_PIN A4
const float Aref = 1.18;
const float magic_const = (110/10 * Aref / 1024);

// ===============================================================
// ==                       SW modules                          ==
// ===============================================================

// --- Core
#define MAX_KEYPAD_BUFFER_LENGTH 3
String keypadbuffer = ""; // input string buffer to read up to 3-digit numbers
uint16_t glsb = 1;        // current base - mp3 file folder - A-1, B-2, C-3, D-4..99

const int E_ADDR_SIGNATURE = 0;
const int E_ADDR_PRIMARY_VOLUME_MEMORY=1;
const int E_ADDR_SECONDARY_VOLUME_MEMORY=2;

const float MINIMUM_BATTERY_LEVEL = 6.2;

// --- Action timer, battery timer
Timer t, b;

// --- State machines
// (SM: STATE: 1-Start,2-SelectMode,3-SelectSound,4-SetVolume TRANSITION: 1-1,1-2,2-1,1-3,3-3,3-1,1-4,4-1)
#define SM_START    1
#define SM_SELECTMODE 2
#define SM_SELECTSOUND  3 
#define SM_SETVOLUME  4

// (IDLE: STATE: 1-Start,2-Waiting,3-Nervous,4-Forgotten TRANSITION: 1-2,2-1,2-3,3-1,3-4,4-1)
#define IDLE_START    1
#define IDLE_WAITING  2
#define IDLE_NERVOUS  3
#define IDLE_FORGOTTEN  4

struct StateMachine{
  uint8_t state;
  uint8_t last_state;
} sm, idlesm;

// --- MP3 - DFPlayer mini mp3 module
#define MAX_VOLUME_LEVEL 30
#define DEFAULT_SOUND_VOLUME 15   // 0..30
#define IDLE_WARNING_VOLUME_LEVEL 25  

uint8_t act_volume = DEFAULT_SOUND_VOLUME;
bool idle_volume_active = false;

// =================================== SETUP ====================
void setup() {
  // Serial communication 38400 baud
  Serial.begin(38400);
  Serial.println(">> Serial communication Initialized");
  Serial.println("------ Speaking library, Firmware version 0.5");

  Serial.println(">> Status LED...");
  // Pin 13 has an LED connected on most Arduino boards:
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);
  Serial.println(">> Status LED Initialized");

  Serial.println(">> MP3 Player...");
    pinMode(MP3_STATUS_PIN, INPUT);
  // MP3 serial communication setup
    mp3Serial.begin(9600);
    mp3_set_serial (mp3Serial); //set Serial for DFPlayer-mini mp3 module
    delay(1);  //wait 1ms for mp3 module to set volume
    e_InitializeVolumeMemory();
    mp3_set_volume (e_readVolume());
    delay(10);  //wait 10ms for mp3 module to recover
    sound_syswelcome();
  Serial.println(">> MP3 Player Initialized");

  Serial.println(">> Core...");
    analogReference(INTERNAL); // use the internal ~1.1volt reference  | change (INTERNAL) to (INTERNAL1V1) for a Mega
    sm.state = SM_START;
    idlesm.state = IDLE_START; 

    #ifdef debug_mode
      b.every(5*1000L,oncheckBatteryLevel);  // 5 sec low battery warning
    #else
      b.every(1*60*1000L,oncheckBatteryLevel);  // 5 min low battery warning
    #endif
  Serial.println(">> Core Initialized");

  digitalWrite(STATUS_LED_PIN, LOW);
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

    // Keypressed-based decision machine

    switch (sm.state){
    case SM_START: // Initial
      if (CNs1t1(_key)) { sm.last_state = SM_START; TNs1t1(_key); }        // A..D game legend set 1..4 shorcut
      else if (CNs1t2(_key)) { sm.last_state = SM_START;  TNs1t2(); }      // * -> SM_SELECTMODE:
      else if (CNs1t4(_key)) { sm.last_state = SM_START; TNs1t4(); }       // # -> SM_SETVOLUME:
      else if (CNs1t3(_key)) { sm.last_state = SM_START; TNs1t3(_key); }   // 0..9 -> SM_SELECTSOUND:
        break;
    case SM_SELECTMODE: // * reading base
      if (CNs2t1(_key)) { sm.last_state = SM_SELECTMODE; TNs2t1(_key); }   // A..D/0..9#
      else { sm.last_state = SM_SELECTMODE; _clear(); }                    // * -> SM_START:
        break;
    case SM_SELECTSOUND: // 0..9 reading sound index
      if (CNs3t3(_key)) { sm.last_state = SM_SELECTSOUND; TNs3t3(_key); }  // 0..9
      else if (CNs3t1(_key)) { sm.last_state = SM_SELECTSOUND; TNs3t1(); } // #
      else if (CNs3t2(_key)) { sm.last_state = SM_SELECTSOUND; TNs3t2(); } // *
      else { sm.last_state = SM_SELECTSOUND; _clear(); }                   // A..D -> SM_START:
        break;
    case SM_SETVOLUME: // # reading volume setting 
      if (CNs4t1(_key)) { sm.last_state = SM_SETVOLUME;  TNs4t1(_key); }  // 0..9
      else { sm.last_state = SM_SETVOLUME; _clear(); }                    // * # A..D -> SM_START:
        break;
    }

  _restartidletimer();     // restart idle timer 

    #ifdef debug_mode
      Serial.println(sm.state);
    #endif
  } //if (_key)
}

// =========================== EEPROM ================================

void e_InitializeVolumeMemory(){
  }

uint8_t e_readVolume(){
  uint8_t value;
  value = EEPROM.read(E_ADDR_PRIMARY_VOLUME_MEMORY);
  if ((value > MAX_VOLUME_LEVEL) || (value == 0)) {
    value = EEPROM.read(E_ADDR_SECONDARY_VOLUME_MEMORY);
    if ((value > MAX_VOLUME_LEVEL) || (value == 0)) {
      value = DEFAULT_SOUND_VOLUME;  
      }
    }
    act_volume = value;
    return value;
  }

void e_saveVolume(uint8_t volume){
  if (act_volume != volume){
    EEPROM.write(E_ADDR_PRIMARY_VOLUME_MEMORY, volume); 
    EEPROM.write(E_ADDR_SECONDARY_VOLUME_MEMORY,volume); 
    act_volume = volume;
    }
  }
    
// ============================ MAIN STATE MACHINE =========================== 

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

bool CNs3t2 (char c) {
  return c == '*';
}

bool CNs3t3 (char c) {
  return (c >= '0') && (c <= '9');
}

bool CNs4t1 (char c) {
  return (c >= '0') && (c <= '9');
}

void TNs1t2 () {
  sound_stop();
  sm.state = SM_SELECTMODE;
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void TNs1t3 (char c) {
  keypadbuffer += c;
  sm.state = SM_SELECTSOUND;
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void TNs1t4 () {
  sm.state = SM_SETVOLUME;
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void TNs1t1(char c) {
  TNs2t1(c);
}

void TNs2t1 (char c) {
  // choose sound library
  sound_stop();
  
  switch (c) {
  case 'A': { glsb = 1; sound_libraryenter(glsb); _clear(); }
    break;
  case 'B': { glsb = 2; sound_libraryenter(glsb); _clear(); }
    break;
  case 'C': { glsb = 3; sound_libraryenter(glsb); _clear(); }
    break;
  case 'D': { glsb = 4; sound_libraryenter(glsb); _clear(); }
    break;
  case '#': { 
        glsb = keypadbuffer.toInt(); 
        if (glsb < 100) {
          sound_libraryenter(glsb);
        }
        else {
          sound_syserror(); //err
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

  sound_libraryplayitem(keypadbuffer.toInt(),glsb);
  _clear();
}

void TNs3t2 () {
  sound_stop();
  sm.state = SM_SELECTMODE;
  digitalWrite(STATUS_LED_PIN, HIGH);
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

  uint8_t volume;
  volume = map(keypadbuffer.toInt(), 0, 9, 0, 30);
  mp3_set_volume(volume);
  e_saveVolume(volume);
  _clear();
}

void _clear ()
{
  sm.state = SM_START;
  keypadbuffer = "";
  digitalWrite(STATUS_LED_PIN, LOW);
}

// ========================= SOUND PLAYBACK =======================

void sound_stop(){
  if(sound_isbusy()){
    mp3_stop();
    delay(50);
  }
}

bool sound_isbusy(){
  return (digitalRead(MP3_STATUS_PIN) == LOW);
}
 
void sound_syswelcome(){
  mp3_play(1);
}

void sound_syserror(){
  sound_stop();
  mp3_play(5);
}

void sound_sysinputtimeout(){
  mp3_play_intercut(1);
}

void sound_libraryenter(uint16_t lib){
  sound_stop();
  mp3_play(255, lib);
  b.after(2*1000L,onLibraryWelcome);          //2 sec delay for library enter sound 
}

void sound_librarywelcome(uint16_t lib){
  sound_stop();
  mp3_play(254, lib);
}

void sound_libraryplayitem(uint16_t i, uint16_t lib){
  sound_stop();
  mp3_play(i, lib);
}

void sound_syslowbattery(){
  mp3_play_intercut(2);
}

void onLibraryWelcome(){
  mp3_play(254, glsb); 
}

// ========================= BATTERY LEVEL =========================

void oncheckBatteryLevel(){
  float batteryLevel = readVin();
  #ifdef debug_mode
    Serial.print("Battery Vin: ");
    Serial.println(batteryLevel);

    if ( batteryLevel > 6.2) {
      for (int i=0; i < 20; i++){
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(50);      
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(50);      
      }
    }
  #else
    if (batteryLevel < MINIMUM_BATTERY_LEVEL) {
      sound_syslowbattery();
    }
  #endif
}

float readVin() {
  unsigned int total; // can hold max 64 readings
  float Vin; // converted to volt

  for (int x = 0; x < 64; x++) { // multiple analogue readings for averaging
    total = total + analogRead(VOLTMETER_PIN); // add each value to a total
  }
  // if (total == (1023 * 64)) { // if overflow

  Vin = (total / 64) * magic_const; // convert readings to volt
 
  #ifdef debug_mode
    Serial.print("current Vin: ");
    Serial.println(Vin);
  #endif

  return Vin; 
}

long readVcc(){
  long result; // Read 1.1V reference against AVcc 
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); 
  delay(2); // Wait for Vref to settle 
  ADCSRA |= _BV(ADSC); // Convert 
  while (bit_is_set(ADCSRA,ADSC)); 
  result = ADCL; result |= ADCH<<8; 
  result = 1126400L / result; // Back-calculate AVcc in mV 
  return result; 
}

// ========================= TIMER ============================

void _restartidletimer ()
{
  idlesm.state = IDLE_START;
  if (idle_volume_active){
    idle_volume_active = false;
    mp3_set_volume(act_volume);
  }
  for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++){t.stop(i);};

  if (sm.state != SM_START) {
    t.after(10 * 1000L, onWaitTooLongForInput);  // set 15 sec input timeout
  }
  t.after(5*60*1000L,onIdleWhile);          // 5 min warning on idle
  t.after(10*60*1000L,onIdleLonger);        // 10 min warning on idle
  t.after(15*60*1000L,onIdleTooLong);       // 15 min warning on idle
}

// ===== Timer: input timeout ==============
void onWaitTooLongForInput()
{
  sound_sysinputtimeout();
  _clear ();
}

// ===== Timer: no activity handlers ==============
void onIdleWhile()
{
  if(!sound_isbusy()){
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
  if(!sound_isbusy()){
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
  if(!sound_isbusy()){
    mp3_play(4);
    idlesm.state = IDLE_FORGOTTEN;
    _clear ();
  }
  else {
    _restartidletimer ();
  }
}

