
#include "esp32-BG95.hpp"
#include "./credentials.h"

#define PWKEY 4

MODEMBGXX modem;

//#define MULTI_CONTEXT // uncomment to use only 1 context
//#define MULTI_TCP // uncomment to have multiple mqtt connections

struct TCP_CONNECTION {
    uint8_t cid; // connection id 1-16, limited to MAX_CONNECTIONS
    uint8_t sslClientID; // connection id 0-5, limited to MAX_CONNECTIONS
    uint8_t clientID; // client idx 0-11, limited to MAX_MQTT_CONNECTIONS
    uint8_t msg_id;
};

TCP_CONNECTION tcp1 = {
  1,
  0,
  0,
  0
};

#ifdef MULTI_TCP
  #ifdef MULTI_CONTEXT
    TCP_CONNECTION tcp2 = {
      2,
      1,
      1,
      0
    };
  #else
  TCP_CONNECTION tcp2 = {
    1,
    1,
    1,
    0
  };
  #endif
#endif

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  modem.init_port(115200,SERIAL_8N1);
  //modem.init(SETTINGS_NB_COPS,AUTO,PWKEY);
  modem.init(0,AUTO,PWKEY);

  // creates 2 contexts
  modem.setup(tcp1.cid,SETTINGS_NB_APN,SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #ifdef MULTI_CONTEXT
  modem.setup(tcp2.cid,"ocnetjws.prtop",SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #endif
  // init contexts

  if(!modem.set_ssl(tcp1.sslClientID))
    Serial.println("failing configuring ssl");

}

void loop() {
  // put your main code here, to run repeatedly:

  if(modem.loop(5000)){ // state was updated
    if(modem.has_context(tcp1.cid)){

      String token = "";
      //String token = "X-Session:xxx";
      //if(!modem.https_get(HTTPS_HOST,"/api/entities",token,tcp1.clientID,tcp1.sslClientID,tcp1.cid)){
      if(!modem.https_post(HTTPS_HOST,HTTPS_PATH,HTTPS_BODY,token,tcp1.clientID,tcp1.sslClientID,tcp1.cid)){
        Serial.printf("http request to: %s%s has failed..\n",HTTPS_HOST,HTTPS_PATH);
      }
      if(!modem.http_wait_response(tcp1.clientID)){
        Serial.println("http request: no response received");
      }

      if(modem.http_response_status().indexOf("200") > -1){
        Serial.println("http request: OK");
        uint16_t len = modem.http_get_body_size();
        Serial.printf("http body size: %d \n",len);
        char* data = (char*)malloc(len);
        if(data != nullptr){
          len = modem.http_get_body(tcp1.clientID,data,len);
          for(uint16_t i = 0; i<len; i++)
            Serial.print((char)data[i]);

          // check md5
          if(!modem.http_check_md5(data,len))
            Serial.println("md5 verification failed");

          free(data);
        }
      }else Serial.println("response: "+modem.http_response_status());
      // get body length
      // get data
    }else{
      modem.open_pdp_context(tcp1.cid);
    }
    #ifdef MULTI_TCP
      if(modem.has_context(tcp2.cid)){

      }else{
        modem.open_pdp_context(tcp2.cid);
      }
    #endif

    modem.log_status();
  }


  delay(200);

}
