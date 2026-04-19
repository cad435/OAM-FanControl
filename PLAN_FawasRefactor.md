# Plan: FawasAirSolitaire Class + ETS Fan Type Dropdown

## Context

The project currently uses MaicoPPB30 for all fan channels. MaicoPPB30 is a push-pull unit with 2 motors (S1+S2) per unit — it uses both HW channels for a single logical fan. The Fawas AirSolitaire is a single-motor fan — one HW channel per fan, supporting 2 independent fans on the Reg1 board.

The FullControl mode and its PWM mapping were added directly to MaicoPPB30, diverging it from upstream. This refactor moves all Fawas/FullControl-specific logic into a dedicated class and restores MaicoPPB30 to (near-)upstream state.

Additionally, an ETS dropdown per channel will let users select the fan type at configuration time, eliminating the need for compile flags to choose between fan classes.

## Architecture Overview

```
Fan (base class)
├── MaicoPPB30     — push-pull, 2 motors (S1+S2), step-based speed 0–5
│                    Constructor: (hw, s1_pin, s2_pin, sw_pin)
│
└── FawasAirSolitaire — single motor, percent-based speed 0–100%, direction
                         Constructor: (hw, pwm_pin, sw_pin)
                         Implements FullControl natively (power/speed/direction IS its control model)
```

FanModule uses the ETS FanType parameter (read at runtime) to instantiate the correct class per channel. No more compile flags for fan type selection.

---

## Part 1: Fan Base Class Changes

### File: `lib/OFM-FanControl/src/Fan.h`

**Change FullControl methods from pure virtual to virtual with empty defaults:**
```cpp
// Full control mode — default no-op, overridden by fans that support it
virtual void setFullControlPower(bool on) {}
virtual void setFullControlSpeed(uint8_t percent) {}
virtual void setFullControlDirection(uint8_t dir) {}
virtual uint8_t getFullControlSpeed() { return 0; }
```

This allows MaicoPPB30 to NOT implement them (stays original), while FawasAirSolitaire overrides them.

### File: `lib/OFM-FanControl/src/Fan.cpp`

No changes needed beyond what's already there. The FullControl mode handling in `setOperatingMode()` stays in the base class — it calls the virtual methods which are no-ops for Maico and real implementations for Fawas.

---

## Part 2: Revert MaicoPPB30 to Upstream

### File: `lib/OFM-FanControl/src/MaicoPPB30.h`

Remove:
- `setFullControlPower/Speed/Direction/getFullControlSpeed` override declarations
- `_fullControlPercent`, `_fullControlDirection` private members
- `updateFullControl()` private method

### File: `lib/OFM-FanControl/src/MaicoPPB30.cpp`

Remove:
- `if (_operatingMode == OperatingMode::FullControl) return;` from `updateMode()`
- All `setFullControlPower/Speed/Direction`, `getFullControlSpeed`, `updateFullControl` implementations

Result: MaicoPPB30 matches upstream except for the base class having `OperatingMode::FullControl` in the enum (benign — it just won't be used).

---

## Part 3: Create FawasAirSolitaire Class

### File: `lib/OFM-FanControl/src/FawasAirSolitaire.h` (NEW)

```cpp
#pragma once
#include "Fan.h"
#include "IFanHardware.h"

class FawasAirSolitaire : public Fan {
public:
    FawasAirSolitaire(IFanHardware& hw, uint8_t pwm_pin, uint8_t sw_pin);

    void changeFanSpeedDelegate(int16_t fanSpeed) override;
    int16_t getFanSpeed() override;

    // Full control — native control model for this fan
    void setFullControlPower(bool on) override;
    void setFullControlSpeed(uint8_t percent) override;
    void setFullControlDirection(uint8_t dir) override;
    uint8_t getFullControlSpeed() override;

protected:
    void updateMode() override;

private:
    const uint8_t _pwmPin;
    const uint8_t _swPin;

    uint8_t _speedPercent = 0;
    int8_t _direction = 1;  // 1=Zuluft(A), -1=Abluft(B)
    bool _powerOn = false;

    // Step-based speed for Manual/Automatic modes (maps to percent internally)
    int16_t _fanStep = 0;
    static constexpr std::array<int16_t, 6> _FanSteps = {0, 20, 40, 60, 80, 100};

    void applyPWM();
    static int16_t getPWMLevel(int16_t fraction, int16_t base = 24, int16_t resolution = 1024);
};
```

### File: `lib/OFM-FanControl/src/FawasAirSolitaire.cpp` (NEW)

Key implementation details:
- **Constructor**: `init(pwm_pin, 0xFF, sw_pin)` — passes 0xFF for unused S2 pin
  (or modify IFanHardware::init to accept optional pin count — TBD, may just reuse existing init with dummy S2)
- **Step-based speed** (Manual/Automatic modes): maps steps 0–5 to percent [0, 20, 40, 60, 80, 100], then applies PWM
- **FullControl**: direct percent + direction control
- **PWM mapping** (from FanActor_TestFirmware):
  - Center (50% duty) = stopped
  - Direction A: offset = +speed → duty = (100 + speed) / 200
  - Direction B: offset = -speed → duty = (100 - speed) / 200
  - Using existing getPWMLevel with base 24: `fraction = 12 + direction * (percent * 12 / 100)`
- **updateMode()**: 
  - Off: set speed 0, power off
  - Manual/Automatic: apply step-based speed
  - FullControl: skip (controlled directly via setFullControl* methods)
- **Power control**: `_hw.setDigital(_swPin, on)` — high-side switch

---

## Part 4: ETS Fan Type Dropdown

### File: `lib/OFM-FanControl/src/Fan.share.xml`

Add new ParameterType:
```xml
<ParameterType Id="%AID%_PT-FanType" Name="FanType">
    <TypeRestriction Base="Value" SizeInBit="2">
        <Enumeration Text="Maico PP B30 (Push-Pull)" Value="0" Id="%AID%_PT-FanType_EN-0" />
        <Enumeration Text="Fawas AirSolitaire 160" Value="1" Id="%AID%_PT-FanType_EN-1" />
    </TypeRestriction>
</ParameterType>
```

### File: `lib/OFM-FanControl/src/Fan.templ.xml`

Add new Parameter per channel (before CH_OpMode, at Offset 0 BitOffset 3 — after StatusLED uses bits 7-5):
```xml
<Parameter Id="%AID%_P-%TT%%CC%011" Name="CH%C%_FanType" ParameterType="%AID%_PT-FanType" 
    Text="Lüftertyp" Value="0">
    <Memory CodeSegment="%AID%_RS-04-00000" Offset="0" BitOffset="0" />
</Parameter>
```

Note: The exact memory offset needs to be determined — must not overlap with existing parameters. Currently Offset 0 is used by CH_OpMode at BitOffset 0 with 3 bits. The FanType (2 bits) could go at Offset 0, BitOffset 3 (bits 4-3). Or use a new offset byte (Offset 15).

**Safer approach**: Use Offset 15, BitOffset 0 (new byte at end of param block). This requires bumping `FAN_ParamBlockSize` from 15 to 16.

Add ParameterRef + ParameterRefRef in Dynamic section (always visible, before OpMode).

Add choose/when for FanType-dependent KO visibility:
- **Vollsteuerung mode**: Only visible when FanType = Fawas (1), since Maico doesn't support it
  - Alternative: show for both but Maico ignores (using empty default virtual methods) — simpler, no conditional visibility needed

**Recommended**: Show Vollsteuerung for ALL fan types. The base class default no-op implementations mean sending FullControl commands to a Maico just does nothing. This avoids complex XML nesting. In a future version, MaicoPPB30 could implement FullControl too if needed.

---

## Part 5: FanModule — Runtime Fan Type Selection

### File: `lib/OFM-FanControl/src/FanModule.h`

Replace static fan instantiation with pointers:
```cpp
#include "MaicoPPB30.h"
#include "FawasAirSolitaire.h"

private:
    RP2040FanHardware _fan1Hw;
    RP2040FanHardware _fan2Hw;
    Fan* _fan1 = nullptr;
    Fan* _fan2 = nullptr;
    FanChannel* _channel[FAN_ChannelCount];
    // ... rest unchanged
```

### File: `lib/OFM-FanControl/src/FanModule.cpp`

In `setup(bool configured)`, read the FanType parameter and instantiate accordingly:
```cpp
void FanModule::setup(bool configured) {
    // Read fan type from ETS parameter for each channel
    // (using _channelIndex trick or direct param access)
    
    uint8_t fanType1 = cycleParamFAN_CH_FanType(0);  // channel 0
    uint8_t fanType2 = cycleParamFAN_CH_FanType(1);  // channel 1
    
    if (fanType1 == 0) // Maico
        _fan1 = new MaicoPPB30(_fan1Hw, FAN1_S1_PWM_PIN, FAN1_S2_PWM_PIN, FAN1_SW_PIN);
    else // Fawas
        _fan1 = new FawasAirSolitaire(_fan1Hw, FAN1_S1_PWM_PIN, FAN1_SW_PIN);
    
    if (fanType2 == 0) // Maico
        _fan2 = new MaicoPPB30(_fan2Hw, FAN2_S1_PWM_PIN, FAN2_S2_PWM_PIN, FAN2_SW_PIN);
    else // Fawas
        _fan2 = new FawasAirSolitaire(_fan2Hw, FAN2_S1_PWM_PIN, FAN2_SW_PIN);
    
    _channel[0] = new FanChannel(0, *_fan1);
    _channel[1] = new FanChannel(1, *_fan2);
    // ... rest of setup
}
```

**Note on parameter access**: OpenKNX channel parameters use `_channelIndex` for the macro. In FanModule (not a Channel subclass), we need to read the raw parameter. Pattern: define a helper or access `knx.paramByte()` directly with calculated offset.

### Remove compile-flag fan selection

The `#ifdef DEVICE_REG1_FAN_ADDON_X2` blocks in hardware.h stay (for pin definitions), but the fan TYPE is now purely runtime via ETS parameter. Both boards can use either fan type.

---

## Part 6: FanChannel — No Changes Expected

FanChannel already talks to `Fan&` — it doesn't know or care about the concrete type. The FullControl KO handlers call `_fan.setFullControlPower()` etc. which dispatch to the correct virtual override. No changes needed.

---

## Part 7: IFanHardware — Minor Consideration

The current `init(s1_pin, s2_pin, sw_pin)` takes 3 pins. FawasAirSolitaire only needs 2 (pwm + sw). Options:
1. Pass 0xFF for unused s2_pin — `init(pwm_pin, 0xFF, sw_pin)`. RP2040 silently ignores invalid pins (already tested with -1 pins). **Simplest, recommended.**
2. Add an overloaded `init(pin, sw_pin)` — more correct but changes the interface.

**Go with option 1** — minimal change, already proven to work.

---

## Summary of File Changes

| File | Action |
|------|--------|
| `Fan.h` | Change 4 pure virtual FullControl methods to virtual with empty defaults |
| `Fan.cpp` | No changes |
| `MaicoPPB30.h` | Remove FullControl overrides + private members (revert to upstream) |
| `MaicoPPB30.cpp` | Remove FullControl implementations + updateMode guard (revert to upstream) |
| `FawasAirSolitaire.h` | **NEW** — single-motor fan class |
| `FawasAirSolitaire.cpp` | **NEW** — PWM control, step mapping, FullControl implementation |
| `FanModule.h` | Fan pointers instead of static objects |
| `FanModule.cpp` | Runtime fan instantiation based on ETS FanType parameter |
| `Fan.share.xml` | Add PT-FanType ParameterType |
| `Fan.templ.xml` | Add FanType parameter + ParameterRef + UI placement |
| `Fan.conf.xml` | Bump version to 0.9 |
| `library.json` | Bump version to 0.9 |

## Version Bump

No version bump because development --> delete the RP2040 flash and reprogramm with ETS

## Verification

1. Run OpenKNXproducer — all sanity checks must pass
2. Build with PlatformIO — clean compile
3. Erase KNX flash (picotool blank.bin at 0x100F4000)
4. Flash firmware + program from ETS with new knxprod
5. Test: set FanType=Fawas in ETS, verify FullControl KOs work
6. Test: set FanType=Maico in ETS, verify step-based control works
7. Verify MaicoPPB30 diff against upstream is minimal (only OperatingMode::FullControl enum in base class)
