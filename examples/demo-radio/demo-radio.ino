
#include "esp32-BG95.hpp"
#include "./credentials.h"

#define PWKEY 4

MODEMBGXX modem;

#define MULTI_CONTEXT // uncomment to use only 1 context

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  modem.init_port(115200,SERIAL_8N1);
  if(!modem.init(CATM1,SETTINGS_NB_COPS,PWKEY))
    Serial.println("modem failed to init");

  // creates 2 contexts
  modem.setup(1,SETTINGS_NB_APN,SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #ifdef MULTI_CONTEXT
  modem.setup(2,SETTINGS_NB_APN2,SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #endif
  // init contexts

}

void loop() {
  // put your main code here, to run repeatedly:


  if(modem.loop(5000)){ // state was updated
    if(!modem.has_context(1)){
      modem.open_pdp_context(1);
    }
    #ifdef MULTI_CONTEXT
      if(!modem.has_context(2)){
        modem.open_pdp_context(2);
      }
    #endif

    modem.log_status();

    tm t;
    if(modem.get_clock(&t)){
      Serial.printf("date: %4d/%2d/%2d \n",t.tm_year+2000,t.tm_mon,t.tm_mday);
      Serial.printf("time: %2d:%2d:%2d \n",t.tm_hour,t.tm_min,t.tm_sec);
    }
    Serial.println("timezone diff: "+String(modem.get_tz()));
  }


  delay(200);

}
