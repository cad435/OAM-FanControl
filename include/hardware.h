#pragma once

#if defined(DEVICE_REG1_FAN_ADDON_X2)
    #include "Reg1_FanAddon_X2.h"
#else
    // Default: original MrSpieb FanControl board
    #include "MrSpiebFanControlHardware.h"
#endif