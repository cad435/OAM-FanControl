#include <knx.h>
#include <Arduino.h>
#include "OpenKNX.h"
#include "FanModule.h"

void setup()
{
	//Bei Firmwareänderungen, die keine neue knxprod benötigen, kann die Revision erhöht werden.
	const uint8_t firmwareRevision = 0;
    openknx.init(firmwareRevision);
    openknx.addModule(1, openknxFanModule);
    //openknx.addModule(9, openknxFileTransferModule); Es können auch weitere Module hinzugefügt werden
    openknx.setup();
}

void loop()
{
	openknx.loop();
}

#ifdef OPENKNX_DUALCORE
void setup1()
{
	openknx.setup1();
}

void loop1()
{
	openknx.loop1();
}
#endif