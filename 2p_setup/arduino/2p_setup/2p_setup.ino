#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t currtouched = 0;

int reward_input_ttl = 3;
int aversive_input_ttl = 4;
int reward_valve_output = 5;
int aversive_valve_output = 6;
int lickport_output = 8;
int reward_input_button = 9;
int aversive_input_button = 10;

int pav_exp_button = 11;
int pav_exp_led = 12;



typedef enum  
{ 
  lookForInput,
  waitForInputToFinish,
  waitForLick,
  openValves,
} state;

typedef enum  
{ 
  pavlovian,
  experiment,
} pav_exp;

state current_state = lookForInput;
pav_exp pav_exp_state = experiment;

// reward -> 1, aversive -> 0, neutral -> 0
int reward_aversive = -1;
int ms_for_lick = 2000;
int ms_rew_valve_open = 1000;
int ms_av_valve_open = 150;
int ms_stimulus_length = 3000;
int ms_lickport_signal = 10;

int pavexp_button_reading;
int pavexp_button_previous = LOW;
int rew_button_reading;
int rew_button_previous = LOW;
int adv_button_reading;
int adv_button_previous = LOW;
long time_button_pav = 0;
long time_button_rew = 0;
long time_button_adv = 0;
long debounce_button = 200;

unsigned long wait_lick_start_time = 0;
unsigned long input_start_time = 0;
unsigned long lick_time = 0;
unsigned long open_start_time = 0;
unsigned long last_pwm_pos_time = 0;
unsigned long last_pwm_neg_time = 0;
int pwm_state = 0;
int pwm_ms_high = 80;
int pwm_ms_low = 20;

void setup() {
  Serial.begin(9600);

  while (!Serial) { // needed to keep leonardo/micro from starting too fast!
    delay(10);
  }
  
  Serial.println("Adafruit MPR121 Capacitive Touch sensor test"); 
  
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

  // set pin mode
  pinMode(reward_input_ttl, INPUT);
  pinMode(aversive_input_ttl, INPUT);
  pinMode(reward_input_button, INPUT);
  pinMode(aversive_input_button, INPUT);
  pinMode(pav_exp_button, INPUT);
  pinMode(reward_valve_output, OUTPUT);
  pinMode(aversive_valve_output, OUTPUT);
  pinMode(lickport_output, OUTPUT);
  pinMode(pav_exp_led, OUTPUT);  

  digitalWrite(reward_valve_output, LOW);
  digitalWrite(aversive_valve_output, LOW);
}

void loop() {

      currtouched = cap.touched();
          
      if ((currtouched & _BV(5))) { 
          Serial.println("lickport licked: state 'openValves'");
          digitalWrite(lickport_output, HIGH);
          Serial.println("LICKPORT SIGNAL");
          delay(ms_lickport_signal);
          digitalWrite(lickport_output, LOW);
          lick_time = millis();
      }

      if (current_state == lookForInput)
      { 
          // read pavlovian exp button
          pavexp_button_reading = digitalRead(pav_exp_button);
          if (pavexp_button_reading == HIGH && pavexp_button_previous == LOW && millis() - time_button_pav > debounce_button)
          {
            if (pav_exp_state == experiment)
              pav_exp_state = pavlovian;
            else
              pav_exp_state = experiment;
            time_button_pav = millis();    
          }
      
          pavexp_button_previous = pavexp_button_reading;
          if (pav_exp_state == experiment)
            digitalWrite(pav_exp_led, LOW);
          else
            digitalWrite(pav_exp_led, HIGH);

          // read reward button
          rew_button_reading = digitalRead(reward_input_button);
          if (rew_button_reading == HIGH && rew_button_previous == LOW && millis() - time_button_rew > debounce_button)
          {
            reward_aversive = 1;
            current_state = openValves;
            open_start_time = millis();
            time_button_rew = millis();    
          }
      
          rew_button_previous = rew_button_reading;

          // read aversive button
          adv_button_reading = digitalRead(aversive_input_button);
          if (adv_button_reading == HIGH && adv_button_previous == LOW && millis() - time_button_adv > debounce_button)
          {
            reward_aversive = 0;
            current_state = openValves;
            open_start_time = millis();
            time_button_adv = millis();    
          }

          adv_button_previous = adv_button_reading;
          
          // read TTL inputs
          int reward_val = digitalRead(reward_input_ttl);
          int aversive_val = digitalRead(aversive_input_ttl);
    
          if (reward_val == HIGH)
          {
              current_state = waitForInputToFinish;
              reward_aversive = 1;  
              input_start_time = millis();
              Serial.println("reward input received: state 'waitForInputToFinish'");
          }  
          
          if (aversive_val ==  HIGH)
          {
              current_state = waitForInputToFinish;  
              reward_aversive = 0;  
              input_start_time = millis();            
              Serial.println("aversive input received: state 'waitForInputToFinish'");
          }  

  }
  else if (current_state == waitForInputToFinish)
  {
      if (millis() - input_start_time > ms_stimulus_length)
      {
        if (pav_exp_state == experiment)
        {
          current_state = waitForLick;
          wait_lick_start_time = millis();
        }
        else
        {
          if (reward_aversive == 1)
          {
            current_state = openValves;
            open_start_time = millis();
          }
          else
          {
            current_state = waitForLick;
            wait_lick_start_time = millis();
          }
        }
        
      }
  }
  else if (current_state == waitForLick)
  {   
      if ((lick_time > wait_lick_start_time) && (millis() - wait_lick_start_time) < ms_for_lick)
          {
              current_state = openValves;
              open_start_time = millis();
          }
      else if ((millis() - wait_lick_start_time) > ms_for_lick)
        {
            Serial.println("waited to long: state 'waitForLick'");
            current_state = lookForInput;
        }

  }
  else if (current_state == openValves)
  {   
      if (reward_aversive == 1)
        {
          if ((millis() - open_start_time) < ms_rew_valve_open)
          {          
              if ((pwm_state == 0) && (millis() - last_pwm_neg_time) > pwm_ms_low) 
              {
                  digitalWrite(reward_valve_output, HIGH);
                  last_pwm_pos_time = millis();
                  pwm_state = 1;
              }
              else if ((pwm_state == 1) && (millis() - last_pwm_pos_time) > pwm_ms_high) 
              {
                  digitalWrite(reward_valve_output, LOW);
                  last_pwm_neg_time = millis();
                  pwm_state = 0;
              }
          }
          else
          {
            Serial.println("dispensed reward: state 'waitForInput'");
            digitalWrite(reward_valve_output, LOW);
            digitalWrite(aversive_valve_output, LOW);
            current_state = lookForInput;
          }   
        }   
      else if (reward_aversive == 0)
       {
          if ((millis() - open_start_time) < ms_av_valve_open)
          {
            if ((pwm_state == 0) && (millis() - last_pwm_neg_time) > pwm_ms_low) 
            {
                digitalWrite(aversive_valve_output, HIGH);
                last_pwm_pos_time = millis();
                pwm_state = 1;
            }
            else if ((pwm_state == 1) && (millis() - last_pwm_pos_time) > pwm_ms_high) 
            {
                digitalWrite(aversive_valve_output, LOW);
                last_pwm_neg_time = millis();
                pwm_state = 0;
            }
          }
          else
          {
            Serial.println("dispensed aversive: state 'waitForInput'");
            digitalWrite(reward_valve_output, LOW);
            digitalWrite(aversive_valve_output, LOW);
            current_state = lookForInput;
          }      
      }
  }
}
