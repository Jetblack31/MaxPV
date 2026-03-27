# ESPHome Migration Setup Guide

## Prerequisites

- ESPHome installed (Python-based, install via: `pip install esphome`)
- Wemos D1 mini with existing MaxPV3 firmware
- Arduino Nano with EcoPV3 firmware running
- Home Assistant instance with ESPHome integration installed
- USB cable for initial flashing

## Installation Steps

### Step 1: Prepare ESPHome Configuration

1. Copy `esphome/` directory to your ESPHome setup location
2. Create `esphome/secrets.yaml` from `esphome/secrets.yaml.template`:
   ```bash
   cp esphome/secrets.yaml.template esphome/secrets.yaml
   ```

3. Edit `esphome/secrets.yaml` with your values:
   ```yaml
   wifi_ssid: "YOUR_SSID"
   wifi_password: "YOUR_PASSWORD"
   api_encryption_key: "YOUR_KEY_HERE"
   ota_password: "YOUR_OTA_PASSWORD"
   ```

### Step 2: Generate Encryption Key

Run this command to generate an API encryption key:
```bash
python3 -c "import secrets; print(secrets.token_hex(16))"
```

Copy the output to `secrets.yaml` as `api_encryption_key`.

### Step 3: Compile the Firmware

```bash
# Navigate to your esphome directory
cd esphome

# Compile the firmware
esphome compile maxpv.yaml

# Or validate syntax without compiling
esphome config maxpv.yaml
```

### Step 4: Connect Wemos D1 Mini to Computer

1. Connect Wemos D1 mini via USB cable
2. Find the serial port:
   - **Windows**: COM3, COM4, etc. (check Device Manager)
   - **Linux**: /dev/ttyUSB0, /dev/ttyACM0, etc.
   - **macOS**: /dev/tty.wchusbserial*, etc.

### Step 5: Flash the Firmware

```bash
# Flash the compiled firmware
esphome run maxpv.yaml

# Or upload via USB cable
esphome upload maxpv.yaml --device /dev/ttyUSB0  # Adjust serial port
```

The device will reboot and attempt to connect to your WiFi network.

### Step 6: Initial WiFi Configuration

1. If the device can't connect to WiFi, it will create a fallback AP:
   - **SSID**: MaxPV_Fallback
   - **Password**: MaxPV_2024

2. Connect to this SSID from your phone/computer
3. A captive portal should open automatically, or navigate to `http://192.168.4.1`
4. Configure:
   - WiFi SSID and password
   - Device will restart and connect to your network

### Step 7: Add to Home Assistant

1. In Home Assistant, go to **Settings → Devices & Services → Integrations**
2. Click **Create Integration → ESPHome**
3. Enter the device IP address or hostname
4. Home Assistant will discover and add the MaxPV3 device
5. Entities will appear in the `maxpv` integration

### Step 8: Verify Communication

Check the ESPHome logs:
```bash
esphome logs maxpv.yaml
```

You should see:
```
[ESP][D]: Setting up EcoPV component
[ESP][I]: Established contact with EcoPV
[ESP][I]: EcoPV Version: X.XX
```

If you don't see these messages:
1. Check the serial cable connection between Wemos and Arduino Nano
2. Verify the Arduino Nano is powered and running
3. Check the baud rate is set correctly (500000)

### Step 9: Configure Home Assistant Dashboard

The sensors are automatically discovered in HA. You can now:
- Create automations based on power thresholds
- Build custom dashboards
- Set up notifications based on energy usage
- Create history graphs

## Troubleshooting

### No Contact with EcoPV

**Problem**: "Disconnected from EcoPV" status

**Solutions**:
1. Check physical serial connection (TX/RX pins)
2. Verify Arduino Nano is powered and running
3. Check baud rate: Should be 500000
4. Look for error messages in ESPHome logs

### WiFi Connection Issues

**Problem**: Device can't connect to WiFi

**Solutions**:
1. Use the fallback AP to reconfigure WiFi
2. Check your WiFi credentials in `secrets.yaml`
3. Ensure 2.4GHz network (ESP8266 doesn't support 5GHz)
4. Move closer to router or use WiFi extender

### Sensors Not Updating

**Problem**: Sensors appear but show no values

**Solutions**:
1. Check EcoPV is sending STATS messages
2. Verify serial buffer size (256 bytes minimum)
3. Check logs for parsing errors
4. Ensure Arduino Nano is in "Routeur running" state

### OTA Updates Failed

**Problem**: "Update failed" message

**Solutions**:
1. Check device has adequate memory (1019 KB for OTA)
2. Ensure stable WiFi connection during update
3. Try flashing via USB cable again
4. Check available RAM: `esphome run maxpv.yaml --device <IP> --update-home-assistant=false`

## Pin Assignments

### Wemos D1 Mini UART Pins
- **RX**: GPIO3 (D9) - Receives data from Arduino Nano
- **TX**: GPIO1 (D4) - Sends commands to Arduino Nano
- **GND**: Ground connection
- **3V3**: Optional power (if supplying power to Arduino Nano)

### Arduino Nano Serial Pins
- **TX**: Pin 1 → Wemos GPIO3 (RX)
- **RX**: Pin 0 → Wemos GPIO1 (TX)
- **GND**: Common ground
- **VCC**: Separate power supply (not from Wemos)

## Configuration Parameters

The following parameters can be configured via Home Assistant.

EcoPV firmware parameters (SETPARAM 01..16):

| Parameter    | Type   | Range           | Description                                    |
| ------------ | ------ | --------------- | ---------------------------------------------- |
| V_CALIB      | Number | 0-2 V/bit       | Voltage calibration factor                     |
| P_CALIB      | Number | 0-1 VA/bit      | Apparent power calibration factor              |
| PHASE_CALIB  | Number | -16 to 48       | Phase correction for current/voltage alignment |
| P_OFFSET     | Number | -100 to 100 W   | Active power offset correction                 |
| P_RESISTANCE | Number | 100-10000 W     | Main heater rated power                        |
| P_MARGIN     | Number | -2000 to 2000 W | Target imported power margin                   |
| GAIN_P       | Number | 0-1000          | Proportional controller gain                   |
| GAIN_I       | Number | 0-1000          | Integral controller gain                       |
| E_RESERVE    | Number | 0-200 J         | Energy reserve before regulation               |
| P_DIV2_ACTIVE| Number | 0-9999 W        | Secondary relay activation threshold           |
| P_DIV2_IDLE  | Number | 0-9999 W        | Secondary relay deactivation threshold         |
| T_DIV2_ON    | Number | 0-240 min       | Minimum secondary relay ON time                |
| T_DIV2_OFF   | Number | 0-240 min       | Minimum secondary relay OFF time               |
| T_DIV2_TC    | Number | 0-60 min        | Averaging time constant for DIV2 logic         |
| CNT_CALIB    | Number | 1-1000 Wh/pulse | Pulse counter energy calibration               |
| P_INSTALLPV  | Number | 100-30000 Wc    | PV installation peak capacity                  |

Additional local ESP controls (not EcoPV SETPARAM values):

|Parameter|Type|Range|Description|
|---------|----|-----|-----------|
|Boost Duration|Number|10-480 min|Duration used by Boost mode command|
|Max Water Temperature|Number|30-90 °C|Local helper value for automations and dashboard|

## Updating the Firmware

### Via OTA (Over-The-Air)

1. Make changes to `esphome/maxpv.yaml`
2. Run:
   ```bash
   esphome run maxpv.yaml
   ```
3. ESPHome will upload to the device via WiFi (no USB needed)
4. Device reboots automatically

### Via USB Cable

If OTA fails:
1. Connect Wemos to USB
2. Run:
   ```bash
   esphome upload maxpv.yaml --device /dev/ttyUSB0
   ```

## Backup and Recovery

### Backup Current Configuration

```bash
# Read firmware from device
esphome read-web-logs maxpv.yaml --api-password <password>
```

### Factory Reset

If the device becomes unresponsive:
1. Hold RESET button on Wemos D1 mini
2. Connect via USB
3. Flash the firmware again using `esphome upload`

## Performance Optimization

### Reduce WiFi Power Consumption
Add to `esphome/maxpv.yaml`:
```yaml
wifi:
  power_save_mode: light
```

### Optimize Scan Interval
Adjust in the configuration:
```yaml
ecopv:
  scan_interval: 2000ms  # Increase to reduce CPU usage
```

### Monitor Memory Usage
Check logs for memory warnings:
```bash
esphome logs maxpv.yaml | grep -i memory
```

## Home Assistant Dashboard and Setup Wizard

Two pre-configured Home Assistant configuration files are included to simplify setup and monitoring:

### MaxPV Dashboard (`homeassistant/maxpv_dashboard.yaml`)

A complete YAML dashboard with multiple panels:

1. **Overview Panel** - Real-time gauges:
   - Grid voltage, current, and power
   - Routed power with color segments
   - Daily energy statistics

2. **System Controls Panel** - Buttons for:
   - Request EcoPV parameters
   - Save configuration
   - Reset energy indices
   - Soft restart EcoPV

3. **Setup Wizard Panel** - Step-by-step 10-step router configuration:
   - Live measurements display
   - Direct parameter controls
   - Automated wizard step buttons
   - Instructions for each step

4. **Operating Modes Panel** - Controls and explanations:
   - SSR mode (Stop/Force/Auto)
   - Relay mode (Stop/Force/Auto)
   - Boost mode toggle

5. **Energy Statistics Panel** - Historical data:
   - Daily, weekly, monthly energy routed/imported/exported
   - Average power measurements

6. **Configuration Panel** - All 16 EcoPV parameters organized by function:
   - Calibration (5 parameters)
   - Regulation (4 parameters)
   - Secondary Load DIV2 Relay (5 parameters)
   - Pulse Counter & PV Installation (2 parameters)
   - Boost & Local Controls (2 parameters)

7. **System Diagnostics Panel** - Status and troubleshooting info

**How to use**:
```bash
cp homeassistant/maxpv_dashboard.yaml <HA_CONFIG>/dashboards/
```

Then in Home Assistant:
1. Go to **Dashboards**
2. Select **MaxPV3 - Solar Router**
3. View real-time power data and control the router

### Setup Wizard Package (`homeassistant/maxpv_wizard_package.yaml`)

Automated helper inputs and scripts that reproduce the 10-step EcoPV configuration process:

**Included components**:
- **Input helpers** for wizard inputs (Step 5 reference power, Step 8 secondary load power)
- **Scripts** for automated setup steps:
  - Step 1: Reset indices, restart, reset calibration
  - Step 3: Auto-compute proportional and integral gains from heater power
  - Step 5: Compute power calibration from reference apparent power
  - Step 6: Correct active power offset with closed clamp
  - Step 8: Auto-configure DIV2 relay thresholds based on loads
  - Step 10: Save config and restart EcoPV

**How to use**:

1. **Enable packages in Home Assistant** (`configuration.yaml`):
   ```yaml
   homeassistant:
     packages:
       maxpv_wizard: !include_dir_named homeassistant/
   ```

2. **Restart Home Assistant** to load helpers and scripts

3. **Run the wizard** from the Setup Wizard panel in the dashboard:
   - Read step instructions
   - Set required helper inputs (Step 5, Step 8)
   - Click the step button
   - Wizard script auto-computes and sets EcoPV parameters
   - Verify results on the next step or in logs

**Example workflow**:
- Step 1: Initialize (resets energy, restarts router)
- Step 2: Manually enter PV installation power
- Step 3: Enter heater power, click button (auto-computes gains)
- Step 4: Adjust voltage calibration while watching live voltage
- Step 5: Enter reference apparent power from meter, click button
- Step 6: Close clamp without wires, click button (corrects offset)
- Step 7: Manually adjust phase correction while watching live power factor
- Step 8: Enter secondary load power, click button (auto-configures relay)
- Step 9: Manually enter pulse counter calibration
- Step 10: Click to save and restart

All 16 configuration parameters are available as editable number entities in the Configuration panel during or after wizard setup.

## Support and Resources

- **ESPHome Documentation**: https://esphome.io/
- **Home Assistant ESPHome Integration**: https://www.home-assistant.io/integrations/esphome/
- **MaxPV Project**: https://github.com/Jetblack31/MaxPV
- **EcoPV Documentation**: https://github.com/Jetblack31/EcoPV

## File Structure

```
esphome/
├── maxpv.yaml                    # Main configuration
├── secrets.yaml                  # (Create from template)
├── secrets.yaml.template         # Template for secrets
└── packages/                    # Optional: modular configs
    ├── sensors.yaml
    ├── controls.yaml
    └── diagnostics.yaml
```
