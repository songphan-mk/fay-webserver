// #include <ESP8266WiFi.h>
// #include <DNSServer.h>
// #include <ESP8266WebServer.h>
// #include <lwip/opt.h>
// #include <lwip/dns.h>
// #include <lwip/init.h>
// #include <lwip/netif.h>
// #include <lwip/timers.h>

// const char *ssid = "CaptivePortal";  // ชื่อ Wi-Fi ที่ต้องการสร้าง
// const char *password = "";           // รหัสผ่าน Wi-Fi (ถ้าต้องการให้เป็น open network ให้ใส่ "")

// IPAddress apIP(192, 168, 1, 1);
// IPAddress netMsk(255, 255, 255, 0);
// DNSServer dnsServer;
// ESP8266WebServer webServer(80);

// // หน้าดีฟอลต์ของ Captive Portal
// const char *html = "<!DOCTYPE html><html><head><title>Captive Portal</title></head><body><h1>Welcome to Captive Portal</h1></body></html>";

// void handleRoot() {
//   webServer.send(200, "text/html", html);
// }

// void setup() {
//   Serial.begin(115200);
  
//   // เริ่มต้นการเชื่อมต่อ Wi-Fi
//   WiFi.softAP(ssid, password);
//   WiFi.softAPConfig(apIP, apIP, netMsk);
  
//   // กำหนดค่า DHCP lease time (TTL) ให้สั้นลง (ในหน่วยวินาที)
//   wifi_softap_set_dhcps_lease_time(120);  // ตั้งเป็น 2 นาที (120 วินาที)
  
//   // เริ่มต้น DNS Server
//   dnsServer.start(53, "*", apIP);
  
//   // ตั้งค่าเว็บเซิร์ฟเวอร์
//   webServer.on("/", handleRoot);
//   webServer.onNotFound(handleRoot);
//   webServer.begin();
  
//   Serial.println("Captive Portal started.");
// }

// void loop() {
//   dnsServer.processNextRequest();
//   webServer.handleClient();
// }

// // ฟังก์ชันสำหรับตั้งค่า DHCP lease time (TTL)
// void wifi_softap_set_dhcps_lease_time(uint32_t ttl_seconds) {
//   struct dhcps_lease lease;
//   lease.start_ip.addr = (uint32_t) apIP[0] | ((uint32_t) apIP[1] << 8) | ((uint32_t) apIP[2] << 16) | ((uint32_t) apIP[3] << 24);
//   lease.end_ip.addr = lease.start_ip.addr;
//   wifi_softap_set_dhcps_lease(&lease);
//   wifi_softap_set_dhcps_lease_time(ttl_seconds);
// }


#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <uri/UriGlob.h>

/*
   This example serves a "hello world" on a WLAN and a SoftAP at the same time.
   The SoftAP allow you to configure WLAN parameters at run time. They are not setup in the sketch but saved on EEPROM.

   Connect your computer or cell phone to wifi network ESP_ap with password 12345678. A popup may appear and it allow you to go to WLAN config. If it does not then navigate to http://192.168.4.1/wifi and config it there.
   Then wait for the module to connect to your wifi and take note of the WLAN IP it got. Then you can disconnect from ESP_ap and return to your regular WLAN.

   Now the ESP8266 is in your network. You can reach it through http://192.168.x.x/ (the IP you took note of) or maybe at http://esp8266.local too.

   This is a captive portal because through the softAP it will redirect any http request to http://192.168.4.1/
*/

/* Set these to your desired softAP credentials. They are not configurable at runtime */


#ifndef APSSID
#define APSSID "fay-server"
//#define APPSK "12345678"
#endif

const char *softAP_ssid = APSSID;
//const char *softAP_password = APPSK;

/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "esp8266";

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[33] = "";
char password[65] = "";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server

WebServer server(80);

 // หน้าดีฟอลต์ของ Captive Portal
const char *html = "<!DOCTYPE html><html><head><title>Captive Portal</title></head><body><h1>Welcome to Captive Portal</h1></body></html>";

void handleRoot() {
  server.send(200, "text/html", html);
}

/* Soft AP network parameters */
IPAddress apIP(172, 217, 28, 1);
IPAddress netMsk(255, 255, 255, 0);


/** Should I connect to WLAN asap? */
boolean connect;

/** Last time I tried to connect to WLAN */
unsigned long lastConnectTry = 0;

/** Current WLAN status */
unsigned int status = WL_IDLE_STATUS;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(softAP_ssid);
  delay(500);  // Without delay I've seen the IP address blank
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);



  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server.on("/", handleRoot);
  //server.on("/wifi", handleWifi);
  //server.on("/wifisave", handleWifiSave);
  server.on(UriGlob("/generate_204*"), handleRoot);  // Android captive portal. Handle "/generate_204_<uuid>"-like requests. Might be handled by notFound handler.
  //server.on("/fwlink", handleRoot);                  // Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.onNotFound(handleRoot);
  server.begin();  // Web server start
  Serial.println("HTTP server started");
  //loadCredentials();           // Load WLAN credentials from network
  connect = strlen(ssid) > 0;  // Request WLAN connect if there is a SSID
}

void connectWifi() {
  Serial.println("Connecting as wifi client...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int connRes = WiFi.waitForConnectResult();
  Serial.print("connRes: ");
  Serial.println(connRes);
}

void loop() {
  if (connect) {
    Serial.println("Connect requested");
    connect = false;
    connectWifi();
    lastConnectTry = millis();
  }
  {
    unsigned int s = WiFi.status();
    if (s == 0 && millis() > (lastConnectTry + 60000)) {
      /* If WLAN disconnected and idle try to connect */
      /* Don't set retry time too low as retry interfere the softAP operation */
      connect = true;
    }
    if (status != s) {  // WLAN status change
      Serial.print("Status: ");
      Serial.println(s);
      status = s;
      if (s == WL_CONNECTED) {
        /* Just connected to WLAN */
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Setup MDNS responder
        if (!MDNS.begin(myHostname)) {
          Serial.println("Error setting up MDNS responder!");
        } else {
          Serial.println("mDNS responder started");
          // Add service to MDNS-SD
          MDNS.addService("http", "tcp", 80);
        }
      } else if (s == WL_NO_SSID_AVAIL) {
        WiFi.disconnect();
      }
    }
  
  }
  // Do work:
  // DNS
  dnsServer.processNextRequest();
  // HTTP
  server.handleClient();
}