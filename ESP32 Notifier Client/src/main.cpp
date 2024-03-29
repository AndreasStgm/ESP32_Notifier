#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define SWITCH 15
#define LED 4

#define CENTRAL_ID 0
#define USER_ID 1

#define RELEASED 0
#define PRESSED 1
#define LONG_PRESSED 2

typedef struct messageStruct
{
    uint8_t senderID;
    String message;
} messageStruct;

//-------------------------Variable Declarations-------------------------
uint8_t macAddressCentral[] = {0x30, 0xAE, 0xA4, 0x96, 0xC9, 0x38};

bool CalledByCentral = false;

messageStruct incomingMessage;
messageStruct outgoingMessage;

uint8_t debounceTime = 50;
uint8_t buttonState = RELEASED; // 0 = released; 1 = short press; 2 = long press

//-------------------------Function Declarations-------------------------
void shortPressHandler();

void OnDataSent(const uint8_t *sentToMacAddress, esp_now_send_status_t status);

void OnDataRecv(const uint8_t *senderMacAddress, const uint8_t *incomingData, int incomingDataLength);

// ----- ISR's -----
void ButtonStateChangeISR()
{
    static unsigned long last_cycle_interrupt_time = 0;
    unsigned long cycle_interrupt_time = millis();

    if (cycle_interrupt_time - last_cycle_interrupt_time > debounceTime)
    {
        if (buttonState == RELEASED) // we enter the pressed state
        {
            buttonState = PRESSED;
        }
        else if (buttonState == PRESSED) // we enter the release state of a short press
        {
            buttonState = RELEASED;
            shortPressHandler();
        }
    }

    last_cycle_interrupt_time = cycle_interrupt_time;
}

//-------------------------

void setup()
{
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    pinMode(SWITCH, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(SWITCH), ButtonStateChangeISR, CHANGE);

    outgoingMessage.senderID = USER_ID;
    outgoingMessage.message = "Response";

    WiFi.mode(WIFI_MODE_STA);     // to start wifi before initialising ESP-NOW is a requirement
    if (esp_now_init() != ESP_OK) // checking if ESP_NOW successfully started
    {
        Serial.println("Error initialising ESP-NOW");
        return;
    }

    esp_now_peer_info_t newPeer;                     // creating new peer
    memcpy(newPeer.peer_addr, macAddressCentral, 6); // copying given adress into peer_addr
    newPeer.channel = 0;
    newPeer.encrypt = false;
    newPeer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&newPeer) != ESP_OK) // checking if peer was successfully created
    {
        Serial.println("Failed to add peer");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
    if (CalledByCentral == true)
    {
        digitalWrite(LED, !digitalRead(LED));
        delay(100);
    }
    delay(10);
}

//-------------------------Functions-------------------------
void shortPressHandler()
{
    if (CalledByCentral == true)
    {
        Serial.println("Sending Response");
        esp_now_send(macAddressCentral, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
    }
}

void OnDataSent(const uint8_t *sentToMacAddress, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        CalledByCentral = false; // response has been succesfully sent so no longer called
        Serial.println("Delivery Successful");
        digitalWrite(LED, HIGH);
        delay(1000);
        digitalWrite(LED, LOW);
    }
    else
    {
        Serial.println("Delivery Failed");
        for (uint8_t i = 0; i < 6; i++)
        {
            digitalWrite(LED, !digitalRead(LED));
            delay(50);
        }
    }
}

void OnDataRecv(const uint8_t *senderMacAddress, const uint8_t *incomingData, int incomingDataLength)
{
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

    if (incomingMessage.senderID == CENTRAL_ID && incomingMessage.message == "Call")
    {
        CalledByCentral = true;
        Serial.println("Call data received from central");
    }
    else if (incomingMessage.senderID == CENTRAL_ID && incomingMessage.message == "Reset")
    {
        CalledByCentral = false;
        digitalWrite(LED, LOW);
        Serial.println("Reset data received from central");
    }
    else
    {
        Serial.println("Sender not recognized");
    }
}