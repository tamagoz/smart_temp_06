#include <stdlib.h>
#include <stdio.h>
#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/DHT/DHT.h>
#include <DeviceConfig.h>
#include <NetworkConfig.h>
#include <BrokerConfig.h>
#include <TimeConfig.h>
#include <Libraries/DHT/DHT.h>
#ifdef LCD_DISPLAY
#include <Libraries/LiquidCrystal/LiquidCrystal_I2C.h>

#define IP_STATE   0
#define TIME_STATE   1

#endif
#include "NtpClientDelegateDemo.h"

//#define WIFI_SSID "GreatODev" // Put you SSID and Password here
//#define WIFI_PWD "1683vios"

#define DEBUG_PRINT(x) {Serial.println(x);}
#define DEBUG_PRINTLN() {Serial.println();}

byte icon_termometer[8] = //icon for termometer
{
    B00100,
    B01010,
    B01010,
    B01110,
    B01110,
    B11111,
    B11111,
    B01110
};

byte icon_water[8] = //icon for water droplet
{
    B00100,
    B00100,
    B01010,
    B01010,
    B10001,
    B10001,
    B10001,
    B01110,
};

#ifdef LCD_DISPLAY
// SDA GPIO2, SCL GPIO0
#define I2C_LCD_ADDR 0x27
//#define I2C_LCD_ADDR 0x3F
LiquidCrystal_I2C *lcd;
uint8_t lcdAddr;
bool lcdFound = true;
#endif

bool mqttStatus = true;
bool lcdstatus = true;

Timer procTimer;
Timer statusTimer;
Timer restartTimer;
Timer systemTime;
Timer systemTime1;


uint32_t getChipId(void);

void connectFail();
void publishMessage();
void onMessageReceived(String topic, String message); // Forward declaration for our callback
void sendSocketClient(String msg);
void startMQTT();

// MQTT client
MqttClient *mqtt = NULL;
HttpServer *server;
FTPServer *ftp;
ntpClientDemo *ntp;
BssList networks;
bool state = true;
DHT *dht;

String deviceId = String(getChipId());
//String message1 = "";
int totalActiveSockets = 0;

//#endif
unsigned long pressed;
bool configStarted = false;

String getTopic  = "";
String setTopic = "";

#ifdef LCD_DISPLAY

void lcdSystemStarting()
{
	if (lcdFound){
		lcd->clear();
		lcd->setCursor(0,0);
		lcd->print("System starting");
		lcd->setCursor(0,1);
		lcd->print("Please wait...");
	}
}

void lcdSystemReset()
{
	if (lcdFound){
		lcd->clear();
		lcd->setCursor(0,0);
		lcd->print("System reset");
		lcd->setCursor(0,1);
		lcd->print("Restart device!");
	}
}

void lcdMqttDisconnect()
{
	if (lcdFound){
		lcd->clear();
		lcd->setCursor(0,0);
		lcd->print("MQTT Client:");
		lcd->setCursor(0,1);
		lcd->print("Reconnecting...");
	}
}

void lcdStartServers()
{
	if (lcdFound){
		lcd->clear();
		lcd->setCursor(0,0);
		lcd->print("AccessPoint Mode");
		lcd->setCursor(0,1);
		lcd->print(" "+WifiAccessPoint.getIP().toString()+" ");
	}
}

void lcdConnectOk()
{
	if (lcdFound){
		lcd->clear();
		lcd->setCursor(0,0);
		lcd->print("WifiStation Mode");
		lcd->setCursor(0,1);
		lcd->print(" "+WifiStation.getIP().toString()+" ");
	}
}

void lcdConnectionFailed()
{
	if (lcdFound){
		lcd->clear();
		lcd->setCursor(0,0);
		lcd->print("Access point");
		lcd->setCursor(0,1);
		lcd->print("Connection lost");
	}
}

void sendSocketIo(int channel){
	String st;
	String msg;

	st = digitalRead(channel)?"1":"0";
	msg = "{ \"Type\": 2, \"gpio\": " + String(channel) + ", \"status\": " + st  + " }";
	sendSocketClient(msg);
}

void timer_handler(int8_t Hour, int8_t Minute)
{

}

//-----------------------------------------------------
void lcdDisplayTime()
{
	DateTime _dateTime = SystemClock.now();
	if (_dateTime.Second == 0){
		timer_handler(_dateTime.Hour, _dateTime.Minute);
	}
//---8776296
	//float h = dht->readHumidity() + 30.1;
	//float t = dht->readTemperature() - 0.4;
//---13668309
	float h = dht->readHumidity(); //+ 37.7;
	float t = dht->readTemperature(); //- 0.8;

	Serial.print(t);
	Serial.print(",");
	Serial.println(h);

	if (lcdFound){
		char buf[64];
		lcd->setCursor(0, 0);
		//sprintf(buf, "%02d/%02d/%d", _dateTime.Day, _dateTime.Month + 1, _dateTime.Year + 543);
		sprintf(buf, "%02d/%02d/%d %02d:%02d", _dateTime.Day, _dateTime.Month + 1, _dateTime.Year + 543, _dateTime.Hour, _dateTime.Minute);
		lcd->print(String(buf));

		lcd->setCursor(0, 1);
		if (!isnan(t) || !isnan(h)){
			lcd->print("\1" + String(t) +  (char)223 + "C " + "\2" + String(h) + "%") ;
		}else{
			lcd->print("----- NaN ------");
		}

		memset(buf, 0, 64);
		sprintf(buf, "%02d:%02d:%02d", _dateTime.Hour, _dateTime.Minute, _dateTime.Second);
		lcd->print(String(buf));


	}

    // check if returns are valid, if they are NaN (not a number)
	// then something went wrong!
	if (!isnan(t) || !isnan(h)){
		String message = "{ \"Type\": 7, \"Id\": \""+deviceId+"\", \"Temperature\": \"" + String(t) + "\", \"Humidity\": \"" + String(h) + "\" }";
		//Serial.println(message);

		sendSocketClient(message);
		if (BrokerSettings.active == true) {
			if (mqtt->getConnectionState() == TcpClientState::eTCS_Connected){
				mqtt->publish( deviceId + "/" +BrokerSettings.user_name+ "/temperature", message);
			}
		}
	}
}

void enablerAccessPoint()
{
	DeviceSettings.load();
	WifiAccessPoint.enable(true);
	//WifiAccessPoint.config("sittipong", DeviceSettings.password, AUTH_OPEN);
	WifiAccessPoint.config(DeviceSettings.ssid + "-" + String(getChipId()), DeviceSettings.password, AUTH_OPEN);
}

void startDisplayTime(){
	if (lcdFound){
		systemTime.initializeMs(10000, lcdDisplayTime).start();
		systemTime1.initializeMs(300000, enablerAccessPoint).start();
	}
}

#endif

//#ifndef LCD_DISPLAY
void blink()
{
	digitalWrite(CONF_PIN, state);
	state = !state;
}
//#endif

uint32_t getChipId(void)
{
    return system_get_chip_id();
}

void allOutputOff()
{
	digitalWrite(CHANNEL1, LOW);
	digitalWrite(CHANNEL2, LOW);
	digitalWrite(CONFC_PIN, HIGH);
}

void sendSocketClient(String msg)
{
	WebSocketsList &clients = server->getActiveWebSockets();
	for (int i = 0; i < clients.count(); i++)
		clients[i].sendString(msg);
}

// Publish our message
void publishMessage()
{
	if (mqtt->getConnectionState() != TcpClientState::eTCS_Connected){
		mqttStatus = false;
		sendSocketClient("{ \"Type\": 3, \"status\": false }");
		delete mqtt;
		procTimer.stop();
		DEBUG_PRINT("MQTT client disconnected!");
		restartTimer.initializeMs(30000, startMQTT).startOnce();
		systemTime.stop();
		DEBUG_PRINT("MQTT client restarting...");
		if (lcdFound){
			lcdMqttDisconnect();
		}else{
			statusTimer.initializeMs(100, blink).start();
		}
	}
}

// Callback for messages, arrived from MQTT server
void onMessageReceived(String topic, String message)
{
	DynamicJsonBuffer jsonBuffer;

	BrokerSettings.load();
	int state = LOW;
	uint16_t gpio = -1;
	char* jsonString = new char[message.length()+1];

	if (topic == setTopic){
		message.toCharArray(jsonString, message.length()+1);
		JsonObject& root = jsonBuffer.parseObject(jsonString);
		//Serial.println(root.toJsonString());
		String id = root["properties"][0]["id"].toString();
		String value = root["properties"][0]["value"].toString();

		String token;
		if (id == BrokerSettings.gpio12Id){
			state = value == "on" ? HIGH : LOW;
			token = BrokerSettings.gpio12Id;
			gpio = CHANNEL1;
		}
		if (id == BrokerSettings.gpio13Id){
			state = value == "on" ? HIGH : LOW;
			token = BrokerSettings.gpio13Id;
			gpio = CHANNEL2;
		}

		if (gpio != -1){
			digitalWrite(gpio, state);
			// websocket broadcast io status
			String st = digitalRead(gpio)?"1":"0";
			sendSocketClient("{ \"Type\": 2, \"gpio\": " + String(gpio) + ", \"status\": " + st  + " }");

			st = digitalRead(gpio)?"on":"off";
			String pubMsg = "{\"properties\":[{ \"token\": \"" + token + "\", \"status\": \"" + st + "\" }]}";
			mqtt->publish("device/"+BrokerSettings.user_name+"/"+BrokerSettings.token+"/status", pubMsg, true);
		}
		delete jsonString;
	}

	if (topic == getTopic){
		Serial.println("Out Topic");

		String status12 = digitalRead(CHANNEL1)?"on":"off";
		String status13 = digitalRead(CHANNEL2)?"on":"off";

		String pubMsg = "{\"properties\":[{ \"status\": \"" + status12  + "\", \"token\": \"" + BrokerSettings.gpio12Id + "\"" + " },{ \"status\": \"" + status13  + "\", \"token\": \"" + BrokerSettings.gpio13Id + "\"" + " }]}";
		//String pubMsg = "{\"properties\":[{ \"type\": 2, \"status\": \"" + status12  + "\", \"gpio\": 12 },{ \"type\": 2, \"status\": \"" + status13  + "\", \"gpio\": 13 },{ \"type\": 2, \"status\": \"" + status14 + "\", \"gpio\": 14 }]}";
		String pubTopic = "device/"+BrokerSettings.user_name+"/"+BrokerSettings.token+"/status";
		mqtt->publish(pubTopic, pubMsg, true);
		//DEBUG_PRINT(pubTopic + ": " + pubMsg);
	}
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	vars["checked12"] = digitalRead(CHANNEL1)?"checked":"";
	vars["checked13"] = digitalRead(CHANNEL2)?"checked":"";
	vars["device_id"] = deviceId;
	vars["status"] = BrokerSettings.active ? mqttStatus ? "Connected" : "Disconnected" : "Disable";
	vars["label_class"] = BrokerSettings.active ? mqttStatus ? "label-success" : "label-danger" : "label-default";

	response.sendTemplate(tmpl); // this template object will be deleted automatically
}

void onNetworkConfig(HttpRequest &request, HttpResponse &response)
{
	//DEBUG_PRINT("onNetworkConfig..");

	TemplateFileStream *tmpl = new TemplateFileStream("network.html");
	auto &vars = tmpl->variables();
	String option = "";
	for (int i = 0; i < networks.count(); i++)
	{
		if (networks[i].hidden) continue;
		if (networks[i].ssid == NetworkSettings.ssid){
			option += "<option selected>" + networks[i].ssid + "</option>";
		}else{
			option += "<option>" + networks[i].ssid + "</option>";
		}
	}
	//Serial.println("Option: "+ option);
	vars["option"] = option;

	if (request.getRequestMethod() == RequestMethod::POST)
	{
		NetworkSettings.ssid = request.getPostParameter("ssid");
		NetworkSettings.password = request.getPostParameter("password");
		NetworkSettings.dhcp = request.getPostParameter("dhcp") == "1";
		NetworkSettings.ip = request.getPostParameter("ip");
		NetworkSettings.netmask = request.getPostParameter("netmask");
		NetworkSettings.gateway = request.getPostParameter("gateway");
		debugf("Updating IP settings: %d", NetworkSettings.ip.isNull());

		NetworkSettings.save();
		vars["dhcpon"] = request.getPostParameter("dhcp") == "1" ? "checked='checked'" : "";
		vars["dhcpoff"] = request.getPostParameter("dhcp") == "0" ? "checked='checked'" : "";
		vars["password"] = request.getPostParameter("password");
		vars["ip"] = request.getPostParameter("ip");
		vars["netmask"] = request.getPostParameter("netmask");
		vars["gateway"] = request.getPostParameter("gateway");

	}else{
		bool dhcp = WifiStation.isEnabledDHCP();
		vars["dhcpon"] = dhcp ? "checked='checked'" : "";
		vars["dhcpoff"] = !dhcp ? "checked='checked'" : "";
		vars["password"] = NetworkSettings.password;
		if (!WifiStation.getIP().isNull())
		{
			vars["ip"] = WifiStation.getIP().toString();
			vars["netmask"] = WifiStation.getNetworkMask().toString();
			vars["gateway"] = WifiStation.getNetworkGateway().toString();
		}
		else
		{
			vars["ip"] = "192.168.1.200";
			vars["netmask"] = "255.255.255.0";
			vars["gateway"] = "192.168.1.1";
		}
	}
	response.sendTemplate(tmpl); // will be automatically deleted
}

void onTimeConfig(HttpRequest &request, HttpResponse &response)
{

}

void onBrokerConfig(HttpRequest &request, HttpResponse &response)
{
	//DEBUG_PRINT("onBrokerConfig..");
	TemplateFileStream *tmpl = new TemplateFileStream("config.html");
	auto &vars = tmpl->variables();

	if (request.getRequestMethod() == RequestMethod::POST)
	{
		BrokerSettings.active = request.getPostParameter("mqtt") == "1";
		BrokerSettings.user_name = request.getPostParameter("user_name");
		BrokerSettings.password = request.getPostParameter("password");
		BrokerSettings.serverHost = request.getPostParameter("broker");
		BrokerSettings.serverPort = request.getPostParameter("port").toInt();
		BrokerSettings.updateInterval = request.getPostParameter("interval").toInt();
		BrokerSettings.token = request.getPostParameter("token");
		BrokerSettings.gpio12Id = request.getPostParameter("gpio12");
		BrokerSettings.gpio13Id = request.getPostParameter("gpio13");


		BrokerSettings.save();
	}
	BrokerSettings.load();
	vars["mqtton"] = BrokerSettings.active ? "checked='checked'" : "";
	vars["mqttoff"] = !BrokerSettings.active ? "checked='checked'" : "";
	vars["user_name"] = BrokerSettings.user_name;
	vars["password"] = BrokerSettings.password;
	vars["broker"] = BrokerSettings.serverHost;
	vars["port"] = BrokerSettings.serverPort;
	vars["interval"] = BrokerSettings.updateInterval;
	vars["token"] = BrokerSettings.token;
	vars["gpio12"] = BrokerSettings.gpio12Id;
	vars["gpio13"] = BrokerSettings.gpio13Id;

	response.sendTemplate(tmpl);
}

void onAjaxGpio(HttpRequest &request, HttpResponse &response)
{
	//DEBUG_PRINT("GPIO: " + request.getQueryParameter("gpio"));
	//DEBUG_PRINT("STATE: " + request.getQueryParameter("state"));

	int gpio = request.getQueryParameter("gpio").toInt();
	digitalWrite(gpio, request.getQueryParameter("state") == "true"?HIGH:LOW);

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["state"] = (bool)digitalRead(gpio);
	json["gpio"] = gpio;
	// send response request
	response.sendJsonObject(stream);

	// websocket broadcast io status
	String st = digitalRead(gpio)?"1":"0";
	String msg = "{ \"Type\": 2, \"gpio\": " + request.getQueryParameter("gpio") + ", \"status\": " + st  + " }";
	sendSocketClient(msg);

	String token;
	BrokerSettings.load();
	if (mqtt != NULL){
		switch (gpio){
			case CHANNEL1: token = BrokerSettings.gpio12Id;
				break;
			case CHANNEL2: token = BrokerSettings.gpio13Id;
				break;

		}
		st = digitalRead(gpio)?"on":"off";
		msg = "{\"properties\":[{ \"Type\": 2, \"token\": \"" +token + "\", \"status\": \"" + st  + "\" }]}";
		mqtt->publish("device/" + BrokerSettings.user_name + "/" + BrokerSettings.token+"/status", msg);

	}
}

void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");
	// Start FTP server
	ftp = new FTPServer();
	ftp->listen(21);
	ftp->addUser("admin", "123"); // FTP account
}

void startMQTT()
{
	BrokerSettings.load();

	if (BrokerSettings.active == true){

		sendSocketClient("{ \"Type\": 3, \"status\": true }");
		//Serial.println("Create MQTT Object");
		mqtt = new MqttClient(BrokerSettings.serverHost, BrokerSettings.serverPort, onMessageReceived);

		// Run MQTT client
		mqtt->connect(deviceId, BrokerSettings.user_name, BrokerSettings.password);
		mqtt->subscribe(getTopic);
		mqtt->subscribe(setTopic);
		// Start publishing loop
		if (!mqttStatus){
			if (lcdFound){
				startDisplayTime();
			}else{
				statusTimer.stop();
				statusTimer.initializeMs(1000, blink).start();
			}
			mqttStatus = true;
		}
		procTimer.initializeMs(BrokerSettings.updateInterval * 1000, publishMessage).start(); // every 5 seconds
	}
}

void startWebServer()
{
	server = new HttpServer();
	server->listen(80);
	server->addPath("/", onIndex);
	server->addPath("/network", onNetworkConfig);
	server->addPath("/config", onBrokerConfig);
	server->setDefaultHandler(onFile);

	// Web Sockets configuration
	server->enableWebSockets(true);
}

void startServers()
{
	//DEBUG_PRINT("System Ready...");
	if (WifiAccessPoint.isEnabled()){
		startFTP();
		startWebServer();
#ifdef LCD_DISPLAY
		if (lcdFound){
			lcdStartServers();
		}else{
			statusTimer.stop();
			statusTimer.initializeMs(100, blink).start();
		}
#endif
   }
}


void connectOk()
{
	startFTP();
	startWebServer();
	// start mqtt after connect 30 sec.
	procTimer.initializeMs(30000, startMQTT).startOnce();
	ntp = new ntpClientDemo("pool.ntp.org");
	if (lcdFound){
		lcdConnectOk();
	    //statusTimer.initializeMs(300000, enablerAccessPoint).start();  //start ap 1 min
	}else{
		statusTimer.stop();
		statusTimer.initializeMs(1000, blink).start();
		//statusTimer.initializeMs(30000, enablerAccessPoint).start();

	}
	restartTimer.initializeMs(30 * 1000, startDisplayTime).start();

}

void disableWifi()
{
	WifiStation.enable(false);
}

void networkScanCompleted(bool succeeded, BssList list)
{
	if (succeeded)
	{
		for (int i = 0; i < list.count(); i++)
			if (!list[i].hidden && list[i].ssid.length() > 0)
				networks.add(list[i]);
	}
	networks.sort([](const BssInfo& a, const BssInfo& b){ return b.rssi - a.rssi; } );

	//DEBUG_PRINT("Network Scan Completed");
	if (!NetworkSettings.exist()){
		procTimer.initializeMs(2000, disableWifi).startOnce();
	}
}

void stationConnect()
{
#ifndef WIFI_SSID
	//attachInterrupt(INT_PIN, interruptHandler, CHANGE);

	if (NetworkSettings.exist())
	{
		NetworkSettings.load();
		//Serial.println("SSID: " + NetworkSettings.ssid);
		//Serial.println("PWD: " + NetworkSettings.password);

		WifiStation.config(NetworkSettings.ssid, NetworkSettings.password);
		if (!NetworkSettings.dhcp && !NetworkSettings.ip.isNull())
			WifiStation.setIP(NetworkSettings.ip, NetworkSettings.netmask, NetworkSettings.gateway);
		WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start

		WifiAccessPoint.enable(false);
	}else{
		if (!lcdFound){
			statusTimer.initializeMs(100, blink).start();
		}
		enablerAccessPoint();
	}
#else
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.waitConnection(connectOk, 20, connectFail);
#endif
}

// Will be called when WiFi station timeout was reached
void connectFail()
{
	//DEBUG_PRINT("I'm NOT CONNECTED. Need help :(");
	if (lcdFound){
		lcdConnectionFailed();
	}else{
		statusTimer.initializeMs(100, blink).start();
	}

	if (WifiAccessPoint.isEnabled()){
		procTimer.initializeMs(2000, disableWifi).startOnce();
	}
	WifiStation.waitConnection(connectOk, 20, connectFail);
}

void IRAM_ATTR interruptHandler()
{
	if (digitalRead(INTC_PIN) == LOW){
		pressed = millis();
	}
	if (digitalRead(INTC_PIN) == HIGH){
		if ((millis() - pressed) > 5000){
			pressed = millis();
			systemTime.stop();
			NetworkSettings.Delete();

			if (lcdFound){
				lcdSystemReset();
			}else{
				statusTimer.stop();
				digitalWrite(CONFC_PIN, LOW);
			}
		}
	}
}

void scanBus(){

	byte error, address;
	//uint8_t address[] = { 0x27, 0x3F };
	lcdFound = false;
	Wire.begin();
	for (address = 1; address < 127; address++){
		WDT.alive();
		Wire.beginTransmission(address);
		error = Wire.endTransmission();
		if (error == 0){
			lcdAddr = address;
			lcdFound = true;
			break;
		}
	}
}

void init()
{
	spiffs_mount();
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial
	Serial.println("SDK: " + String(system_get_sdk_version()));
	SystemClock.setTimeZone(7);
	attachInterrupt(INT_PIN, interruptHandler, CHANGE);

#ifdef LCD_DISPLAY
	//WDT.enable(false); // First (but not the best) option: fully disable watch dog timer
	Wire.pins(5, 4);
	scanBus();
	if (lcdFound){
		lcd = new LiquidCrystal_I2C(lcdAddr, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
		lcd->begin(16, 2);
		lcd->backlight();
		lcd->createChar(1, icon_termometer);
		lcd->createChar(2, icon_water);

		lcdSystemStarting();
	}else{
		statusTimer.initializeMs(100, blink).start();
	}

#endif

	dht = new DHT(2, DHT22, 30);
	dht->begin();

	pinMode(CHANNEL1, OUTPUT);
	pinMode(CHANNEL2, OUTPUT);

//#ifndef LCD_DISPLAY
	pinMode(CONF_PIN, OUTPUT);
//#endif
	allOutputOff();
	BrokerSettings.load();

	getTopic = "devices/" + BrokerSettings.user_name + "/" + BrokerSettings.token+"/get";
	setTopic = "devices/"+ BrokerSettings.user_name + "/" + BrokerSettings.token+"/set";
	// check wifi station config
	WifiStation.enable(true);
	stationConnect();
	// wifi scan access point
	WifiStation.startScan(networkScanCompleted);
	// Run WEB server on system ready
	System.onReady(startServers);
}
