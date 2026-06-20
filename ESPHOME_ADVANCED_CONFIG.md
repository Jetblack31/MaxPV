# ESPHome MaxPV - Advanced Configuration Guide

Current status: the active configuration in this repository is now YAML-only and lives in `esphome/maxpv.yaml`.
The custom-component examples kept below are historical notes from the first migration draft and are no longer used by the build.

## Parameter Mapping

The Arduino Nano responds with a PARAM message containing configuration values. The format is:
```
PARAM,<param0>,<param1>,<param2>,...,<paramN>
```

Each parameter has a specific type and index. The EcoPV firmware defines these in `EcoPV3.ino`.

### Known Parameters (from EcoPV3)

All 16 parameters are defined in `EcoPV3/EcoPV3.ino` in the `pvrParamConfig[]` array (`NB_PARAM = 16`).

| PARAM index (0-based) | Name | Type | Range | Unit | Default | SETPARAM # |
|----------------------|------|------|-------|------|---------|------------|
| 0 | `V_CALIB` | float | 0–2 | V/bit | 0.800 | 01 |
| 1 | `P_CALIB` | float | 0–1 | VA/bit | 0.111 | 02 |
| 2 | `PHASE_CALIB` | int | -16 to 48 | steps | 13 | 03 |
| 3 | `P_OFFSET` | int | -100 to 100 | W | -15 | 04 |
| 4 | `P_RESISTANCE` | int | 100–10000 | W | 2000 | 05 |
| 5 | `P_MARGIN` | int | -2000 to 2000 | W | 10 | 06 |
| 6 | `GAIN_P` | int | 0–1000 | — | 8 | 07 |
| 7 | `GAIN_I` | int | 0–1000 | — | 45 | 08 |
| 8 | `E_RESERVE` | byte | 0–200 | J | 5 | 09 |
| 9 | `P_DIV2_ACTIVE` | int | 0–9999 | W | 1000 | 10 |
| 10 | `P_DIV2_IDLE` | int | 0–9999 | W | 200 | 11 |
| 11 | `T_DIV2_ON` | byte | 0–240 | min | 5 | 12 |
| 12 | `T_DIV2_OFF` | byte | 0–240 | min | 5 | 13 |
| 13 | `T_DIV2_TC` | byte | 0–60 | min | 1 | 14 |
| 14 | `CNT_CALIB` | float | 0–1000 | Wh/pulse | 1.0 | 15 |
| 15 | `P_INSTALLPV` | int | 100–30000 | Wc | 3000 | 16 |

Note: `PARAM` responses are **zero-based** in the payload, but `SETPARAM` commands use **1-based** numbering.
Types: `float` = dataType 1, `int` = dataType 0, `byte` = dataType 4.

## Serial Communication Details

### Message Format

All messages to Arduino terminate with `#`:
```
COMMAND,param1,param2,...,END#
```

### STATS Message Format

```
STATS,Vrms,Irms,Pact,Papp,Prouted,Pexport[,Temperature,Trelayplus,eimported,erouted,eexport,eimpulsion]#
```

Where:
- `Vrms`: Voltage RMS (V) - float, 1 decimal
- `Irms`: Current RMS (A) - float, 3 decimals  
- `Pact`: Active Power (W) - float, 1 decimal
- `Papp`: Apparent Power (VA) - float, 1 decimal
- `Prouted`: Routed Power (W) - float, 1 decimal
- `Pexport`: Export Power (W) - float, 1 decimal
- Additional fields (optional, may vary by EcoPV version)

### Command Format Examples

**Request Parameters**
```
PARAM,END#
```
Response:
```
PARAM,50,30.5,100.5,3000,1,...#
```

**Set SSR Mode**
```
SETSSR,AUTO,END#
SETSSR,FORCE,END#
SETSSR,STOP,END#
```

**Set Relay Mode**
```
SETRELAY,AUTO,END#
SETRELAY,FORCE,END#
SETRELAY,STOP,END#
```

**Set Parameter**
```
SETPARAM,00,50,END#    # Set P_PILOTAGE to 50
SETPARAM,01,35,END#    # Set P_SEUILBAS to 35
```

**Other Commands**
```
VERSION,END#       # Get firmware version
SAVECFG,END#       # Save configuration to EEPROM
LOADCFG,END#       # Load configuration from EEPROM
SAVEINDX,END#      # Save energy indices
INDX0,END#         # Reset energy indices to 0
RESET,END#         # Soft restart the router
```

## Custom Component Architecture

### Component Lifecycle

1. **Setup Phase** (`setup()`)
   - Configure UART
   - Initialize buffers
   - Send initial VERSION request

2. **Loop Phase** (`loop()`)
   - Read serial data continuously
   - Process complete messages (terminated by '#')
   - Update sensor values
   - Monitor connection status
   - Periodically request parameters

3. **Message Processing** (`process_line_()`)
   - Identify message type (STATS, PARAM, VERSION, etc.)
   - Update appropriate entities
   - Track last contact time

### Adding New Sensors

To add a new sensor from the STATS message in the current YAML-only configuration:

1. **In `esphome/maxpv.yaml`**, add a new template sensor in the `sensor:` section:
   ```yaml
   sensor:
     - platform: template
       name: "My New Sensor"
       id: my_new_sensor
       unit_of_measurement: "unit"
       accuracy_decimals: 2
       icon: mdi:icon-name
   ```

2. **In the UART debug handler lambda** (in the `uart:` section), add a line to publish the value:
   ```cpp
   if (values.size() > N) {
     id(my_new_sensor).publish_state(to_float(values[N]));
   }
   ```
   Replace `N` with the 0-based STATS field index:
   - `values[0]` = "STATS" label
   - `values[1]` = Vrms
   - `values[2]` = Irms
   - `values[3]` = Pact
   - `values[4]` = Papp
   - `values[5]` = Prouted
   - `values[6]` = Pexport
   - And so on for additional fields

3. **Verify and deploy**:
   ```bash
   esphome compile maxpv.yaml
   esphome run maxpv.yaml
   ```
   The new sensor will appear in Home Assistant with live values from EcoPV.

## Troubleshooting Serial Communication

### Monitor Raw Serial Output

Create a diagnostic configuration:
```yaml
uart:
  id: ecopv_serial
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 500000
  debug:
    direction: RX
    dummy_receiver: false
    after:
      delimiter: "#"
```

View logs:
```bash
esphome logs maxpv.yaml
```

### Test Serial Connection

Use ESPHome's serial monitor:
```bash
esphome monitor maxpv.yaml
```

You should see:
- STATS messages arriving every 1-5 seconds
- Version number after initialization
- Response to sent commands

### Common Serial Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| No messages received | Wrong baud rate | Check: 500000 baud |
| Garbled output | Wrong pins (RX/TX swapped) | Check pin configuration |
| Periodic disconnects | Loose connection | Check cable, add strain relief |
| Timeout errors | Buffer too small | Increase RX_BUFFER_SIZE to 256+ |
| Message corruption | Interference | Use shielded cable, add ferrite |

## Performance Optimization

### Reduce Update Frequency

If the router is overloaded:
```yaml
ecopv:
  scan_interval: 5000ms  # Request parameters every 5 seconds instead of 1
```

### Optimize STATS Parsing

Only parse needed fields:
```cpp
// Only parse first 6 fields
if (values.size() < 7) return;
```

### Monitor Memory Usage

Add to `esphome/maxpv.yaml`:
```yaml
debug:
  update_interval: 30s
```

## Advanced Features (Planned)

### Boost Mode Scheduling
```yaml
automation:
  - trigger:
      platform: time
      at: "06:00:00"
    action:
      service: esphome.maxpv_request_parameters
```

### Temperature-Based Control
```yaml
automation:
  - trigger:
      platform: numeric_state
      entity_id: sensor.water_tank_temperature
      below: 45
    action:
      - service: select.select_option
        data:
          option: "Force"
          entity_id: select.ssr_operating_mode
```

### Historical Data Logging
Use Home Assistant's built-in statistics:
- Daily energy routed
- Average power during daylight
- Peak routed power

### MQTT Bridge (if needed)
```yaml
mqtt:
  broker: 192.168.1.100
  discovery: true
  discovery_prefix: homeassistant
```

## Firmware Customization

### Add Relay Time Tracking

Modify STATS parsing to extract relay run time:
```cpp
if (values.size() > 8 && this->relay_time_sensor_) {
  try {
    long relay_minutes = std::stol(values[8]);
    this->relay_time_sensor_->publish_state(relay_minutes);
  } catch (const std::invalid_argument &e) {
    // Handle error
  }
}
```

### Add Temperature Threshold Alerts

In the component:
```cpp
void EcoPVComponent::check_temperature_() {
  if (temperature > MAX_TEMP) {
    // Automatically set SSR to STOP
    this->set_ssr_mode("Stop");
  }
}
```

## Testing and Validation

### Simulated STATS Testing

For development without hardware:
```bash
# In a terminal, simulate ESP serial:
python3 -c "
import serial
import time
ser = serial.Serial('/dev/ttyUSB0', 500000)
while True:
    ser.write(b'STATS,230.5,12.345,2840.1,2900.0,1500.5,125.3,45.2,480#')
    time.sleep(2)
"
```

### Unit Testing

Create test message files:
```txt
# test_messages.txt
STATS,230.5,12.345,2840.1,2900.0,1500.5,125.3,45.2,480#
VERSION,EcoPV 3.60#
PARAM,50,30,100,3000,1,23,15,180,480,21#
```

## Migration Checklist

- [ ] ESPHome installed and configured
- [ ] UART pins (RX/TX) physically connected to Arduino Nano
- [ ] Baud rate set to 500000
- [ ] Compile without errors
- [ ] Flash to Wemos D1 mini
- [ ] WiFi connection established
- [ ] ESPHome integration in Home Assistant working
- [ ] Sensors receiving data from Arduino Nano
- [ ] Controls (mode switches) functioning
- [ ] Dashboard created and tested
- [ ] Automations set up
- [ ] Backup of configuration files made
- [ ] Old MaxPV web interface disabled (optional)

## Support Resources

- ESPHome Custom Components: https://esphome.io/custom/custom_component.html
- UART Documentation: https://esphome.io/components/uart.html
- Sensor Integration: https://esphome.io/components/sensor/index.html
- Home Assistant Developer Docs: https://developers.home-assistant.io/
