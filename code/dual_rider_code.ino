#include <Arduino.h>
//#include <ESP32CAN.h>
//#include <CAN_config.h>
#include <HX711_ADC.h>
//#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
//#endif
#include "pitches.h"  //add note library


//// buzzer constant
//notes in the melody
int melody[]={NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
//note durations. 4=quarter note / 8=eighth note
int noteDurations[]={4, 8, 8, 4, 4, 4, 4, 4};
/////////////////////////////



/// dual rider constant   
const float weight_variation  = 15 ; 
const float calibrationValue = 19.78 ;   //// use calibration code to get your calibration value 
const float offset_value = 430597 ;     /// to get your unique offset_value set this value to zero,go to line 133 and replace it with this instruction  current = ((LoadCell.getData()) ;    upload the code, open serial monitor, copy average value showed on serial monitor, past the new offset_value here go to line 133 and replace it with this  current = ((LoadCell.getData()-offset_value)/1000) ;  
const int serialPrintInterval = 800;          //period to get data from load cells 
const float two_person_size = 150 ;         // maximum weight estimation for 1 person   /// weight more than that will be considered as two riders



//pins:
const int HX711_dout = 21; //mcu > HX711 dout pin
const int HX711_sck = 19; //mcu > HX711 sck pin
const int buzzer = 26;   

//CAN_device_t CAN_cfg;               // CAN Config
unsigned long previousMillis = 0;   // will store last time a CAN Message was send
const int interval = 1000;          // interval at which send CAN Messages (milliseconds)
const int rx_queue_size = 10;       // Receive Queue size




//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
bool buzzer_flag ;
const int calVal_calVal_eepromAdress = 0;
unsigned long t = 0;
float data_weight[2];
float variation ;
int n_var ;
int person_count ; 
bool pb ;
float last ; 
float current ;
char message[20]; 
bool skip_first_read ;
//static//
boolean newDataReady = 0;

void setup() {
  Serial.begin(57600); 
  delay(10);
/*  Serial.println("CAN initialization");
  CAN_cfg.speed = CAN_SPEED_125KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_23;
  CAN_cfg.rx_pin_id = GPIO_NUM_22;
  CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
  // Init CAN Module
  ESP32Can.CANInit();*/
  pinMode(buzzer, OUTPUT);
   digitalWrite(buzzer, LOW);
  Serial.println();
  Serial.println("Starting...");
  //float calibrationValue; // calibration value
  // uncomment this if you want to set this value in the sketch    //////// use calibration code and get your own calibration value ////20.62
  LoadCell.setSamplesInUse(0);
//#if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
//#endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch this value from eeprom

  LoadCell.begin();
  //LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 10; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = false ; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  Serial.print("Calibration value: ");
  Serial.println(LoadCell.getCalFactor());
  Serial.print("HX711 measured conversion time ms: ");
  Serial.println(LoadCell.getConversionTime());
  Serial.print("HX711 measured sampling rate HZ: ");
  Serial.println(LoadCell.getSPS());
  Serial.print("HX711 measured settlingtime ms: ");
  Serial.println(LoadCell.getSettlingTime());
  Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
  if (LoadCell.getSPS() < 7) {
    Serial.println("!!Sampling rate is lower than specification, check MCU>HX711 wiring and pin designations");
  }
  else if (LoadCell.getSPS() > 100) {
    Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
  }
  skip_first_read = false ;  
  buzzer_flag = false ;
 //noTone(buzzer);
 xTaskCreate(&load, "load", 2048, NULL, 2, NULL);
 //xTaskCreate(&load1, "load1", 2048, NULL, 2, NULL);
 xTaskCreate(&buzzerTask, "buzzerTask",2048,NULL,2,NULL);

 digitalWrite(buzzer, HIGH);
 delay(200);
 digitalWrite(buzzer, LOW);
 delay(200);
}

void load(void *params) {
  while(1){
  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;
  // get smoothed value from the dataset:
  if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
       
      dual_rider_detection() ;
      Serial.print("Load_cell output val: ");
      Serial.print(current);
      Serial.print("  Kg ");
      Serial.print("  ");
      Serial.print ("  N° of rider   " ) ;
      Serial.print(person_count);
      Serial.print("   n_var :");
      Serial.print(n_var);
      Serial.println("");
      newDataReady = 0;
      if(person_count>1){
        buzzer_flag = true ;
      }
      else {
        buzzer_flag = false ; 
      }
      //CAN_communication_N_Rider();
      t = millis();
        }
       }
    delay(10);
   }
}




void dual_rider_detection(){
      current = ((LoadCell.getData()-offset_value)/1000) ;                   
      variation = current-last ;
      if (variation>weight_variation)    // if we have a weight variation more than 10Kg than increment n_var
      {
        n_var = n_var + 1 ;
      }
      if (variation<(-weight_variation))   // if we have a weight variation more than --10kg than decrement n_var
      {
        n_var = n_var - 1 ;
      }


/// adjust variation 
  if (current<75 && current >35){   //// one rider can cause many variation
    n_var = 2 ; 
  }
  if (current<35 && current >10){   
    n_var = 1 ;
  }

      
      if (current<2 ){  // set n_var to zero when there is no weight over board
        n_var = 0 ;
        person_count = 0 ; 
      }

      if (current< 50 && current>30 ) {        ////////////////////        if weight is between 30 and 50 Kg so it's 1 rider 
        person_count  = 1 ;
        n_var = 2 ;
      }
      if (current >two_person_size){    ///////////////////////////////// //// if weight is more than 150 Kg it's 2 rider
        person_count = 2 ;
        n_var = 4 ;
      }

      
      /////// Analyze n_var 
      switch(n_var){
        case 0  : 
        person_count  = 0 ;
        buzzer_flag = false ;
        break;
        case 1 : 
        person_count = 0 ; 
        buzzer_flag = false ;
        break;
        case 2 : 
        person_count = 1 ;
        buzzer_flag = false ;
        break;
        case 3 : 
        person_count = 1 ;
        buzzer_flag = false ;
        break;
        case 4 : 
        person_count = 2 ; 
        buzzer_flag = true ;
        break;
        default  : 
        person_count = 2 ;
        buzzer_flag = true ;
        break;
      }
      
      last = current ;
  
}



/*void  load1(void *params){
while(1){
   CAN_frame_t tx_frame;
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.MsgID = 0x010;                   //// target ID (Llama eye board ID)
    tx_frame.FIR.B.DLC = 5;                   ////  LEN
    tx_frame.data.u8[0] = 0x02;                ///// STX       
    tx_frame.data.u8[1] = 0x30;               //// CMD
    tx_frame.data.u8[2] = 0x02;               ///// dyn
    tx_frame.data.u8[3] = person_count;               ////data  
    tx_frame.data.u8[4] = 0xFF;                ////cks
    
    Serial.println("sending ..............");
    ESP32Can.CANWriteFrame(&tx_frame);
delay(500);
}
  
}*/

void  buzzerTask(void *params)
{
  Serial.println("Started buzzer task");
  while (1)
  {
    delay(2);
    if (buzzer_flag)
    {
      digitalWrite(buzzer, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      delay(5000);
    }
    else
    {
     digitalWrite(buzzer, LOW);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    delay(10);
  }
}
  


void loop()
{ 
//Serial.println("get_command");  
//R_data();
delay(10);
  
}
