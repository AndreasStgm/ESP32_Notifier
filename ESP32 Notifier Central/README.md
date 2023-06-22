# ESP32_Notifier

Extra test module has MAC of: 10:52:1C:64:34:50

Needs these lines of code:
`uint8_t macAddressKristiina[] = {0x10, 0x52, 0x1C, 0x64, 0x34, 0x50};`

`memcpy(newPeer.peer_addr, macAddressKristiina, 6);
  if (esp_now_add_peer(&newPeer) != ESP_OK)
  {
    Serial.println("Failed to add Kristiina peer");
    return;
  }`

Client USER_ID needs to be set to 2.
Client SWITCH needs to be set to 15.
Client LED needs to be set to 4.
