#include "plugin.h"

bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
    dprintf("pluginInit(pluginHandle: %d)\n", pluginHandle);
    return true;
}

void pluginStop()
{
    dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle);
}

void pluginSetup()
{
    dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle);
}
