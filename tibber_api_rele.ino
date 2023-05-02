//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
// Denne koden henter morgendagens priser fra tibber sitt API, regner ut gjennomsnittsprisen for dagen og lukker en releutgang (PIN 5 (D1) blir HØY) basert på om prisen overstiger X antall //
// øre over snitt i en eller flere timer i løpet av dagen. Du kan se resultatet i serial output eller på en nettside publisert på samme IP som ESP8266.                                 //
// Pristerskelen for aktivering er på 10øre (0.10kr) pr. kWh. Pristerskelen kan endres via nettsiden                                                                                    //
// Sjekk at SHA1 fingeravtrykket er riktig ifht. Tibber sin API-host hvis du får 400-feil. API-Kallet skjer klokken 13.50 hver dag. (morgendagens strømpriser blir publisert ca. 13.00) //
// Jeg har kommentert ut en del prints til serial jeg har brukt til debugging, men jeg har ikke fjernet de i tilfelle noen andre får bruk for de. 
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//


//Includes
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <TZ.h>

//Konstanter
const int relay = 5;
const char* ssid =  "SSID"; //Bytt til din WiFi SSID
const char* pass =  "PASSORD";//Bytt til ditt WiFi-passord
const char* fingerprint = "01 E9 FC DD 05 1D 0B C3 21 DB 38 0E 8C C1 10 4E 38 CF 40 DF";
const char* host = "https://api.tibber.com/v1-beta/gql";

//Ekstravariabler
bool thresholdChanged = false;
const int dailyActivationTime = 13 * 3600 + 50 * 60;
float priceThreshold = 0.10;
unsigned long lastActivationCheck = 0;
bool firstRun = true;
float avgPrice;
String intervals;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Webserver på port 80
ESP8266WebServer server(80);

//Sjekk tilstanden til releutgangen
void printRelayState() {
  bool state = digitalRead(relay);
}

//Klokke
void updateSystemTime() {
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();
  timeval tv = {currentTime, 0};
  settimeofday(&tv, nullptr);
}

//Leser payload fra API-kallet og returnerer tidsintervallene hvor prisene er X over gjennomsnittsprisen
void getTimeIntervals(String payload) {
  StaticJsonDocument<2048> doc;
  deserializeJson(doc, payload);

  int numHomes = doc["data"]["viewer"]["homes"].size();
  JsonObject priceInfo;

  for (int i = 0; i < numHomes; i++) {
    if (!doc["data"]["viewer"]["homes"][i]["currentSubscription"].isNull()) {
      priceInfo = doc["data"]["viewer"]["homes"][i]["currentSubscription"]["priceInfo"];
      break;
    }
  }

  int numEntries = priceInfo["tomorrow"].size();
  float total = 0.0;
  float prices[numEntries];
  String startsAt[numEntries];

  for (int i = 0; i < numEntries; i++) {
    prices[i] = priceInfo["tomorrow"][i]["total"];
    startsAt[i] = priceInfo["tomorrow"][i]["startsAt"].as<const char*>();
    Serial.print("Pris ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(prices[i]);
    total += prices[i];
  }
    printRelayState();

  avgPrice = total / numEntries;
  intervals = "";

  for (int i = 0; i < numEntries; i++) {
    if (prices[i] >= avgPrice + priceThreshold) {
      intervals += startsAt[i].substring(11, 19) + " ";
    }
  }
}

//Funksjon som aktiverer og deaktiverer releutgangen
void setRelayState(bool state) {
  digitalWrite(relay, state ? HIGH : LOW);
 //Serial.print("Rele er ");
 //Serial.println(state ? "PÅ" : "AV");
}

//Sjekker om tiden er innenfor de spesifikke intervallene
bool shouldActivateRelay() {
  time_t currentTime;
  time(&currentTime);
  struct tm *timeInfo = localtime(&currentTime);
  int currentSeconds = timeInfo->tm_hour * 3600 + timeInfo->tm_min * 60 + timeInfo->tm_sec;

  int intervalStartHour, intervalStartMinute, intervalStartSecond;
  char buffer[20];
  int index = 0;

  for (unsigned int i = 0; i < intervals.length(); i++) {
    if (intervals[i] == ' ') {
      buffer[index] = '\0';
      sscanf(buffer, "%02d:%02d:%02d", &intervalStartHour, &intervalStartMinute, &intervalStartSecond);
      int intervalStart = intervalStartHour * 3600 + intervalStartMinute * 60 + intervalStartSecond;

      if (currentSeconds >= intervalStart && currentSeconds < intervalStart + 3600) {
      //Serial.print("Current time: ");
      //Serial.print(timeInfo->tm_hour);
      //Serial.print(":");
      //Serial.print(timeInfo->tm_min);
      //Serial.print(":");
      //Serial.println(timeInfo->tm_sec);

      //Serial.print("Interval: ");
      //Serial.print(intervalStartHour);
      //Serial.print(":");
      //Serial.print(intervalStartMinute);
      //Serial.print(":");
      //Serial.println(intervalStartSecond);

      //Serial.println("Should activate relay");
        return true;
      }
      index = 0;
    } else {
      buffer[index++] = intervals[i];
    }
  }
  return false;
}

//Webside
void handleRoot() {
  String html = R"(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f0f0f0;
        margin: 0;
        padding: 0;
      }
      .container {
        max-width: 800px;
        margin: 0 auto;
        padding: 1rem;
        background-color: #fff;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
      }
      h1 {
        font-size: 1.5rem;
        text-align: center;
        color: #333;
      }
      p {
        font-size: 1rem;
        line-height: 1.6;
        color: #333;
        text-align: center;
      }
      form {
        display: flex;
        flex-direction: column;
        align-items: center;
      }
      label {
        font-weight: bold;
        margin-bottom: 0.5rem;
      }
      input[type='text'] {
        font-size: 1rem;
        padding: 0.25rem 0.5rem;
        border: 1px solid #ccc;
        border-radius: 3px;
        margin-bottom: 1rem;
      }
      input[type='submit'] {
        font-size: 1rem;
        background-color: #007BFF;
        color: #fff;
        padding: 0.5rem 1rem;
        border: none;
        border-radius: 3px;
        cursor: pointer;
      }
      input[type='submit']:hover {
        background-color: #0056b3;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Tibber API Data</h1>
      <p>Gjennomsnittspris: <br>)";
  html += String(avgPrice);
  html += R"(</p>
      <p>Tider hvor prisen er over gjennomsnitt + pristerskel: <br>)";
  html += intervals;
  html += R"(</p>
      <form method='POST' action='/threshold'>
        <label for='threshold'>Pristerskel i kroner:</label>
        <input type='text' id='threshold' name='threshold' value=')";
  html += String(priceThreshold);
  html += R"('><br>
      <input type='submit' value='Sett verdi'>
      </form>
      <br>
      <form method='POST' action='/test_relay'>
      <input type='submit' value='Test Rele'>
      </form>
    </div>
  </body>
  </html>
  )";
  server.send(200, "text/html", html);
}


//Aktiver releutgangen i 30 sek
void activateRelayFor10Seconds() {
  setRelayState(true);
  delay(30000);
  setRelayState(false);
}

//Test rele fra nettsiden
void handleTestRelay() {
  if (server.method() == HTTP_POST) {
    activateRelayFor10Seconds();
    String html = "<html><body>";
    html += "<h1>Rele Test</h1>";
    html += "<p>Releutgangen ble aktivert i 30 sekunder.</p>";
    html += "<a href=\"/\">Tilbake</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    return;
  }
  server.send(400, "text/plain", "Bad Request");
}

//Pristerskel handler
void handleThreshold() {
  if (server.method() == HTTP_POST) {
    String newThresholdStr = server.arg("threshold");
    float newThreshold = newThresholdStr.toFloat();
    if (newThreshold != priceThreshold) {
      priceThreshold = newThreshold;
      thresholdChanged = true;
      String html = "<html><body>";
      html += "<h1>Ny pristerskel satt</h1>";
      html += "<p>Den nye verdien er: " + String(priceThreshold) + "</p>";
      html += "<a href=\"/\">Tilbake</a>";
      html += "</body></html>";
      server.send(200, "text/html", html);
      return;
    }
  }
  server.send(400, "text/plain", "ugyldig forespørsel");
}

void setup() {
  Serial.begin(9600);
  delay(10);
  
  Serial.println("Kobler til ");
  Serial.println(ssid); 
 
  WiFi.begin(ssid, pass); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi tilkoblet");
  Serial.print("IP addresse: ");
  Serial.println(WiFi.localIP());
  configTzTime(TZ_Europe_Oslo, "pool.ntp.org", "time.nist.gov");
  server.on("/", handleRoot);
  server.on("/threshold", HTTP_POST, handleThreshold);
  server.on("/test_relay", HTTP_POST, handleTestRelay);
  server.begin();

  timeClient.begin();
  timeClient.setTimeOffset(0); // Set your time zone offset in seconds, e.g., 2 * 3600 for UTC+2
  timeClient.setUpdateInterval(3600000); // Update interval in milliseconds (1 hour in this example)

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

}

void loop() {
  char timeBuffer[32];
  server.handleClient();
  updateSystemTime();
  time_t currentTime;
  time(&currentTime);
  struct tm *timeInfo = localtime(&currentTime);
  int currentSeconds = timeInfo->tm_hour * 3600 + timeInfo->tm_min * 60 + timeInfo->tm_sec;
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo);
//  Serial.print("Current time: ");
//  Serial.println(timeBuffer);

  if ((firstRun || (currentSeconds >= dailyActivationTime && currentSeconds < dailyActivationTime + 60) || thresholdChanged) && millis() - lastActivationCheck > 60000) {
    lastActivationCheck = millis();

    WiFiClientSecure client;
    client.setFingerprint(fingerprint);

    //Gjør API-Kallet
    HTTPClient http;
    http.begin(client, host);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer TIBBER_API_NØKKEL"); //Bytt til din API-nøkkel
    int httpCode = http.POST("{\"query\":\"query { viewer { homes { currentSubscription { priceInfo { tomorrow { total startsAt } } } } } }\"}");

    //Respons fra API (i JSON format)
    String payload = http.getString();

    if (httpCode == HTTP_CODE_OK) {

      getTimeIntervals(payload);
      Serial.print("Gjennomsnittspris: ");
      Serial.println(avgPrice);
      Serial.print("Timer med pris over gjennomsnitt: ");
      Serial.println(intervals);

      firstRun = false;
      thresholdChanged = false;

    } else {
      Serial.print("Feilkode: ");
      Serial.println(httpCode);
    }

    http.end();
  }

  //Sjekk om releet skal være aktivert.
  bool shouldActivate = shouldActivateRelay();
  //Serial.print("Skal rele være aktivert?: ");
 // Serial.println(shouldActivate);
  if (shouldActivate) {
    setRelayState(true);
  } else {
    setRelayState(false);
  }

  delay(10000);
}
