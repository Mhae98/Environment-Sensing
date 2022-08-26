#include <WiFiNINA_Generic.h>
#include <EEPROM.h>
#include "bsec.h"
#include "secrets.h"

// Helper functions declarations
void checkIaqSensorStatus(void);
void errLeds(void);

// Variables for bsec
#define STATE_SAVE_PERIOD  UINT32_C(24 * 60 * 60 * 1000) // 1 time a day; hours * minutes * senconds * milliseconds
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};
Bsec iaqSensor;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
bool first_update;
String output;
const String descriptions[11] = {
  "Temperature raw",
  "Temperature",
  "Humidity raw",
  "Humidity",
  "Pressure",
  "Gas resistance",
  "IAQ",
  "IAQ accuracy",
  "Static IAQ",
  "CO2 equivalent",
  "Breath VOC equivalent"
};
String outVals[11] = {"", "", "", "", "", "", "", "", "", "", ""};

// WIFI information
char ssid[] = WIFI_SSID;        // your network SSID (name)
char pass[] = WIFI_PWD;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)
int status = WL_IDLE_STATUS;
WiFiClient client; 
WiFiServer server(80);

/* =====START===== Code for running server =====START===== */

// Setting up server for core 0
void setup() {
  Serial.begin(115200);
  WiFi.setHostname("Arduino Sensor");
  WiFi.lowPowerMode();
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    // don't continue
    while (true);
  }
  connect_wifi();
  server.begin();                           // start the web server on port 80
}

// Run webserver on core 0
void loop() {
  checkForClient();
}

void checkForClient() {
  check_WIFI_status();
  client = server.available();              // listen for incoming clients
  if (client) {                             // if you get a client,
    Serial.println("Client connected");
    bool send_data = false;
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        if (currentLine.endsWith("GET /DATA")) {
          send_data = true;
        }
        // The HTTP response ends with another blank line:
      }
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();
    if (send_data) {
      output = "";
      for (int i = 0; i < 10; i++) {
        output += outVals[i] + ",";
      }
      output += outVals[10];
      client.print(output);
    }
    else {
      client.print("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"refresh\" content=\"10\"><title>Sensor Log</title></head>");
      client.print("<body bgcolor=\"black\" text=\"#FF8C00\"><h1> Sensor Data </h1>");
      client.print("<table border=\"1\" style=\"width:400px\">");
      client.print("<tr><th style=\"width:50%\">Description</th><th>Value</th></tr>");
      for (int i = 0; i <= 10; i++) {
        client.print("<tr><td>" + descriptions[i] + "</td><td>" + outVals[i] + "</td></tr>");
      }
      client.print("</table></body></html>");
    }
    client.println();
    client.flush();
    Serial.println("Client disconnected");
  }
  client.stop();
}

// Checks if wifi is still connected and reconnects if neccessary
void check_WIFI_status() {
  int wifi_status = WiFi.status();
  if (wifi_status == WL_CONNECTED) {
    return;
  }
  else if (wifi_status == WL_CONNECTION_LOST || wifi_status == WL_DISCONNECTED) {
    WiFi.disconnect();
    connect_wifi();
  }
}

// Connects to in variables specified network
void connect_wifi() {
  status = WiFi.begin(ssid, pass);
  // wait 2 seconds for connection:
  delay(2000);
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 2 seconds for connection:
    delay(2000);
  }
}

/* =====END===== Code for running server =====END===== */

/* =====START===== Code for reading sensor =====START===== */
// Setup 1 initializing bsec on second core
void setup1() {
  Serial.begin(115200);
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length
  Wire.begin();
  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  checkIaqSensorStatus();
  iaqSensor.setConfig(bsec_config_iaq);
  // iaqSensor.setTemperatureOffset(4);
  loadState();

  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Print the header
  output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";
}


// Loop running on second core; Collects data from bme680 sensor
void loop1() {
  if (iaqSensor.run()) { // If new data is available
    outVals[0] = String(iaqSensor.rawTemperature);
    outVals[1] = String(iaqSensor.temperature);
    outVals[2] = String(iaqSensor.rawHumidity);
    outVals[3] = String(iaqSensor.humidity);
    outVals[4] = String(iaqSensor.pressure);
    outVals[5] = String(iaqSensor.gasResistance);
    outVals[6] = String(iaqSensor.iaq);
    outVals[7] = String(iaqSensor.iaqAccuracy);
    outVals[8] = String(iaqSensor.staticIaq);
    outVals[9] = String(iaqSensor.co2Equivalent);
    outVals[10] = String(iaqSensor.breathVocEquivalent);
    updateState();
  } else {
    checkIaqSensorStatus();
  }
}

// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK) {
    if (iaqSensor.status < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.status);
      for (;;) {
        errLeds(); /* Halt in case of failure */
      }
    } else {
      output = "BSEC warning code : " + String(iaqSensor.status);
      }
  }

  if (iaqSensor.bme680Status != BME680_OK) {
    if (iaqSensor.bme680Status < BME680_OK) {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

// Read sensor state from EEPROM
void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

// Save sensor state to EEPROM
void updateState()
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0) {
    if (iaqSensor.iaqAccuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
  } 
  else {
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
    else {
      // reset counter to 0 after overflow of millis()
      if (stateUpdateCounter > 1 && (stateUpdateCounter - 1) * STATE_SAVE_PERIOD > millis()) {
        stateUpdateCounter = 0;
      }
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);
      checkIaqSensorStatus();
      for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
        EEPROM.write(i + 1, bsecState[i]);
      }
  
      EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
      EEPROM.commit();
      update = false;
  }
}

/* =====END===== Code for reading sensor =====END===== */
