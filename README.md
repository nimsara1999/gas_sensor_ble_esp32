# ESP32-S3 Gateway

## Release Notes

## V S3-1.0.3

- Changed api path to new server endpoint.
- Receive API key from the Android app and use it for authentication when sending data.
- Turn the LED on and keep when searching automatically for an Wi-Fi network
- Increased delay between data payload packets to 10000 ms.
- Added delay of 2000 ms after an failed attempt to send data to the server.
- When sending a data packet, retry until 4 failed attempts and ignore the packet. Show a red pulse with LED.

## LED Blinking Patterns

### Color Codes

- **Cyan (Green + Blue):** EEPROM
- **Yellow (Red + Green):** Firmware
- **Blue:** AP / Wi-Fi

---

### Error Patterns

- **Low free heap memory:**

  - Red - 8 pulses of 100ms on and off time

- **Connection to server failed (single time):**

  - Red - 2 short pulses

- **Connection to server failed (for n times):**
  - Red - _n_ number of medium-length (400ms) pulses

---

### Success Patterns

- **Data sent successfully:**

  - Green - 2 short pulses (one of 30ms and the next of 60ms)

- **Successfully connected to a WiFi network:**
  - Blue (300ms) and then Green (300ms) two times

---

### Info Patterns

- **Automatically put to AP mode when starting:**

  - Blue - 3 short pulses (30ms)

- **Single try to connect to a WiFi network:**

  - White - 1 short pulse (50ms)

- **Starting Gateway:**

  - White, Cyan, Magenta, Yellow, White short pulses (100ms)

- **Ready to receive data:**

  - White, Green, White, Green, White, Green

- **AP Mode:**
  - Blue - Long (1000ms) blink

---

### Mixed Patterns

- **Connection to WiFi network failed after trying multiple attempts:**

  - Blue (300ms) and then Red (300ms) two times

- **Searching for a SYNCed sensor started:**

  - 3 Purple LED short pulses (50ms)

- **SYNCed sensor found and sent to the App for confirmation:**
  - 2 Purple LED short pulses (100ms)

---

### EEPROM Patterns

- **Data saved to / loaded from EEPROM:**
  - Cyan long single pulse followed by two Green short pulses

---

### Firmware Update Patterns

- **FW Update check failed (due to no internet):**

  - 2 Red short pulses (30ms)

- **FW Update checked, no updates available:**

  - Yellow long (400ms) pulse followed by a single short Green pulse (70ms)

- **FW Update checked, updates available. Starting update...:**

  - 5 Yellow long pulses (800ms) for 5 seconds

- **Update failed:**
  - Long Yellow pulse (500ms) followed by 3 Red short pulses (50ms)
