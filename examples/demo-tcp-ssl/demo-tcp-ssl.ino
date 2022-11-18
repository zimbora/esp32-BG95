
#include "esp32-BG95.hpp"
#include "./credentials.h"

#define PWKEY 4

MODEMBGXX modem;

//#define MULTI_CONTEXT // uncomment to use only 1 context
//#define MULTI_TCP // uncomment to have multiple mqtt connections

struct TCP_CONNECTION {
    uint8_t cid; // connection id 1-16, limited to MAX_CONNECTIONS
    uint8_t clientID; // client idx 0-10, limited to MAX_TCP_CONNECTIONS
    uint8_t ssl_cid; // ssl client idx 0-5, limited to MAX_TCP_CONNECTIONS
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
  modem.init(SETTINGS_NB_COPS,AUTO,PWKEY);

  // creates 2 contexts
  modem.setup(tcp1.cid,SETTINGS_NB_APN,SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #ifdef MULTI_CONTEXT
  modem.setup(tcp2.cid,"ocnetjws.prtop",SETTINGS_NB_USERNAME,SETTINGS_NB_PASSWORD);
  #endif

  if(!modem.set_ssl(tcp1.cid))
    Serial.println("failing configuring ssl");
  // init contexts

}

void loop() {
  // put your main code here, to run repeatedly:

  uint16_t len = modem.tcp_has_data(tcp1.clientID);
  if(len > 0){
    char* data = (char*)malloc(len);
    if(data != nullptr){
      len = modem.tcp_recv(tcp1.clientID,data,len);
      for(uint16_t i = 0; i<len; i++){
        Serial.print((char)data[i]);
      }
      free(data);
    }
  }

  if(modem.loop(5000)){ // state was updated
    if(modem.has_context(tcp1.cid)){
      String host = "www.google.com";
      String path = "/";
      if(!modem.tcp_connected(tcp1.clientID)){
        if(modem.tcp_connect_ssl(tcp1.cid,tcp1.ssl_cid,tcp1.clientID, host,443)){
          String request = "GET " + path + " HTTP/1.1\r\n" +
          "Host: " + host + "\r\n" +
          "Cache-Control: no-cache\r\n" +
          "Connection: close\r\n\r\n";

          if(!modem.tcp_send(tcp1.clientID,(char*)request.c_str(),request.length()))
          Serial.printf("failure doing http request: %s \n",request.c_str());

        }else Serial.printf("Connection to %s has failed \n",host.c_str());
      }else Serial.println("Socket is open");

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
