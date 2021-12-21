#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define SEND_SWITCH 32
#define RESET_SWITCH 33
#define STATUS_LED 14
#define JASPER_LED 25
#define ANDREAS_LED 26
#define BART_LED 27

#define CENTRAL_ID 0
#define ANDREAS_ID 1
#define JASPER_ID 2
#define BART_ID 3

typedef struct messageStruct
{
  uint8_t senderID;
  String message;
} messageStruct;

//-------------------------Variable Declarations-------------------------
uint8_t macAddressAndreas[] = {0x30, 0xAE, 0xA4, 0x96, 0xC8, 0x90};
uint8_t macAddressJasper[] = {0x30, 0xAE, 0xA4, 0x96, 0xEB, 0x48};
uint8_t macAddressBart[] = {0x30, 0xAE, 0xA4, 0x9B, 0xB4, 0x14};

bool responseAndreas = false;
bool responseJasper = false;
bool responseBart = false;

messageStruct incomingMessage;
messageStruct outgoingMessage;

//-------------------------Function Declarations-------------------------
void OnDataSent(const uint8_t *sentToMacAddress, esp_now_send_status_t status);

void OnDataRecv(const uint8_t *senderMacAddress, const uint8_t *incomingData, int incomingDataLength);

//-------------------------

void setup()
{
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(SEND_SWITCH, INPUT_PULLUP);
  pinMode(RESET_SWITCH, INPUT_PULLUP);
  pinMode(ANDREAS_LED, OUTPUT);
  pinMode(JASPER_LED, OUTPUT);
  pinMode(BART_LED, OUTPUT);

  outgoingMessage.senderID = CENTRAL_ID;
  outgoingMessage.message = "Call";

  WiFi.mode(WIFI_MODE_STA);     //to start wifi before initialising ESP-NOW is a requirement
  if (esp_now_init() != ESP_OK) //checking if ESP_NOW successfully started
  {
    Serial.println("Error initialising ESP-NOW");
    return;
  }

  esp_now_peer_info_t newPeer;                     //creating new peer
  memcpy(newPeer.peer_addr, macAddressAndreas, 6); //copying given adress into peer_addr
  newPeer.channel = 0;
  newPeer.encrypt = false;
  newPeer.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&newPeer) != ESP_OK) //checking if peer was successfully created
  {
    Serial.println("Failed to add Andreas peer");
    return;
  }
  memcpy(newPeer.peer_addr, macAddressJasper, 6);
  if (esp_now_add_peer(&newPeer) != ESP_OK)
  {
    Serial.println("Failed to add Jasper peer");
    return;
  }
  memcpy(newPeer.peer_addr, macAddressBart, 6);
  if (esp_now_add_peer(&newPeer) != ESP_OK)
  {
    Serial.println("Failed to add Bart peer");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  if (digitalRead(SEND_SWITCH) == LOW)
  {
    Serial.println("Sending Call");
    outgoingMessage.message = "Call";
    esp_now_send(0, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage)); //when peer_addr = 0 -> send to all known peers
  }
  else if (digitalRead(RESET_SWITCH) == LOW)
  {
    Serial.println("Resetting states");
    outgoingMessage.message = "Reset";
    esp_now_send(0, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
    responseAndreas = false;
    responseJasper = false;
    responseBart = false;
  }
  digitalWrite(ANDREAS_LED, responseAndreas);
  digitalWrite(JASPER_LED, responseJasper);
  digitalWrite(BART_LED, responseBart);
  delay(100);
}

//-------------------------Functions-------------------------
void OnDataSent(const uint8_t *sentToMacAddress, esp_now_send_status_t status)
{
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    Serial.println("Delivery Successful");
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
  }
  else
  {
    for (uint8_t i = 0; i < 6; i++)
    {
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    }
  }
}

void OnDataRecv(const uint8_t *senderMacAddress, const uint8_t *incomingData, int incomingDataLength)
{
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

  if (incomingMessage.senderID == ANDREAS_ID && incomingMessage.message == "Response")
  {
    Serial.println("Return data received from Andreas");
    responseAndreas = true;
  }
  else if (incomingMessage.senderID == JASPER_ID && incomingMessage.message == "Response")
  {
    Serial.println("Return data received from Jasper");
    responseJasper = true;
  }
  else if (incomingMessage.senderID == BART_ID && incomingMessage.message == "Response")
  {
    Serial.println("Return data received from Bart");
    responseBart = true;
  }
  else
  {
    Serial.println("Sender of data not recognized");
  }
}