# esp32-bg95

## Description
package to communicate with BG95 modem

## Functionalities

- APN (multi connection)
- NTP (clock sync at each hour)
- SMS
- MQTT (multi connection)
- TCP
- HTTP to develop ..

## Implementation
The library will create a hierarchy of folders and files like unix filesystem.
A folder will only be deleted if has no files inside. To delete the files inside you need to
pass the level of depth that you want to delete those files.
Folders cannot contain "." in the name and files are mandatory to contain "." in the name

## Public Methods

### TCP

### MQTT

[bool tcp_connect(uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait = 80000)](#TCP-connect-1)
[bool tcp_connect(uint8_t contextID, uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait = 80000)](#TCP-connect-2)
[bool tcp_connected(uint8_t cid)](#TCP-connected)
[bool tcp_close(uint8_t cid)](#TCP-close)
[bool tcp_send(uint8_t cid, uint8_t *data, uint16_t size)](#TCP-send)
[uint16_t tcp_recv(uint8_t cid, uint8_t *data, uint16_t size)](#TCP-recv)
[uint16_t tcp_has_data(uint8_t cid)](#TCP-has-data)

[void MQTT_init(bool(*callback)(String topic,String payload))](#MQTT-init)
[bool MQTT_setup(uint8_t clientID, uint8_t contextID, String will_topic, String payload)](#MQTT-setup)
[bool MQTT_connect(uint8_t clientID, const char* uid, const char* user, const char* pass, const char* host, uint16_t port = 1883)](#MQTT-connect)
[bool MQTT_connected(uint8_t clientID)](#MQTT-connected)
[int8_t MQTT_disconnect(uint8_t clientID)](#MQTT-disconnect)
[bool MQTT_subscribeTopic(uint8_t clientID, uint16_t msg_id, String topic,uint8_t qos)](#MQTT-subscribeTopic)
[bool MQTT_subscribeTopics(uint8_t clientID, uint16_t msg_id, String topic[],uint8_t qos[], uint8_t len)](#MQTT-subscribeTopics)
[int8_t MQTT_unSubscribeTopic(uint8_t clientID, uint16_t msg_id, String topic[], uint8_t len)](#MQTT-unSubscribeTopic)
[int8_t MQTT_publish(uint8_t clientID, uint16_t msg_id,uint8_t qos, uint8_t retain, String topic, String msg)](#MQTT-publish)
[void MQTT_readAllBuffers(uint8_t clientID)](#MQTT-readAllBuffers)


## Examples
  Run programs inside examples folder to check how it works

### demo mqtt
  Establish connection to one or multiple broker, subscribes topics and receive messages (add publish messages to the example)
  Multiples contexts can also be tested

### demo tcp
  Establish connection to one or multiple hosts, make a request, receive response
  Multiples contexts can also be tested

## Unit Test with Arduino
  Not available for now
### unitTest
  Not available for now

## Public Methods - Extension

### TCP connect 1
* connect to a host:port
*
* @clientID - connection id 1-11, yet it is limited to MAX_TCP_CONNECTIONS
* @proto - "UDP" or "TCP"
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
```
bool MODEMBGXX::tcp_connect(uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait)
```

### TCP connect 2
* connect to a host:port
*
* @ccontextID - context id 1-16, yet it is limited to MAX_CONNECTIONS
* @clientID - connection id 1-11, yet it is limited to MAX_TCP_CONNECTIONS
* @proto - "UDP" or "TCP"
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
```
bool MODEMBGXX::tcp_connect(uint8_t contextID, uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait)
```

### TCP connected
* return tcp connection status
```
bool MODEMBGXX::tcp_connected(uint8_t clientID)
```

### TCP close
* close tcp connection
*
* return tcp connection status
```
bool MODEMBGXX::tcp_close(uint8_t clientID)
```

### TCP send
* send data through open channel
*
* returns true if succeed
```
bool MODEMBGXX::tcp_send(uint8_t clientID, uint8_t *data, uint16_t size)
```

### TCP recv
* copies data to pointer if available
*
* returns len of data copied
```
uint16_t MODEMBGXX::tcp_recv(uint8_t clientID, uint8_t *data, uint16_t size)
```

### TCP has data
* returns len of data available for clientID
```
uint16_t MODEMBGXX::tcp_has_data(uint8_t clientID)
```

### MQTT init
* init mqtt
*
* @callback - register callback to parse mqtt messages
```
void MODEMBGXX::MQTT_init(bool(*callback)(String,String))
```

### MQTT setup
* setup mqtt
*
* @clientID - supports 5 clients, yet is limited to MAX_MQTT_CONNECTIONS
* @contextID - index of TCP tcp[] - choose 1 connection
* @will_topic - topic to be sent if mqtt loses connection
* @payload - payload to be sent with will topic
*
* returns true if configuration was succeed
```
bool MODEMBGXX::MQTT_setup(uint8_t clientID, uint8_t contextID, String will_topic, String payload)
```

### MQTT connect
* Connects to a mqtt broker
*
* @clientID: 0-5, limited to MAX_CONNECTIONS
* @uid: id to register device on broker
* @uid: uid to register device on broker
* @user: credential
* @pass: credential
* @host: DNS or IP
* @port: default 1883
*
* return true if connection is open
```
bool MODEMBGXX::MQTT_connect(uint8_t clientID, const char* uid, const char* user, const char* pass, const char* host, uint16_t port)
```

### MQTT connected
* return true if connection is open
```
bool MODEMBGXX::MQTT_connected(uint8_t clientID)
```

### MQTT disconnect
* 0 Failed to close connection
*	1 Connection closed successfully
```
int8_t MODEMBGXX::MQTT_disconnect(uint8_t clientID)
```

### MQTT subscribeTopic
* return true if has succeed
```
bool MODEMBGXX::MQTT_subscribeTopic(uint8_t clientID, uint16_t msg_id, String topic,uint8_t qos)
```

### MQTT subscribeTopics
* return true if has succeed
```
bool MODEMBGXX::MQTT_subscribeTopics(uint8_t clientID, uint16_t msg_id, String topic[],uint8_t qos[], uint8_t len)
```

### MQTT unSubscribeTopic
* return true if has succeed
```
int8_t MODEMBGXX::MQTT_unSubscribeTopic(uint8_t clientID, uint16_t msg_id, String topic[], uint8_t len)
```

### MQTT publish
*	return
*	-1 error
*	0 Packet sent successfully and ACK received from server (message that published when <qos>=0 does not require ACK)
*	1 Packet retransmission
*	2 Failed to send packet
```
int8_t MODEMBGXX::MQTT_publish(uint8_t clientID, uint16_t msg_id,uint8_t qos, uint8_t retain, String topic, String msg)
```

### MQTT readAllBuffers
* Forces reading data from mqtt modem buffers
* call it only if unsolicited messages are not being processed
```
void MODEMBGXX::MQTT_readAllBuffers(uint8_t clientID)
```
