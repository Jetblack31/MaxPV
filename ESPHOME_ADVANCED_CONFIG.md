# ESPHome MaxPV - Advanced Configuration Guide

## Parameter Mapping

The Arduino Nano responds with a PARAM message containing configuration values. The format is:
```
PARAM,<param0>,<param1>,<param2>,...,<paramN>
```

Each parameter has a specific type and index. The EcoPV firmware defines these in `EcoPV3.ino`.

### Known Parameters (from EcoPV3)

| Index | Name | Type | Range | Unit | Default |
|-------|------|------|-------|------|---------|
| 0 | P_PILOTAGE | int | 0-100 | % | 50 |
| 1 | P_SEUILBAS | float | 0.0-500.0 | W | 30 |
| 2 | P_SEUILHAUT | float | 0.0-500.0 | W | 100 |
| 3 | P_INSTALLPV | int | 100-50000 | Wc | 3000 |
| 4 | CNT_CALIB | int | 1-1000 | Wh/pulse | 1 |
| ... | ... | ... | ... | ... | ... |

*Note: Complete parameter list needs to be extracted from the EcoPV3 source code*

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

To add a new sensor from the STATS message:

1. **In `ecopv/ecopv.h`**, add member variable:
   ```cpp
   sensor::Sensor *my_new_sensor_{nullptr};
   ```

2. **Add setter method**:
   ```cpp
   void set_my_new_sensor(sensor::Sensor *sensor) {
     this->my_new_sensor_ = sensor;
   }
   ```

3. **In `ecopv/ecopv.cpp`**, update `parse_stats_()`:
   ```cpp
   if (this->my_new_sensor_) {
     this->my_new_sensor_->publish_state(std::stof(values[10])); // Adjust index
   }
   ```

4. **In `ecopv/__init__.py`**, add configuration:
   ```python
   CONF_MY_NEW_SENSOR = "my_new_sensor"
   # Add to CONFIG_SCHEMA
   cv.Optional(CONF_MY_NEW_SENSOR): cv.use_id(sensor.Sensor),
   ```

5. **In `maxpv.yaml`**, add sensor and register with component:
   ```yaml
   sensor:
     - platform: template
       name: "My New Sensor"
       id: my_new_sensor
   ```

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
