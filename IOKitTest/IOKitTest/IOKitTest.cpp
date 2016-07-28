#include "IOKitTest.hpp"
#include <IOKit/IOLib.h>

#define super IOService

OSDefineMetaClassAndStructors(com_zdziarski_driver_IOKitTest, IOService)

bool com_zdziarski_driver_IOKitTest::init (OSDictionary *dict)
{
    bool res = super::init(dict);
    IOLog("IOKitTest::init\n");
    setProperty("IOUserClientClass", "com_zdziarski_driver_IOKitTestClient");

    return res;
}

void com_zdziarski_driver_IOKitTest::free (void)
{
    IOLog("IOKitTest::free\n");
    super::free();
}

IOService *com_zdziarski_driver_IOKitTest::probe (IOService *provider, SInt32* score)
{
    IOService *res = super::probe(provider, score);
    IOLog("IOKitTest::probe\n");
    return res;
}

bool com_zdziarski_driver_IOKitTest::start (IOService *provider)
{
    bool res = super::start(provider);
    super::registerService();

    IOLog("IOKitTest::start\n");
    return res;
}

void com_zdziarski_driver_IOKitTest::stop (IOService *provider)
{
    OSString *stopMessage;
    
    IOLog("IOKitTest::stop\n");
    stopMessage = OSDynamicCast(OSString, getProperty("StopMessage"));
    if (stopMessage) {
        IOLog("Stop message: %s\n", stopMessage->getCStringNoCopy());
    }
    
    super::stop(provider);
}

IOReturn com_zdziarski_driver_IOKitTest::setProperties(OSObject* properties)
{
    OSDictionary *propertyDict;
    
    propertyDict = OSDynamicCast(OSDictionary, properties);
    if (propertyDict != NULL)
    {
        OSObject *theValue;
        OSString *theString;
        
        theValue = propertyDict->getObject("StopMessage");
        theString = OSDynamicCast(OSString, theValue);
        if (theString != NULL)
        {
            setProperty("StopMessage", theString);
            return kIOReturnSuccess;
        }
    }
    
    return kIOReturnUnsupported;
}