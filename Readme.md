# esp32-BG95

## Description
package to communicate with BG95 modem

## Functionalities

- Radio (GSM/GPRS/NB-IoT/CATM1) - operator selection
- APN (multi connection)
- NTP (clock sync at each hour)
- GNSS
- SMS
- MQTT (multi connection)
- TCP (multi connection + SSL)
- HTTP (GET requests)
- HTTPS (GET and Post requests (supporting json also))
- COAP (not implemented for now)
- LwM2M (not implemented for now)
- DFOTA (to develop..)
- PSM (not implemented for now)

## Implementation
Library to interact with BGxx enabling functionalities listed above

## Public Methods

- [MODEMBGXX()](#Constructor-1)
- [MODEMBGXX(HardwareSerial *serial_modem)](#Constructor-2)
- [MODEMBGXX(HardwareSerial *serial_modem, HardwareSerial *serial_log)](#Constructor-3)
- [bool init(uint8_t radio, uint16_t cops, uint8_t pwkey)](#Init)
- [void init_port(uint32_t baudrate, uint32_t config)](#Init-port)
- [void disable_port()](#Disable-port)
- [bool powerCycle()](#PowerCycle)
- [bool setup(uint8_t cid, String apn, String username, String password)](#Setup)
- [bool loop(uint32_t loop = 10)](#Loop)

### Info

- [String get_imei(uint32_t wait = 5000)](#Info-get-imei)
- [String get_ccid(uint32_t wait = 5000)](#Info-get-ccid)
- [String get_imsi(uint32_t wait = 5000)](#Info-get-imsi)
- [String get_subscriber_number(uint16_t wait = 3000)](#Info-get-subscribe-number)

### Radio

- add rssi
- technology in use
- operator in use

### APN

- [String get_ip(uint8_t cid = 1, uint32_t wait = 5000)](#APN-get-ip)
- [bool apn_connected(uint8_t cid = 1)](#APN-connected)
- [bool has_context(uint8_t cid = 1)](#APN-has-context)
- [bool open_pdp_context(uint8_t cid = 1)](#APN-open-PDP-context)
- [bool close_pdp_context(uint8_t cid = 1)](#APN-close-PDP-context)
- [String check_context_state(uint8_t contextID)](#APN-check-context-state)
- [String check_connection_state(uint8_t connectionID)](#APN-check-connection-state)

### SMS

- [bool sms_check_handler()](#SMS-check-handler)
- [bool sms_handler(void(*handler)(uint8_t, String, String))](#SMS-handler)
- [bool sms_send(String origin, String message)](#SMS-send)
- [bool sms_remove(uint8_t index)](#SMS-remove)

### TCP

- [bool tcp_connect(uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait = 80000)](#TCP-connect-1)
- [bool tcp_connect(uint8_t contextID, uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait = 80000)](#TCP-connect-2)
- [bool tcp_connect_ssl(uint8_t contextID, uint8_t sslClientID, uint8_t clientID, String host, uint16_t port, uint16_t wait = 10000)](#TCP-connect-3)
- [bool tcp_connected(uint8_t cid)](#TCP-connected)
- [bool tcp_close(uint8_t cid)](#TCP-close)
- [bool tcp_send(uint8_t cid, uint8_t *data, uint16_t size)](#TCP-send)
- [uint16_t tcp_recv(uint8_t cid, uint8_t *data, uint16_t size)](#TCP-recv)
- [uint16_t tcp_has_data(uint8_t cid)](#TCP-has-data)

### MQTT

- [void MQTT_init(bool(*callback)(String topic,String payload))](#MQTT-init)
- [bool MQTT_setup(uint8_t clientID, uint8_t contextID, String will_topic, String payload)](#MQTT-setup)
- [bool MQTT_connect(uint8_t clientID, const char* uid, const char* user, const char* pass, const char* host, uint16_t port = 1883)](#MQTT-connect)
- [bool MQTT_connected(uint8_t clientID)](#MQTT-connected)
- [int8_t MQTT_disconnect(uint8_t clientID)](#MQTT-disconnect)
- [bool MQTT_subscribeTopic(uint8_t clientID, uint16_t msg_id, String topic,uint8_t qos)](#MQTT-subscribeTopic)
- [bool MQTT_subscribeTopics(uint8_t clientID, uint16_t msg_id, String topic- [],uint8_t qos- [], uint8_t len)](#MQTT-subscribeTopics)
- [int8_t MQTT_unSubscribeTopic(uint8_t clientID, uint16_t msg_id, String topic- [], uint8_t len)](#MQTT-unSubscribeTopic)
- [int8_t MQTT_publish(uint8_t clientID, uint16_t msg_id,uint8_t qos, uint8_t retain, String topic, String msg)](#MQTT-publish)
- [void MQTT_readAllBuffers(uint8_t clientID)](#MQTT-readAllBuffers)

### HTTP
- [bool http_get(String host, String path, String token, uint8_t clientID, uint8_t contextID)](#HTTP-request)
- [bool https_get(String host, String path, uint8_t clientID, String token, uint8_t sslClientID, uint8_t contextID)](#HTTPS-request)
- [bool https_post(String host, String path, String body, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID)](#HTTPS-post)
- [bool https_post_json(String host, String path, String body, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID)](#HTTPS-post-json)
- [bool http_wait_response(uint8_t clientID)](#HTTP-wait-response)
- [uint16_t http_get_header_length(uint8_t clientID)](#HTTP-get-header-length)
- [void http_parse_header(char* data, uint16_t len)](#HTTP-parse-header)
- [String http_response_status()](#HTTP-response-status)
- [uint16_t http_get_body_size()](#HTTP-body-size)
- [uint16_t http_get_body(uint8_t clientID, char* data, uint16_t len, uint16_t wait = 10000)](#HTTP-body)
- [bool http_check_md5(char* data, uint16_t len)](#HTTP-check-mm5)

## Examples
  Run programs inside examples folder to check how it works

### demo mqtt
  Establish connection to one or multiple broker, subscribes topics and receive messages (add publish messages to the example)
  Multiples contexts can also be tested

### demo tcp
  Establish connection to one or multiple hosts, make a request, receive response
  Multiples contexts can also be tested

### demo radio
  Use it to test different radio technologies

## Unit Test with Arduino
  Not available for now
### unitTest
  Not available for now

## Public Methods - Extension

#### Constructor 1
```
MODEMBGXX()]
```

#### Constructor 2
* @serial_modem - Serial port for modem connection
```
MODEMBGXX(HardwareSerial *serial_modem)]
```

#### Constructor 3
* @serial_modem - Serial port for modem connection
* @serial_log - Serial port for logs
```
MODEMBGXX(HardwareSerial *serial_modem, HardwareSerial *serial_log)]
```

#### Init
* call it to initialize state machine
```
bool init(uint8_t radio, uint16_t cops, uint8_t pwkey)]
```

#### Init port
* call it to initialize serial port
```
void init_port(uint32_t baudrate, uint32_t config)]
```

#### Disable port
* call it to disable serial port
```
void disable_port()]
```

#### PowerCycle
* switch off and on
```
bool powerCycle()]
```

#### Setup
* setup APN configuration
*
* @cid - 0-16, limited to MAX_CONNECTIONS
*
* returns true if succeed
```
bool setup(uint8_t cid, String apn, String username, String password)]
```

#### Loop
* check for pending commands, received data and updates state machine
*
* returns true if something has changed on state machine
```
bool loop(uint32_t loop = 10)]
```

### Info

#### Info get imei
```
String get_imei(uint32_t wait = 5000)](
```

#### Info get ccid
```
String get_ccid(uint32_t wait = 5000)](
```

#### Info get imsi
```
String get_imsi(uint32_t wait = 5000)](
```

#### Info get subscribe number
```
String get_subscriber_number(uint16_t wait = 3000)](
```

### Radio


### APN

#### APN get ip
* get IP of a context
```
String get_ip(uint8_t cid = 1, uint32_t wait = 5000)
```

#### APN connected
* check if modem is connected to apn
```
bool apn_connected(uint8_t cid = 1)
```

#### APN has context
* check if modem has IP
```
bool has_context(uint8_t cid = 1)
```

#### APN open PDP context
* open context
```
bool open_pdp_context(uint8_t cid = 1)
```

#### APN close PDP context
* close context
```
bool close_pdp_context(uint8_t cid = 1)
```

#### APN check context state
* returns the state of context id
```
String check_context_state(uint8_t contextID)
```

#### APN check connection state
* returns the state of connection id
```
String check_connection_state(uint8_t connectionID)
```

### SMS

#### SMS check handler
* check if there is some function to deal with sms
```
bool sms_check_handler()
```

#### SMS handler
pass callback for sms
```
bool sms_handler(void(*handler)(uint8_t, String, String))
```

#### SMS send
* send sms
```
bool sms_send(String origin, String message)
```

#### SMS remove
* remove sms
```
bool sms_remove(uint8_t index)
```

### TCP
#### TCP connect 1
* connect to a host:port
*
* @clientID - connection id 0-11, yet it is limited to MAX_TCP_CONNECTIONS
* @proto - "UDP" or "TCP"
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
```
bool MODEMBGXX::tcp_connect(uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait)
```

#### TCP connect 2
* connect to a host:port
*
* @ccontextID - context id 1-16, yet it is limited to MAX_CONNECTIONS
* @clientID - connection id 0-11, yet it is limited to MAX_TCP_CONNECTIONS
* @proto - "UDP" or "TCP"
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
```
bool MODEMBGXX::tcp_connect(uint8_t contextID, uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait)
```

#### TCP connect 3
* connect to a host:port
*
* @ccontextID - context id 1-16, yet it is limited to MAX_CONNECTIONS
* @sslClientID - connection id 0-5, yet it is limited to MAX_TCP_CONNECTIONS
* @clientID - connection id 0-11, yet it is limited to MAX_TCP_CONNECTIONS
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
```
bool MODEMBGXX::tcp_connect_ssl(uint8_t contextID, uint8_t sslClientID, uint8_t clientID, String host, uint16_t port, uint16_t wait)
```


#### TCP connected
* return tcp connection status
```
bool MODEMBGXX::tcp_connected(uint8_t clientID)
```

#### TCP close
* close tcp connection
*
* return tcp connection status
```
bool MODEMBGXX::tcp_close(uint8_t clientID)
```

#### TCP send
* send data through open channel
*
* returns true if succeed
```
bool MODEMBGXX::tcp_send(uint8_t clientID, uint8_t *data, uint16_t size)
```

#### TCP recv
* copies data to pointer if available
*
* returns len of data copied
```
uint16_t MODEMBGXX::tcp_recv(uint8_t clientID, uint8_t *data, uint16_t size)
```

#### TCP has data
* returns len of data available for clientID
```
uint16_t MODEMBGXX::tcp_has_data(uint8_t clientID)
```

### MQTT
#### MQTT init
* init mqtt
*
* @callback - register callback to parse mqtt messages
```
void MODEMBGXX::MQTT_init(bool(*callback)(String,String))
```

#### MQTT setup
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

#### MQTT connect
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

#### MQTT connected
* return true if connection is open
```
bool MODEMBGXX::MQTT_connected(uint8_t clientID)
```

#### MQTT disconnect
* 0 Failed to close connection
*	1 Connection closed successfully
```
int8_t MODEMBGXX::MQTT_disconnect(uint8_t clientID)
```

#### MQTT subscribeTopic
* return true if has succeed
```
bool MODEMBGXX::MQTT_subscribeTopic(uint8_t clientID, uint16_t msg_id, String topic,uint8_t qos)
```

#### MQTT subscribeTopics
* return true if has succeed
```
bool MODEMBGXX::MQTT_subscribeTopics(uint8_t clientID, uint16_t msg_id, String topic[],uint8_t qos[], uint8_t len)
```

#### MQTT unSubscribeTopic
* return true if has succeed
```
int8_t MODEMBGXX::MQTT_unSubscribeTopic(uint8_t clientID, uint16_t msg_id, String topic[], uint8_t len)
```

#### MQTT publish
*	return
*	-1 error
*	0 Packet sent successfully and ACK received from server (message that published when <qos>=0 does not require ACK)
*	1 Packet retransmission
*	2 Failed to send packet
```
int8_t MODEMBGXX::MQTT_publish(uint8_t clientID, uint16_t msg_id,uint8_t qos, uint8_t retain, String topic, String msg)
```

#### MQTT readAllBuffers
* Forces reading data from mqtt modem buffers
* call it only if unsolicited messages are not being processed
```
void MODEMBGXX::MQTT_readAllBuffers(uint8_t clientID)
```

### HTTP

# HTTP request
* sends request
*
* @host - domain
* @path - start path with '/'
* clientID - socket id
* contextID - context to be used
*
* returns true if request has been correctly sent
```
bool http_do_request(String host, String path, uint8_t clientID, uint8_t contextID)
```

# HTTPS request
* sends https request
*
* @host - domain
* @path - start path with '/'
* @clientID - socket id
* @sslClientID - socket ssl id
* @contextID - context to be used
*
* returns true if request has been correctly sent
```
bool https_do_request(String host, String path, uint8_t clientID, uint8_t sslClientID, uint8_t contextID)
```

# HTTPS post
* sends https post
*
* @host - domain
* @path - start path with '/'
* @body -
* @token - pair of key words to be added to header
* @clientID - socket id
* @sslClientID - socket ssl id
* @contextID - context to be used
*
* returns true if request has been correctly sent
```
bool https_post(String host, String path, String body, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID)
```

# HTTPS post json
* sends https post in json format
*
* @host - domain
* @path - start path with '/'
* @body -
* @token - pair of key words to be added to header
* @clientID - socket id
* @sslClientID - socket ssl id
* @contextID - context to be used
*
* returns true if request has been correctly sent
```
bool https_post_json(String host, String path, String body, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID)
```

# HTTP wait response
* After request has been sent, it waits for response and parse header
*
* returns true if header was detected and parsed thereafter
```
bool http_wait_response(uint8_t clientID)
```

# HTTP get header length

```
uint16_t http_get_header_length(uint8_t clientID)
```

# HTTP parse header

```
void http_parse_header(char* data, uint16_t len)
```

# HTTP response status
* return http response result
```
String http_response_status()
```

# HTTP body size
* returns body size to be read
```
uint16_t http_get_body_size()
```

# HTTP body
* gets body data
```
uint16_t http_get_body(uint8_t clientID, char* data, uint16_t len, uint16_t wait = 10000)
```

# HTTP check md5
* check md5 file (Content-MD5: has to be received on header),
* otherwise it will returns false
```
bool http_check_md5(char* data, uint16_t len)
```
