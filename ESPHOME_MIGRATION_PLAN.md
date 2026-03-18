# MaxPV to ESPHome Migration Plan

## Project Overview
Transform MaxPV from a custom Arduino/ESP8266 web interface to an ESPHome configuration with Home Assistant integration.

### Architecture
- **Current**: Arduino Nano (routing controller) + Wemos D1 mini (WiFi/Web UI)
- **Target**: Arduino Nano (unchanged) + Wemos D1 mini (running ESPHome)

## Serial Communication Protocol

The Wemos/ESP communicates with Arduino Nano at **500000 baud** via serial protocol with text-based commands:

### Commands (ESP → Arduino)
- `PARAM,END#` - Retrieve all parameters
- `VERSION,END#` - Get firmware version
- `SETPARAM,<param_id>,<value>,END#` - Configure parameter
- `SETRELAY,<mode>,END#` - Set relay mode (STOP/FORCE/AUTO)
- `SETSSR,<mode>,END#` - Set SSR/TRIAC mode (STOP/FORCE/AUTO)
- `SAVECFG,END#` - Save config to EEPROM
- `LOADCFG,END#` - Load config from EEPROM
- `SAVEINDX,END#` - Save energy indices
- `INDX0,END#` - Reset energy indices
- `RESET,END#` - Soft reset controller

### Responses (Arduino → ESP)
- `STATS,<Vrms>,<Irms>,<Pact>,<Papp>,<Prouted>,<Pexport>,...` - Real-time statistics
- `PARAM,<v1>,<v2>,<v3>,...` - Parameters (comma-separated with types: int/float/byte)
- `VERSION,<version>` - Firmware version
- `FORMAT` - EEPROM format response

## ESPHome Implementation Strategy

### Phase 1: Serial Communication Bridge
Create ESPHome UART component to handle serial communication:
- Initialize UART at 500000 baud
- Parse incoming STATS messages
- Send commands to Arduino Nano
- Timeout handling and error recovery

### Phase 2: Sensor Definitions
Map STATS message fields to Home Assistant sensors:
- `Vrms` → Voltage sensor (V)
- `Irms` → Current sensor (A)
- `Pact` → Active power sensor (W)
- `Papp` → Apparent power sensor (VA)
- `Prouted` → Routed power sensor (W)
- `Pexport` → Export power sensor (W)
- Energy indices as long-term statistics sensors

### Phase 3: Switch & Button Controls
Implement Home Assistant controls:
- SSR mode selector (Off/On/Auto)
- Relay mode selector (Stop/Force/Auto)
- Boost mode toggle with duration
- Configuration reset/save buttons

### Phase 4: Number Entities (Configuration Parameters)
Expose key configuration parameters:
- P_INSTALLPV (PV installation power in Wc)
- CNT_CALIB (Pulse counter calibration)
- Maximum/Minimum thresholds
- Temperature limits
- MQTT/network parameters (if needed)

### Phase 5: Home Assistant Dashboard
Create equivalent interface in HA:
- Power monitoring graphs
- Historical energy data
- Control panels for routing modes
- System status indicators

## Files to Create

1. **esphome/maxpv.yaml** - Main ESPHome configuration
2. **esphome/maxpv_components.yaml** - Component packages (optional, for modularity)
3. **homeassistant/maxpv_dashboard.yaml** - HA dashboard/lovelace UI
4. **Documentation/ESPHOME_SETUP.md** - Installation/setup guide

## Expected Sensor Outputs

From STATS message parsing:
```
sensor.maxpv_voltage_rms      # Vrms (V)
sensor.maxpv_current_rms      # Irms (A)  
sensor.maxpv_power_active     # Pact (W)
sensor.maxpv_power_apparent   # Papp (VA)
sensor.maxpv_power_routed     # Prouted (W)
sensor.maxpv_power_export     # Pexport (W)
sensor.maxpv_energy_routed    # Energy index (kWh)
sensor.maxpv_energy_import    # Energy index (kWh)
sensor.maxpv_energy_export    # Energy index (kWh)

switch.maxpv_ssr_mode         # SSR control (binary)
switch.maxpv_relay_mode       # Relay control (binary)
switch.maxpv_boost_mode       # Boost mode
```

## Implementation Notes

- **Serial Buffer**: ESP RX buffer set to handle STATS messages (~200 bytes)
- **UART Timeout**: 100ms (matching Arduino configuration)
- **Update Frequency**: STATS messages sent continuously; throttle HA updates to 1-5 second intervals
- **Error Handling**: Monitor for serial connection loss; expose system status
- **OTA Updates**: ESPHome built-in OTA will replace ElegantOTA
- **MQTT**: Can be removed or kept optional for third-party integrations

## Migration Procedure

1. Backup current MaxPV3 configuration
2. Install ESPHome compiler/tools
3. Create ESPHome YAML configuration
4. Compile and flash to Wemos D1 mini via serial/USB
5. Configure WiFi provisioning (ESPHome's native captive portal)
6. Add ESPHome integration to Home Assistant
7. Verify sensors and controls appear in HA
8. Create HA dashboard/automations to replace MaxPV web UI
9. Optional: Remove MaxPV web files from filesystem

## Advantages of ESPHome Approach

✅ Native Home Assistant integration (no MQTT needed)
✅ Simplified firmware maintenance vs custom Arduino code
✅ Built-in OTA updates
✅ Native HA UI dashboard support
✅ Better security (encrypted communication with HA)
✅ Smaller code footprint
✅ Easier customization for future features
✅ Better community support

## Potential Challenges

⚠️ Serial protocol complexity requires careful parsing
⚠️ Custom data types in PARAM messages need handling
⚠️ Energy history tracking needs persistent storage
⚠️ Timing-critical operations may need optimization
⚠️ Testing required to match existing behavior

## Next Steps

1. Create initial ESPHome configuration file (maxpv.yaml)
2. Implement serial UART component with command sending
3. Parse STATS messages into Home Assistant sensors
4. Test communication with running Arduino Nano
5. Implement control switches and buttons
6. Create Home Assistant dashboard
