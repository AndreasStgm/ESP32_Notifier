#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

const bool VISITOR_ENABLED = true;

const uint8_t SEND_SWITCH = 32;
const uint8_t RESET_SWITCH = 33;
const uint8_t STATUS_LED = 14;
const uint8_t JASPER_LED = 25;
const uint8_t ANDREAS_LED = 26;
const uint8_t BART_LED = 27;

const uint8_t CENTRAL_ID = 0;
const uint8_t ANDREAS_ID = 1;
const uint8_t JASPER_ID = 2;
const uint8_t BART_ID = 3;

enum class ButtonState
{
    RELEASED,
    PRESSED,
    LONG_PRESSED
};

enum class DeviceState
{
    WAITING,
    SHORT_PRESS,
    LONG_PRESS,
    RESET
};

typedef struct messageStruct
{
    uint8_t senderID;
    String message;
} messageStruct;

//-------------------------Variable Declarations-------------------------
const uint8_t debounceTime = 50;
const uint16_t longPressTime = 1000;
ButtonState sendButtonState = ButtonState::RELEASED; // 0 = released; 1 = short press; 2 = long press
ButtonState resetButtonState = ButtonState::RELEASED;

DeviceState currentState = DeviceState::WAITING;

const uint8_t macAddressAndreasRoom[] = {0x30, 0xAE, 0xA4, 0x96, 0xC8, 0x90};
const uint8_t macAddressAndreasStudy[] = {0x54, 0x43, 0xB2, 0xAB, 0xE9, 0xD0};
const uint8_t macAddressJasper[] = {0x30, 0xAE, 0xA4, 0x96, 0xEB, 0x48};
const uint8_t macAddressBart[] = {0x30, 0xAE, 0xA4, 0x9B, 0xB4, 0x14};
const uint8_t macAddressKristiina[] = {0x10, 0x52, 0x1C, 0x64, 0x34, 0x50};

bool responseAndreas = false;
bool responseJasper = false;
bool responseBart = false;

messageStruct incomingMessage;
messageStruct outgoingMessage;

const uint8_t ledArray[] = {JASPER_LED, BART_LED, ANDREAS_LED};
const uint8_t idArray[] = {JASPER_ID, BART_ID, ANDREAS_ID};
uint8_t currentPositionInArrays = 0;
bool oneUserSelectionMode = false;

//-------------------------Function Declarations-------------------------
void sendStateChangeISR();
void resetStateChangeISR();

void OnDataSent(const uint8_t *sentToMacAddress, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *senderMacAddress, const uint8_t *incomingData, int incomingDataLength);

void stateHandler();
void shortPressHandler();
void longPressHandler();
void resetHandler();
void waitingHandler();

void stateComplete();
void flashStatusLed(uint8_t flashAmount, uint16_t delayTime);

//=========================SETUP=========================

void setup()
{
    Serial.begin(115200);
    pinMode(STATUS_LED, OUTPUT);
    pinMode(SEND_SWITCH, INPUT_PULLUP);
    pinMode(RESET_SWITCH, INPUT_PULLUP);
    pinMode(ANDREAS_LED, OUTPUT);
    pinMode(JASPER_LED, OUTPUT);
    pinMode(BART_LED, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(SEND_SWITCH), sendStateChangeISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RESET_SWITCH), resetStateChangeISR, CHANGE);

    outgoingMessage.senderID = CENTRAL_ID;
    outgoingMessage.message = "Call";

    WiFi.mode(WIFI_MODE_STA);     // to start wifi before initialising ESP-NOW is a requirement
    if (esp_now_init() != ESP_OK) // checking if ESP_NOW successfully started
    {
        Serial.println("Error initialising ESP-NOW");
        return;
    }

    esp_now_peer_info_t newPeer;                         // creating new peer
    memcpy(newPeer.peer_addr, macAddressAndreasRoom, 6); // copying given adress into peer_addr
    newPeer.channel = 0;
    newPeer.encrypt = false;
    newPeer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&newPeer) != ESP_OK) // checking if peer was successfully created
    {
        Serial.println("Failed to add Andreas bedroom peer");
        return;
    }
    memcpy(newPeer.peer_addr, macAddressAndreasStudy, 6);
    if (esp_now_add_peer(&newPeer) != ESP_OK)
    {
        Serial.println("Failed to add Andreas study room peer");
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
    if (VISITOR_ENABLED)
    {
        memcpy(newPeer.peer_addr, macAddressKristiina, 6);
        if (esp_now_add_peer(&newPeer) != ESP_OK)
        {
            Serial.println("Failed to add Kristiina peer");
            return;
        }
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
}

//=========================LOOP=========================
void loop()
{
    stateHandler();

    delay(1);
}

//-------------------------ISR's-------------------------
void sendStateChangeISR()
{
    static unsigned long last_cycle_interrupt_time = 0;
    unsigned long cycle_interrupt_time = millis();

    if (cycle_interrupt_time - last_cycle_interrupt_time > longPressTime)
    {
        if (sendButtonState == ButtonState::RELEASED) // we enter the pressed state
        {
            sendButtonState = ButtonState::PRESSED;
        }
        else if (sendButtonState == ButtonState::PRESSED) // we enter the release state of a long press
        {
            sendButtonState = ButtonState::RELEASED;

            currentState = DeviceState::LONG_PRESS;
        }
    }
    else if (cycle_interrupt_time - last_cycle_interrupt_time > debounceTime)
    {
        if (sendButtonState == ButtonState::RELEASED) // we enter the pressed state
        {
            sendButtonState = ButtonState::PRESSED;
        }
        else if (sendButtonState == ButtonState::PRESSED) // we enter the release state of a short press
        {
            sendButtonState = ButtonState::RELEASED;

            currentState = DeviceState::SHORT_PRESS;
        }
    }

    last_cycle_interrupt_time = cycle_interrupt_time;
}

void resetStateChangeISR()
{
    static unsigned long last_cycle_interrupt_time = 0;
    unsigned long cycle_interrupt_time = millis();

    if (cycle_interrupt_time - last_cycle_interrupt_time > debounceTime)
    {
        if (resetButtonState == ButtonState::RELEASED) // we enter the pressed state
        {
            resetButtonState = ButtonState::PRESSED;
        }
        else if (resetButtonState == ButtonState::PRESSED) // we enter the release state of a short press
        {
            resetButtonState = ButtonState::RELEASED;

            currentState = DeviceState::RESET;
        }
    }

    last_cycle_interrupt_time = cycle_interrupt_time;
}

//-------------------------Functions-------------------------
void OnDataSent(const uint8_t *sentToMacAddress, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        Serial.println("Delivery Successful");
        flashStatusLed(1, 100);
    }
    else
    {
        Serial.println("Delivery Failed");
        flashStatusLed(1, 500);
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

void stateHandler()
{
    switch (currentState)
    {
    case DeviceState::SHORT_PRESS:
        shortPressHandler();
        break;

    case DeviceState::LONG_PRESS:
        longPressHandler();
        break;

    case DeviceState::RESET:
        resetHandler();
        break;

    case DeviceState::WAITING:
        waitingHandler();
        break;

    default:
        Serial.println("<StateHandler function> default case: you should not be here.");
        break;
    }
}

//-------------------------SHORT PRESS-------------------------
void shortPressHandler()
{
    if (oneUserSelectionMode)
    {
        Serial.println("Selecting next user to send call to.");
        currentPositionInArrays++;
        if (currentPositionInArrays >= sizeof(ledArray))
        {
            currentPositionInArrays = 0;
        }

        for (uint8_t i = 0; i < sizeof(ledArray); i++)
        {
            digitalWrite(ledArray[i], LOW);
        }
    }
    else
    {
        Serial.println("> Sending call to all users.");
        outgoingMessage.message = "Call";
        esp_now_send(0, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage)); // when peer_addr = 0 -> send to all known peers
    }

    stateComplete();
}

//-------------------------LONG PRESS-------------------------
void longPressHandler()
{
    if (!oneUserSelectionMode)
    {
        Serial.println("> One user send mode started.");
        oneUserSelectionMode = true;
    }
    else
    {
        outgoingMessage.message = "Call";

        switch (idArray[currentPositionInArrays])
        {
        case ANDREAS_ID:
            esp_now_send(macAddressAndreasRoom, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
            esp_now_send(macAddressAndreasStudy, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
            break;
        case JASPER_ID:
            esp_now_send(macAddressJasper, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
            if (VISITOR_ENABLED)
            {
                esp_now_send(macAddressKristiina, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
            }
            break;
        case BART_ID:
            esp_now_send(macAddressBart, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
            break;
        default:
            Serial.println("<One user send switch> default case: you should not be here.");
            break;
        }

        Serial.println("> One user send mode ended.");
        oneUserSelectionMode = false;
        currentPositionInArrays = 0;
    }

    stateComplete();
}

//-------------------------RESET-------------------------
void resetHandler()
{
    Serial.println("> Resetting states");

    outgoingMessage.message = "Reset";
    esp_now_send(0, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
    responseAndreas = false;
    responseJasper = false;
    responseBart = false;

    oneUserSelectionMode = false;
    currentPositionInArrays = 0;

    sendButtonState = ButtonState::RELEASED;
    resetButtonState = ButtonState::RELEASED;

    stateComplete();
}

//-------------------------WAITING-------------------------
void waitingHandler()
{
    if (oneUserSelectionMode)
    {
        digitalWrite(ledArray[currentPositionInArrays], !digitalRead(ledArray[currentPositionInArrays]));
        delay(500);
    }
    else
    {
        digitalWrite(ANDREAS_LED, responseAndreas);
        digitalWrite(JASPER_LED, responseJasper);
        digitalWrite(BART_LED, responseBart);
    }
}

//-------------------------Helper Functions-------------------------
void stateComplete()
{
    currentState = DeviceState::WAITING;

    digitalWrite(STATUS_LED, LOW);
}

void flashStatusLed(uint8_t flashAmount, uint16_t delayTime)
{
    for (uint8_t i = 0; i < flashAmount; i++)
    {
        digitalWrite(STATUS_LED, HIGH);
        delay(delayTime);
        digitalWrite(STATUS_LED, LOW);
        delay(100);
    }
}