# MaxPV ESPHome Serial Protocol Quick Reference

## Command Summary

### System Commands
| Command | Format | Response | Purpose |
|---------|--------|----------|---------|
| Get Parameters | `PARAM,END#` | `PARAM,p0,p1,p2,...#` | Read all config parameters |
| Get Version | `VERSION,END#` | `VERSION,3.60#` | Firmware version |
| Save Config | `SAVECFG,END#` | (none) | Save EEPROM |
| Load Config | `LOADCFG,END#` | (none) | Load EEPROM |
| Save Energy | `SAVEINDX,END#` | (none) | Save indices |
| Reset Energy | `INDX0,END#` | (none) | Zero indices |
| Soft Restart | `RESET,END#` | (none) | Reboot router |

### Control Commands
| Command | Format | Response | Purpose |
|---------|--------|----------|---------|
| SSR Mode | `SETSSR,{AUTO\|FORCE\|STOP},END#` | (none) | Main heater mode |
| Relay Mode | `SETRELAY,{AUTO\|FORCE\|STOP},END#` | (none) | Secondary load mode |
| Set Parameter | `SETPARAM,nn,value,END#` | (none) | Write single parameter |

## Data Formats

### STATS Message (Incoming)
```
STATS,Vrms,Irms,Pact,Papp,Prouted,Pexport[,Temp,Trelay,Eimpo,Eroute,Eexpo,Epulse]#
```

**Example:**
```
STATS,230.5,12.345,2840.1,2900.0,1500.5,125.3,45.2,480,1500.5,12000.5,500.2,2500.0#
```

**Field Breakdown:**
- `Vrms`: 230.5 V (voltage)
- `Irms`: 12.345 A (current)
- `Pact`: 2840.1 W (active power)
- `Papp`: 2900.0 VA (apparent power)
- `Prouted`: 1500.5 W (routed power)
- `Pexport`: 125.3 W (export power)
- `Temp`: 45.2 °C (water temperature)
- `Trelay`: 480 min (relay on time)
- `Eimpo`: 1500.5 kWh (energy imported)
- `Eroute`: 12000.5 kWh (energy routed)
- `Eexpo`: 500.2 kWh (energy exported)
- `Epulse`: 2500.0 kWh (pulse counter)

### PARAM Message (Incoming)
```
PARAM,p0,p1,p2,...,pN#
```

Parameters in format: int or float depending on index
- Integer: "50"
- Float: "30.5"

### VERSION Message (Incoming)
```
VERSION,EcoPV 3.60#
```

## Message Timing

| Event | Interval | Origin |
|-------|----------|--------|
| STATS | 1-5 sec | Arduino Nano (continuous) |
| PARAM Request | 1 sec* | ESP (on demand) |
| Firmware | At startup | ESP (on demand) |

*Configurable in `scan_interval` setting

## Serial Settings

| Setting | Value |
|---------|-------|
| Baud Rate | 500000 |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Flow Control | None |
| RX Buffer | 256 bytes (minimum) |

## Physical Connections (Wemos D1 mini)

```
Wemos D1 mini          Arduino Nano
───────────────        ────────────────
GPIO1 (TX/D4)  ────→   RX (Pin 0)
GPIO3 (RX/D9)  ←────   TX (Pin 1)
GND            ────→   GND
3V3 (optional) ←────   (use separate power)
```

## Example Communication Session

```
> PARAM,END#
< PARAM,50,30,100,3000,1,23,15,180,480,21#

> VERSION,END#
< VERSION,EcoPV 3.60#

> SETSSR,AUTO,END#
(response in next STATS message)

< STATS,230.5,12.3,2840.1,2900.0,1500.5,125.3#
< STATS,230.4,12.2,2835.0,2895.0,1505.0,130.0#
< STATS,230.6,12.4,2845.0,2905.0,1495.0,120.0#
```

## Sensor Mapping

| Home Assistant Entity | STATS Field | Index | Unit |
|----------------------|-------------|-------|------|
| grid_voltage_rms | Vrms | 1 | V |
| grid_current_rms | Irms | 2 | A |
| grid_power_active | Pact | 3 | W |
| grid_power_apparent | Papp | 4 | VA |
| routed_power | Prouted | 5 | W |
| export_power | Pexport | 6 | W |
| water_temperature | Temp | 7 | °C |
| relay_on_time | Trelay | 8 | min |
| total_energy_imported | Eimpo | 9 | kWh |
| total_energy_routed | Eroute | 10 | kWh |
| total_energy_exported | Eexpo | 11 | kWh |
| pv_counter_energy | Epulse | 12 | kWh |

## Error Handling

### Timeout Detection
- No STATS message for > 10 seconds = Connection lost
- Triggers: `contact_established_binary = false`
- Action: Attempt parameter re-request

### Invalid Messages
- Message doesn't end with `#` = Ignored
- Incomplete fields in STATS = Message skipped
- Parse error in number = Field skipped, log warning

### Recovery
1. Automatic: Component continues attempting communication
2. Manual: Use "Request EcoPV Parameters" button
3. Reset: Soft restart Arduino via "Soft Restart EcoPV" button

## Parameter Write Example

To set PV installation power to 3500 Wc (parameter 3):
```
> SETPARAM,03,3500,END#
> PARAM,END#        (verify)
< PARAM,50,30,100,3500,1,...#
```

## Debugging Commands (via ESPHome logs)

Enable debug logging in `maxpv.yaml`:
```yaml
logger:
  level: DEBUG
  logs:
    ecopv: DEBUG
```

Monitor:
```bash
esphome logs maxpv.yaml | grep ecopv
```

## Safety Features

⚠️ **Automatic Safeguards:**
- If no contact for 10 seconds: Mark as disconnected
- Max SSR/Relay commands: 1 per second
- EEPROM writes: Limited to prevent wear-out
- Buffer overflow: Messages > 256 bytes truncated

⚠️ **Manual Safeguards:**
- Always verify PARAM response after SETPARAM
- Test changes on off-peak hours first
- Monitor water temperature during mode changes
- Keep firmware backups before OTA updates

## Performance Optimization

### High Frequency Updates
To increase sensor update rate to 0.5 sec:
```yaml
ecopv:
  scan_interval: 500ms
```

### Low Frequency Updates
To decrease load (1 parameter request per 5 sec):
```yaml
ecopv:
  scan_interval: 5000ms
```

### Monitor Memory
ESPHome automatically logs memory warnings:
```
[esphome:esp8266] Heap: 42480 / 50688 (83%)
```

If consistently > 90%, reduce update frequency.

## Troubleshooting Quick Lookup

| Symptom | Debug Step | Solution |
|---------|-----------|----------|
| No STATS received | Check levels with `esphome logs` | Verify RX/TX connection |
| Garbled STATS | Monitor raw serial | Swap TX/RX pins |
| Random disconnects | Check WiFi signal | Move closer to AP |
| Parameter set fails | Verify PARAM echo | Re-send SETPARAM |
| Sensors stuck | Check last STATS time | Soft restart Arduino |

---

**Note**: All messages must terminate with `#` character
**Baud Rate**: CRITICAL - Must be exactly 500000
**Buffer Size**: Minimum 256 bytes recommended
