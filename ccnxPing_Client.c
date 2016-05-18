/*
 * Copyright (c) 2016, Xerox Corporation (Xerox) and Palo Alto Research Center, Inc (PARC)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL XEROX OR PARC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ################################################################################
 * #
 * # PATENT NOTICE
 * #
 * # This software is distributed under the BSD 2-clause License (see LICENSE
 * # file).  This BSD License does not make any patent claims and as such, does
 * # not act as a patent grant.  The purpose of this section is for each contributor
 * # to define their intentions with respect to intellectual property.
 * #
 * # Each contributor to this source code is encouraged to state their patent
 * # claims and licensing mechanisms for any contributions made. At the end of
 * # this section contributors may each make their own statements.  Contributor's
 * # claims and grants only apply to the pieces (source code, programs, text,
 * # media, etc) that they have contributed directly to this software.
 * #
 * # There is no guarantee that this section is complete, up to date or accurate. It
 * # is up to the contributors to maintain their portion of this section and up to
 * # the user of the software to verify any claims herein.
 * #
 * # Do not remove this header notification.  The contents of this section must be
 * # present in all distributions of the software.  You may only modify your own
 * # intellectual property statements.  Please provide contact information.
 *
 * - Palo Alto Research Center, Inc
 * This software distribution does not grant any rights to patents owned by Palo
 * Alto Research Center, Inc (PARC). Rights to these patents are available via
 * various mechanisms. As of January 2016 PARC has committed to FRAND licensing any
 * intellectual property used by its contributions to this software. You may
 * contact PARC at cipo@parc.com for more information or visit http://www.ccnx.org
 */
/**
 * @author Nacho Solis, Christopher A. Wood, Palo Alto Research Center (Xerox PARC)
 * @copyright (c) 2016, Xerox Corporation (Xerox) and Palo Alto Research Center, Inc (PARC).  All rights reserved.
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

#include "ccnxPing_Stats.h"
#include "ccnxPing_Common.h"

typedef enum {
    CCNxPingClientMode_None = 0,
    CCNxPingClientMode_Flood,
    CCNxPingClientMode_PingPong,
    CCNxPingClientMode_All
} CCNxPingClientMode;

typedef struct ccnx_Ping_client {
    CCNxPortal *portal;
    CCNxPingStats *stats;
    CCNxPingClientMode mode;

    CCNxName *prefix;

    size_t numberOfOutstanding;
    uint64_t receiveTimeoutInUs;
    int interestCounter;
    int count;
    uint64_t intervalInMs;
    int payloadSize;
    int nonce;
} CCNxPingClient;

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

    return ccnxPingCommon_SetupPortalFactory(keystoreName, keystorePassword, subjectName);
}

/**
 * Release the references held by the `CCNxPingClient`.
 */
static bool
_ccnxPingClient_Destructor(CCNxPingClient **clientPtr)
{
    CCNxPingClient *client = *clientPtr;
    ccnxPortal_Release(&(client->portal));
    ccnxName_Release(&(client->prefix));
    return true;
}

parcObject_Override(CCNxPingClient, PARCObject,
                    .destructor = (PARCObjectDestructor *) _ccnxPingClient_Destructor);

parcObject_ImplementAcquire(ccnxPingClient, CCNxPingClient);
parcObject_ImplementRelease(ccnxPingClient, CCNxPingClient);

/**
 * Create a new empty `CCNxPingClient` instance.
 */
static CCNxPingClient *
ccnxPingClient_Create(void)
{
    CCNxPingClient *client = parcObject_CreateInstance(CCNxPingClient);

    CCNxPortalFactory *factory = _setupClientPortalFactory();
    client->portal = ccnxPortalFactory_CreatePortal(factory, ccnxPortalRTA_Message);
    ccnxPortalFactory_Release(&factory);

    client->stats = ccnxPingStats_Create();
    client->interestCounter = 100;
    client->prefix = ccnxName_CreateFromCString(ccnxPing_DefaultPrefix);
    client->receiveTimeoutInUs = ccnxPing_DefaultReceiveTimeoutInUs;
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
_ccnxPingClient_CreateNextName(CCNxPingClient *client)
{
    client->interestCounter++;
    char *suffixBuffer = NULL;
    asprintf(&suffixBuffer, "%x", client->nonce);
    CCNxName *name1 = ccnxName_ComposeNAME(ccnxName_Copy(client->prefix), suffixBuffer);
    parcMemory_Deallocate(&suffixBuffer);

    suffixBuffer = NULL;
    asprintf(&suffixBuffer, "%u", client->payloadSize);
    CCNxName *name2 = ccnxName_ComposeNAME(name1, suffixBuffer);
    ccnxName_Release(&name1);

    suffixBuffer = NULL;
    asprintf(&suffixBuffer, "%06lu", (long) client->interestCounter);
    CCNxName *name3 = ccnxName_ComposeNAME(name2, suffixBuffer);
    ccnxName_Release(&name2);

    return name3;
}

/**
 * Convert a timeval struct to a single microsecond count.
 */
static uint64_t
_ccnxPingClient_CurrentTimeInUs(PARCClock *clock)
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
_ccnxPingClient_RunPing(CCNxPingClient *client, size_t totalPings, uint64_t delayInUs)
{
    PARCClock *clock = parcClock_Wallclock();

    size_t outstanding = 0;
    bool checkOustanding = client->numberOfOutstanding > 0;

    for (int pings = 0; pings <= totalPings; pings++) {
        uint64_t nextPacketSendTime = 0;
        uint64_t currentTimeInUs = 0;

        // Continue to send ping messages until we've reached the capacity
        if (pings < totalPings && (!checkOustanding || (checkOustanding && outstanding < client->numberOfOutstanding))) {
            CCNxName *name = _ccnxPingClient_CreateNextName(client);
            CCNxInterest *interest = ccnxInterest_CreateSimple(name);
            CCNxMetaMessage *message = ccnxMetaMessage_CreateFromInterest(interest);

            if (ccnxPortal_Send(client->portal, message, CCNxStackTimeout_Never)) {
                currentTimeInUs = _ccnxPingClient_CurrentTimeInUs(clock);
                nextPacketSendTime = currentTimeInUs + delayInUs;

                ccnxPingStats_RecordRequest(client->stats, name, currentTimeInUs);
            }

            outstanding++;
            ccnxName_Release(&name);
        } else {
            // We're done with pings, so let's wait to see if we have any stragglers
            currentTimeInUs = _ccnxPingClient_CurrentTimeInUs(clock);
            nextPacketSendTime = currentTimeInUs + client->receiveTimeoutInUs;
        }

        // Now wait for the responses and record their times
        uint64_t receiveDelay = nextPacketSendTime - currentTimeInUs;
        CCNxMetaMessage *response = ccnxPortal_Receive(client->portal, &receiveDelay);
        while (response != NULL && (!checkOustanding || (checkOustanding && outstanding < client->numberOfOutstanding))) {
            uint64_t currentTimeInUs = _ccnxPingClient_CurrentTimeInUs(clock);
            if (ccnxMetaMessage_IsContentObject(response)) {
                CCNxContentObject *contentObject = ccnxMetaMessage_GetContentObject(response);

                CCNxName *responseName = ccnxContentObject_GetName(contentObject);
                size_t delta = ccnxPingStats_RecordResponse(client->stats, responseName, currentTimeInUs, response);

                // Only display output if we're in ping mode
                if (client->mode == CCNxPingClientMode_PingPong) {
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
    printf("                CCNx Simple Ping Performance Test\n");
    printf("                :  You must have ccnxPingServer running\n");
    printf("\n");
    printf("Example:\n");
    printf("    ccnxPing_Client -l ccnx:/some/prefix -c 100 -f");
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
_ccnxPingClient_ParseCommandline(CCNxPingClient *client, int argc, char *argv[argc])
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

    client->payloadSize = ccnxPing_DefaultPayloadSize;

    int c;
    while ((c = getopt_long(argc, argv, "phfc:s:i:l:o:", longopts, NULL)) != -1) {
        switch (c) {
            case 'p':
                if (client->mode != CCNxPingClientMode_None) {
                    return false;
                }
                client->mode = CCNxPingClientMode_PingPong;
                break;
            case 'f':
                if (client->mode != CCNxPingClientMode_None) {
                    return false;
                }
                client->mode = CCNxPingClientMode_Flood;
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

    if (client->mode == CCNxPingClientMode_None) {
        _displayUsage(argv[0]);
        return false;
    }

    return true;
};

static void
_ccnxPingClient_DisplayStatistics(CCNxPingClient *client)
{
    bool ableToCompute = ccnxPingStats_Display(client->stats);
    if (!ableToCompute) {
        parcDisplayIndented_PrintLine(0, "No packets were received. Check to make sure the client and server are configured correctly and that the forwarder is running.\n");
    }
}

static void
_ccnxPingClient_RunPingormanceTest(CCNxPingClient *client)
{
    switch (client->mode) {
        case CCNxPingClientMode_All:
            _ccnxPingClient_RunPing(client, mediumNumberOfPings, 0);
            _ccnxPingClient_DisplayStatistics(client);

            ccnxPingStats_Release(&client->stats);
            client->stats = ccnxPingStats_Create();

            _ccnxPingClient_RunPing(client, smallNumberOfPings, ccnxPing_DefaultReceiveTimeoutInUs);
            _ccnxPingClient_DisplayStatistics(client);
            break;
        case CCNxPingClientMode_Flood:
            _ccnxPingClient_RunPing(client, client->count, 0);
            _ccnxPingClient_DisplayStatistics(client);
            break;
        case CCNxPingClientMode_PingPong:
            _ccnxPingClient_RunPing(client, client->count, client->intervalInMs * 1000);
            _ccnxPingClient_DisplayStatistics(client);
            break;
        case CCNxPingClientMode_None:
        default:
            fprintf(stderr, "Error, unknown mode");
            break;
    }
}

int
main(int argc, char *argv[argc])
{
    parcSecurity_Init();

    CCNxPingClient *client = ccnxPingClient_Create();

    bool runPing = _ccnxPingClient_ParseCommandline(client, argc, argv);
    if (runPing) {
        _ccnxPingClient_RunPingormanceTest(client);
    }

    ccnxPingClient_Release(&client);

    parcSecurity_Fini();

    return EXIT_SUCCESS;
}
