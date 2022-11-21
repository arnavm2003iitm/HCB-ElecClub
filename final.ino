// ===========================================
//          MQTT Client Definitions
// ===========================================

TaskHandle_t mqttClient;

#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

char msgmqtt[50];

long last_time = millis();
long time_diff = 0;

const char* ssid = "arnav_mob";
const char* pass = "arnav123";

const char* ip_add = "192.168.117.27";
int port = 1883;

// ==========================================
//           Reed Matrix Definitions
// ==========================================
// Major assumption: the move will be done under 2 seconds

TaskHandle_t readBitMat;

int decoder[3] = {2, 4, 16}; // left to right -> LSB to MSB
int mux_sel[3] = {17, 5, 18}; // left to right -> LSB to MSB
int mux_out = 19;

bool moved = 0;

bool bitMat[8][8] = {{1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1}};
bool bitMatLast[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};
bool bitMatInit[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};
bool bitMatFinal[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};

long initial_time = 0;
long current_time = 0;

void setup() 
{
  Serial.begin(115200);

  // ============= WiFi Set up =============

  WiFi.disconnect();
  delay(3000);

  Serial.println("START");
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.println(".");
  }

  Serial.println("Connected");

  // ============= MQTT Client Set up ========

  client.setServer(ip_add, port);
  client.setCallback(callback);

  xTaskCreatePinnedToCore(
    mqttClient_code,
    "MQTT Client",
    10000,
    NULL,
    0,
    &mqttClient,
  1);

  // ========== Reed Matrix Set up =========
  
  for(int i = 0; i < 3; i++)
  {
    pinMode(decoder[i], OUTPUT);
    pinMode(mux_sel[i], OUTPUT);
  }
  pinMode(mux_out, INPUT);

  //assign the core-0 to the task of reading bit matrix
  xTaskCreatePinnedToCore(
    readBitMat_code,
    "Read Bit-Matrix",
    10000,
    NULL,
    0,
    &readBitMat,
    0);
}

void loop() 
{  
  /*for(int i = 0; i < 8; i++)
  {
    for(int j = 0; j < 8; j++)
    {
      Serial.print(bitMat[i][j]);
      Serial.print(" "); 
    }
    Serial.println();
  }

  Serial.println();
  delay(500);*/
}

// CHECK : In which cores are the functions callback() and reconnectmqttserver() running? Use xPortGetCoreID()
 
void mqttClient_code(void * pvParameters)
{
  Serial.print("mqttClient in core ");
  Serial.println(xPortGetCoreID());

  while(true)
  {
    WiFi.mode(WIFI_STA);

    if(!client.connected())  
      reconnectmqttserver();

    client.loop();
  }
}

void readBitMat_code(void * pvParameters)
{
  Serial.print("readBitMat in core ");
  Serial.println(xPortGetCoreID());

  const TickType_t processDelay = 10 / portTICK_PERIOD_MS; // Task is halted for 10ms after every execution
  
  while(true)
  {
    current_time = millis();

    // this part will be executed every time 
    // Reads from the bit-matrix, stores it into bitMat and shifts the data of that variable to bitMatLast
    for(int i = 0; i < 8; i++)
    {
      for(int k = 1; k <= 3; k++)
        digitalWrite(mux_sel[k - 1], (i % (int)pow(2, k) > (int)pow(2, k - 1) - 1) ? 1 : 0);
    
      for(int j = 0; j < 8; j++)
      {
        for(int k = 1; k <= 3; k++)
          digitalWrite(decoder[k - 1], (j % (int)pow(2, k) > (int)pow(2, k - 1) - 1) ? 1 : 0);   

        bitMatLast[i][j] = bitMat[i][j];
        bitMat[i][j] = digitalRead(mux_out);
        
        delayMicroseconds(10);
      }
    }  

    
    // this part will only be executed when moved is not 1
    // Checks if any bit is different in the bitMat from bitMatLast. If it is, then stores bitMatLast to bitMatInit and starts the timer, sets moved to 1
    for(int i = 0; i < 8 && move != 1; i++)
      for(int j = 0; j < 8 && move != 1; j++)
        if(bitMatLast[i][j] != bitMat[i][j])
        {
          moved = 1;
          initial_time = millis();

          for(int k = 0; k < 8; k++)
            for(int l = 0; l < 8; l++)
              bitMatInit[k][l] = bitMatLast[i][j];
        }

    
    // this part will only be executed when move is 1
    // Checks if the timer has reached a time of 2 seconds. If yes then stores bitMat to final.
    if(moved == 1 && (current_time - initial_time >= 2000))
    {
      for(int i = 0; i < 8; i++)
        for(int j = 0; j < 8; j++)
        {
          bitMatFinal[i][j] = bitMat[i][j];
          moved = 0;
        }

      
    }
    

    vTaskDelay(processDelay); // replace with vTaskDelayUntil
  }
}

String coordinateToUCI(int x, int y)
{
  String coordinate = char(x + 97) + String(y + 1);
  return coordinate;
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String MQTT_DATA = "";

  for(int i = 0; i < length; i++)
    MQTT_DATA += (char)payload[i];

  Serial.print(MQTT_DATA);
}

void reconnectmqttserver()
{
  while(!client.connected())
  {
    Serial.print("Attempting MQTT Connection...");

    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if(client.connect(clientId.c_str()))
    {
      Serial.println("Connected");
      client.subscribe("response");
    }
    else
    {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}