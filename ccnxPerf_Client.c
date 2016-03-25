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

#include <ccnx/api/ccnx_Portal/ccnx_Portal.h>
#include <ccnx/api/ccnx_Portal/ccnx_PortalRTA.h>

#include <parc/algol/parc_Clock.h>

#include <parc/algol/parc_Object.h>

#include <parc/security/parc_Security.h>
#include <parc/security/parc_IdentityFile.h>

#include "ccnxPerf_Stats.h"
#include "ccnxPerf_Common.h"

typedef enum {
    CCNxPerfClientMode_None = 0,
    CCNxPerfClientMode_Flood,
    CCNxPerfClientMode_PingPong,
    CCNxPerfClientMode_All
} CCNxPerfClientMode;

typedef struct ccnx_perf_client {
    CCNxPortal *portal;
    CCNxPerfStats *stats;
    CCNxPerfClientMode mode;

    CCNxName *prefix;

    size_t numberOfOutstanding;
    uint64_t receiveTimeoutInUs;
    int interestCounter;
    int count;
    uint64_t intervalInMs;
    int payloadSize;
    int nonce;
} CCNxPerfClient;

/**
 * Create a new CCNxPortalFactory instance using a randomly generated identity saved to
 * the specified keystore.
 *
 * @return A new CCNxPortalFactory instance which must eventually be released by calling ccnxPortalFactory_Release().
 */
static CCNxPortalFactory *
_setupClientPortalFactory(void)
{
    const char *keystoreName = "client.keystore";
    const char *keystorePassword = "keystore_password";
    const char *subjectName = "client";

    return ccnxPerfCommon_SetupPortalFactory(keystoreName, keystorePassword, subjectName);
}

/**
 * Release the references held by the `CCNxPerfClient`.
 */
static bool
_ccnxPerfClient_Destructor(CCNxPerfClient **clientPtr)
{
    CCNxPerfClient *client = *clientPtr;
    ccnxPortal_Release(&(client->portal));
    ccnxName_Release(&(client->prefix));
    return true;
}

parcObject_Override(CCNxPerfClient, PARCObject,
                    .destructor = (PARCObjectDestructor *) _ccnxPerfClient_Destructor);

parcObject_ImplementAcquire(ccnxPerfClient, CCNxPerfClient);
parcObject_ImplementRelease(ccnxPerfClient, CCNxPerfClient);

/**
 * Create a new empty `CCNxPerfClient` instance.
 */
static CCNxPerfClient *
ccnxPerfClient_Create(void)
{
    CCNxPerfClient *client = parcObject_CreateInstance(CCNxPerfClient);

    CCNxPortalFactory *factory = _setupClientPortalFactory();
    client->portal = ccnxPortalFactory_CreatePortal(factory, ccnxPortalRTA_Message);
    ccnxPortalFactory_Release(&factory);

    client->stats = ccnxPerfStats_Create();
    client->interestCounter = 100;
    client->prefix = ccnxName_CreateFromCString(ccnxPerf_DefaultPrefix);
    client->receiveTimeoutInUs = ccnxPerf_DefaultReceiveTimeoutInUs;
    client->count = 10;
    client->intervalInMs = 1000;
    client->nonce = rand();
    client->numberOfOutstanding = 0;

    return client;
}

/**
 * Get the next `CCNxName` to issue. Increment the interest counter
 * for the client.
 */
static CCNxName *
_ccnxPerfClient_CreateNextName(CCNxPerfClient *client)
{
    char *suffixBuffer = NULL;
    asprintf(&suffixBuffer, "/%x/%u/%06lu",
             client->nonce,
             client->payloadSize,
             (long) client->interestCounter);
    client->interestCounter++;
    CCNxName *name = ccnxName_ComposeNAME(ccnxName_Copy(client->prefix), suffixBuffer);
    parcMemory_Deallocate(&suffixBuffer);
    return name;
}

/**
 * Convert a timeval struct to a single microsecond count.
 */
static uint64_t
_ccnxPerfClient_CurrentTimeInUs(PARCClock *clock)
{
    struct timeval currentTimeVal;
    parcClock_GetTimeval(clock, &currentTimeVal);
    uint64_t microseconds = currentTimeVal.tv_sec * 1000000 + currentTimeVal.tv_usec;
    return microseconds;
}

/**
 * Run a single ping test.
 */
static void
_ccnxPerfClient_RunPing(CCNxPerfClient *client, size_t totalPings, uint64_t delayInUs)
{
    PARCClock *clock = parcClock_Wallclock();

    size_t outstanding = 0;
    bool checkOustanding = client->numberOfOutstanding > 0;

    for (int pings = 0; pings <= totalPings; pings++) {
        uint64_t nextPacketSendTime = 0;
        uint64_t currentTimeInUs = 0;

        // Continue to send ping messages until we've reached the capacity
        if (pings < totalPings && (!checkOustanding || (checkOustanding && outstanding < client->numberOfOutstanding))) {
            CCNxName *name = _ccnxPerfClient_CreateNextName(client);
            CCNxInterest *interest = ccnxInterest_CreateSimple(name);
            CCNxMetaMessage *message = ccnxMetaMessage_CreateFromInterest(interest);

            if (ccnxPortal_Send(client->portal, message, CCNxStackTimeout_Never)) {
                currentTimeInUs = _ccnxPerfClient_CurrentTimeInUs(clock);
                nextPacketSendTime = currentTimeInUs + delayInUs;

                ccnxPerfStats_RecordRequest(client->stats, name, currentTimeInUs);
            }

            outstanding++;
            ccnxName_Release(&name);
        } else {
            // We're done with pings, so let's wait to see if we have any stragglers
            currentTimeInUs = _ccnxPerfClient_CurrentTimeInUs(clock);
            nextPacketSendTime = currentTimeInUs + client->receiveTimeoutInUs;
        }

        // Now wait for the responses and record their times
        uint64_t receiveDelay = nextPacketSendTime - currentTimeInUs;
        CCNxMetaMessage *response = ccnxPortal_Receive(client->portal, &receiveDelay);
        while (response != NULL && (!checkOustanding || (checkOustanding && outstanding < client->numberOfOutstanding))) {
            uint64_t currentTimeInUs = _ccnxPerfClient_CurrentTimeInUs(clock);
            if (ccnxMetaMessage_IsContentObject(response)) {
                CCNxContentObject *contentObject = ccnxMetaMessage_GetContentObject(response);

                CCNxName *responseName = ccnxContentObject_GetName(contentObject);
                size_t delta = ccnxPerfStats_RecordResponse(client->stats, responseName, currentTimeInUs, response);

                // Only display output if we're in ping mode
                if (client->mode == CCNxPerfClientMode_PingPong) {
                    size_t contentSize = parcBuffer_Remaining(ccnxContentObject_GetPayload(contentObject));
                    char *nameString = ccnxName_ToString(responseName);
                    printf("%zu bytes from %s: time=%zu us\n", contentSize, nameString, delta);
                    parcMemory_Deallocate(&nameString);
                }
            }
            ccnxMetaMessage_Release(&response);

            if (pings < totalPings) {
                receiveDelay = nextPacketSendTime - currentTimeInUs;
            } else {
                receiveDelay = client->receiveTimeoutInUs;
            }

            response = ccnxPortal_Receive(client->portal, &receiveDelay);
            outstanding--;
        }
    }
}

/**
 * Display the usage message.
 */
static void
_displayUsage(char *progName)
{
    printf("%s -p [ -c count ] [ -s size ] [ -i interval ]\n", progName);
    printf("%s -f [ -c count ] [ -s size ]\n", progName);
    printf("%s -h\n", progName);
    printf("                CCNx Simple Performance Test\n");
    printf("                :  You must have ccnxPerfServer running\n");
    printf("\n");
    printf("Example:\n");
    printf("    ccnxPerf_Client -l ccnx:/some/prefix -c 100 -f");
    printf("\n");
    printf("Options  \n");
    printf("     -h (--help) Show this help message\n");
    printf("     -p (--ping) ping mode - \n");
    printf("     -f (--flood) flood mode - send as fast as possible\n");
    printf("     -c (--count) Number of count to run\n");
    printf("     -i (--interval) Interval in milliseconds between interests in ping mode\n");
    printf("     -s (--size) Size of the interests\n");
    printf("     -l (--locator) Set the locator for this server. The default is 'ccnx:/locator'. \n");
}

/**
 * Parse the command lines to initialize the state of the
 */
static bool
_ccnxPerfClient_ParseCommandline(CCNxPerfClient *client, int argc, char *argv[argc])
{
    static struct option longopts[] = {
        { "ping",        no_argument,       NULL, 'p' },
        { "flood",       no_argument,       NULL, 'f' },
        { "count",       required_argument, NULL, 'c' },
        { "size",        required_argument, NULL, 's' },
        { "interval",    required_argument, NULL, 'i' },
        { "locator",     required_argument, NULL, 'l' },
        { "outstanding", required_argument, NULL, 'o' },
        { "help",        no_argument,       NULL, 'h' },
        { NULL,          0,                 NULL, 0   }
    };

    int c;
    while ((c = getopt_long(argc, argv, "phfc:s:i:l:o:", longopts, NULL)) != -1) {
        switch (c) {
            case 'p':
                if (client->mode != CCNxPerfClientMode_None) {
                    return false;
                }
                client->mode = CCNxPerfClientMode_PingPong;
                break;
            case 'f':
                if (client->mode != CCNxPerfClientMode_None) {
                    return false;
                }
                client->mode = CCNxPerfClientMode_Flood;
                break;
            case 'c':
                sscanf(optarg, "%u", &(client->count));
                break;
            case 'i':
                sscanf(optarg, "%llu", &(client->intervalInMs));
                break;
            case 's':
                sscanf(optarg, "%u", &(client->payloadSize));
                break;
            case 'o':
                sscanf(optarg, "%zu", &(client->numberOfOutstanding));
                break;
            case 'l':
                client->prefix = ccnxName_CreateFromCString(optarg);
                break;
            case 'h':
                _displayUsage(argv[0]);
                return false;
            default:
                break;
        }
    }

    if (client->mode == CCNxPerfClientMode_None) {
        _displayUsage(argv[0]);
        return false;
    }

    return true;
};

static void
_ccnxPerfClient_DisplayStatistics(CCNxPerfClient *client)
{
    bool ableToCompute = ccnxPerfStats_Display(client->stats);
    if (!ableToCompute) {
        parcDisplayIndented_PrintLine(0, "No packets were received. Check to make sure the client and server are configured correctly and that the forwarder is running.\n");
    }
}

static void
_ccnxPerfClient_RunPerformanceTest(CCNxPerfClient *client)
{
    switch (client->mode) {
        case CCNxPerfClientMode_All:
            _ccnxPerfClient_RunPing(client, mediumNumberOfPings, 0);
            _ccnxPerfClient_DisplayStatistics(client);

            ccnxPerfStats_Release(&client->stats);
            client->stats = ccnxPerfStats_Create();

            _ccnxPerfClient_RunPing(client, smallNumberOfPings, ccnxPerf_DefaultReceiveTimeoutInUs);
            _ccnxPerfClient_DisplayStatistics(client);
            break;
        case CCNxPerfClientMode_Flood:
            _ccnxPerfClient_RunPing(client, client->count, 0);
            _ccnxPerfClient_DisplayStatistics(client);
            break;
        case CCNxPerfClientMode_PingPong:
            _ccnxPerfClient_RunPing(client, client->count, client->intervalInMs * 1000);
            _ccnxPerfClient_DisplayStatistics(client);
            break;
        case CCNxPerfClientMode_None:
        default:
            fprintf(stderr, "Error, unknown mode");
            break;
    }
}

int
main(int argc, char *argv[argc])
{
    parcSecurity_Init();

    CCNxPerfClient *client = ccnxPerfClient_Create();

    bool runPerf = _ccnxPerfClient_ParseCommandline(client, argc, argv);
    if (runPerf) {
        _ccnxPerfClient_RunPerformanceTest(client);
    }

    ccnxPerfClient_Release(&client);

    parcSecurity_Fini();

    return EXIT_SUCCESS;
}
