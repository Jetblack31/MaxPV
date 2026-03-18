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

The following parameters can be configured via Home Assistant:

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| PV Installation Power | Number | 1000-50000 Wc | Total PV system capacity |
| Counter Calibration | Number | 1-1000 Wh/pulse | Energy per pulse counter increment |
| Boost Duration | Number | 10-480 min | How long to run boost mode |
| Max Water Temperature | Number | 30-90 °C | Temperature limit for water heater |

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

## Next Steps

1. **Create Dashboard**: Build a Home Assistant dashboard showing power flows
2. **Automations**: Create automations for energy routing modes
3. **Statistics**: Monitor daily/monthly energy production
4. **Alerts**: Set up notifications for system issues
5. **Integration**: Connect with other home automation systems

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
├── ecopv/
│   ├── __init__.py              # Python component config
│   ├── ecopv.h                  # C++ header
│   └── ecopv.cpp                # C++ implementation
└── packages/                    # Optional: modular configs
    ├── sensors.yaml
    ├── controls.yaml
    └── diagnostics.yaml
```
