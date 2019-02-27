
#include <WiFi.h>
#include <Ticker.h>
#include "DHTesp.h"
////////////////////////////
// Requires 

const char* ssid     = "ESP32-AP";
//min 8 char for password
const char* password = "12345678"; 

//set web server port number to 80
WiFiServer server(80);

//store the HTTP request
String header;

// Place holders
String temp = "10";
String Humidity ="20"; 

// DHT11
const int dhtPin = 18;
DHTesp dht;

TaskHandle_t readSensTaskHandle = NULL;

// Ticker for senser reading
Ticker sensTicker;
const int tickerTime = 20; // Seconds
bool taskEnabled = true;

SemaphoreHandle_t xSemaphore;
TempAndHumidity EnvValues;

// Read values from DHT11
void readSensTask(void *pvParameters)
{
  Serial.println("tempTask loop started");
  while(1)
  {
    if(taskEnabled)
    {
      Serial.println("attempting to read sens");
      // Takes about 250ms
      //takes controll of variable
      xSemaphoreTake(xSemaphore, portMAX_DELAY);
      EnvValues = dht.getTempAndHumidity();
      xSemaphoreGive(xSemaphore);
      delay(20);
      
      // Check for failed attempts
      if (dht.getStatus() != 0)
      {
        Serial.println("DHT11 error status: " + String(dht.getStatusString()));
      }
      
      Serial.println("Temp:" +String(EnvValues.temperature) + " Humidity:" + String(EnvValues.humidity));
    }
    // Pause task and wait for ISR
    vTaskSuspend(NULL);
  }
}

bool initDHTSens()
{
  byte resultVal = 0;
  // Set the lib for the DHT11
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.println("DHT initiated");
  // Start task
  xTaskCreatePinnedToCore(
    readSensTask,
    "readSensTask",
    4000,
    NULL,
    5,
    &readSensTaskHandle,
    1);

    if(readSensTaskHandle == NULL)
    {
      Serial.println("Failed to create readSensTask");
      return false;
    }
    else
    {
      // Start timerfor reading data off sensor
      sensTicker.attach(tickerTime, triggerReadSens);
    }
    return true;
}
// Triggers the reading of DHT11
void triggerReadSens()
{

  if(readSensTask != NULL)
  {
    xTaskResumeFromISR(readSensTaskHandle);
  }
}

void setup() {
  xSemaphore = xSemaphoreCreateMutex();
  Serial.begin(115200);

  Serial.print("Setting AP (Access Point)…");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();

  initDHTSens();
  delay(2000);
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // Send response
          xSemaphoreTake( xSemaphore, portMAX_DELAY);
          int temp = EnvValues.temperature;
          int hmdt = EnvValues.humidity;
          xSemaphoreGive( xSemaphore);
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("</style></head>");
            client.println("<body><h1>ESP32 Web Server</h1>");
            client.println("<p>Sensor Temp:" + String(temp) + "&degC </p>");
            client.println("<p>Sensor Humidity:"+ String(hmdt) + "</p>");

            client.println("</body></html>");
            
            // Blank line at end of HTTP
            client.println();
            
            break;
          } else {
            // Clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {
          // if you got anything else but a carriage return character add to end of current line
          currentLine += c;      
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
