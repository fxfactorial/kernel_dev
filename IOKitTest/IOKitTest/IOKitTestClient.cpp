//
//  IOKitTestClient.cpp
//  IOKitTest
//
//  Created by Jonathan Zdziarski on 7/25/16.
//  Copyright © 2016 Jonathan Zdziarski. All rights reserved.
//

#include "IOKitTestClient.hpp"

#define super IOUserClient
OSDefineMetaClassAndStructors(com_zdziarski_driver_IOKitTestClient, IOUserClient)

const IOExternalMethodDispatch
com_zdziarski_driver_IOKitTestClient::sMethods[kTestClientMethodCount] =
{
    { &com_zdziarski_driver_IOKitTestClient::sPostConsoleMessage, 0, sizeof(ConsoleMessage), 0, 0 },
    { &com_zdziarski_driver_IOKitTestClient::sPostConsoleMessageAsync, 0, sizeof(ConsoleMessage), 0, 0 },
    { &com_zdziarski_driver_IOKitTestClient::sAssignBuffer, 2, 0, 0, 0 },
    { &com_zdziarski_driver_IOKitTestClient::sReleaseBuffer, 0, 0, 0, 0 }
};

IOReturn com_zdziarski_driver_IOKitTestClient::sPostConsoleMessage(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
    IOReturn ret;
    com_zdziarski_driver_IOKitTestClient *me;
    
    me = (com_zdziarski_driver_IOKitTestClient *)target;
    ret = me->postConsoleMessage((ConsoleMessage *)args->structureInput, 0);
    
    return ret;
}

void com_zdziarski_driver_IOKitTestClient::ConsoleThreadFunc(void *parameter, wait_result_t)
{
    ConsoleMessageParams *params = (ConsoleMessageParams *)parameter;
    com_zdziarski_driver_IOKitTestClient *me;
    
    me = (com_zdziarski_driver_IOKitTestClient *)params->userClient;
    me->postConsoleMessage(&params->consoleMessage, 10000);
    sendAsyncResult64(params->asyncRef, kIOReturnSuccess, NULL, 0);
    params->userClient->release();
    IOFree(params, sizeof(ConsoleMessageParams));
}

IOReturn com_zdziarski_driver_IOKitTestClient::sPostConsoleMessageAsync(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
    ConsoleMessageParams *params;
    thread_t newThread;
    
    params = (ConsoleMessageParams*)IOMalloc(sizeof(ConsoleMessageParams));
    bcopy(args->asyncReference, params->asyncRef, sizeof(OSAsyncReference64));
    bcopy((const void *)args->structureInput, (void *)&params->consoleMessage, sizeof(ConsoleMessage));
    params->userClient = target;
    target->retain();
    
    kernel_thread_start(ConsoleThreadFunc, params, &newThread);
    thread_deallocate(newThread);
    return kIOReturnSuccess;
}

IOReturn com_zdziarski_driver_IOKitTestClient::postConsoleMessage(ConsoleMessage *consoleMessage, int delay)
{
    if (delay)
        IODelay(delay * consoleMessage->code);
    printf("[%d] %s delay: %d\n", consoleMessage->code, consoleMessage->message, delay);
    return KERN_SUCCESS;
}

IOReturn com_zdziarski_driver_IOKitTestClient::sAssignBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
    com_zdziarski_driver_IOKitTestClient *me;
    uint64_t buf = 0, buflen = 0;
    IOReturn ret = KERN_SUCCESS;
    void *bufaddr;
    
    me = (com_zdziarski_driver_IOKitTestClient *)target;
    
    buf = args->scalarInput[0];
    buflen = args->scalarInput[1];
    bufaddr = (void *)buf;
    
    printf("received buf %llu, len %llu, addr %p\n", buf, buflen, bufaddr);
    
    return mapUserSpaceBuffer(me, bufaddr, buflen);
    return ret;
}

kern_return_t com_zdziarski_driver_IOKitTestClient::mapUserSpaceBuffer(OSObject *target, void *buf, uint64_t len)
{
    com_zdziarski_driver_IOKitTestClient *me;
    bool prepared = false;
    char msg[128];
    
    me = (com_zdziarski_driver_IOKitTestClient *)target;

    me->m_memoryDescriptor = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)buf, len, kIODirectionInOut, me->m_task);
    if (! me->m_memoryDescriptor)
        goto fail;
    
    if (me->m_memoryDescriptor->prepare() != kIOReturnSuccess)
        goto fail;
    prepared = true;
    
    me->m_memoryMap = me->m_memoryDescriptor->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapReadOnly);
    if (! me->m_memoryMap)
        goto fail;
    
    me->m_buffer = (void *)me->m_memoryMap->getVirtualAddress();
    if (!me->m_buffer)
        goto fail;
    
    printf("successfully mapped user buffer to kernel memory %p", (void *)me->m_buffer);
    bcopy(me->m_buffer, msg, sizeof(msg));
    printf("message from ground control: %s\n", msg);
    return KERN_SUCCESS;
    
fail:
    if (me->m_memoryDescriptor) {
        if (prepared)
            me->m_memoryDescriptor->complete();
        me->m_memoryDescriptor->release();
        
    }
    
    if (me->m_memoryMap)
        me->m_memoryMap->release();
    
    me->m_memoryDescriptor = NULL;
    me->m_memoryMap = NULL;
    me->m_buffer = NULL;
    
    return KERN_MEMORY_ERROR;
}

IOReturn com_zdziarski_driver_IOKitTestClient::sReleaseBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
    com_zdziarski_driver_IOKitTestClient *me;
    
    me = (com_zdziarski_driver_IOKitTestClient *)target;

    me->m_buffer = NULL;
    me->m_memoryMap->release();
    me->m_memoryDescriptor->complete();
    me->m_memoryDescriptor->release();
    
    me->m_memoryMap = NULL;
    me->m_memoryDescriptor = NULL;
    
    return KERN_SUCCESS;
}

IOReturn com_zdziarski_driver_IOKitTestClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args, IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
    
    IOLog("%s[%p]::%s(%d, %p, %p, %p, %p)\n", getName(), this, __FUNCTION__,
          selector, args, dispatch, target, reference);
    
    if (selector >= kTestClientMethodCount)
        return kIOReturnUnsupported;
    
    dispatch = (IOExternalMethodDispatch *)&sMethods[selector];
    target = this;
    reference = NULL;
    return super::externalMethod(selector, args, dispatch, target, reference);
}

bool com_zdziarski_driver_IOKitTestClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties)
{
    printf("%s client init\n", __PRETTY_FUNCTION__);
    if (!owningTask)
        return false;
    
    if (! super::initWithTask(owningTask, securityToken, type, properties))
        return false;
    
    m_task = owningTask;
    m_taskIsAdmin = false;
    
    IOReturn ret = clientHasPrivilege(securityToken, kIOClientPrivilegeAdministrator);
    if (ret == kIOReturnSuccess)
    {
        m_taskIsAdmin = true;
    }
    
    return true;
}

bool com_zdziarski_driver_IOKitTestClient::start(IOService *provider)
{
    printf("%s client start\n", __PRETTY_FUNCTION__);
    if (! super::start(provider))
        return false;
    
    m_driver = OSDynamicCast(com_zdziarski_driver_IOKitTest, provider);
    if (!m_driver)
        return false;
    return true;
}

IOReturn com_zdziarski_driver_IOKitTestClient::clientClose(void)
{
    printf("%s client close\n", __PRETTY_FUNCTION__);
    terminate();
    return kIOReturnSuccess;
}

void com_zdziarski_driver_IOKitTestClient::stop(IOService *provider)
{
    super::stop(provider);
}

void com_zdziarski_driver_IOKitTestClient::free(void)
{
    super::free();
}

