//
//  main.c
//  IOKitUserClient
//
//  Created by Jonathan Zdziarski on 7/26/16.
//  Copyright © 2016 Jonathan Zdziarski. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <errno.h>

#define DRIVER "com_zdziarski_driver_IOKitTest"
#define BUFSIZE 4096

typedef struct ConsoleMessage
{
    char message[1024];
    uint32_t code;
} ConsoleMessage;

enum TestClientRequestCode {
    kTestClientPostConsoleMessage,
    kTestClientPostConsoleMessageAsync,
    kTestClientAssignBuffer,
    kTestClientReleaseBuffer,
    
    kTestClientMethodCount
};

kern_return_t kernTestClientPostConsoleMessage(io_connect_t connection, const char *msg, uint32_t code)
{
    ConsoleMessage m;
    strncpy(m.message, msg, sizeof(m.message)-1);
    m.code = code;
    
    return IOConnectCallMethod(connection, kTestClientPostConsoleMessage,
                               NULL, 0, &m, sizeof(ConsoleMessage), NULL, NULL, NULL, NULL);
}

kern_return_t kernTestClientAssignBuffer(io_connect_t connection, void *buf, uint32_t buflen)
{
    uint64_t args[2];
    
    args[0] = (uint64_t) buf;
    args[1] = buflen;
    
    return IOConnectCallMethod(connection, kTestClientAssignBuffer, args, 2, NULL, 0, NULL, NULL, NULL, NULL);
}

kern_return_t kernTestClientReleaseBuffer(io_connect_t connection)
{
    return IOConnectCallMethod(connection, kTestClientReleaseBuffer, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
}

int main(int argc, char *argv[]) {
    io_iterator_t iter = 0;
    io_service_t service = 0;
    kern_return_t kr;
    
    CFDictionaryRef matchDict = IOServiceMatching(DRIVER);
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchDict, &iter);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices failed on error %d\n", kr);
        return -1;
    }
    
    while ((service = IOIteratorNext(iter)) != 0)
    {
        task_port_t owningTask = mach_task_self();
        io_connect_t driverConnection;
        CFStringRef className;
        uint32_t type = 0;
        kern_return_t kr;
        io_name_t name;
        
        className = IOObjectCopyClass(service);
        IORegistryEntryGetName(service, name);

        // example: set property
        fprintf(stderr, "found driver '%s'\n", name);
        IORegistryEntrySetCFProperty(service, CFSTR("StopMessage"), CFSTR("the driver has stopped"));
        
        // example: establish connection
        kr = IOServiceOpen(service, owningTask, type, &driverConnection);
        if (kr == KERN_SUCCESS) {
            void *buf;
            int i;
            
            fprintf(stderr, "connected to driver %s\n", DRIVER);
            
            // example: call method
            for(i=0;i<10;++i) {
                fprintf(stderr, "invoking kTestClientPostConsoleMessage: [%d] I am counting\n", i);
                kr = kernTestClientPostConsoleMessage(driverConnection, "I am counting", i);
                if (kr != KERN_SUCCESS)
                    fprintf(stderr, "kTestClientPostConsoleMessage failed on error %d\n", kr);
                
                sleep(1);
            }
            
            // example: share memory with kernel
            buf = calloc(1, BUFSIZE);
            strcpy(buf, "Ground Control to Major Tom\n");
            fprintf(stderr, "assigning memory buffer at %p[%llu] of %d bytes\n", buf, (uint64_t)buf, BUFSIZE);
            kr = kernTestClientAssignBuffer(driverConnection, buf, BUFSIZE);
            if (kr == KERN_SUCCESS) {
                
                
                fprintf(stderr, "releasing memory buffer at %p of %d bytes\n", buf, BUFSIZE);
                kr = kernTestClientReleaseBuffer(driverConnection);
                if (kr != KERN_SUCCESS) {
                    fprintf(stderr, "kernTestClientReleaseBuffer failed on error %d\n", kr);
   
                }
                free(buf);
            } else {
                fprintf(stderr, "kernTestClientAssignBuffer failed on error %d\n", kr);
            }
            
            // close connection
            fprintf(stderr, "closing connection to %s\n", DRIVER);
            IOServiceClose(service);
        } else {
            fprintf(stderr, "IOServiceOpen failed on error %d\n", kr);
        }
        
        fprintf(stderr, "releasing objects\n");
        CFRelease(className);
        IOObjectRelease(service);
        IOObjectRelease(iter);
    }
    return 0;
}
