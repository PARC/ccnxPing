/*
 * Copyright (c) 2016, Xerox Corporation (Xerox)and Palo Alto Research Center (PARC)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Patent rights are not granted under this agreement. Patent rights are
 *       available under FRAND terms.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL XEROX or PARC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @author Nacho Solis, Christopher A. Wood, Palo Alto Research Center (Xerox PARC)
 * @copyright 2016, Xerox Corporation (Xerox)and Palo Alto Research Center (PARC).  All rights reserved.
 */
#include <stdio.h>

#include <getopt.h>

#include <LongBow/runtime.h>

#include <parc/algol/parc_Object.h>

#include <parc/security/parc_Security.h>
#include <parc/security/parc_IdentityFile.h>

#include <ccnx/common/ccnx_Name.h>

#include <ccnx/api/ccnx_Portal/ccnx_Portal.h>
#include <ccnx/api/ccnx_Portal/ccnx_PortalRTA.h>

#include "ccnxPerf_Common.h"

typedef struct ccnx_perf_server {
    CCNxPortal *portal;
    CCNxName *prefix;
    size_t payloadSize;

    uint8_t generalPayload[ccnxPerf_MaxPayloadSize];
} CCNxPerfServer;

/**
 * Create a new CCNxPortalFactory instance using a randomly generated identity saved to
 * the specified keystore.
 *
 * @return A new CCNxPortalFactory instance which must eventually be released by calling ccnxPortalFactory_Release().
 */
static CCNxPortalFactory *
_setupServerPortalFactory(void)
{
    const char *keystoreName = "server.keystore";
    const char *keystorePassword = "keystore_password";
    const char *subjectName = "server";

    return ccnxPerfCommon_SetupPortalFactory(keystoreName, keystorePassword, subjectName);
}

/**
 * Release the references held by the `CCNxPerfClient`.
 */
static bool
_ccnxPerfServer_Destructor(CCNxPerfServer **serverPtr)
{
    CCNxPerfServer *server = *serverPtr;
    ccnxPortal_Release(&(server->portal));
    ccnxName_Release(&(server->prefix));
    return true;
}

parcObject_Override(CCNxPerfServer, PARCObject,
                    .destructor = (PARCObjectDestructor *) _ccnxPerfServer_Destructor);

parcObject_ImplementAcquire(ccnxPerfServer, CCNxPerfServer);
parcObject_ImplementRelease(ccnxPerfServer, CCNxPerfServer);

/**
 * Create a new empty `CCNxPerfServer` instance.
 */
static CCNxPerfServer *
ccnxPerfServer_Create(void)
{
    CCNxPerfServer *server = parcObject_CreateInstance(CCNxPerfServer);

    CCNxPortalFactory *factory = _setupServerPortalFactory();
    server->portal = ccnxPortalFactory_CreatePortal(factory, ccnxPortalRTA_Message);
    ccnxPortalFactory_Release(&factory);

    server->prefix = ccnxName_CreateFromCString(ccnxPerf_DefaultPrefix);
    server->payloadSize = ccnxPerf_DefaultPayloadSize;

    return server;
}

/**
 * Create a `PARCBuffer` payload of the server-configured size.
 */
PARCBuffer *
_ccnxPerfServer_MakePayload(CCNxPerfServer *server)
{
    PARCBuffer *payload = parcBuffer_Wrap(server->generalPayload, ccnxPerf_MaxPayloadSize, 0, server->payloadSize);
    return payload;
}

/**
 * Run the `CCNxPerfServer` indefinitely.
 */
static void
_ccnxPerfServer_Run(CCNxPerfServer *server)
{
    size_t yearInSeconds = 60 * 60 * 24 * 365;
    if (ccnxPortal_Listen(server->portal, server->prefix, yearInSeconds, CCNxStackTimeout_Never)) {
        while (true) {
            CCNxMetaMessage *request = ccnxPortal_Receive(server->portal, CCNxStackTimeout_Never);

            // This should never happen.
            if (request == NULL) {
                break;
            }

            CCNxInterest *interest = ccnxMetaMessage_GetInterest(request);
            if (interest != NULL) {
                CCNxName *interestName = ccnxInterest_GetName(interest);

                PARCBuffer *payload = _ccnxPerfServer_MakePayload(server);

                CCNxContentObject *contentObject = ccnxContentObject_CreateWithNameAndPayload(interestName, payload);
                CCNxMetaMessage *message = ccnxMetaMessage_CreateFromContentObject(contentObject);
                
                if (ccnxPortal_Send(server->portal, message, CCNxStackTimeout_Never) == false) {
                    fprintf(stderr, "ccnxPortal_Send failed: %d\n", ccnxPortal_GetError(server->portal));
                }

                ccnxMetaMessage_Release(&message);
                parcBuffer_Release(&payload);

            }
            ccnxMetaMessage_Release(&request);
        }
    }
}

/**
 * Display the usage message.
 */
static void
_displayUsage(char *progName)
{
    printf("%s [-l locator] [-s size] \n", progName);
    printf("%s -h\n", progName);
    printf("           CCNx Simple Performance Test\n");
    printf("\n");
    printf("Example:\n");
    printf("    ccnxPerf_Server -l ccnx:/some/prefix -s 4096");
    printf("\n");
    printf("Options  \n");
    printf("     -h (--help) Show this help message\n");
    printf("     -l (--locator) Set the locator for this server. The default is 'ccnx:/locator'. \n");
    printf("     -s (--size) Set the payload size\n");
}

/**
 * Parse the command lines to initialize the state of the
 */
static bool
_ccnxPerfServer_ParseCommandline(CCNxPerfServer *server, int argc, char *argv[argc])
{
    static struct option longopts[] = {
        { "locator", required_argument, NULL, 'l' },
        { "size",    required_argument, NULL, 's' },
        { "help",    no_argument,       NULL, 'h' },
        { NULL,      0,                 NULL, 0   }
    };

    int c;
    while ((c = getopt_long(argc, argv, "l:s:h", longopts, NULL)) != -1) {
        switch (c) {
            case 'l':
                server->prefix = ccnxName_CreateFromCString(optarg);
                break;
            case 's':
                sscanf(optarg, "%zu", &(server->payloadSize));
                break;
            case 'h':
                _displayUsage(argv[0]);
                return false;
            default:
                break;
        }
    }

    return true;
};

int
main(int argc, char *argv[argc])
{
    parcSecurity_Init();

    CCNxPerfServer *server = ccnxPerfServer_Create();

    bool runServer = _ccnxPerfServer_ParseCommandline(server, argc, argv);
    if (runServer) {
        _ccnxPerfServer_Run(server);
    }

    ccnxPerfServer_Release(&server);

    parcSecurity_Fini();

    return EXIT_SUCCESS;
}
