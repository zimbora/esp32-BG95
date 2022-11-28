
#include "esp32-BG95.hpp"
#include "./credentials.h"

#define PWKEY 4

#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

MODEMBGXX modem;

//#define MULTI_CONTEXT // uncomment to use only 1 context
//#define MULTI_MQTT // uncomment to have multiple mqtt connections

struct MQTT_CONNECTION {
    uint8_t cid; // connection id 1-16, limited to MAX_CONNECTIONS
    uint8_t clientID; // client idx 0-5, limited to MAX_MQTT_CONNECTIONS
    uint8_t msg_id;
};

MQTT_CONNECTION mqtt1 = {
  1,
  0,
  0
};
#ifdef MULTI_MQTT
  #ifdef MULTI_CONTEXT
    MQTT_CONNECTION mqtt2 = {
      2,
      1,
      0
    };
  #else
  MQTT_CONNECTION mqtt2 = {
    1,
    1,
    0
  };
  #endif
#endif

String topic[] = {
  "esp32/freeRTOS/+/status"
};
uint8_t topic_qos[] = {
  2
};

bool (*mqtt_callback)(uint8_t,String,String);
bool mqtt_parse_msg(uint8_t clientID, String topic, String payload){
  Serial.printf("client id: %d \n",clientID);
  Serial.printf("parse topic: %s \n",topic.c_str());
  Serial.printf("with payload: %s \n",payload.c_str());
  return true;
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  modem.init_port(115200,SERIAL_8N1);
  modem.init(SETTINGS_NB_COPS,AUTO,PWKEY);

  mqtt_callback = &mqtt_parse_msg;
  modem.MQTT_init(mqtt_callback);

  // creates 2 contexts
  modem.setup(mqtt1.cid,SETTINGS_NB_APN,SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #ifdef MULTI_CONTEXT
  modem.setup(mqtt2.cid,"ocnetjws.prtop",SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #endif
  // init contexts

  // creates 2 mqtt instances - pass here callback
  modem.MQTT_setup(mqtt1.clientID,mqtt1.cid,"state","offline");
  #ifdef MULTI_MQTT
  modem.MQTT_setup(mqtt2.clientID,mqtt2.cid,"state","offline");
  #endif
}

void loop() {
  // put your main code here, to run repeatedly:


  if(modem.loop(5000)){ // state was updated
    if(modem.has_context(mqtt1.cid)){
      if(!modem.MQTT_connected(mqtt1.clientID)){
        if(modem.MQTT_connect(mqtt1.clientID,"test1",MQTT_USER_1,MQTT_PASSWORD_1,MQTT_HOST_1,1883))
          modem.MQTT_subscribeTopics(mqtt1.clientID,++mqtt1.msg_id,topic,topic_qos,NUMITEMS(topic));
      }
    }else{
      modem.open_pdp_context(mqtt1.cid);
    }
    #ifdef MULTI_MQTT
      if(modem.has_context(mqtt2.cid)){
        if(!modem.MQTT_connected(mqtt2.clientID)){
          if(modem.MQTT_connect(mqtt2.clientID,"test2",MQTT_USER_2,MQTT_PASSWORD_2,MQTT_HOST_2,1883))
            modem.MQTT_subscribeTopics(mqtt2.clientID,++mqtt2.msg_id,topic,topic_qos,NUMITEMS(topic));
        }
      }else{
        modem.open_pdp_context(mqtt2.cid);
      }
    #endif

    modem.log_status();
  }


  delay(200);

}
