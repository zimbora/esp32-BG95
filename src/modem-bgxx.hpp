#ifndef MODEM_BGXX_H
#define MODEM_BGXX_H

#include <Arduino.h>
#include <Time.h>
#include <TimeLib.h>

#define GSM 											1
#define GPRS 											2
#define NB 												3
#define CATM1 										4
#define AUTO 											5

#define NOT_REGISTERED						0
#define REGISTERED								1
#define CONNETING									2
#define DENIED										3
#define UNKNOW										4
#define ROAMING										4

#define MQTT_STATE_DISCONNECTED 	0
#define MQTT_STATE_INITIALIZING 	1
#define MQTT_STATE_CONNECTING 		2
#define MQTT_STATE_CONNECTED 			3
#define MQTT_STATE_DISCONNECTING 	4

// CONSTANTS
#define   AT_WAIT_RESPONSE      	10 // milis
#define   AT_TERMINATOR     			'\r\n'
#define   MAX_CONNECTIONS       	4
#define   MAX_TCP_CONNECTIONS     2
#define   MAX_MQTT_CONNECTIONS    2
#define   CONNECTION_BUFFER    		650 // bytes !! Do not decrease this length, FOTA will fail otherwise
//#define   CONNECTION_BUFFER    		1500 // it works with freeRTOS - check with ESP32 smartboard
#define   CONNECTION_STATE   			10000 // millis
#define   SMS_CHECK_INTERVAL 			30000 // milli

#define   DEBUG_BG95
#define   DEBUG_BG95_HIGH

#define MAX_SMS 10

struct SMS {
	bool    used   	= false;
	uint8_t index  	= 0;
	char origin[20] = "";
	char msg[256]    = "";
};

// configurations
struct Modem {
	uint8_t pwkey;
	bool ready;
	bool did_config;
	bool sim_ready;
	uint8_t radio;
	uint16_t cops;
	bool force;
	char tech_string[16];
	uint8_t technology;
};

struct APN {
	char name[64];
	uint8_t contextID; // context id 1-16
	bool active;
	bool connected;
	char ip[15];
};

struct TCP {
	char server[64];
	uint16_t port;
	uint8_t contextID; // context id 1-16
	uint8_t connectID; // connect id 0-11
	uint8_t socket_state;
	bool active;
	bool connected;
};

struct MQTT {
	char host[64];
	uint8_t contextID; // index for TCP tcp[] 1-16, limited to MAX_CONNECTIONS
	uint8_t clientID; // client id 0-5 (limited to MAX_MQTT_CONNECTIONS)
	uint8_t socket_state;
	bool active;
	bool connected;
};

class MODEMBGXX {
	public:

		// State
		String state;
		// IMEI
		String imei;
		// IP address
		String ip_address;
		// pending texts
		SMS message[MAX_SMS];

		HardwareSerial *log_output = &Serial;
		HardwareSerial *modem = &Serial2;

		MODEMBGXX(){};
		/*
		* @serial_modem - Serial port for modem connection
		*/
		MODEMBGXX(HardwareSerial *serial_modem){
			modem = serial_modem;
		};
		/*
		* @serial_modem - Serial port for modem connection
		* @serial_log - Serial port for logs
		*/
		MODEMBGXX(HardwareSerial *serial_modem, HardwareSerial *serial_log){
			modem = serial_modem;
			log_output = serial_log;
		};

		/*
		* call it to initialize state machine
		*/
		bool init(uint8_t radio, uint8_t cops, uint8_t pwkey);
		/*
		* call it to initialize serial port
		*/
		void init_port(uint32_t baudrate, uint32_t config);
		/*
		* call it to disable serial port
		*/
		void disable_port();
		/*
		* switch off and on
		*/
		bool powerCycle();
		/*
		* setup APN configuration
		*
		* @cid - 0-16, limited to MAX_CONNECTIONS
		*/
		bool setup(uint8_t cid, String apn, String username, String password);
		//
		/*
		* check for pending commands and received data
		*/
		bool loop(uint32_t loop = 10);

		// --- MODEM static registered numbers ---
		String get_imei(uint32_t wait = 5000);
		String get_ccid(uint32_t wait = 5000);
		String get_imsi(uint32_t wait = 5000);
		String get_subscriber_number(uint16_t wait = 3000);

		int16_t rssi(); // return last read value

		// --- CONTEXT ---
		String get_ip(uint8_t cid = 1, uint32_t wait = 5000);
		/*
		* check if modem is connected to apn
		*/
		bool apn_connected(uint8_t cid = 1);
		/*
		* check if modem has IP
		*/
		bool has_context(uint8_t cid = 1);

		/*
		* open context
		*/
		bool open_pdp_context(uint8_t cid = 1);
		/*
		* close context
		*/
		bool close_pdp_context(uint8_t cid = 1);
		/*
		* returns the state of context id
		*/
		String check_context_state(uint8_t contextID);
		/*
		* returns the state of connection id
		*/
		String check_connection_state(uint8_t connectionID);

		// --- SMS ---
		/*
		* check if there is some function to deal with sms
		*/
		bool sms_handler();
		/*
		* pass callback for sms
		*/
		bool sms_handler(void(*handler)(uint8_t, String, String));
		/*
		* send sms
		*/
		bool sms_send(String origin, String message);
		/*
		* remove sms
		*/
		bool sms_remove(uint8_t index);

		// --- TCP ---
		bool tcp_connect(uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait = 10000);
		bool tcp_connect(uint8_t contextID, uint8_t clientID, String proto, String host, uint16_t port, uint16_t wait = 10000);
		bool tcp_connected(uint8_t cid);
		bool tcp_close(uint8_t cid);
		bool tcp_send(uint8_t cid, uint8_t *data, uint16_t size);
		uint16_t tcp_recv(uint8_t cid, uint8_t *data, uint16_t size);
		uint16_t tcp_has_data(uint8_t cid);

		// --- CLOCK ---
		/*
		* use it to get network clock
		*/
		bool get_clock(tm* t); // get unix timestamp
		/*
		* get timezone difference
		*/
		int32_t get_tz();

		// --- LOCATION ---
		/*
		* get info from near cells
		*/
		String scan_cells();
		/*
		* get gps position
		*/
		String get_position();

		// --- MQTT ---
    void MQTT_init(bool(*callback)(uint8_t clientID, String topic,String payload));
    bool MQTT_setup(uint8_t clientID, uint8_t contextID, String will_topic, String payload);
    bool MQTT_connect(uint8_t clientID, const char* uid, const char* user, const char* pass, const char* host, uint16_t port = 1883);
    bool MQTT_connected(uint8_t clientID);
    int8_t MQTT_disconnect(uint8_t clientID);
    bool MQTT_subscribeTopic(uint8_t clientID, uint16_t msg_id, String topic,uint8_t qos);
    bool MQTT_subscribeTopics(uint8_t clientID, uint16_t msg_id, String topic[],uint8_t qos[], uint8_t len);
    int8_t MQTT_unSubscribeTopic(uint8_t clientID, uint16_t msg_id, String topic[], uint8_t len);
    int8_t MQTT_publish(uint8_t clientID, uint16_t msg_id,uint8_t qos, uint8_t retain, String topic, String msg);
		void MQTT_readAllBuffers(uint8_t clientID);

		void log_status();
	private:

		int32_t tz = 0;

		uint8_t cereg; // Unsolicited LTE commands
		uint8_t cgreg; // Unsolicited GPRS commands

		// --- TCP ---
		// size of each buffer
		uint16_t buffer_len[MAX_TCP_CONNECTIONS];
		// data pending of each connection
		bool data_pending[MAX_TCP_CONNECTIONS];
		// validity of each connection state
		uint32_t connected_until[MAX_TCP_CONNECTIONS];
		// last connection start
		uint32_t connected_since[MAX_TCP_CONNECTIONS];
		// data buffer for each connection
		char buffers[MAX_TCP_CONNECTIONS][CONNECTION_BUFFER];
		// --- --- ---

		uint32_t rssi_until = 20000;
		uint32_t loop_until = 0;
		uint32_t join_until = 5000;
		uint32_t ready_until = 15000;

		// last rssi
		int16_t rssi_last = 99;

		// validity of sms check
		uint32_t sms_until = 60000;

		void (*sms_handler_func)(uint8_t, String, String) = NULL;

		/*
		* check if modem is ready (if it's listening for AT commands)
		*/
		bool ready();
		/*
		* wait for at response
		*/
		bool wait_modem_to_init();
		/*
		* switch modem on
		*/
		bool switchOn();
		/*
		* switch modem off
		*/
		bool switch_radio_off();
		/*
		* increase time for next modem reboot
		*/
		void increase_modem_reboot();
		/*
		* register on network
		*/
		bool enable_pdp(uint8_t cid);
		/*
		* deregister from network
		*/
		bool disable_pdp(uint8_t cid);
		/*
		* reset state machine
		*/
		bool reset();
		/*
		* configure base settings like ECHO mode and multiplex, check for sim card
		*/
		bool config();
		/*
		* configure base settings like ECHO mode and multiplex
		*/
		bool configure_radio_mode(uint8_t radio,uint8_t cops);


		// --- TCP ---
		void 			tcp_check_data_pending();
		void 			tcp_read_buffer(uint8_t index, uint16_t wait = 100);

		// --- NETWORK STATE ---
		int16_t 	get_rssi();
		void 			get_state(); // get network state
		int8_t 		get_actual_mode();

		// --- CLOCK ---
		void 			sync_clock_ntp(bool force = false); // private

		void 			check_commands();

		// --- MQTT ---
    bool MQTT_open(uint8_t clientID, const char* host, uint16_t port);
    bool MQTT_isOpened(uint8_t clientID, const char* host, uint16_t port);
    bool MQTT_close(uint8_t clientID);
		void MQTT_checkConnection();
    void MQTT_readMessages(uint8_t clientID);

		// check for new SMS messages
		void check_sms();

		// process pending SMS messages
		void process_sms(uint8_t index);

		//void check_modem_buffers();
		String check_messages();

		String parse_command_line(String line, bool set_data_pending = true);
		void read_data(uint8_t index, String command, uint16_t bytes);

		// run a command and check if it matches an OK or ERROR result String
		bool check_command(String command, String ok_result, uint32_t wait = 5000);
		bool check_command(String command, String ok_result, String error_result, uint32_t wait = 5000);
		bool check_command_no_ok(String command, String ok_result, uint32_t wait = 5000);
		bool check_command_no_ok(String command, String ok_result, String error_result, uint32_t wait = 5000);

		// send a command (or data for that matter)
		void send_command(uint8_t *command, uint16_t size);
		void send_command(String command, bool mute = false);

		String get_command(String command, uint32_t timeout = 300);
		String get_command(String command, String filter, uint32_t timeout = 300);
		String get_command_critical(String command, String filter, uint32_t timeout = 300);
		String get_command_no_ok(String command, String filter, uint32_t timeout = 300);
		String get_command_no_ok_critical(String command, String filter, uint32_t timeout = 300);
		String mqtt_message_received(String line);

		bool wait_command(String command, uint32_t timeout = 300);

		// debugging
		void log(String text);
		String date();
		String pad2(int value);
		boolean isNumeric(String str);
};

#endif
