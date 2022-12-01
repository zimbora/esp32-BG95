#include "esp32-BG95.hpp"

bool (*parseMQTTmessage)(uint8_t,String,String);
void(*tcpOnClose)(uint8_t clientID);

void MODEMBGXX::init_port(uint32_t baudrate, uint32_t serial_config) {
	init_port(baudrate, serial_config, 16, 17);
}

void MODEMBGXX::init_port(uint32_t baudrate, uint32_t serial_config, uint8_t rx_pin, uint8_t tx_pin) {

	//modem->begin(baudrate);
	modem->begin(baudrate,serial_config, rx_pin, tx_pin);
	#ifdef DEBUG_BG95
	log("modem bus inited");
	#endif
	while (modem->available()) {
		String command = modem->readStringUntil(AT_TERMINATOR);

		command.trim();

		if (command.length() > 0) {
			#ifdef DEBUG_BG95
			log("[init port] ignoring '" + command + "'");
			#endif
		}
	}
}

void MODEMBGXX::disable_port() {
	modem->end();
}

bool MODEMBGXX::init(uint8_t radio, uint16_t cops, uint8_t pwkey){

	op.pwkey = pwkey;
	pinMode(pwkey, OUTPUT);

	op.radio = radio;
	op.cops = cops;

	if(!powerCycle())
		return false;

	if(!config())
		return false;

	if(!configure_radio_mode(radio,cops))
		return false;

	return true;
}

bool MODEMBGXX::apn_connected(uint8_t contextID) {
	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	return apn[contextID-1].connected;
}

bool MODEMBGXX::has_context(uint8_t contextID) {
	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	//return String(apn[contextID-1].ip) != "";
	return apn[contextID-1].connected;
}

bool MODEMBGXX::ready() {

	if(ready_until > millis())
		return op.ready;

	ready_until = millis() + 2000;

	uint32_t t = millis() + 3000;

	while (t > millis()) {
		if (check_command("AT", "OK", "ERROR", 1000)) {
			op.ready = true;
			return op.ready;
		}
		delay(1000);
	}

	op.ready = false;
	return op.ready;
}

bool MODEMBGXX::wait_modem_to_init(){
	if(wait_command("RDY",10000)){
		reset();
		if(wait_command("APP RDY",10000)){
			increase_modem_reboot();
			return true;
		}
	}
	return false;
}

bool MODEMBGXX::switchOn(){

	#ifdef DEBUG_BG95
	log("switching modem");
	#endif
	digitalWrite(op.pwkey, HIGH);
	delay(2000);

	digitalWrite(op.pwkey, LOW);

	wait_modem_to_init();
}

void MODEMBGXX::increase_modem_reboot(){
	next_retry = millis() + 5*60*1000; // 15 minutes
}

bool MODEMBGXX::powerCycle(){

	#ifdef DEBUG_BG95
	log("power cycle modem");
	#endif
	digitalWrite(op.pwkey, HIGH);
	delay(2000);

	digitalWrite(op.pwkey, LOW);

	if(!wait_modem_to_init()){

		#ifdef DEBUG_BG95
		log("power cycle modem");
		#endif
		digitalWrite(op.pwkey, HIGH);
		delay(2000);

		digitalWrite(op.pwkey, LOW);

		return wait_modem_to_init();
	}

	return true;
}

bool MODEMBGXX::reset() {

	// APN reset
	op.ready = false;

	for (uint8_t i = 0; i < MAX_CONNECTIONS; i++) {
		data_pending[i]    = false;
		apn[i].connected = false;
	}
	for (uint8_t i = 0; i < MAX_TCP_CONNECTIONS; i++) {
		tcp[i].connected = false;
	}
	for (uint8_t i = 0; i < MAX_MQTT_CONNECTIONS; i++) {
		mqtt[i].connected = false;
	}

	return true;
}

bool MODEMBGXX::setup(uint8_t cid, String apn_, String username, String password) {

	if(cid == 0 || cid > MAX_CONNECTIONS)
		return false;

	apn[cid-1].active = true;
	apn[cid-1].retry = 0;

	if (!op.did_config){
		config();
	}

	memset(apn[cid-1].name,0,sizeof(apn[cid].name));
	memcpy(apn[cid-1].name,apn_.c_str(),apn_.length());

	return check_command("AT+QICSGP="+String(cid)+",1,\"" + apn_ + "\",\"" + username + "\",\"" + password + "\"", "OK", "ERROR");  // replacing CGDCONT
}

bool MODEMBGXX::set_ssl(uint8_t ssl_cid){

	if(!check_command("AT+QSSLCFG=\"sslversion\","+String(ssl_cid)+",4","OK","ERROR")) // allow all
	//if(!check_command("AT+QSSLCFG=\"sslversion\","+String(ssl_cid)+",1","OK","ERROR")) // TLS v3.0
		return false;

	if(!check_command("AT+QSSLCFG=\"ciphersuite\","+String(ssl_cid)+",0XFFFF","OK","ERROR")) // support all
	//if(!check_command("AT+QSSLCFG=\"ciphersuite\","+String(ssl_cid)+",0X0035","OK","ERROR")) // TLS_RSA_WITH_AES_256_CBC_SHA
		return false;

	//if(!check_command("AT+QSSLCFG=\"seclevel\","+String(ssl_cid)+",1","OK","ERROR")) // Verify server certificate
	if(!check_command("AT+QSSLCFG=\"seclevel\","+String(ssl_cid)+",0","OK","ERROR")) //Don't verify server certificate
		return false;

	if(!check_command("AT+QSSLCFG=\"SNI\","+String(ssl_cid)+",1","OK","ERROR")) //set SNI (required for some servers hosting multiple domains/certs on a single IP)
		return false;

	if(!check_command("AT+QSSLCFG=\"cacert\","+String(ssl_cid)+",\"cacert.pem\"","OK","ERROR")) //Not needed for seclevel 0
		return false;

	return true;
}

bool MODEMBGXX::loop(uint32_t wait) {

	check_messages();

	tcp_check_data_pending();

	if(loop_until < millis()){

		get_state();

		MQTT_checkConnection();

		loop_until = millis()+wait;

		sync_clock_ntp();

		return true;
	}


	return false;
}

void MODEMBGXX::get_state() {

	String state = "";

	check_command("AT+CREG?","OK","ERROR",3000); // both

	get_command("AT+QIACT?",15000);

	get_rssi();

	return;
}

int8_t MODEMBGXX::get_actual_mode(){
	return op.technology;
}

void MODEMBGXX::log_status() {

	//log("imei: "+get_imei());
	//log("ccid: "+get_ccid());
	//log("has context: "+String(has_context()));
	log_output->println();
	log("--- MODEM STATE ---");
	log("technology: "+String(op.tech_string));
	log("rssi: "+String(rssi()));

	for(uint8_t i = 0; i<MAX_CONNECTIONS; i++){
		if(!apn[i].active)
			continue;
		if(apn[i].connected)
			log("apn name: "+String(apn[i].name) + " connected with ip: "+String(apn[i].ip));
		else
			log("apn name: "+String(apn[i].name) + " disconnected");
	}

	for(uint8_t i = 0; i<MAX_TCP_CONNECTIONS; i++){
		if(!tcp[i].active)
			continue;
		if(tcp[i].connected)
			log("tcp server: "+ String(tcp[i].server) +":"+String(tcp[i].port)+ " connected");
		else
			log("tcp server: "+ String(tcp[i].server) +":"+String(tcp[i].port)+ " disconnected");
	}

	for(uint8_t i = 0; i<MAX_MQTT_CONNECTIONS; i++){
		if(!mqtt[i].active)
			continue;
		if(mqtt[i].connected)
			log("mqtt host: "+ String(mqtt[i].host) + " connected");
		else
			log("mqtt host: "+ String(mqtt[i].host) + " disconnected");
	}
	log_output->println();

	return;
}

bool MODEMBGXX::config() {

	if(!op.did_config){

		if (!check_command("ATE0", "OK", "ERROR")) return false;
		#ifdef DEBUG_BG95
		log("[config] echo mode off");
		#endif

		check_command("AT+CREG=2","OK","ERROR",3000); //

		//check_command("AT+QCFG=\"band\"","OK","ERROR",3000); //

		get_imei();
		#ifdef DEBUG_BG95
		log("[config] imei: " + imei);
		#endif
		delay(5000);

		check_command("AT+CSCS=\"IRA\"", "OK");

		if (check_command("AT+CMGF=1", "OK", "ERROR")){
			#ifdef DEBUG_BG95
			log("[config] sms text mode on");
			#endif
		}else log("couldn't configure sms");

		op.did_config = true;
	}


	if(get_ccid() == ""){
		log("no sim card detected");
		return false;
	}

	op.sim_ready = true;

	get_imsi();

	return true;
}

/*
* configure radio technology and operator
*
* @radio - selects radio technology
* @cops - operator selection (if 0 uses auto mode)
* @force - if false and cops define, modem will enter in auto mode otherwise will try only the selected radio
*
* returns true if succeed
*/
bool MODEMBGXX::configure_radio_mode(uint8_t radio,uint16_t cops, bool force){

	uint8_t mode = 0;

	if(force) mode = 1;
	else mode = 4;

	if(op.radio == GPRS && op.technology != GPRS){
		if(cops != 0){
			if(!check_command("AT+COPS="+String(mode)+",2,\""+String(cops)+"\",0","OK","ERROR",45000))
				return false;
		}else{
			//check_command("AT+QCFG=\"iotopmode\",0,1","OK",3000);
			check_command("AT+QCFG=\"nwscanmode\",1,1","OK",3000);
		}
	}else if(op.radio == NB && op.technology != NB){
		if(cops != 0){
			if(!check_command("AT+COPS="+String(mode)+",2,\""+String(cops)+"\",9","OK","ERROR",45000))
			return false;
		}else{
			check_command("AT+QCFG=\"iotopmode\",1,1","OK",3000);
			check_command("AT+QCFG=\"nwscanmode\",0,1","OK",3000);
		}
	}else if(op.radio == CATM1 && op.technology != CATM1){
		if(cops!=0){
			//if(!check_command("AT+COPS=1,2,"+String(cops)+",8","OK","ERROR",45000)){
			if(!check_command("AT+COPS="+String(mode)+",2,\""+String(cops)+"\",8","OK","ERROR",45000))
				return false;
		}else{
			check_command("AT+QCFG=\"iotopmode\",0,1","OK",3000);
			check_command("AT+QCFG=\"nwscanmode\",0,1","OK",3000);
		}
	}else if(op.radio == AUTO){
		if(cops != 0){
			if(!check_command("AT+COPS="+String(mode)+",2,\""+String(cops)+"\"","OK","ERROR",45000))
				return false;
		}else{
			check_command("AT+QCFG=\"iotopmode\",2,1","OK",3000);
			check_command("AT+QCFG=\"nwscanmode\",0,1","OK",3000);
		}
	}
	else return false;

	return true;
}

// --- SMS ---
void MODEMBGXX::check_sms() {
	send_command("AT+CMGL=\"ALL\"");
	//send_command("AT+CMGL=\"REC UNREAD\"");
	delay(AT_WAIT_RESPONSE);

	String   sms[7]; // index, status, origin, phonebook, date, time, msg
	//uint8_t  index     = 0;
	uint32_t timeout   = millis() + 10000;
	bool     found_sms = false;
	uint8_t counter = 0;

	while (timeout >= millis()) {
		if (modem->available()) {
			String ret = modem->readStringUntil(AT_TERMINATOR);

			ret.trim();
			if (ret.length() == 0) continue;
			if (ret == "OK") break;
			if (ret == "ERROR") break;

			log("[sms] '" + ret + "'");

			String index = "", msg_state = "", origin = "", msg = "";

			if (!ret.startsWith("+CMGL:")) {
				parse_command_line(ret, true);
			}

			/*
			if (found_sms) continue;

			found_sms = true;
			*/
			ret = ret.substring(6);
			ret.trim();

			uint8_t word = 0, last_i = 0;
			for (uint8_t i = 0; i < ret.length(); i++) {
				if (ret.charAt(i) == ',') {
					switch (word){
						case 0:
							message[counter].used   = true;
							index = ret.substring(0,i);
							#ifdef DEBUG_BG95
							log("index: "+index);
							#endif
							message[counter].index  = (uint8_t) index.toInt();
							last_i = i;
							break;
						case 1:
							msg_state = ret.substring(last_i,i);
							#ifdef DEBUG_BG95
							log("msg_state: "+msg_state);
							#endif
							last_i = i;
							break;
						case 2:
							origin = ret.substring(last_i,i);
							origin.replace("\"","");
							origin.replace(",","");
							//origin.replace("+","");
							#ifdef DEBUG_BG95
							log("origin: "+origin);
							#endif
							memcpy(message[counter].origin,origin.c_str(),i-last_i);
							last_i = i;
							break;
						default:
							break;
					}
					word++;
				}
			}

			if (modem->available()) {
				String ret = modem->readStringUntil(AT_TERMINATOR);
				ret.trim();
				msg = ret;
				#ifdef DEBUG_BG95
				log("msg: "+msg);
				#endif
				memcpy(message[counter].msg,msg.c_str(),ret.length());
			}

			if(counter < MAX_SMS-1)
				counter++;
			else
				continue;

		} else {
			delay(AT_WAIT_RESPONSE);
		}
	}

	for(uint8_t i = 0; i<counter; i++){
		process_sms(i);
		message[i].used = false;
		memset(message[i].origin,0,20);
		memset(message[i].msg,0,256);
	}
	return;
}

void MODEMBGXX::process_sms(uint8_t index) {
	if (message[index].used && message[index].index >= 0) {
		message[index].used = false;

		sms_handler_func(message[index].index, String(message[index].origin), String(message[index].msg));
	}
}

bool MODEMBGXX::sms_send(String origin, String message) {
	#ifdef DEBUG_BG95
	log("[sms] sending..");
	#endif
	// clean buffer
	while(modem->available())
		modem->read();

	if (!check_command_no_ok("AT+CMGS=\"" + origin + "\"", ">", "ERROR")) return false;
	if (!check_command(message + "\x1A", "OK", "ERROR")) return false;

	#ifdef DEBUG_BG95
	log("[sms] sent");
	#endif
	return true;
}

bool MODEMBGXX::sms_remove(uint8_t index) {
	#ifdef DEBUG_BG95
	log("[sms] removing " + String(index) + "..");
	#endif
	if (!check_command("AT+CMGD=" + String(index), "OK", "ERROR", 7000)) return false;
	#ifdef DEBUG_BG95
	log("[sms] removed");
	#endif
	return true;
}

bool MODEMBGXX::sms_check_handler() {
	return (sms_handler_func != NULL);
}

bool MODEMBGXX::sms_handler(void(*handler)(uint8_t, String, String)) {
	if (sms_check_handler()) return false;

	sms_handler_func = handler;

	return true;
}

// --- --- ---

// --- TCP ---

void MODEMBGXX::tcp_set_callback_on_close(void(*callback)(uint8_t clientID)){

	tcpOnClose = callback;
}
/*
* connect to a host:port
*
* @clientID - connection id 1-11, yet it is limited to MAX_TCP_CONNECTIONS
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
*/
bool MODEMBGXX::tcp_connect(uint8_t clientID, String host, uint16_t port, uint16_t wait) {

	uint8_t contextID = 1;
	if(apn_connected(contextID) != 1)
		return false;

	if (clientID >= MAX_TCP_CONNECTIONS) return false;

	memset(tcp[clientID].server,0,sizeof(tcp[clientID].server));
	memcpy(tcp[clientID].server,host.c_str(),host.length());
	tcp[clientID].port = port;
	tcp[clientID].active = true;
	tcp[clientID].ssl = false;

	if(check_command_no_ok("AT+QIOPEN="+String(contextID)+","+String(clientID) + ",\"TCP\",\"" + host + "\","
	+ String(port),"+QIOPEN: "+String(clientID)+",0","ERROR"),wait){
		tcp[clientID].connected = true;
		return true;
	}else tcp_close(clientID);

	get_command("AT+QIGETERROR");

	return false;
}

/*
* connect to a host:port
*
* @ccontextID - context id 1-16, yet it is limited to MAX_CONNECTIONS
* @clientID - connection id 0-11, yet it is limited to MAX_TCP_CONNECTIONS
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
*/
bool MODEMBGXX::tcp_connect(uint8_t contextID, uint8_t clientID, String host, uint16_t port, uint16_t wait) {
	if(apn_connected(contextID) != 1)
		return false;

	if (contextID == 0 || contextID > MAX_CONNECTIONS) return false;

	if (clientID >= MAX_TCP_CONNECTIONS) return false;

	memset(tcp[clientID].server,0,sizeof(tcp[clientID].server));
	memcpy(tcp[clientID].server,host.c_str(),host.length());
	tcp[clientID].port = port;
	tcp[clientID].active = true;
	tcp[clientID].ssl = false;

	if(check_command_no_ok("AT+QIOPEN="+String(contextID)+","+String(clientID) + ",\"TCP\",\"" + host + "\","
	+ String(port),"+QIOPEN: "+String(clientID)+",0","ERROR"),wait){
		tcp[clientID].connected = true;
		return true;
	}else tcp_close(clientID);

	get_command("AT+QIGETERROR");

	return false;
}

/*
* connect to a host:port using ssl
*
* @contextID - context id 1-16, yet it is limited to MAX_CONNECTIONS
* @sslClientID - id 0-5
* @clientID - connection id 0-11, yet it is limited to MAX_TCP_CONNECTIONS
* @host - can be IP or DNS
* @wait - maximum time to wait for at command response in ms
*
* return true if connection was established
*/
bool MODEMBGXX::tcp_connect_ssl(uint8_t contextID, uint8_t sslClientID, uint8_t clientID, String host, uint16_t port, uint16_t wait) {
	if(apn_connected(contextID) != 1)
		return false;

	if (contextID == 0 || contextID > MAX_CONNECTIONS) return false;

	if (clientID >= MAX_TCP_CONNECTIONS) return false;

	memset(tcp[clientID].server,0,sizeof(tcp[clientID].server));
	memcpy(tcp[clientID].server,host.c_str(),host.length());
	tcp[clientID].port = port;
	tcp[clientID].active = true;
	tcp[clientID].ssl = true;
	tcp[clientID].sslClientID = sslClientID;

	if(check_command_no_ok("AT+QSSLOPEN="+String(contextID)+","+String(sslClientID)+","+String(clientID) + ",\"" + host + "\","
	+ String(port),"+QSSLOPEN: "+String(clientID)+",0","ERROR"),wait){
		tcp[clientID].connected = true;
		return true;
	}else tcp_close(clientID);

	get_command("AT+QIGETERROR");

	return false;
}

/*
* return tcp connection status
*/
bool MODEMBGXX::tcp_connected(uint8_t clientID) {

	if (clientID >= MAX_TCP_CONNECTIONS) return false;
	return tcp[clientID].connected;
}
/*
* close tcp connection
*
* return tcp connection status
*/
bool MODEMBGXX::tcp_close(uint8_t clientID) {

	if(clientID >= MAX_TCP_CONNECTIONS)
		return false;

	tcp[clientID].active = false;
	connected_since[clientID] = 0;
	data_pending[clientID] = false;

	if(tcp[clientID].ssl){
		if(check_command("AT+QSSLCLOSE=" + String(clientID),"OK", "ERROR", 10000)){
			tcp[clientID].connected = false;
			if(tcpOnClose != NULL)
			tcpOnClose(clientID);
		}
	}else{
		if(check_command("AT+QICLOSE=" + String(clientID),"OK", "ERROR", 10000)){
			tcp[clientID].connected = false;
			if(tcpOnClose != NULL)
			tcpOnClose(clientID);
		}
	}

	return tcp[clientID].connected;
}
/*
* send data through open channel
*
* returns true if succeed
*/
bool MODEMBGXX::tcp_send(uint8_t clientID, const char *data, uint16_t size) {
	if (clientID >= MAX_CONNECTIONS) return false;
	if (tcp_connected(clientID) == 0) return false;

	while (modem->available()) {
		String line = modem->readStringUntil(AT_TERMINATOR);

		line.trim();

		parse_command_line(line, true);
	}

	while (modem->available()) modem->read(); // delete garbage on buffer

	if(tcp[clientID].ssl){
		if (!check_command_no_ok("AT+QSSLSEND=" + String(clientID) + "," + String(size), ">", "ERROR"))
			return false;
	}else{
		if (!check_command_no_ok("AT+QISEND=" + String(clientID) + "," + String(size), ">", "ERROR"))
			return false;
	}

	send_command(data, size);
	delay(AT_WAIT_RESPONSE);

	uint32_t timeout        = millis() + 10000;
	String new_data_command = "";

	while (timeout >= millis()) {
		if (modem->available()) {
			String line = modem->readStringUntil(AT_TERMINATOR);

			line.trim();

			if (line.length() == 0) continue;

			//log("[send] ? '" + line + "'");
			#ifdef DEBUG_BG95
			log("parse: "+line);
			#endif
			parse_command_line(line, true);

			if (line.indexOf("SEND OK") > -1 || line.indexOf("OK") > -1) return true;

		}

		delay(AT_WAIT_RESPONSE);
	}

	tcp_check_data_pending();

	return false;
}
/*
* copies data to pointer if available
*
* returns len of data copied
*/
uint16_t MODEMBGXX::tcp_recv(uint8_t clientID, char *data, uint16_t size) {

	if (clientID >= MAX_TCP_CONNECTIONS) return false;

	if (buffer_len[clientID] == 0) return 0;

	uint16_t i;

	if (buffer_len[clientID] < size)
		size = buffer_len[clientID];

	for (i = 0; i < size; i++) {
		data[i] = buffers[clientID][i];
	}

	if (buffer_len[clientID] > size){
		for (i = size; i < buffer_len[clientID]; i++) {
			buffers[clientID][i - size] = buffers[clientID][i];
		}
	}

	buffer_len[clientID] -= size;

	return size;
}
/*
* returns len of data available for clientID
*/
uint16_t MODEMBGXX::tcp_has_data(uint8_t clientID){

	if (clientID >= MAX_TCP_CONNECTIONS) return 0;

	return buffer_len[clientID];
}

/*
* sends request
*
* @host - domain
* @path - start path with '/'
* @token - pair of key words to be added to header
* clientID - socket id
* contextID - context to be used
*
* returns true if request has been correctly sent
*/
bool MODEMBGXX::http_get(String host, String path, String token, uint8_t clientID, uint8_t contextID){

	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	if(clientID >= MAX_TCP_CONNECTIONS)
		return false;

	if(token != "")
		token += "\r\n";

	if(has_context(contextID)){
		if(tcp_connect(contextID,clientID,host,80)){
			String request = "GET " + path + " HTTP/1.1\r\n" +
						 "Host: " + host + "\r\n" +
						 "Cache-Control: no-cache\r\n" +
						 token +
						 "Connection: close\r\n\r\n";

			if(!tcp_send(clientID,request.c_str(),request.length())){
				Serial.printf("failure doing http request: %s \n",request.c_str());
				tcp_close(clientID);
			}

		}else log_output->printf("Connection to %s has failed \n",host.c_str());
	}else{
		log("no context");
		return false;
	}

  return true;
}

/*
* sends https request
*
* @host - domain
* @path - start path with '/'
* @token - pair of key words to be added to header
* @clientID - socket id
* @sslClientID - socket ssl id
* @contextID - context to be used
*
* returns true if request has been correctly sent
*/
bool MODEMBGXX::https_get(String host, String path, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID){

	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	if(clientID >= MAX_TCP_CONNECTIONS)
		return false;

	if(token != "")
		token += "\r\n";

	if(has_context(contextID)){
		if(!tcp_connected(clientID)){
			if(tcp_connect_ssl(contextID,sslClientID,clientID,host,443)){
				String request = "GET " + path + " HTTP/1.1\r\n" +
				"Host: " + host + "\r\n" +
				"Cache-Control: no-cache\r\n" +
				token +
				"Connection: close\r\n\r\n";
				Serial.printf("request: %s",request.c_str());
				if(!tcp_send(clientID,request.c_str(),request.length())){
					Serial.printf("failure doing http request: %s \n",request.c_str());
					tcp_close(clientID);
				}
			}else log_output->printf("Connection to %s has failed \n",host.c_str());
		}
	}else{
		log("no context");
		return false;
	}

  return true;
}

/*
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
*/
bool MODEMBGXX::https_post(String host, String path, String body, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID){

	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	if(clientID >= MAX_TCP_CONNECTIONS)
		return false;

	if(token != "")
		token += "\r\n";

	if(has_context(contextID)){
		if(!tcp_connected(clientID)){
			if(tcp_connect_ssl(contextID,sslClientID,clientID,host,443)){

				String request = "POST " + path + " HTTP/1.1\r\n" +
					"Host: " + host + "\r\n" +
					"Cache-Control: no-cache\r\n" +
					"Content-Length: " + body.length() + "\r\n" +
					"Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n" +
					token +
					"Connection: close\r\n\r\n" +
					body;

				Serial.println(request);

				if(!tcp_send(clientID,request.c_str(),request.length())){
					Serial.printf("failure doing http request: %s \n",request.c_str());
					tcp_close(clientID);
				}
			}else log_output->printf("Connection to %s has failed \n",host.c_str());
		}
	}else{
		log("no context");
		return false;
	}

  return true;
}

/*
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
*/
bool MODEMBGXX::https_post_json(String host, String path, String body, String token, uint8_t clientID, uint8_t sslClientID, uint8_t contextID){

	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	if(clientID >= MAX_TCP_CONNECTIONS)
		return false;

	if(has_context(contextID)){
		if(!tcp_connected(clientID)){
			if(tcp_connect_ssl(contextID,sslClientID,clientID,host,443)){

				String request = "POST " + path + " HTTP/1.1\r\n" +
					"Host: " + host + "\r\n" +
					"Cache-Control: no-cache\r\n" +
					"Content-Length: " + body.length() + "\r\n" +
					"Content-Type: application/json; charset=utf-8\r\n" +
					"Connection: close\r\n\r\n" +
					body;

				Serial.println(request);

				if(!tcp_send(clientID,request.c_str(),request.length())){
					Serial.printf("failure doing http request: %s \n",request.c_str());
					tcp_close(clientID);
				}
			}else log_output->printf("Connection to %s has failed \n",host.c_str());
		}
	}else{
		log("no context");
		return false;
	}

  return true;
}
/*
* After request has been sent, it waits for response and parse header
*
* returns true if header was detected and parsed thereafter
*/
bool MODEMBGXX::http_wait_response(uint8_t clientID){

	uint32_t request_timeout = millis() + 15000;

	while(!tcp_has_data(clientID) && request_timeout > millis()){
		check_messages();
		tcp_check_data_pending();
	}

	if(request_timeout < millis())
		return false;

	uint16_t header_len = http_get_header_length(clientID);
	log_output->printf("header_length: %d \n",header_len);

	if(header_len == 0)
		return false;

	char* data = (char*)malloc(header_len);
	if(data == nullptr)
		return false;

	uint16_t len = tcp_recv(clientID,data,header_len);

	http_parse_header((char*)data,len);
	free(data);

	// stores body
	return true;
}

uint16_t MODEMBGXX::http_get_header_length(uint8_t clientID){

	if (clientID >= MAX_TCP_CONNECTIONS) return false;

	if (buffer_len[clientID] == 0) return 0;

	uint16_t i;
	uint8_t last_char = 0;
	for(i=0; i<buffer_len[clientID]; i++){
		char a = buffers[clientID][i-3];
		char b = buffers[clientID][i-2];
		char c = buffers[clientID][i-1];
		char d = buffers[clientID][i];

		if(i>=3){
			if(a == '\r' && b == '\n' && c == '\r' && d == '\n')
				return i+1;
		}
	}

	return 0;

}

void MODEMBGXX::http_parse_header(char* data, uint16_t len){

	http.body_len = 0;
	memset(http.responseStatus,0,sizeof(http.responseStatus));
	memset(http.contentType,0,sizeof(http.contentType));
	memset(http.md5,0,sizeof(http.md5));

	std::string header;
	header.assign(&data[0],&data[len-1]);

	uint16_t index = 0;
	uint16_t i = 0;
	while(true){

		index = header.find("\r\n");
		if(index == std::string::npos)
			break;
		std::string line = header.substr(0,index+2);
		#ifdef DEBUG_BG95
		Serial.printf("line: %s",line.c_str());
		#endif
		if(i==0){
			String res_str = String(line.c_str());
			if(res_str.length() < sizeof(http.responseStatus)){
				memcpy(http.responseStatus,res_str.c_str(),sizeof(res_str));
			}else{
				memcpy(http.responseStatus,res_str.c_str(),sizeof(http.responseStatus));
			}
		}

		if(line.find("Content-Length: ") != std::string::npos){
			String length_str = String(line.substr(16).c_str());
			http.body_len = length_str.toInt();
		}else if(line.find("Content-MD5: ") != std::string::npos){
			String md5_str = String(line.substr(13).c_str());
			uint8_t j = 0;
			while(j<sizeof(http.md5)){
				http.md5[j] = str2hex(md5_str.substring(j*2,j*2+2));
				j++;
			}
		}else if(line.find("Content-Type: ") != std::string::npos){
			String ctype = String(line.substr(14).c_str());
			if(ctype.length() < sizeof(http.md5)){
				memcpy(http.contentType,ctype.c_str(),sizeof(ctype));
			}else{
				memcpy(http.contentType,ctype.c_str(),sizeof(http.contentType));
			}
		}

		if(index+3 >= header.length())
			break;
		header = header.substr(index+2);
		i++;

	}
	return;
}

/*
* return http response result
*/
String MODEMBGXX::http_response_status(){

	return String(http.responseStatus);
}

/*
* return md5 of last request
*/
String MODEMBGXX::http_md5(){

	return String(http.md5);
}

/*
* returns body size to be read
*/
uint16_t MODEMBGXX::http_get_body_size(){
	return http.body_len;
}
/*
* gets body data
*/
uint16_t MODEMBGXX::http_get_body(uint8_t clientID, char* data, uint16_t len, uint16_t wait){

	uint16_t count = 0;
	uint32_t timeout = millis() + wait;

	while(true){

		uint16_t size = tcp_has_data(clientID);
		if(size+count > len)
			return count; // buffer cannot read all data

		// get data from internal buffer
		if(size > 0){
			count += tcp_recv(clientID,&data[count],size);
			//count += tcp_recv(clientID,&data[count],len-count);
			Serial.printf("bytes available on buffer %d \n", count);
			return count;
		}

		if(count == len)
			return count; // buffer is full

		// get data from modem
		if(!tcp_has_data(clientID)){
			check_messages();
			tcp_check_data_pending();
		}

		if(timeout < millis())
			return count;
  }
}
/*
* check md5 file (Content-MD5: has to be received on header),
* otherwise it will returns false
*/
bool MODEMBGXX::http_check_md5(char* data, uint16_t len){

	char hash[16];
	mbedtls_md_init(&ctx);
  mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) data, len);
  mbedtls_md_finish(&ctx, (unsigned char*)hash);
  mbedtls_md_free(&ctx);

	if(memcmp(hash,http.md5,16) == 0)
		return true;

	return false;
}

String MODEMBGXX::get_subscriber_number(uint16_t wait){
	return "";

	uint32_t timeout = millis() + wait;

	send_command("AT+CNUM");
	delay(AT_WAIT_RESPONSE);

	String number;

	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue;

			if (response != "ERROR" && isNumeric(response)) {
				#ifdef DEBUG_BG95
					log("[number] response = '" + response + "'");
				#endif
			}
		}

		delay(AT_WAIT_RESPONSE);
	}

	return "";

}

/*
* returns the state of context id
*/
String MODEMBGXX::check_context_state(uint8_t contextID){

	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return "";

	String query = "AT+QISTATE=0,"+String(contextID);
	return get_command(query);
}

/*
* returns the state of connection id
*/
String MODEMBGXX::check_connection_state(uint8_t connectionID){

	if(connectionID >= MAX_CONNECTIONS)
		return "";

	String query = "";
	if(tcp[connectionID].ssl)
		query = "AT+QSSLSTATE=1,"+String(connectionID);
	else
		query = "AT+QISTATE=1,"+String(connectionID);

	return get_command(query);
}

/*
void MODEMBGXX::check_modem_buffers() {
	for (uint8_t cid = 0; cid < MAX_TCP_CONNECTIONS; cid++) {
		if(tcp[cid]) // 2 is reserverd for MQTT
			data_pending[cid] = true;
	}
}
*/

String MODEMBGXX::check_messages() {

	String command = "";
	while (modem->available() > 0) {

		command = modem->readStringUntil(AT_TERMINATOR);

		command.trim();

		#ifdef DEBUG_BG95_HIGH
		log("[command] '" + command + "'");
		#endif

		if (command.length() == 0) continue;
		// if (command.length() == 1 && command.startsWith(">")) continue;
		// if (command.length() == 2 && command.startsWith("> ")) continue;

		command = parse_command_line(command, true);
	}
	return command;
}

String MODEMBGXX::parse_command_line(String line, bool set_data_pending) {
	//log("parse: "+line);

	String _cgreg = "+CGREG: ";
	String _cereg = "+CEREG: ";
	String _creg = "+CREG: ";
	int8_t index = -1;

	if(line.startsWith("AT+")){
		log("echo is enabled, disable it");
		send_command("ATE0");
	}

	if (line.startsWith(_cgreg)) {
		index = line.indexOf(",");
		if(index > -1)
			line = line.substring(index+1,index+2);
		else line = line.substring(_cgreg.length(),_cgreg.length()+1);
		if(isNumeric(line)){
			int radio_state = line.toInt();
			#ifdef DEBUG_BG95
			switch(radio_state){
				case 0:
					log("EGPRS not registered");
					break;
				case 1:
					log("EGPRS register");
					break;
				case 2:
					log("EGPRS connecting");
					break;
				case 3:
					log("EGPRS registration denied");
					break;
				case 4:
					log("EGPRS Unknow");
					break;
				case 5:
					log("EGPRS registered, roaming");
					break;
			}
			#endif
		}
		return "";
	}else if (line.startsWith(_cereg)) {
		index = line.indexOf(",");
		if(index > -1)
			line = line.substring(index+1,index+2);
		else line = line.substring(_cereg.length(),_cereg.length()+1);
		if(isNumeric(line)){
			int8_t radio_state = line.toInt();
			switch(radio_state){
				case 0:
					log("LTE not registered");
					break;
				case 1:
					log("LTE registered");
					break;
				case 2:
					log("LTE connecting");
					break;
				case 3:
					log("LTE registration denied");
					break;
				case 4:
					log("LTE Unknow");
					break;
				case 5:
					log("LTE registered, roaming");
					break;
			}
		}
		return "";
	}else if (line.startsWith(_creg)) {
		String connected_ = "";
		String technology_ = "";

		index = line.indexOf(",");
		if(index > -1){
			line = line.substring(index+1);
			index = line.indexOf(",");
			if(index > -1){
				connected_ = line.substring(0,index);
				//log("connected: "+String(connected_));
				line = line.substring(index+1);
			}else{
				connected_ = line;
				//log("connected: "+String(connected_));
			}
		}else return "";

		index = line.indexOf(",");
		if(index > -1){
			line = line.substring(index+1);
			index = line.indexOf(",");
			if(index > -1){
				technology_ = line.substring(index+1);
				//log("technology: "+String(technology_));
			}else return "";
		}else return "";

		if(isNumeric(connected_)){
			int8_t act = -1;
			int8_t radio_state = connected_.toInt();
			if(technology_ != "")
				act = technology_.toInt();
			else act = -1;

			switch(radio_state){
				case 0:
					 //st.apn_lte = false;
					 //st.apn_gsm = false;
					break;
				case 1:
					if(act == 0){
						//st.apn_gsm = true;
						//st.apn_lte = false;
					}else if(act == 9){
						//st.apn_gsm = false;
						//st.apn_lte = true;
					}
					break;
				case 2:
					if(act == 0){
						////st.apn_gsm = 2;
						//st.apn_gsm = false;
						//st.apn_lte = false;
					}else if(act == 9){
						//st.apn_gsm = false;
						////st.apn_lte = 2;
						//st.apn_lte = false;
					}
					break;
				case 3:
					//st.apn_gsm = false;
					//st.apn_lte = false;
					break;
				case 4:
					//st.apn_gsm = false;
					//st.apn_lte = false;
					break;
				case 5:
					if(act == 0){
						//st.apn_gsm = true;
						//st.apn_lte = false;
					}else if(act == 9){
						//st.apn_gsm = false;
						//st.apn_lte = true;
					}
					break;
			}
		}

	}else if (line.startsWith("+QIOPEN:")) {

		int8_t index = line.indexOf(",");
		uint8_t cid = 0;
		int8_t state = -1;
		if(index > -1){
			cid = line.substring(index-1,index).toInt();
			state = line.substring(index+1).toInt();
		}
		if(cid >= MAX_TCP_CONNECTIONS)
			return "";

		if(state == 0){
			#ifdef DEBUG_BG95
			log("TCP is connected");
			#endif
			tcp[cid].connected = true;
		}else{
			log("Error opening TCP");
			tcp[cid].connected = false;
		}
		/*
		for (uint8_t index = 0; index < MAX_CONNECTIONS; index++) {
			if (!line.startsWith("C: " + String(index) + ",")) continue;

			connected_until[index] = millis() + CONNECTION_STATE;
			connected_state[index] = line.endsWith("\"CONNECTED\"");

			#ifdef DEBUG_BG95
			if (connected_state[index]) log("socket " + String(index) + " = connected");
			#endif
		}
		*/
		return "";
	}else if (line.startsWith("+QIURC: \"recv\",") ) {
		String cid_str = line.substring(15);
		uint8_t cid = cid_str.toInt();
		if(cid >= MAX_TCP_CONNECTIONS)
			return "";
		if (set_data_pending) {
			data_pending[cid] = true;
			return "";
		} else {
			tcp_read_buffer(cid);
			return "";
		}
	}else if (line.startsWith("+QIURC: \"closed\",") ) {
		#ifdef DEBUG_BG95
		log("QIURC closed: "+line);
		#endif
		int8_t index = line.indexOf(",");
		int8_t cid = -1;
		if(index > -1){
			state = state.substring(index+1,index+1);
			cid = state.toInt();
			if(cid >= MAX_TCP_CONNECTIONS)
				return "";
			log("connection: "+String(cid)+" closed");
			tcp[cid].connected = false;
			tcp_close(cid);
		}
	}else if (line.startsWith("+QSSLURC: \"recv\",") ) {
		String cid_str = line.substring(17);
		uint8_t cid = cid_str.toInt();
		if(cid >= MAX_TCP_CONNECTIONS)
			return "";
		if (set_data_pending) {
			data_pending[cid] = true;
			return "";
		} else {
			tcp_read_buffer(cid);
			return "";
		}
	}else if (line.startsWith("+QSSLURC: \"closed\",") ) {
		#ifdef DEBUG_BG95
		log("QIURC closed: "+line);
		#endif
		int8_t index = line.indexOf(",");
		int8_t cid = -1;
		if(index > -1){
			state = state.substring(index+1,index+1);
			cid = state.toInt();
			if(cid >= MAX_TCP_CONNECTIONS)
				return "";
			log("connection: "+String(cid)+" closed");
			tcp[cid].connected = false;
			tcp_close(cid);
		}
	}else if (line.startsWith("+CMTI")){
		check_sms();
		return "";
	}else if (line.startsWith("+QIACT: ")) {
		line = line.substring(8);
		int8_t index = line.indexOf(",");
		uint8_t cid = 0;
		if(index > -1){
			cid = line.substring(0,index).toInt(); // connection
			line = line.substring(index+1);
		}else{
			return "";
		}

		if(cid == 0 || cid > MAX_CONNECTIONS)
			return "";

		int8_t state = line.substring(0,1).toInt(); // connection
		if(state == 1){
			#ifdef DEBUG_BG95_HIGH
			log("network connected: "+String(cid));
			#endif
			apn[cid-1].connected = true;
			apn[cid-1].retry = 0;
		}else{
			#ifdef DEBUG_BG95
			log("network disconnected: "+String(cid));
			#endif
			apn[cid-1].connected = false;
		}
		line = line.substring(index+1);

		index = line.lastIndexOf(",");
		if(index > -1){
			String ip = line.substring(index+1);
			ip.replace("\"","");
			memset(apn[cid-1].ip,0,sizeof(apn[cid-1].ip));
			memcpy(apn[cid-1].ip,ip.c_str(),ip.length());
		}else{
			return "";
		}

	}else if(line.startsWith("+QMTSTAT")){
		// error ocurred, channel is disconnected
		String filter = "+QMTSTAT: ";
		uint8_t index = line.indexOf(filter);
		line = line.substring(index);
		index = line.indexOf(",");
		if(index > -1){
			String client = line.substring(filter.length(),index);
			#ifdef DEBUG_BG95_HIGH
			log("client: "+client);
			#endif
			if(isNumeric(client)){
				uint8_t id = client.toInt();
				if(id <= MAX_MQTT_CONNECTIONS){
					mqtt[id].socket_state = MQTT_STATE_DISCONNECTED;
					mqtt[id].connected = false;
				}
				#ifdef DEBUG_BG95
				log("MQTT closed");
				#endif
			}
		}
	}else if(line.startsWith("+QMTRECV:")){
		return mqtt_message_received(line);
	}else if (line.startsWith("+QMTCONN: ")) {
		String filter = "+QMTCONN: ";
		line = line.substring(filter.length());
		index = line.indexOf(",");
		if(index > -1){
			uint8_t cidx = line.substring(0,index).toInt();
			if(cidx < MAX_MQTT_CONNECTIONS){
				String state = line.substring(index+1,index+2);
				if(isdigit(state.c_str()[0])){
					mqtt[cidx].socket_state = (int)state.toInt();
					mqtt[cidx].connected = (int)(state.toInt()==MQTT_STATE_CONNECTED);
					#ifdef DEBUG_BG95
					if(mqtt[cidx].connected)
						log("mqtt client "+String(cidx)+" is connected");
					else
						log("mqtt client "+String(cidx)+" is disconnected");
					#endif
					return "";
				}

			}
		}
	}else if(line.startsWith("OK"))
		return "";

	return line;
}

String MODEMBGXX::mqtt_message_received(String line){

	String filter = "+QMTRECV: ";
	int index = line.indexOf(",");
	uint8_t clientID = 0;
	if(index > -1){ // filter found
		String aux = line.substring(filter.length(),index); // client id
		if(isNumeric(aux)){
			clientID = aux.toInt();
		}else return "";

		aux = line.substring(index+1,index+2); // channel
		if(isNumeric(aux)){
			uint8_t channel = (uint8_t)aux.toInt();
			if(channel < 5)
				mqtt_buffer[channel] = 0; // I do not know the length of the payload that will be read
			line = line.substring(index+2); // null
		}else return "";

		index = line.indexOf(",");
		if(index > -1){ // has payload
			line = line.substring(index+1);
			index = line.indexOf(",");
			if(index > -1){
				String topic = line.substring(0,index);
				String payload = line.substring(index+1);
				if(parseMQTTmessage != NULL)
					parseMQTTmessage(clientID,topic,payload);
			}
		}
	}

	return "";
}

// deprecated
void MODEMBGXX::read_data(uint8_t index, String command, uint16_t bytes) {
	// 4 assumes MAX_CONNECTIONS will be under 10 (0 to 9)
	#ifdef DEBUG_BG95_HIGH
	log("[read_data] '" + command + "'");
	#endif

	//command = command.substring(4, command.length() - 2);

	//bytes = command.toInt();

	#ifdef DEBUG_BG95_HIGH
	log("[read_data] available bytes (" + String(index) + ") = " + String(bytes));
	#endif

	if (bytes == 0) return;

	if(bytes + index < CONNECTION_BUFFER){
		command.toCharArray(&buffers[index][buffer_len[index]],bytes);
		buffer_len[index] += bytes;
	}else log("buffer is full");

	/**
	 * This previous read can take "forever" and expire the connection state,
	 * so in this particular case, we will assume the connection is still valid
	 * and reset the timestamp.
	 **/
	connected_until[index] = millis() + CONNECTION_STATE;

	#ifdef DEBUG_BG95_HIGH
	log("[read_data] all bytes read, in buffer " + String(buffer_len[index]) + " bytes");
	#endif

	while (modem->available()) {
		String line = modem->readStringUntil(AT_TERMINATOR);

		line.trim();

		if (line == "OK") break;

		if (!parse_command_line(line, true)) {
			#ifdef DEBUG_BG95_HIGH
			log("[read_data] ? = '" + line + "'");
			#endif
		}
	}

	return tcp_check_data_pending();
}

bool MODEMBGXX::open_pdp_context(uint8_t contextID) {
	if(contextID == 0 || contextID > MAX_CONNECTIONS)
		return false;

	if(op.technology == 0)
		return false;

	if(apn[contextID-1].connected)
		return false;

	apn[contextID-1].retry *= 2;
	if(apn[contextID-1].retry > 6*60*60*1000)
		apn[contextID-1].retry = 6*60*60*1000;

	if(!check_command("AT+QIACT="+String(contextID), "OK", "ERROR",30000)){
		close_pdp_context(contextID);
		return false;
	}
	return true;
}

bool MODEMBGXX::close_pdp_context(uint8_t tcp_cid) {

	if(tcp_cid < 1 || tcp_cid > MAX_CONNECTIONS)
		return false;

	return check_command("AT+QIDEACT="+String(tcp_cid), "OK", "ERROR");
}

bool MODEMBGXX::disable_pdp(uint8_t tcp_cid) {

	if(tcp_cid < 1 || tcp_cid > MAX_CONNECTIONS)
		return false;

	if (!check_command("AT+CGACT=0,"+String(tcp_cid), "OK", "ERROR", 10000)) return false;

	#ifdef DEBUG_BG95
	log("PDP disabled");
	#endif

	return true;
}

bool MODEMBGXX::enable_pdp(uint8_t tcp_cid) {

	if(tcp_cid < 1 || tcp_cid > MAX_CONNECTIONS)
		return false;

	if (!check_command("AT+CGACT=1,"+String(tcp_cid), "OK", "ERROR", 10000)) return false;

	#ifdef DEBUG_BG95
	log("PDP enabled");
	#endif

	return true;
}

int16_t MODEMBGXX::get_rssi() {

	if (rssi_until > millis()) {
		return rssi_last;
	}

	rssi_until = millis() + 20000;

	String command = "AT+QCSQ";
	String response = get_command(command,"+QCSQ: ",300);

	response.trim();
	if (response.length() == 0) return 0;

	int p = response.indexOf(",");
	String tech = response.substring(0,p);
	tech.replace("\"","");
	#ifdef DEBUG_BG95_HIGH
	log("tech in use: "+tech);
	#endif
	if(tech == "NBIoT")
		op.technology = NB;
	else if(tech == "GSM")
		op.technology = GPRS;
	else if(tech == "eMTC")
		op.technology = CATM1;
	else if(tech == "NOSERVICE")
		op.technology = 0;

	memset(op.tech_string,0,sizeof(op.tech_string));
	memcpy(op.tech_string,tech.c_str(),sizeof(op.tech_string));

	response = response.substring(p+1);
	p = response.indexOf(",");
	if(p > -1)
		response = response.substring(0, p);

	int rssi = 0;
	if(isNumeric(response)){
		rssi = response.toInt();
		#ifdef DEBUG_BG95_HIGH
		log("rssi: "+response);
		#endif
	}

	if(rssi != 0)
		rssi_last = rssi;

	rssi_until = millis() + 5000;
	return rssi_last;

	/*
	for CSQ command
	if (rssi == 0) {
		rssi_last = -115;
	} else if (rssi == 1) {
		rssi_last = -111;
	} else if (rssi == 31) {
		rssi_last = -52;
	} else if (rssi >= 2 && rssi <= 30) {
		rssi_last = -110 + (rssi * 56 / 29);
	} else {
		rssi_last = 99;
	}
	*/
}

int16_t MODEMBGXX::rssi(){

	return rssi_last;
}

String MODEMBGXX::technology(){

	return String(op.tech_string);
}

// use it to get network clock
bool MODEMBGXX::get_clock(tm* t){
	String response = get_command("AT+CCLK?","+CCLK: ",300);

	if(response.length() == 0)
		return false;

	uint8_t index = 0;

	index = response.indexOf("/");
	if(index == -1)
		return false;

	t->tm_year = (int)response.substring(index-2,index).toInt();
	response = response.substring(index+1);

	//log("year: "+String(t->tm_year));
	//log("response: "+response);

	index = response.indexOf("/");
	if(index == -1)
		return false;

	t->tm_mon = (int)response.substring(index-2,index).toInt();
	response = response.substring(index+1);

	//log("month: "+String(t->tm_mon));
	//log("response: "+response);

	index = response.indexOf(",");
	if(index > 0)
		t->tm_mday = (int)response.substring(index-2,index).toInt();
	response = response.substring(index+1);

	//log("day: "+String(t->tm_mday));
	//log("response: "+response);

	index = response.indexOf(":");
	if(index == -1)
		return false;

	t->tm_hour = (int)response.substring(index-2,index).toInt();
	response = response.substring(index+1);

	//log("hour: "+String(t->tm_hour));
	//log("response: "+response);

	index = response.indexOf(":");
	if(index == -1)
		return false;

	t->tm_min = (int)response.substring(index-2,index).toInt();
	response = response.substring(index+1);

	//log("min: "+String(t->tm_min));
	//log("response: "+response);

	bool minus = false;
	index = response.indexOf("+");
	if(index == -1){
		index = response.indexOf("-");
		minus = true;
	}

	t->tm_sec = (int)response.substring(index-2,index).toInt();
	response = response.substring(index+1);

	//log("sec: "+String(t->tm_sec));

	if(t->tm_year < 19)
		return false;

	if(response.toInt() != 0){
		if(minus){
			tz = -response.toInt()/4*3600;
			tz -= response.toInt()%4*15*60;

			t->tm_hour -= response.toInt()/4;
			t->tm_min -= response.toInt()%4*15;
		}else{
			tz = response.toInt()/4*3600;
			tz += response.toInt()%4*15*60;

			t->tm_hour += response.toInt()/4;
			t->tm_min += response.toInt()%4*15;
		}
	}

	return true;
}

int32_t MODEMBGXX::get_tz(){
	return tz;
}

void MODEMBGXX::sync_clock_ntp(bool force){

	if(clock_sync_timeout<millis() || force)
		clock_sync_timeout = millis()+(3600*1000);
	else return;

	//get_command_no_ok("AT+QNTP=1,\"202.120.2.101\",123","+QNTP: ",60000);
	if(!apn[0].connected)
		return;

	get_command("AT+QNTP=1,\"pool.ntp.org\",123","+QNTP: ",60000);
}

void MODEMBGXX::update_sys_clock(){

	struct tm t;
	if(get_clock(&t)){

		int y = t.tm_year;
	  int mo = t.tm_mon;
	  int d = t.tm_mday;
	  int h = t.tm_hour;
	  int m = t.tm_min;
	  int s = t.tm_sec;


		#ifdef DEBUG_BG95
		log_output->printf("date(yyyy/mm/dd hh:mm:ss) - %d/%d/%d %d:%d:%d \n",y+2000,mo,d,h,m,s);
		log("tz: "+String(tz));
		#endif

	  setTime(h, m, s, d, mo, y);

		log_output->println(now());
	}

}

String MODEMBGXX::scan_cells(){
	#ifdef DEBUG_BG95
	log("getting cells information..");
	#endif

	//send_command("AT+CNETSCAN");
	send_command("AT+QENG=\"NEIGHBOURCELL\"");
	delay(AT_WAIT_RESPONSE);

	uint32_t timeout = millis() + 50000;
	String cells = "";

	uint16_t counter = 0;
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);
			counter += response.length();
			response.trim();

			if (response.length() == 0) continue;

			log(response);

			if (response == "ERROR")
				return "";
			else if(response == "OK"){
				#ifdef DEBUG_BG95
				log("[cells info] response = '" + response + "'");
				#endif
				return cells;
			}else cells += response;

			if(counter >= 255){
				log("!! overflow");
				return cells;
			}
		}

		delay(AT_WAIT_RESPONSE);
	}
	return cells;
}

String MODEMBGXX::get_position(){
	#ifdef DEBUG_BG95
	log("getting cells information..");
	#endif
	log(get_command("AT+QGPSCFG=\"gnssconfig\""));

	if (!check_command("AT+QGPS=1", "OK", "ERROR", 400)) return "";

	String response = "";
	//uint32_t timeout = millis() + 120000;
	uint32_t timeout = millis() + 20000;
	while(millis() < timeout){
		response = get_command("AT+QGPSLOC=2","+QGPSLOC: ",300);
		#ifdef DEBUG_BG95
		log(response);
		log("response len: "+String(response.length()));
		#endif
		if(response.length() > 0){
			check_command("AT+QGPSEND", "OK", "ERROR", 400);
			return response;
		}
		delay(2000);
	}

	check_command("AT+QGPSEND", "OK", "ERROR", 400);
	/*

	check_command("AT+QGPS=1", "OK", "ERROR", 400);

	check_command("AT+QGPSCFG=\"nmeasrc\",1", "OK", "ERROR", 400);

	check_command("AT+QGPSGNMEA=\"GGA\"", "OK", "ERROR", 400);

	check_command("AT+QGPSGNMEA=\"RMC\"", "OK", "ERROR", 400);

	check_command("AT+QGPSGNMEA=\"GSV\"", "OK", "ERROR", 400);

	check_command("AT+QGPSGNMEA=\"GSA\"", "OK", "ERROR", 400);

	check_command("AT+QGPSGNMEA=\"VTG\"", "OK", "ERROR", 400);

	check_command("AT+QGPSGNMEA=\"GNS\"", "OK", "ERROR", 400);

	check_command("AT+QGPSCFG=\"nmeasrc\",0", "OK", "ERROR", 400);
	*/
	return response;
}

bool MODEMBGXX::switch_radio_off(){
	#ifdef DEBUG_BG95
	log("switch radio off");
	#endif
	if(check_command("AT+CFUN=0","OK","ERROR",15000)){
		for (uint8_t i = 0; i < MAX_CONNECTIONS; i++) {
			data_pending[i]    = false;
			apn[i].connected = false;
		}
		for (uint8_t i = 0; i < MAX_TCP_CONNECTIONS; i++) {
			tcp[i].connected = false;
		}
		for (uint8_t i = 0; i < MAX_MQTT_CONNECTIONS; i++) {
			mqtt[i].connected = false;
		}
		return true;
	}else return false;
}

String MODEMBGXX::get_imei(uint32_t wait) {
	if(imei != "")
		return imei;

	String command = "AT+CGSN";
	imei = get_command(command,300);
	return imei;
}

String MODEMBGXX::get_ccid(uint32_t wait) {
	uint32_t timeout = millis() + wait;

	String command = "AT+QCCID";
	return get_command(command,"+QCCID: ",1000);
}

String MODEMBGXX::get_imsi(uint32_t wait) {
	uint32_t timeout = millis() + wait;

	String command = "AT+CIMI";
	return get_command(command,300);
}

String MODEMBGXX::get_ip(uint8_t cid, uint32_t wait) {
	if(cid == 0 || cid > MAX_CONNECTIONS)
		return "";

	uint32_t timeout = millis() + wait;

	String command = "AT+CGPADDR="+String(cid);
	return get_command(command,"+CGPADDR: "+String(cid)+",",300);
}

// --- MQTT ---
/*
* init mqtt
*
* @callback - register callback to parse mqtt messages
*/
void MODEMBGXX::MQTT_init(bool(*callback)(uint8_t,String,String)) {
	parseMQTTmessage = callback;

	uint8_t i = 0;
	while(i<MAX_MQTT_CONNECTIONS){
		mqtt[i++].active = false;
	}
}

/*
* setup mqtt
*
* @clientID - supports 5 clients, yet is limited to MAX_MQTT_CONNECTIONS
* @contextID - index of TCP tcp[] - choose 1 connection
* @will_topic - topic to be sent if mqtt loses connection
* @payload - payload to be sent with will topic
*
* returns true if configuration was succeed
*/
bool MODEMBGXX::MQTT_setup(uint8_t clientID, uint8_t contextID, String will_topic, String payload) {

	if(clientID >= MAX_MQTT_CONNECTIONS)
		return false;

	mqtt[clientID].contextID = contextID;
	mqtt[clientID].clientID = clientID;
	mqtt[clientID].active = true;


	String s = "AT+QMTCFG=\"pdpcid\","+String(clientID)+","+String(mqtt[clientID].contextID);
	check_command(s.c_str(),"OK",2000);
		//return false;

	s = "AT+QMTCFG=\"recv/mode\","+String(clientID)+",0";
	check_command(s.c_str(),"OK",2000);
		//return false;

	s = "AT+QMTCFG=\"will\","+String(clientID)+",1,2,1,\""+will_topic+"\",\""+payload+"\"";
	check_command(s.c_str(),"OK",2000);
		//return false;

	return true;
}

bool MODEMBGXX::MQTT_set_ssl(uint8_t clientID, uint8_t contextID, uint8_t sslClientID){
	String s = "AT+QMTCFG=\"ssl\","+String(clientID)+",1,"+String(sslClientID);
	if(!check_command(s.c_str(),"OK",2000)) return false;
	return set_ssl(sslClientID);
}

/*
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
*/
bool MODEMBGXX::MQTT_connect(uint8_t clientID, const char* uid, const char* user, const char* pass, const char* host, uint16_t port) {
	if(clientID >= MAX_CONNECTIONS)
		return false;

	String host_str = String(host);
	memset(mqtt[clientID].host,0,sizeof(mqtt[clientID].host));
	memcpy(mqtt[clientID].host,host_str.c_str(),host_str.length());

	#ifdef DEBUG_BG95
	log("connecting to: "+String(host));
	#endif
	if(!MQTT_isOpened(clientID,host,port)){
		if(!MQTT_open(clientID,host,port))
			return false;
	}

	uint8_t state = mqtt[clientID].socket_state;

	if(state == MQTT_STATE_DISCONNECTING){
		MQTT_close(clientID);
		MQTT_disconnect(clientID);
		MQTT_readAllBuffers(clientID);
		return false;
	}

	if(state != MQTT_STATE_CONNECTING && state != MQTT_STATE_CONNECTED){
		String s = "AT+QMTCONN="+String(clientID)+",\""+String(uid)+"\",\""+String(user)+"\",\""+String(pass)+"\"";
		if(check_command_no_ok(s.c_str(),"+QMTCONN: "+String(clientID)+",0,0",5000))
			mqtt[clientID].socket_state = MQTT_STATE_CONNECTED;
	}

	if(mqtt[clientID].socket_state == MQTT_STATE_CONNECTED){
		mqtt[clientID].connected = true;
	}
	
	return mqtt[clientID].connected;
}

/*
* return true if connection is open
*/
bool MODEMBGXX::MQTT_connected(uint8_t clientID){
	if(clientID > MAX_MQTT_CONNECTIONS)
		return false;

	return mqtt[clientID].connected;
}

/*
* 0 Failed to close connection
*	1 Connection closed successfully
*/
int8_t MODEMBGXX::MQTT_disconnect(uint8_t clientID) {
	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	String s = "AT+QMTDISC="+String(clientID);
	String f = "+QMTDISC: "+String(clientID)+",";
	String response = get_command(s.c_str(),f.c_str(),5000);

	if(response.length() > 0){
		int8_t rsp = -1;
		if(isdigit(response.c_str()[0]))
			rsp = (int)response.toInt();

		if(rsp == 0)
			mqtt[clientID].connected = false;

		return (mqtt[clientID].connected == 0);
	}else return 0;
}

/*
* return true if has succeed
*/
bool MODEMBGXX::MQTT_subscribeTopic(uint8_t clientID, uint16_t msg_id, String topic,uint8_t qos) {
	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	String s;
	s.reserve(512);
	s = "AT+QMTSUB="+String(clientID)+","+String(msg_id);
	s += ",\""+topic+"\","+String(qos);

	String f = "+QMTSUB: "+String(clientID)+","+String(msg_id)+",";
	//logging.println("BGxx","String: ",s);
	//String response = getAtCommandResponseNoOK(s.c_str(),f.c_str(),18000);
	String response = get_command_no_ok(s.c_str(),f.c_str(),18000);
	int8_t index = response.indexOf(",");
	#ifdef DEBUG_BG95_HIGH
	log("response: "+String(response));
	#endif
	if(index > -1){
		response = response.substring(0,index);
		if(isNumeric(response)){
			if((int8_t)response.toInt() == 0){
				#ifdef DEBUG_BG95_HIGH
				log("packet sent successfully");
				#endif
				return true;
			}
		}
	}
	return false;
}


/*
* return true if has succeed
*/
bool MODEMBGXX::MQTT_subscribeTopics(uint8_t clientID, uint16_t msg_id, String topic[],uint8_t qos[], uint8_t len) {
	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	String s;
	s.reserve(512);
	s = "AT+QMTSUB="+String(clientID)+","+String(msg_id);
	uint8_t i = 0;
	while(i<len){
		s += ",\""+topic[i]+"\","+String(qos[i]);
		i++;
	}

	String f = "+QMTSUB: "+String(clientID)+","+String(msg_id)+",";

	String response = get_command_no_ok(s.c_str(),f.c_str(),18000);
	int8_t index = response.indexOf(",");
	#ifdef DEBUG_BG95_HIGH
	log("response: "+String(response));
	#endif
	if(index > -1){
		response = response.substring(0,index);
		if(isNumeric(response)){
			if((int8_t)response.toInt() == 0){
				#ifdef DEBUG_BG95_HIGH
				log("packet sent successfully");
				#endif
				return true;
			}
		}
	}
	return false;
}

/*
* return true if has succeed
*/
int8_t MODEMBGXX::MQTT_unSubscribeTopic(uint8_t clientID, uint16_t msg_id, String topic[], uint8_t len) {
	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	String s = "AT+QMTUNS="+String(clientID)+","+String(msg_id);
	uint8_t i = 0;
	while(i<len){
		s += ","+topic[i];
		i++;
	}

	String f = "+QMTUNS: "+String(clientID)+","+String(msg_id)+",";
	String response = get_command(s.c_str(),f.c_str(),10000);
	response = response.substring(0,1);
	return (int8_t)response.toInt();
}

/*
*	return
*	-1 error
*	0 Packet sent successfully and ACK received from server (message that published when <qos>=0 does not require ACK)
*	1 Packet retransmission
*	2 Failed to send packet
*/
int8_t MODEMBGXX::MQTT_publish(uint8_t clientID, uint16_t msg_id,uint8_t qos, uint8_t retain, String topic, String msg) {
	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	if(!mqtt[clientID].connected) return -1;

	String payload = msg;

	uint32_t msg_id_ = msg_id;
	if(qos == 0)
		msg_id_ = 0;

	String s = "AT+QMTPUBEX="+String(clientID)+","+String(msg_id_)+","+String(qos)+","+String(retain)+",\""+topic+"\",\""+payload+"\"";
	String f = "+QMTPUB: "+String(clientID)+","+String(msg_id_)+",";

	String response = get_command_no_ok_critical(s.c_str(),f.c_str(),15000);
	response = response.substring(0,1);
	if(response.length() > 0){
		if(isdigit(response.c_str()[0])){
			#ifdef DEBUG_BG95_HIGH
			log("message sent");
			#endif
			return (int)response.toInt();
		}
	}
	return -1;
}

/*
* Forces reading data from mqtt modem buffers
* call it only if unsolicited messages are not being processed
*/
void MODEMBGXX::MQTT_readAllBuffers(uint8_t clientID) {
	if(clientID >= MAX_CONNECTIONS)
		return;

	String s = "";
	int8_t i = 0;

	if(MQTT_connected(clientID)){
		while(i<5){
			s = "AT+QMTRECV="+String(clientID)+","+String(i);
			get_command(s.c_str(),400);
			mqtt_buffer[i++] = -1;
		}
	}

	return;
}


/*
* private - Updates mqtt.socket_state of each initialized connection
*
*	0 MQTT is disconnected
*	1 MQTT is initializing
*	2 MQTT is connecting
*	3 MQTT is connected
*	4 MQTT is disconnecting
*/
void MODEMBGXX::MQTT_checkConnection(){

	String s = "AT+QMTCONN?";
	String res = get_command(s.c_str(),2000);

	return;
}

/*
* private
*/
bool MODEMBGXX::MQTT_open(uint8_t clientID, const char* host, uint16_t port) {

	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	if(MQTT_connected(clientID)){
		mqtt[clientID].connected = true;
	}else{
		MQTT_close(clientID);
		String s = "AT+QMTOPEN="+String(clientID)+",\""+String(host)+"\","+String(port);
		if(check_command_no_ok(s.c_str(),"+QMTOPEN: "+String(clientID)+",0",5000))
			mqtt[clientID].connected = true;
		else mqtt[clientID].connected = false;
	}

	return mqtt[clientID].connected;
}
/*
* private
*/
bool MODEMBGXX::MQTT_isOpened(uint8_t clientID, const char* host, uint16_t port) {

	String s = "AT+QMTOPEN?";//+to_string(clientID)+",\""+string(settings.mqtt.host)+"\","+to_string(settings.mqtt.port);
	String ans = "+QMTOPEN: "+String(clientID)+",\""+String(host)+"\","+String(port);

	return check_command(s.c_str(),ans.c_str(),2000);
}
/*
* private
*/
bool MODEMBGXX::MQTT_close(uint8_t clientID) {
	if(clientID >= MAX_CONNECTIONS)
		return clientID;

	String s = "AT+QMTCLOSE="+String(clientID);
	String f = "+QMTCLOSE: "+String(clientID)+",";
	String response = get_command(s.c_str(),f.c_str(),10000);

	if(response.length() > 0){
		if(isdigit(response.c_str()[0])){
			mqtt[clientID].connected = false;
			return (int)response.toInt() == 0;
		}
	}

	return false;
}
/*
* private - Reads data on mqtt modem buffers. Call it with high frequency
* No need to use if mqtt messages came on unsolicited response (current configuration)
*/
void MODEMBGXX::MQTT_readMessages(uint8_t clientID) {
	if(clientID >= MAX_CONNECTIONS)
		return;

	String s = "";
	int8_t i = 0;
	bool read = false;

	/* check if a buffer has messages */
	while(i<5){
		if(mqtt_buffer[i] != -1){
			s = "AT+QMTRECV="+String(clientID)+","+String(i);
			get_command(s.c_str(),400);
			mqtt_buffer[i] = -1;
		}
		i++;
	}

	return;
}

// --- --- ---

// --- private TCP ---

/*
* read tcp buffers from modem
*/
void MODEMBGXX::tcp_check_data_pending() {
	for (uint8_t index = 0; index < MAX_CONNECTIONS; index++) {
		if (!data_pending[index]) continue;

		tcp_read_buffer(index);
	}
}

void MODEMBGXX::tcp_read_buffer(uint8_t index, uint16_t wait) {

	while (modem->available())
		modem->readStringUntil(AT_TERMINATOR);

	int16_t left_space = CONNECTION_BUFFER-buffer_len[index];
	if( left_space <= 10)
		return;

	if(tcp[index].ssl){
		send_command("AT+QSSLRECV=" + String(index) + "," + String(left_space));

	}else{
		send_command("AT+QIRD=" + String(index) + "," + String(left_space));
	}

	delay(AT_WAIT_RESPONSE);

	uint32_t timeout = millis() + wait;
	String info = "";
	bool end = false;

	while (timeout >= millis() && !end) {
		while (modem->available()) {
			info = modem->readStringUntil(AT_TERMINATOR);

			info.trim();

			if (info.length() == 0) continue;

			/*
			if (info.startsWith("+CIPRXGET: 1," + String(index))) {
				read_data(index, info);
				break;
			}
			*/
			if (info.startsWith("+QIRD: ")) {

				log(info); // +QIRD
				info = info.substring(7);
				String size = info.substring(0,1);
				uint16_t len = info.toInt();
				if(len > 0){
					if(len + buffer_len[index] <= CONNECTION_BUFFER){
						uint16_t n = modem->readBytes(&buffers[index][buffer_len[index]], len);
						buffer_len[index] += n;
					}else{
						log("buffer is full, data read after this will be discarded");
					}
				}
				if(buffer_len[index] == 0)
					data_pending[index] = false;
				else
					data_pending[index] = true;

				break;
			}else if (info.startsWith("+QSSLRECV: ")) {

				log(info); // +QIRD
				info = info.substring(11);
				String size = info.substring(0,1);
				uint16_t len = info.toInt();
				if(len > 0){
					if(len + buffer_len[index] <= CONNECTION_BUFFER){
						uint16_t n = modem->readBytes(&buffers[index][buffer_len[index]], len);
						buffer_len[index] += n;
					}else{
						log("buffer is full, data read after this will be discarded");
					}
				}
				if(buffer_len[index] == 0)
					data_pending[index] = false;
				else
					data_pending[index] = true;

				break;
			}else if (info == "OK") {
				end = true;
				return;
			}else if(info == "ERROR"){
				if(buffer_len[index] == 0)
					data_pending[index] = false;
				else
					data_pending[index] = true;
				return;
			}else{
				parse_command_line(info);
			}
		}

		delay(AT_WAIT_RESPONSE);
	}

}

void MODEMBGXX::send_command(String command, bool mute) {

	#ifdef DEBUG_BG95_HIGH
	if (!mute) log(">> " + command);
	#endif

	modem->println(command);
	modem->flush();

}

void MODEMBGXX::send_command(uint8_t *command, uint16_t size) {

	String data = "";

	if (modem->available()) {
		String response = modem->readStringUntil(AT_TERMINATOR);
		response.trim();

		if (response.length() != 0){
			#ifdef DEBUG_BG95_HIGH
			log("<< " +response);
			#endif

			data += parse_command_line(response);
		}
	}

	delay(AT_WAIT_RESPONSE);


	#ifdef DEBUG_BG95_HIGH
	log("<< " + String((char*)command));
	#endif

	for (uint16_t i = 0; i < size; i++) {
		modem->write(command[i]);
	}
	modem->flush();

}

String MODEMBGXX::get_command(String command, uint32_t timeout){

	send_command(command);
	delay(AT_WAIT_RESPONSE);

	String data = "";
	timeout += millis();
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);
			response.trim();

			if (response.length() == 0) continue; // garbage

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			data += parse_command_line(response);

			//if(response.indexOf("OK") > -1 && data.length() > 0)
			if(response.indexOf("OK") > -1)
				return data;

			if(response.indexOf("ERROR") > -1)
				return "ERROR";
		}

		delay(AT_WAIT_RESPONSE);
	}

	return data;
}

// filters the rcv response and waits for OK to validate it
String MODEMBGXX::get_command(String command, String filter, uint32_t timeout){

	send_command(command);
	delay(AT_WAIT_RESPONSE);

	String data = "";
	timeout += millis();
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue; // garbage

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if(response.startsWith(filter))
				data = response.substring(filter.length());

			/*
			if(response.indexOf("OK") > -1)
				return data;
			*/
			if(response.indexOf("ERROR") > -1)
				return "";
		}else{
			if(data.length() > 0)
				return data;
		}

		delay(AT_WAIT_RESPONSE);
	}

	return data;
}

// filters the rcv response and waits for OK to validate it
String MODEMBGXX::get_command_critical(String command, String filter, uint32_t timeout){

	send_command(command);
	delay(AT_WAIT_RESPONSE);

	String data = "";
	timeout += millis();
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue; // garbage

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			//parse_command_line(response);

			if(response.startsWith(filter))
				data = response.substring(filter.length());

			if(response.indexOf("OK") > -1 && data.length() > 0)
				return data;

			if(response.indexOf("ERROR") > -1)
				return "";
		}else{
			if(data.length() > 0)
				return data;
		}

		delay(AT_WAIT_RESPONSE);
	}

	return data;
}

// filters the rcv response
String MODEMBGXX::get_command_no_ok(String command, String filter, uint32_t timeout){

	send_command(command);
	delay(AT_WAIT_RESPONSE);

	String data = "";
	timeout += millis();
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue; // garbage

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if(response.startsWith(filter)){
				data = response.substring(filter.length());
				return data;
			}

			if(response.indexOf("ERROR") > -1)
				return "";
		}

		delay(AT_WAIT_RESPONSE);
	}

	return data;
}

// filters the rcv response
String MODEMBGXX::get_command_no_ok_critical(String command, String filter, uint32_t timeout){

	send_command(command);
	delay(AT_WAIT_RESPONSE);

	String data = "";
	timeout += millis();
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue; // garbage

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			response = parse_command_line(response,true);

			if(response.startsWith("+QMTRECV:")) // parse MQTT received messages
				mqtt_message_received(response);
			else if(response.startsWith(filter)){
				data = response.substring(filter.length());
				return data;
			}

			if(response.indexOf("ERROR") > -1)
				return "";
		}

		delay(AT_WAIT_RESPONSE);
	}

	return data;
}

// wait for at command status
bool MODEMBGXX::wait_command(String filter, uint32_t timeout){

	delay(AT_WAIT_RESPONSE);

	String data = "";
	timeout += millis();
	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue; // garbage

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if(response.startsWith(filter)){
				return true;
			}

		}

		delay(AT_WAIT_RESPONSE);
	}

	return false;
}

// use it when OK comes in the end
bool MODEMBGXX::check_command(String command, String ok_result, uint32_t wait) {
	bool response_expected = false;
	send_command(command);
	delay(AT_WAIT_RESPONSE);

	uint32_t timeout = millis() + wait;

	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue;

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if (response == ok_result) {
				response_expected = true;
				//break;
			}

			if (response == "ERROR") {
				break;
			}

			if (response.indexOf("+CME ERROR") > -1)
				break;

			if (response.indexOf("OK") > -1)
				break;

		}

		delay(AT_WAIT_RESPONSE);
	}
	/*
	if(response_expected){
		// check if there is an ok in the end of sentence
		String response = modem->readStringUntil(AT_TERMINATOR);
		response.trim();
		#ifdef DEBUG_BG95_HIGH
			log("<< " +response);
		#endif
	}
	*/
	return response_expected;
}

// use it when OK comes in the end
bool MODEMBGXX::check_command(String command, String ok_result, String error_result, uint32_t wait) {
	bool response_expected = false;
	send_command(command);
	delay(AT_WAIT_RESPONSE);

	uint32_t timeout = millis() + wait;

	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue;

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if (response == ok_result) {
				response_expected = true;
			}

			if (response == error_result) {
				break;
			}

			if (response.indexOf("+CME ERROR") > -1)
				break;

			if (response.indexOf("OK") > -1)
				break;

		}

		delay(AT_WAIT_RESPONSE);
	}

	return response_expected;
}

// use it when OK comes before the ok_result
bool MODEMBGXX::check_command_no_ok(String command, String ok_result, uint32_t wait) {
	bool response_expected = false;
	send_command(command);
	delay(AT_WAIT_RESPONSE);

	uint32_t timeout = millis() + wait;

	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue;

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if (response == ok_result) {
				response_expected = true;
				break;
			}

			if (response == "ERROR") {
				break;
			}

			if (response.indexOf("+CME ERROR") > -1)
				break;

		}

		delay(AT_WAIT_RESPONSE);
	}

	return response_expected;
}

// use it when OK comes before the ok_result
bool MODEMBGXX::check_command_no_ok(String command, String ok_result, String error_result, uint32_t wait) {
	bool response_expected = false;
	send_command(command);
	delay(AT_WAIT_RESPONSE);

	uint32_t timeout = millis() + wait;

	while (timeout >= millis()) {
		if (modem->available()) {
			String response = modem->readStringUntil(AT_TERMINATOR);

			response.trim();

			if (response.length() == 0) continue;

			#ifdef DEBUG_BG95_HIGH
				log("<< " +response);
			#endif

			parse_command_line(response);

			if (response == ok_result) {
				response_expected = true;
				break;
			}

			if (response == error_result) {
				break;
			}

			if (response.indexOf("+CME ERROR") > -1)
				break;

		}

		delay(AT_WAIT_RESPONSE);
	}

	return response_expected;
}

void MODEMBGXX::check_commands() {

	if (modem->available()) {
		String response = modem->readStringUntil(AT_TERMINATOR);

		response.trim();

		#ifdef DEBUG_BG95_HIGH
			log("<< " +response);
		#endif

		parse_command_line(response);
	}

}

void MODEMBGXX::log(String text) {
	//log_output->println("[" + date() + "] bgxx: " + text);
	log_output->println("bgxx: " + text);
}

String MODEMBGXX::date() {
	return String(year()) + "-" + pad2(month()) + "-" + pad2(day()) + " " + pad2(hour()) + ":" + pad2(minute()) + ":" + pad2(second());
}

String MODEMBGXX::pad2(int value) {
	return String(value < 10 ? "0" : "") + String(value);
}

boolean MODEMBGXX::isNumeric(String str) {
    unsigned int StringLength = str.length();

    if (StringLength == 0) {
        return false;
    }

    boolean seenDecimal = false;

    for(unsigned int i = 0; i < StringLength; ++i) {
        if (isDigit(str.charAt(i))) {
            continue;
        }

        if (str.charAt(i) == '.') {
            if (seenDecimal) {
                return false;
            }
            seenDecimal = true;
            continue;
        }

				if (str.charAt(i) == '-')
            continue;

        return false;
    }
    return true;
}

int MODEMBGXX::str2hex(String str){
  return (int)strtol(str.c_str(), NULL, 16);
}
