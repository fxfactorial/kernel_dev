//
//  IOKitTestClient.hpp
//  IOKitTest
//
//  Created by Jonathan Zdziarski on 7/25/16.
//  Copyright © 2016 Jonathan Zdziarski. All rights reserved.
//

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include "IOKitTest.hpp"

#ifndef IOKitTestClient_hpp
#define IOKitTestClient_hpp


typedef struct ConsoleMessage
{
    char message[1024];
    uint32_t code;
} ConsoleMessage;

typedef struct ConsoleMessageParams
{
    ConsoleMessage consoleMessage;
    OSAsyncReference64 asyncRef;
    OSObject *userClient;
} ConsoleMessageParams;


enum TestClientRequestCode {
    kTestClientPostConsoleMessage,
    kTestClientPostConsoleMessageAsync,
    kTestClientAssignBuffer,
    kTestClientReleaseBuffer,
    
    kTestClientMethodCount
};

class com_zdziarski_driver_IOKitTestClient : public IOUserClient
{
    OSDeclareDefaultStructors(com_zdziarski_driver_IOKitTestClient)
    
private:
    task_t m_task;
    bool m_taskIsAdmin;
    com_zdziarski_driver_IOKitTest *m_driver;
    IOMemoryMap *m_memoryMap;
    IOMemoryDescriptor *m_memoryDescriptor;
    void *m_buffer;

public:
    virtual bool initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) override;
    virtual bool start(IOService *provider) override;
    virtual IOReturn clientClose(void) override;
    virtual void stop(IOService *provider) override;
    virtual void free(void) override;

    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args, IOExternalMethodDispatch *dispatch,
                            OSObject *target, void *reference) override;

protected:
    static const IOExternalMethodDispatch sMethods[kTestClientMethodCount];
    static IOReturn sPostConsoleMessage(OSObject *target, void *reference, IOExternalMethodArguments *args);
    static IOReturn sPostConsoleMessageAsync(OSObject *target, void *reference, IOExternalMethodArguments *args);
    static IOReturn sAssignBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args);
    static IOReturn sReleaseBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args);

    static void ConsoleThreadFunc(void *parameter, wait_result_t);
    static kern_return_t mapUserSpaceBuffer(OSObject *target, void *buf, uint64_t len);
    IOReturn postConsoleMessage(ConsoleMessage *consoleMessage, int delay);
};

#endif /* IOKitTestClient_hpp */
