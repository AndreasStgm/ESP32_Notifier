# ESP32_Notifier

Uses multiple ESP32's in different locations. One central ESP sends out a ping to all other clients. Clients each have a butten to reply to ping and send a ping back to ACK. Status of ACK's can be seen on central ESP32.

The user ID's are the following:
CENTRAL_ID = 0;
ANDREAS_ID = 1;
JASPER_ID = 2;
BART_ID = 3

Features to be added:

0. ~~Button debouncing (already added on clients) (debouncing seems to sometimes cause presses not to register)~~
1. ~~Long press to select only one user to send message to.~~
2. PWM to dim the led brightness
3. OTA updates
