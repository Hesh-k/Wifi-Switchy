#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <TimeLib.h>
#include <EEPROM.h>

const char* ssid = "Alfa";
const char* password = "12345677";

ESP8266WebServer server(80);

WiFiUDP udp;
unsigned int localPort = 2390;
IPAddress timeServerIP;
const char* NTPServerName = "time.nist.gov";

const float timeZone = 5.5;  // Add your location's pst

int ledPin = 2;
bool ledState = false;
int onHour = 0;
int onMinute = 0;
int offHour = 0;
int offMinute = 0;
bool timerEnabled = false;

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Read timer values from EEPROM
  EEPROM.begin(1024);
  onHour = EEPROM.read(0);
  onMinute = EEPROM.read(1);
  offHour = EEPROM.read(2);
  offMinute = EEPROM.read(3);
  timerEnabled = (bool)EEPROM.read(4);
  EEPROM.end();

  udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);  // resync every hour
  server.on("/", handleRoot);


  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/timer", handleTimer);
  server.begin();
}

void loop() {
  server.handleClient();

  if (timerEnabled) {
    if (hour() == onHour && minute() == onMinute) {
      digitalWrite(ledPin, HIGH);
    } else if (hour() == offHour && minute() == offMinute) {
      digitalWrite(ledPin, LOW);
    }
  }
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>LED Control</h1>";
  html += "<p>Current time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()) + "</p>";
  html += "<p>LED is currently " + String(ledState ? "on" : "off") + "</p>";
  html += "<form method='POST' action='/timer'>";
  html += "<p><label>On time:</label> <input type='time' name='on_time_input' value='" + formatTime(onHour, onMinute) + "'></p>";
  html += "<p><label>Off time:</label> <input type='time' name='off_time_input' value='" + formatTime(offHour, offMinute) + "'></p>";
  html += "<p><label>Timer enabled:</label> <input type='checkbox' name='timer_enabled_input' " + String(timerEnabled ? "checked" : "") + "></p>";
  html += "<p><input type='submit' name='save_button' value='Save'></p>";
  html += "</form>";
  html += "<p><a href='/on'><button>Turn LED off</button></a></p>";
  html += "<p><a href='/off'><button>Turn LED on</button></a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


void handleOn() {
  digitalWrite(ledPin, HIGH);
  ledState = true;
  server.send(200, "text/plain", "LED turned on");
}

void handleOff() {
  digitalWrite(ledPin, LOW);
  ledState = false;
  server.send(200, "text/plain", "LED turned off");
}

void handleTimer() {
  onHour = server.arg("on_time_input").substring(0, 2).toInt();
  onMinute = server.arg("on_time_input").substring(3, 5).toInt();
  offHour = server.arg("off_time_input").substring(0, 2).toInt();
  offMinute = server.arg("off_time_input").substring(3, 5).toInt();
  timerEnabled = server.hasArg("timer_enabled_input");

  // Write timer values to EEPROM
  EEPROM.begin(1024);
  EEPROM.write(0, onHour);
  EEPROM.write(1, onMinute);
  EEPROM.write(2, offHour);
  EEPROM.write(3, offMinute);
  EEPROM.write(4, (byte)timerEnabled);
  EEPROM.end();

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}



String formatTime(int hour, int minute) {
  String result = "";
  if (hour < 10) {
    result += "0";
  }
  result += String(hour) + ":";
  if (minute < 10) {
    result += "0";
  }
  result += String(minute);
  return result;
}

time_t getNtpTime() {
  while (udp.parsePacket() > 0)
    ;
  Serial.println("Requesting time from NTP server");
  if (WiFi.hostByName(NTPServerName, timeServerIP)) {
    Serial.println("Time server IP address: ");
    Serial.println(timeServerIP);
    sendNTPpacket(timeServerIP);
    unsigned long timeout = millis() + 1000;
    while (!udp.available() && millis() < timeout)
      ;
    if (udp.available()) {
      uint8_t buffer[48];
      udp.read(buffer, 48);
      unsigned long secondsSince1900 = buffer[40] << 24 | buffer[41] << 16 | buffer[42] << 8 | buffer[43];
      return (time_t)secondsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    } else {
      Serial.println("No response from NTP server");
      return 0;
    }
  } else {
    Serial.println("DNS lookup failed for NTP server");
    return 0;
  }
}

void sendNTPpacket(IPAddress& address) {
  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, 48);
  udp.endPacket();
}
