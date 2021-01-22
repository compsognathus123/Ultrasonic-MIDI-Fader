#include <Wire.h> 
#include <Adafruit_CAP1188.h>

#include <MIDIUSB.h>

#define CAP_RESET  A3
#define CAP_INTERRUPT 0

#define USONIC_TRIGGER  9
#define USONIC_ECHO 8

#define NUM_FADER 4
#define SMOOTHING_VALUE 4

Adafruit_CAP1188 cap = Adafruit_CAP1188(CAP_RESET);

volatile bool touch_detected;

unsigned long millis_loop;

unsigned long usonic_last_measurement;
unsigned long usonic_inrange;
unsigned int usonic_distance;
unsigned char usonic_smoother[SMOOTHING_VALUE];
unsigned char usonic_smoother_index;
boolean usonic_enabled;

unsigned int init_max_fade_distance;
unsigned int max_fade_distance;
unsigned char fader_value[NUM_FADER];
unsigned char current_fader;

volatile int irupt;

void capInterrupt()
{
   irupt++;
   touch_detected = true;
}

void setup() 
{
   Serial.begin(9600);
   Serial.println("Setup start");
   
   pinMode(CAP_INTERRUPT, INPUT);
   pinMode(CAP_RESET, OUTPUT);
   pinMode(USONIC_TRIGGER, OUTPUT);
   pinMode(USONIC_ECHO, INPUT);
   
   initCapsense();
   
   init_max_fade_distance = 60;
   max_fade_distance = init_max_fade_distance;
   
   Serial.println("Setup done!");
}

void initCapsense()
{
   cap.begin();
   delay(100);
   
   cap.writeRegister(0x41, 0x39);  // 0x41 default 0x39 use 0x41  — Set "speed up" setting back to off
   //cap.writeRegister(0x44, 0x41);  // 0x44 default 0x40 use 0x41  — Set interrupt on press but not release
   //cap.writeRegister(0x28, 0x00);  // 0x28 default 0xFF use 0x00  — Turn off interrupt repeat on button hold
   
   delay(1000);
   attachInterrupt(digitalPinToInterrupt(CAP_INTERRUPT), capInterrupt, FALLING);
}

float time2Distance(unsigned long duration)
{
   return duration * 0.5 * 0.03432;
}

float getUsonicDistance()
{
   unsigned long sonic_time = 0;
   
   digitalWrite(USONIC_TRIGGER, HIGH);
   delayMicroseconds(100);
   digitalWrite(USONIC_TRIGGER, LOW);
   
   sonic_time = pulseIn(USONIC_ECHO, HIGH);
   
   return time2Distance(sonic_time);
}

void sendMidiCC(byte channel, byte control, byte value) 
{
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}

unsigned char smoothUsonic(unsigned char new_usonic_distance)
{
   usonic_smoother_index++;
   if(usonic_smoother_index >= SMOOTHING_VALUE) usonic_smoother_index = 0;  
   Serial.print("smoother index: ");
   Serial.println(usonic_smoother_index); 
   usonic_smoother[usonic_smoother_index] = new_usonic_distance;

   unsigned int smoothed = 0;
   for(int i = 0; i < SMOOTHING_VALUE; i++)
   {
      smoothed += usonic_smoother[i];
      //Serial.println();
   }

   return smoothed/SMOOTHING_VALUE;
}

long stopwatch;

void loop() 
{
   millis_loop = millis();
   
   if(touch_detected)
   {          
      usonic_enabled = !usonic_enabled;
      max_fade_distance = init_max_fade_distance;
      fader_value[current_fader] = 0;
      usonic_inrange = millis();

       if(!usonic_enabled)
       {
         sendMidiCC(0, current_fader, 0);
    
       }
      //uint8_t touched = cap.touched();
      Serial.println(irupt);
      Serial.print("Usonic enabled: ");
      Serial.println(usonic_enabled);       
      touch_detected = false;
   }       

   if(usonic_enabled && millis_loop - usonic_last_measurement > 20)
   {
      /*Serial.print("delta Measurement: ");
      Serial.println(millis()-stopwatch);
      stopwatch = millis();*/
      
      usonic_distance = getUsonicDistance();
      usonic_last_measurement = millis();
      
      if(usonic_distance < max_fade_distance)
      {
         if(usonic_last_measurement - usonic_inrange < 500)
         {                        
            unsigned char new_fader_value = map(usonic_distance, 0, max_fade_distance, 0, 127);
            
            if(new_fader_value != fader_value[current_fader])
            {
               
               fader_value[current_fader] = smoothUsonic(new_fader_value);
               /*Serial.print("usonic smooth: ");
               Serial.print(fader_value[current_fader]);
               Serial.print("\t usonic: ");
               Serial.println(new_fader_value);*/
               sendMidiCC(0, current_fader, fader_value[current_fader]);
            }            
            
            //fader_value[current_fader] = usonic_distance/max_fade_distance * 255;
         }
         else
         {
            //Wiedereintritt -> neues Maximum FIXXEN!!
            max_fade_distance = usonic_distance * (255.0/fader_value[current_fader]);
            max_fade_distance = init_max_fade_distance;
            Serial.print("Max Fade Dist: ");
            Serial.println(max_fade_distance);
         }
         usonic_inrange = usonic_last_measurement;
      }
      
   }

   
   
  

}
