# MaxPV to ESPHome Migration

Complete guide to transforming the MaxPV solar routing system from a custom Arduino/ESP8266 web interface to a native ESPHome + Home Assistant integration.

## Overview

This project migrates the MaxPV3 system from:
- **Current**: Custom web UI + Arduino Nano controller + Wemos ESP8266 WiFi interface
- **Target**: ESPHome-based ESP8266 + Native Home Assistant integration (no web UI needed)

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Home Assistant Instance                  │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         ESPHome Integration                          │   │
│  │  • Automatic device discovery                        │   │
│  │  • Real-time sensor updates                          │   │
│  │  • Control services (mode switching, etc.)           │   │
│  └──────────────────────────────────────────────────────┘   │
└────┬─────────────────────────────────────────────────────────┘
     │ WiFi / API (encrypted)
     │
┌────▼──────────────────────────────────────────────────────────┐
│              Wemos D1 Mini (ESP8266)                           │
│  ┌──────────────────────────────────────────────────────┐    │
│  │  ESPHome Runtime                                    │    │
│  │  • Serial communication (500000 baud)              │    │
│  │  • STATS message parsing                           │    │
│  │  • Command transmission                            │    │
│  └──────────────────────────────────────────────────────┘    │
└────┬──────────────────────────────────────────────────────────┘
     │ Serial Connection (UART)
     │
┌────▼──────────────────────────────────────────────────────────┐
│         Arduino Nano (EcoPV3 Controller)                       │
│  • AC voltage/current monitoring                              │
│  • Power routing algorithm                                    │
│  • SSR/Relay control                                          │
│  • Energy counters                                            │
└──────────────────────────────────────────────────────────────┘
```

## Quick Start

### 1. Prerequisites
- ESPHome installed: `pip install esphome`
- Home Assistant with ESPHome integration
- Wemos D1 mini with USB connection capability
- Arduino Nano running EcoPV3 firmware

### 2. Files Overview

| File | Purpose |
|------|---------|
| [ESPHOME_MIGRATION_PLAN.md](ESPHOME_MIGRATION_PLAN.md) | Detailed migration strategy |
| [ESPHOME_SETUP_GUIDE.md](ESPHOME_SETUP_GUIDE.md) | Step-by-step installation instructions |
| [ESPHOME_ADVANCED_CONFIG.md](ESPHOME_ADVANCED_CONFIG.md) | Technical deep-dive and customization |
| `esphome/maxpv.yaml` | Single-file ESPHome configuration with inline UART parsing |
| `homeassistant/maxpv_dashboard.yaml` | HA dashboard template |

### 3. Installation (5-10 minutes)

```bash
# 1. Copy ESPHome configuration
cp -r esphome/ ~/esphome/maxpv/

# 2. Set up secrets
cd ~/esphome/maxpv
cp esphome/secrets.yaml.template esphome/secrets.yaml
# Edit secrets.yaml with your WiFi credentials

# 3. Compile and flash
esphome run esphome/maxpv.yaml --device /dev/ttyUSB0

# 4. WiFi setup
# Device creates fallback AP if connection fails
# Connect to MaxPV_Fallback (password: MaxPV_2024)

# 5. Add to Home Assistant
# Go to Settings > Devices & Services > Integrations
# Search for "ESPHome" and add maxpv device
```

## Key Features

✅ **Real-time Power Monitoring**
- Grid voltage, current, and power
- Active/apparent power differentiation
- Power routing and export tracking
- Energy indices (daily/lifetime)

✅ **Router Mode Control**
- SSR (heater element) mode: Stop/Force/Auto
- Secondary relay mode: Stop/Force/Auto
- Boost mode activation
- Temperature thresholds

✅ **Configuration Management**
- Parameter read/write to EEPROM
- PV installation power tracking
- Counter calibration
- Energy index reset

✅ **Home Assistant Native Integration**
- Automatic device discovery
- Encrypted communication
- Native dashboard support
- Automation triggers
- History tracking

✅ **System Diagnostics**
- Connection status monitoring
- Router running state
- Firmware version display
- Error logging

## Advantages Over Original Setup

| Aspect | MaxPV3 Web UI | ESPHome + HA |
|--------|---|---|
| **Interface** | Custom web portal | Native HA dashboard |
| **Accessibility** | Only at static IP | HA accessible everywhere |
| **Mobile-friendly** | Limited | Full HA mobile apps |
| **Automations** | Web API calls | Native HA automations |
| **Integrations** | Direct API | MQTT/REST/native integrations |
| **Security** | Basic HTTP | Encrypted API + local access |
| **Updates** | Manual file upload | OTA or WiFi |
| **Maintenance** | Custom code | ESPHome community support |

## Communication Protocol

The Wemos communicates with Arduino Nano at 500000 baud via serial UART:

### Outgoing Commands
```
PARAM,END#              → Request parameters
VERSION,END#            → Request version
SETSSR,AUTO,END#        → Set SSR mode
SETRELAY,FORCE,END#     → Set relay mode
SAVECFG,END#            → Save configuration
RESET,END#              → Soft restart
```

### Incoming Data
```
STATS,230.5,12.3,2840.1,2900.0,1500.5,125.3,...#
PARAM,50,30,100,3000,1,...#
VERSION,EcoPV 3.60#
```

## Troubleshooting Quick Reference

| Issue | Check |
|-------|-------|
| No contact with EcoPV | Serial cable (RX/TX), baud rate 500000 |
| Sensors not updating | Arduino Nano running, STATS messages in logs |
| WiFi connection fails | SSID/password in secrets.yaml, 2.4GHz network |
| OTA update fails | WiFi stability, adequate free RAM |
| Device reboots | ESPhome logs for memory/stability issues |

See [ESPHOME_SETUP_GUIDE.md](ESPHOME_SETUP_GUIDE.md#troubleshooting) for detailed solutions.

## File Structure

```
MaxPV/
├── README.md (this file)
├── ESPHOME_MIGRATION_PLAN.md      (migration strategy)
├── ESPHOME_SETUP_GUIDE.md         (installation guide)
├── ESPHOME_ADVANCED_CONFIG.md     (technical reference)
│
├── esphome/
│   ├── maxpv.yaml                 (main config and inline UART parsing)
│   └── secrets.yaml.template      (copy to secrets.yaml)
│
├── homeassistant/
│   └── maxpv_dashboard.yaml       (HA dashboard)
│
├── MaxPV3/                        (original firmware)
├── EcoPV3/                        (Arduino controller)
└── ... (other original files)
```

## Next Steps

1. **First-time setup**: Follow [ESPHOME_SETUP_GUIDE.md](ESPHOME_SETUP_GUIDE.md)
2. **Understanding the internals**: Read [ESPHOME_MIGRATION_PLAN.md](ESPHOME_MIGRATION_PLAN.md)
3. **Customization**: Refer to [ESPHOME_ADVANCED_CONFIG.md](ESPHOME_ADVANCED_CONFIG.md)
4. **Create HA dashboard**: Use [maxpv_dashboard.yaml](homeassistant/maxpv_dashboard.yaml) as template
5. **Set up automations**: Use HA native automations (e.g., mode based on power threshold)

## Customization Examples

### Automatic Mode Based on Power

```yaml
# In Home Assistant automations
- alias: "Enable SSR when surplus power > 500W"
  trigger:
    platform: numeric_state
    entity_id: sensor.routed_power
    above: 500
  action:
    - service: select.select_option
      target:
        entity_id: select.ssr_operating_mode
      data:
        option: "Auto"
```

### Daily Energy Notifications

```yaml
- alias: "Daily energy report at 21:00"
  trigger:
    platform: time
    at: "21:00:00"
  action:
    - service: notify.telegram
      data:
        message: >
          ☀️ Today's Energy Summary:
          🔌 Routed: {{ states('sensor.total_energy_routed') }} kWh
          📊 Imported: {{ states('sensor.total_energy_imported') }} kWh
```

### Temperature-Based Shutdown

```yaml
- alias: "Stop heating if temperature critical"
  trigger:
    platform: numeric_state
    entity_id: sensor.water_tank_temperature
    above: 80
  action:
    - service: select.select_option
      target:
        entity_id: select.ssr_operating_mode
      data:
        option: "Stop"
```

## Performance Notes

- **Update interval**: Default 1 second (parameter requests)
- **STATS frequency**: Continuous (1-5 sec from Arduino)
- **Memory**: ~85% of ESP8266 RAM used in normal operation
- **WiFi**: Stable connection required for reliable OTA updates

## Known Limitations

⚠️ **Single ESP8266**: Temperature sensor requires 1-Wire library (can be added)
⚠️ **EEPROM writes**: Limit to prevent wear (< 100k cycles per variable)
⚠️ **OTA updates**: Requires 1MB free space on ESP8266

## Future Enhancements

- [ ] Temperature sensor integration via DS18B20
- [ ] MQTT broker support for legacy systems
- [ ] Boost mode scheduling
- [ ] Historical data storage on ESP
- [ ] Multi-relay expansion support
- [ ] Integration with solar forecast APIs

## Support & Resources

| Resource | Link |
|----------|------|
| **ESPHome Docs** | https://esphome.io/ |
| **Home Assistant ESPHome Integration** | https://www.home-assistant.io/integrations/esphome/ |
| **MaxPV Original Project** | https://github.com/Jetblack31/MaxPV |
| **EcoPV Project** | https://github.com/Jetblack31/EcoPV |
| **ESPHome UART Docs** | https://esphome.io/components/uart.html |

## License

This ESPHome migration maintains the same GNU Lesser General Public License v2.1 as the original MaxPV project.

## Contributing

Found an issue or have an improvement? 
1. Check existing documentation
2. Review [ESPHOME_ADVANCED_CONFIG.md](ESPHOME_ADVANCED_CONFIG.md) troubleshooting section
3. Test with current hardware
4. Submit with detailed reproduction steps

## Acknowledgments

- Original MaxPV/EcoPV by Bernard Legrand
- ESPHome community and developers
- Home Assistant ecosystem

---

**Updated**: 2026-03-17
**Status**: Ready for Production
**Tested On**: Wemos D1 mini, Arduino Nano, Home Assistant 2024+
