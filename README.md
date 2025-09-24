# 🔥 Firefly Host

The **Firefly Host** is the central ESP32 firmware of the Firefly project.  
It serves the [firefly-ui](https://github.com/KooleControls/firefly-ui) React frontend over Ethernet (or Wi-Fi, with channel restrictions), and bridges communication with multiple [firefly-guest](https://github.com/KooleControls/firefly-guest) ESP32-C3 devices via ESP-NOW.

- Runs a lightweight web server to serve the UI and provide API endpoints  
- Receives button press events from guests and logs them  
- Sends commands (like blink LED) back to guests  
- Acts as the integration point between embedded devices and the web interface  

---

## ⚡ Network Modes

### Recommended: Ethernet
- Use an ESP32 board with Ethernet (e.g. ESP32-Ethernet-Kit, Olimex ESP32-POE, or equivalent).  
- Ethernet keeps Wi-Fi free for ESP-NOW, ensuring reliable guest communication.  
- No interference between serving the UI and handling ESP-NOW packets.  

### Alternative: Wi-Fi
- If Ethernet is unavailable, the host can run on Wi-Fi.  
- In this mode, **the Wi-Fi channel must be fixed** so ESP-NOW and the access point share the same channel.  
- This may reduce stability if many clients are connected simultaneously.  

---

## 🚀 Getting Started

### Prerequisites
- ESP-IDF (v5.5 recommended)
- Ethernet-capable ESP32 board (preferred)  
- For Wi-Fi mode: ability to force AP/station channel

### Build & Flash
```bash
# Clone the repo
git clone https://github.com/KooleControls/firefly-host.git
cd firefly-host

# Build and flash with ESP-IDF
idf.py build flash monitor
```

---

## 📡 How It Works

1. **UI Serving**  
   - Serves [firefly-ui](https://github.com/KooleControls/firefly-ui) files from flash.  
   - Files should be pre-compressed (`.gz`) using `npm run buildgz` in the UI repo.  
   - Upload them (via FTP or OTA) to the ESP32 filesystem.  

2. **Guest Communication**  
   - Receives ESP-NOW packets from [firefly-guest](https://github.com/KooleControls/firefly-guest) devices.  
   - Logs activity such as button presses.  
   - Provides API endpoints for the UI to fetch logs and control guests.  

3. **Command Propagation**  
   - API calls from the UI (e.g. "blink LED on device X") are translated into ESP-NOW messages sent to the appropriate guest(s).  

---

## 🧩 How to Play

1. Flash the host firmware onto an Ethernet-capable ESP32.  
2. Connect the board to a router (Ethernet preferred).  
3. Flash one or more [firefly-guest](https://github.com/KooleControls/firefly-guest) devices.  
4. Upload the [firefly-ui](https://github.com/KooleControls/firefly-ui) build files to the host.  
5. Connect your computer/phone to the same network and open the UI in a browser.  
6. Press a button on a guest device → watch the log update in real time.  
7. Trigger a command in the UI → watch the guest LED blink.  

---

✨ Firefly is part of the [KooleControls](https://github.com/KooleControls) collection of projects for exploring embedded systems, wireless communication, and modern web development.
