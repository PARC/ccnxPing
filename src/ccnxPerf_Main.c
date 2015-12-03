/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 * Copyright 2014-2015 Palo Alto Research Center, Inc. (PARC), a Xerox company.  All Rights Reserved.
 * The content of this file, whole or in part, is subject to licensing terms.
 * If distributing this software, include this License Header Notice in each
 * file and provide the accompanying LICENSE file.
 */
/**
 * @author Glenn Scott, Computing Science Laboratory, PARC
 * @copyright 2014-2015 Palo Alto Research Center, Inc. (PARC), A Xerox Company. All Rights Reserved.
 */
#include <stdio.h>

#include <LongBow/runtime.h>

#include <ccnx/api/ccnx_Portal/ccnx_Portal.h>
#include <ccnx/api/ccnx_Portal/ccnx_PortalRTA.h>

#include <parc/security/parc_Security.h>
#include <parc/security/parc_IdentityFile.h>
#include <parc/security/parc_PublicKeySignerPkcs12Store.h>

#define NAME_BUFFER_SIZE 100

enum options_errors {
    OPTIONS_PARSE_ERROR = -1
};

/* enum perf_mode
 * The type of performance test that we are doing
 */
enum perf_mode {
    PERF_MODE_NONE = 0,
    PERF_MODE_LATENCY,
    PERF_MODE_PINGPONG,
    PERF_MODE_ALL
};

typedef struct Perf_Options {
    uint64_t receive_timeout_us;
    CCNxPortalFactory *factory;
    CCNxPortal *portal;
    enum perf_mode mode;
    int interest_counter;
    char * prefix;
    char * service_name;
} perf_options;


uint64_t get_now_us(){
    struct timeval tv_now;
    gettimeofday(&tv_now,NULL);
    return (uint64_t )((tv_now.tv_sec * 1000000) + tv_now.tv_usec);
}

PARCIdentity *
createAndGetIdentity(void)
{
    const char *keystoreName = "consumer_keystore";
    const char *keystorePassword = "keystore_password";
    unsigned int keyLength = 1024;
    unsigned int validityDays = 30;
    char *subjectName = "consumer";

    bool success = parcPublicKeySignerPkcs12Store_CreateFile(keystoreName, keystorePassword, subjectName, keyLength, validityDays);
    assertTrue(success,
               "parcPublicKeySignerPkcs12Store_CreateFile('%s', '%s', '%s', %d, %d) failed.",
               keystoreName, keystorePassword, subjectName, keyLength, validityDays);

    PARCIdentityFile *identityFile = parcIdentityFile_Create(keystoreName, keystorePassword);
    PARCIdentity *identity = parcIdentity_Create(identityFile, PARCIdentityFileAsPARCIdentity);
    parcIdentityFile_Release(&identityFile);

    return identity;
}

CCNxPortalFactory *
setupConsumerFactory(void)
{
    PARCIdentity *identity = createAndGetIdentity();

    CCNxPortalFactory *factory = ccnxPortalFactory_Create(identity);
    parcIdentity_Release(&identity);
    return factory;
}

int create_name(perf_options * options, char * buffer, int bufferSize){
    snprintf(buffer,bufferSize,"lci:/%s/%s/%08lu",options->prefix, options->service_name,(long)options->interest_counter);
    options->interest_counter = options->interest_counter + 1;
    return 0;
}

int perf_ping(perf_options * options, int total_pings, uint64_t delay_us) {
    CCNxName *name = NULL;
    CCNxInterest *interest = NULL;
    CCNxMetaMessage *message = NULL;
    CCNxMetaMessage *response = NULL;
    char nameBuffer[NAME_BUFFER_SIZE];


    uint64_t total_rtt = 0;
    uint64_t rtt;

    uint64_t send_us;
    uint64_t receive_delay_us;
    uint64_t now_us;
    uint64_t next_packet_send_us;

    for(int pings = 0; pings <= total_pings ; pings++) {


        if (pings < total_pings) {
            // We want to send an interest ping
            create_name(options,nameBuffer,NAME_BUFFER_SIZE);
            printf("Using name: %s\n",nameBuffer);
            name = ccnxName_CreateFromURI(nameBuffer);
            interest = ccnxInterest_CreateSimple(name);
            message = ccnxMetaMessage_CreateFromInterest(interest);
            ccnxName_Release(&name);
            if (ccnxPortal_Send(options->portal, message, CCNxStackTimeout_Never)) {
                now_us = get_now_us();
                send_us = now_us;
                next_packet_send_us = now_us + delay_us;
            }
        }
        else {
            // We're done with pings, so let's wait to see if we have any stragglers
            now_us = get_now_us();
            send_us = now_us;
            next_packet_send_us = now_us + options->receive_timeout_us;
            printf("Done with the pings, now we get to wait for stragglers\n");
        }

        while (ccnxPortal_IsError(options->portal) == false) {
            receive_delay_us = next_packet_send_us - now_us;
            printf("Receive with delay: receive_delay %llu  (now %llu, delay %llu)\n",receive_delay_us, now_us, delay_us);
            printf("receive_delay: %llu NOW: %llu, Next_packet %llu\n",receive_delay_us,now_us,next_packet_send_us);
            response = ccnxPortal_Receive(options->portal, &receive_delay_us);
            while(response != NULL) {
                // We received a response, process the response
                now_us = get_now_us();
                if (ccnxMetaMessage_IsContentObject(response)) {
                    CCNxContentObject *contentObject = ccnxMetaMessage_GetContentObject(response);

                    rtt = now_us - send_us;

                    total_rtt = total_rtt + rtt;

                    char *nameOfInterest = ccnxName_ToString(ccnxContentObject_GetName(contentObject));

                    printf("%s %luus\n", nameOfInterest, (long)rtt);
                }
                ccnxMetaMessage_Release(&response);
                // We have processed a response, we should wait more until the timeout occurs or we get another response.
                receive_delay_us = next_packet_send_us - now_us;
                printf("We didn't time out\n");
                printf("receive_delay: %llu NOW: %llu, Next_packet %llu\n",receive_delay_us,now_us,next_packet_send_us);
                response = ccnxPortal_Receive(options->portal, &receive_delay_us);
            }
            // We timed out
            printf(" timed out (delay: %llu,  receive_delay_us: %llu)\n",delay_us, receive_delay_us);
        }

    }

    printf("Avg: %lu  (%lu)\n",(long)total_rtt/total_pings,(long)total_pings);
    return 0;
}

int help(){
    printf("print the help here!");
    return 0;
}

int options_init(perf_options * options){
    memset(options,0,sizeof(perf_options));
    options->mode = PERF_MODE_NONE;
    return 0;
}

int options_parse_commandline(perf_options * options, int argc, char* argv[]){
    int c;
    while((c = getopt(argc,argv,"ph")) != -1){
        switch(c){
            case 'p':
                if(options->mode != PERF_MODE_NONE){
                    // Error, multiple modes specified
                    return OPTIONS_PARSE_ERROR;
                }
                options->mode = PERF_MODE_PINGPONG;
                break;
            case 'h':
                help();
                exit(EXIT_SUCCESS);
                //break;
            default:
                break;
        }
    }

    if(options->mode == PERF_MODE_NONE){
        // set the default mode
        options->mode = PERF_MODE_LATENCY;
    }
    return 0;
};

// Initialize our portal
int portal_initialize(perf_options * options){
    parcSecurity_Init();

    options->factory = setupConsumerFactory();
    options->portal = ccnxPortalFactory_CreatePortal(options->factory, ccnxPortalRTA_Message, &ccnxPortalAttributes_NonBlocking);

    assertNotNull(options->portal, "Expected a non-null CCNxPortal pointer.");
    return 0;
}

int portal_finalize(perf_options * options){
    ccnxPortal_Release(&(options->portal));

    ccnxPortalFactory_Release(&(options->factory));

    parcSecurity_Fini();
    return 0;
}

int data_initialize(perf_options * options){
    options->interest_counter = 100;
    options->prefix = "localhost";
    options->service_name = "ping";
    options->receive_timeout_us = 5000000;
    return 0;
}

int performance_test_run(perf_options * options){
    perf_ping(options, 10, 0);
    perf_ping(options, 10, 1000000);
    return 0;
}

int data_print_summary(perf_options * options){
    return 0;
}

int
main(int argc, char *argv[argc])
{
    perf_options options;
    options_init(&options);
    options_parse_commandline(&options, argc, argv);
    data_initialize(&options);
    portal_initialize(&options);
    performance_test_run(&options);
    data_print_summary(&options);
    portal_finalize(&options);
    exit(EXIT_SUCCESS);
}
