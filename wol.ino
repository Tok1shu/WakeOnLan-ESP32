#include <WiFi.h>
#include <WiFiUdp.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

const char* WIFI_SSID     = "Explosive Network";
const char* WIFI_PASSWORD = "XXXXX";
const char* BOT_TOKEN     = "XXXXX";
const long  ALLOWED_CHAT_ID = 123123;

byte MAC_ADDR[] = { 0x9C, 0x6B, 0x00, 0x98, 0x17, 0x67 };
IPAddress BROADCAST_IP(192, 168, 1, 255);

#define LED_PIN 2
#define WOL_PORT 9
#define CHECK_INTERVAL 2000

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
WiFiUDP udp;
unsigned long lastCheck = 0;

void sendWoL() {
  byte packet[102];
  for (int i = 0; i < 6; i++) packet[i] = 0xFF;
  for (int i = 1; i <= 16; i++)
    for (int j = 0; j < 6; j++)
      packet[i * 6 + j] = MAC_ADDR[j];
  udp.beginPacket(BROADCAST_IP, WOL_PORT);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
}

void blinkLED(int times, int ms = 150) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(ms);
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
  }
}

void handleMessages(int count) {
  for (int i = 0; i < count; i++) {
    telegramMessage& msg = bot.messages[i];
    if (msg.chat_id.toInt() != ALLOWED_CHAT_ID) {
      bot.sendMessage(msg.chat_id, "No access", "");
      continue;
    }
    String text = msg.text;
    String name = msg.from_name;
    if (text == "/wake") {
      sendWoL();
      blinkLED(3);
      bot.sendMessage(msg.chat_id, "OK", "");
    } else if (text == "/start" || text == "/help") {
      String help = "🤖 *ESP32 WoL Bot*\n\n";
      help += "/wake — wake up, Walter\n";
      help += "/status — status ESP32\n";
      help += "/check — check if PC is online\n";
      help += "/help — this msg";
      bot.sendMessage(msg.chat_id, help, "Markdown");
    } else if (text == "/status") {
      String info = "📡 *status ESP32*\n\n";
      info += "WiFi: " + String(WiFi.RSSI()) + " dBm\n";
      info += "IP: " + WiFi.localIP().toString() + "\n";
      info += "Uptime: " + String(millis() / 1000) + " sec";
      bot.sendMessage(msg.chat_id, info, "Markdown");
      blinkLED(1);
    } else if (text == "/check") {
      WiFiClient checkClient;
      if (checkClient.connect("192.168.1.100", 1212)) {
        checkClient.print("GET / HTTP/1.0\r\nHost: 192.168.1.100\r\n\r\n");
        delay(500);
        String response = checkClient.readString();
        if (response.indexOf("OK") >= 0) {
          bot.sendMessage(msg.chat_id, "PC is up", "");
        } else {
          bot.sendMessage(msg.chat_id, "PC responded but something is wrong", "");
        }
        checkClient.stop();
      } else {
        bot.sendMessage(msg.chat_id, "PC is offline", "");
      }
    } else {
      bot.sendMessage(msg.chat_id, "Unknown command. Try /help", "");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    blinkLED(1, 300);
  }
  blinkLED(5, 100);
  client.setInsecure();
  udp.begin(WOL_PORT);
}

void loop() {
  if (millis() - lastCheck > CHECK_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      int newMessages = bot.getUpdates(bot.last_message_received + 1);
      if (newMessages) handleMessages(newMessages);
    } else {
      WiFi.reconnect();
    }
    lastCheck = millis();
  }
}
