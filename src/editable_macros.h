
//#define DEBUG_BG95 // define it for debug
//#define DEBUG_BG95_HIGH // define it for high debug

#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS				  4
#endif
#ifndef MAX_TCP_CONNECTIONS
#define MAX_TCP_CONNECTIONS			2
#endif
#ifndef MAX_MQTT_CONNECTIONS
#define MAX_MQTT_CONNECTIONS		2
#endif
#ifndef CONNECTION_BUFFER
#define CONNECTION_BUFFER			  650 // bytes
#endif
#ifndef CONNECTION_STATE
#define CONNECTION_STATE			  10000 // millis
#endif
#ifndef SMS_CHECK_INTERVAL
#define SMS_CHECK_INTERVAL			30000 // milli
#endif
#ifndef MQTT_RECV_MODE
#define MQTT_RECV_MODE				  1 // 0 - for urc containing payload; 1 - to use buffers
#endif
