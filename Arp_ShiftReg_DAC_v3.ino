#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define NOTE_COUNT 12
#define MAX_NOTE 48

// not needed #define NOTE_PIN_START 2

   int newClockState;
    int startPot;
    int rangePot;
    int noteRange;
    int minRange;
    int maxRange;
    bool modeSwitchUp;
    bool modeSwitchDown;
    bool trueNoteExist;
    int minNote;
    int maxNote;
    int volt;
    

enum eMode {
    up = 0,
    down,
    upAndDown,
    arpRandom
};

Adafruit_MCP4725 dac;
bool notes[NOTE_COUNT];
int clockState = LOW;
int iterator = 0;
bool goUp = false;

enum eMode mode = arpRandom;

#include <EEPROM.h>

uint8_t firstEight;// for settings bits in the shift register
uint8_t secondEight;// for settings bits in the shift register

//pins for 74HC595
const int latchPin = 10;
const int clockPin = 11;
const int dataPin = 12;

//Mux 4067 setup
int s0 = 4;
int s1 = 5;
int s2 = 6;
int s3 = 7;
byte data;
int dataRead = 3;
int trigRead = 8; // 8 or 9 can be used, incase you solder the wrong pin like I just did :-)

int controlPin[] = {s0, s1, s2, s3};

  int muxChannel[16][4]={
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };


struct STATE_STRUCT {

   unsigned long time_triggered;
   boolean ON;
   boolean TRIGGERED;

};

STATE_STRUCT LED_STATE_STRUCT[14]; // use instead of arrays, more efficient?

const int RE_TRIGGER_THRESHOLD = 350;// time in ms before button can be hit again, stops bouncing.


///used for testing when no trigger available, just comment out the 
const int CLOCK_TIME = 100;// time in ms before button can be tried.
unsigned long clock_tick; // for testing


void setup ()
{
  // clock_tick = millis(); // uncomment out when testing without trigger
  
  Serial.begin(9600);

    // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
    // For MCP4725A0 the address is 0x60 or 0x61
    // For MCP4725A2 the address is 0x64 or 0x65
    dac.begin(0x60); // 0x60 for the ebay knockoffs
    
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(dataRead, INPUT);
pinMode(trigRead, INPUT);
  pinMode(s0, OUTPUT); 
  pinMode(s1, OUTPUT); 
  pinMode(s2, OUTPUT); 
  pinMode(s3, OUTPUT); 
  digitalWrite(s0, LOW);
  digitalWrite(s1, LOW);
  digitalWrite(s2, LOW);
  digitalWrite(s3, LOW);

  for(int i = 0; i < 14; i ++){
      if (EEPROM.read(i) > 0) {
        LED_STATE_STRUCT[i].ON = true;
      }else{
        LED_STATE_STRUCT[i].ON = false;
      }
  
    LED_STATE_STRUCT[i].time_triggered = millis();
  }
  outputTrigger(); // setup last saved state.



}

int noteToVolt(int noteIn) {
    return (int)round(((float)(4095.0/5.0)/12.0) * noteIn);
}


void loop() {
 
/*
 // testing only for when no trigger input is available
if (millis() - clock_tick > CLOCK_TIME){
  clock_tick = millis();
  if (newClockState == LOW) {
    newClockState = HIGH;
  } else{
      newClockState = LOW;    
  }
}
*/


    
  for(int i = 0; i < 14; i ++){

    if (readMux(i) == HIGH){
  
        if (LED_STATE_STRUCT[i].ON == false && millis () - LED_STATE_STRUCT[i].time_triggered > RE_TRIGGER_THRESHOLD){
            
            LED_STATE_STRUCT[i].ON = true;
            LED_STATE_STRUCT[i].time_triggered = millis();
            outputTrigger();
           EEPROM.update(i, 1);
        } else 
        if (LED_STATE_STRUCT[i].ON == true && millis () - LED_STATE_STRUCT[i].time_triggered > RE_TRIGGER_THRESHOLD){
          
            LED_STATE_STRUCT[i].ON = false;
            LED_STATE_STRUCT[i].time_triggered = millis();    //      EEPROM.update(i, 0);
            outputTrigger();
           EEPROM.update(i, 0);
        }
    }

  }

  // comment out below line if testing without trigger input
   newClockState = digitalRead(trigRead);//analogRead(0) > 200; // original code no longer needed

    startPot = analogRead(1);
    rangePot = analogRead(6);

    noteRange = (int)fmin(round(rangePot/21.), (double)MAX_NOTE);
    minRange = (int)fmin(round(startPot/21.), (double)MAX_NOTE);
    maxRange = (int)fmin(minRange + noteRange, (double)MAX_NOTE);
/*
Serial.print(noteRange);
Serial.print(" - ");
Serial.print(minRange);
Serial.print(" - ");
Serial.print(maxRange);
Serial.print(" - ");
Serial.print(iterator);
Serial.print(" - ");
Serial.print(iterator/4);
Serial.println();
*/
    modeSwitchUp = LED_STATE_STRUCT[12].ON; //analogRead(2) > 500; //amended code for push buttons
    modeSwitchDown = LED_STATE_STRUCT[13].ON; //analogRead(3) > 500;//amended code for push buttons
    enum eMode mode;
    if (modeSwitchUp && modeSwitchDown) {
        mode = upAndDown;
        
    } else if (modeSwitchUp && !modeSwitchDown) {
        mode = up;
    } else if (!modeSwitchUp && modeSwitchDown) {
        mode = down;
    } else {
        mode = arpRandom;
    }


   for (int i = 0; i < NOTE_COUNT; i++) {
        notes[i] = LED_STATE_STRUCT[i].ON;//digitalRead(NOTE_PIN_START + i) == HIGH; ////amended code for push buttons
    }

    trueNoteExist = false;
    //if all notes false, all notes true
    for (int i = 0; i < NOTE_COUNT; i++) {
        trueNoteExist = trueNoteExist || notes[i];
    }
    
    if (!trueNoteExist) {
        for (int i = 0; i < NOTE_COUNT; i++) {
            notes[i] = true;
        }
    }

//compute min and max note
    minNote = MAX_NOTE;
    maxNote = 0;
    
    for (int i = minRange; i <= maxRange; i++) {
        if (notes[i%NOTE_COUNT]) {
            if (i < minNote) {
                minNote = i;
            }
            if (i > maxNote) {
                maxNote = i;
            }
        }
    }

    if (minNote > maxNote) {
        for (int i = minRange; i <= MAX_NOTE; i++) {
            if (notes[i%NOTE_COUNT]) {
                minNote = i;
                maxNote = i;
                break;
            }
        }
    }

    if (mode == arpRandom) {
        iterator = random(minNote, maxNote+1);
        mode = random(100) > 50 ? up : down;
    }

    if (newClockState != clockState) {


/*
  for(int i = 0; i < 14; i ++){
      Serial.print(LED_STATE_STRUCT[i].ON);
  }
  Serial.println();
*/


       
        clockState = newClockState;
        if (newClockState == HIGH) {
   
            for (int i = minNote; i <= maxNote; i++) {

                switch (mode) {
                  case up: {
                      iterator++;
                      goUp = true;
                      if (iterator > maxNote) {
                          iterator = minNote;
                      }
                  }
                      break;
                
                  case down: {
                      iterator--;
                      goUp = false;
                      if (iterator < minNote) {
                          iterator = maxNote;
                      }
                  }
                      break;
                
                  case upAndDown: {
                      if (goUp) {
                          iterator++;
                          if (iterator > maxNote) {
                              iterator = maxNote-1;
                              goUp = false;
                          }
                      } else {
                          iterator--;
                          if (iterator < minNote) {
                              iterator = minNote+1;
                              goUp = true;
                          }
                      }
                  }
                      break;
                
                  default:
                      break;
               }

              iterator = fmin(fmax(iterator, minNote), maxNote);


              if (notes[iterator%NOTE_COUNT]) {

           //   LED_STATE_STRUCT[iterator-3].TRIGGERED = true;
           //   Serial.println(iterator);
          //   ledTrigger(); // future enhancement ideas, flash closest led to active note
                  break;
              }

           }

           volt = noteToVolt(iterator);
           dac.setVoltage(volt, false); // keep false, no need to store to onboard eeprom
        }
    }

}

void outputTrigger(){

firstEight = 0;
secondEight = 0;
// in this case we are only using 12 shift register outputs, so tweak as needed from total of 16 possible.
    
    digitalWrite(latchPin, LOW);

        for (byte r = 8; r < 16; r++){

           if (LED_STATE_STRUCT[r].ON == true){
              secondEight = (secondEight << 1) + 0x1;
            }else{
              secondEight = (secondEight << 1) + 0x0;
          }
         }
         
    shiftOut(dataPin, clockPin, LSBFIRST, secondEight); //secondEight

         for (byte r = 0; r < 8; r++){
           if (LED_STATE_STRUCT[r].ON == true){
              firstEight = (firstEight << 1) + 0x1;
            }else{
              firstEight = (firstEight << 1) + 0x0;
  
          }
         }
    
    shiftOut(dataPin, clockPin, LSBFIRST, firstEight); // firstEight
    digitalWrite(latchPin, HIGH);

 
}


byte readMux(int channel){

  for(int i = 0; i < 4; i ++){
    digitalWrite(controlPin[i], muxChannel[channel][i]);
  }

  data = digitalRead(dataRead);

  return data;
}



void ledTrigger(){

  // future enhancement idea, WIP.
              
firstEight = 0;
secondEight = 0;

    digitalWrite(latchPin, LOW);

        for (byte r = 8; r < 16; r++){
         
           if (LED_STATE_STRUCT[r].ON == true && LED_STATE_STRUCT[r].TRIGGERED == false){
              secondEight = (secondEight << 1) + 0x1;
            }else{
              secondEight = (secondEight << 1) + 0x0;
              LED_STATE_STRUCT[r].TRIGGERED = false;
          }
         }
         
    shiftOut(dataPin, clockPin, LSBFIRST, secondEight); //secondEight

         for (byte r = 0; r < 8; r++){
           if (LED_STATE_STRUCT[r].ON == true && LED_STATE_STRUCT[r].TRIGGERED == false){
              firstEight = (firstEight << 1) + 0x1;
            }else{
              firstEight = (firstEight << 1) + 0x0;
  LED_STATE_STRUCT[r].TRIGGERED = false;
          }
         }
    
    shiftOut(dataPin, clockPin, LSBFIRST, firstEight); // firstEight
    digitalWrite(latchPin, HIGH);

 
}




void testTrigger(){
    // just used for testing shift register patterns working ok          
firstEight = 0;
secondEight = 0;

    digitalWrite(latchPin, LOW);

         
    shiftOut(dataPin, clockPin, LSBFIRST, B11110000); //secondEight


    shiftOut(dataPin, clockPin, LSBFIRST, B11111110); // firstEight
    digitalWrite(latchPin, HIGH);

 
}



