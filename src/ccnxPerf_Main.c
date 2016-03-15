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
#include <getopt.h>

#include <LongBow/runtime.h>

#include <ccnx/api/ccnx_Portal/ccnx_Portal.h>
#include <ccnx/api/ccnx_Portal/ccnx_PortalRTA.h>

#include <parc/security/parc_Security.h>
#include <parc/security/parc_IdentityFile.h>
#include <parc/security/parc_PublicKeySignerPkcs12Store.h>

#define NAME_BUFFER_SIZE 100
#define MAX_STAT_ENTRIES 10000
#define RECEIVE_TIMEOUT_US 1000000

enum options_errors {
    OPTIONS_PARSE_ERROR = -1
};

/* enum perf_mode
 * The type of performance test that we are doing
 */
enum perf_mode {
    PERF_MODE_NONE = 0,
    PERF_MODE_FLOOD,
    PERF_MODE_PINGPONG,
    PERF_MODE_ALL
};

enum stats_print {
    STATS_PRINT_FALSE = 0,
    STATS_PRINT_TRUE
};

typedef struct Perf_Stat_Entry {
    uint64_t send_us;
    uint64_t received_us;
    uint64_t rtt;
    size_t size;
    char nameSent[NAME_BUFFER_SIZE];
    CCNxMetaMessage * message;
} perf_stat_entry;

typedef struct Perf_Stats {
    uint64_t total_rtt;
    int total_pings_received;
    int total_pings_sent;
    perf_stat_entry pings[MAX_STAT_ENTRIES];
    int next_stat_entry;
} perf_stats;

typedef struct Perf_Options {
    uint64_t receive_timeout_us;
    CCNxPortalFactory *factory;
    CCNxPortal *portal;
    enum perf_mode mode;
    int interest_counter;
    int iterations;
    uint64_t interval_us;
    char * prefix;
    char * service_name;
    perf_stats stats;
    int nonce;
    int payload_size;
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
    snprintf(buffer,bufferSize,"lci:/%s/%s/%x/%u/%06lu",
             options->prefix,
             options->service_name,
             options->nonce,
             options->payload_size,
             (long)options->interest_counter);
    options->interest_counter = options->interest_counter + 1;
    return 0;
}

int stats_add_interest_sent(perf_stats * stats, char * name, uint64_t time_us, CCNxMetaMessage * message){
    stats->pings[stats->next_stat_entry].send_us = time_us;
    strncpy(stats->pings[stats->next_stat_entry].nameSent,name,NAME_BUFFER_SIZE);
    stats->total_pings_sent = stats->total_pings_sent + 1;
    //printf("%llu :: SEND %s\n",(time_us / 1000) % 100000, name);
    stats->next_stat_entry = stats->next_stat_entry + 1;
    return 0;
}

int stats_print_average(perf_stats * stats){
    printf("Sent = %u : Received = %u : AvgDelay %llu us\n",
        stats->total_pings_sent, stats->total_pings_received, stats->total_rtt / stats->total_pings_received);
    return 0;
}

int stats_reset(perf_stats * stats){
    memset(stats,0,sizeof(perf_stats));
    return 0;
}

int stats_add_content_received(perf_stats * stats, char * name, uint64_t time_us, CCNxMetaMessage * message, enum stats_print print){
    stats->total_pings_received = stats->total_pings_received + 1;
    //printf("%llu :: RECV %s\n",(time_us / 1000) % 100000, name);
    for(int i = 0; i<MAX_STAT_ENTRIES; i++) {
        if (strncmp(stats->pings[i].nameSent, name, NAME_BUFFER_SIZE) == 0) {
            stats->pings[i].received_us = time_us;
            stats->pings[i].rtt = stats->pings[i].received_us - stats->pings[i].send_us;
            stats->total_rtt = stats->total_rtt + stats->pings[i].rtt;
            CCNxContentObject * contentObject = ccnxMetaMessage_GetContentObject(message);
            PARCBuffer * payload = ccnxContentObject_GetPayload(contentObject);
            stats->pings[i].size = parcBuffer_Capacity(payload);

            if (print == STATS_PRINT_TRUE) {
                printf("%s\t%lluus\t%lub\n", name,
                        stats->pings[i].rtt,
                        stats->pings[i].size);
            }
            return 0;
        }
    }
    printf("ERROR: %s not found at %llu\n",name,time_us);
    return 0;
}

int perf_ping(perf_options * options, int total_pings, uint64_t delay_us) {
    CCNxName *name = NULL;
    CCNxInterest *interest = NULL;
    CCNxMetaMessage *message = NULL;
    CCNxMetaMessage *response = NULL;
    char nameBuffer[NAME_BUFFER_SIZE];

    uint64_t send_us;
    uint64_t receive_delay_us;
    uint64_t now_us;
    uint64_t next_packet_send_us;

    for(int pings = 0; pings <= total_pings ; pings++) {

        if (pings < total_pings) {
            // We want to send an interest ping
            create_name(options,nameBuffer,NAME_BUFFER_SIZE);
            name = ccnxName_CreateFromCString(nameBuffer);
            interest = ccnxInterest_CreateSimple(name);
            message = ccnxMetaMessage_CreateFromInterest(interest);
            ccnxName_Release(&name);
            if (ccnxPortal_Send(options->portal, message, CCNxStackTimeout_Never)) {
                now_us = get_now_us();
                send_us = now_us;
                next_packet_send_us = now_us + delay_us;
                stats_add_interest_sent(&options->stats, nameBuffer,send_us,message);

                if(ccnxPortal_IsError(options->portal)) {
                    int error = ccnxPortal_GetError(options->portal);
                    printf("ERROR SEND %s\n",strerror(error));
                }
            }
        }
        else {
            // We're done with pings, so let's wait to see if we have any stragglers
            now_us = get_now_us();
            send_us = now_us;
            next_packet_send_us = now_us + options->receive_timeout_us;
            //printf("Wait for %llu\n",next_packet_send_us - now_us);
        }


        receive_delay_us = next_packet_send_us - now_us;
        response = ccnxPortal_Receive(options->portal, &receive_delay_us);
        while(response != NULL) {
            if(ccnxPortal_IsError(options->portal)) {
                int error = ccnxPortal_GetError(options->portal);
                printf("ERROR RECV %s\n",strerror(error));
            }
            now_us = get_now_us();
            if (ccnxMetaMessage_IsContentObject(response)) {
                CCNxContentObject *contentObject = ccnxMetaMessage_GetContentObject(response);

                char *nameOfContent = ccnxName_ToString(ccnxContentObject_GetName(contentObject));

                stats_add_content_received(&options->stats, nameOfContent,now_us,response,STATS_PRINT_TRUE);
            }
            ccnxMetaMessage_Release(&response);
            if(pings < total_pings){
                receive_delay_us = next_packet_send_us - now_us;
            }
            else {
                receive_delay_us = options->receive_timeout_us;
            }
            //printf("Wait for %llu\n",next_packet_send_us - now_us);
            response = ccnxPortal_Receive(options->portal, &receive_delay_us);
        }
    }

    return 0;
}

int help(){
    printf("print the help here!\n");
    return 0;
}

int options_init(perf_options * options){
    memset(options,0,sizeof(perf_options));
    options->mode = PERF_MODE_NONE;
    return 0;
}

int options_parse_commandline(perf_options * options, int argc, char* argv[]){
    static struct option longopts[] = {
            { "ping",       no_argument,        NULL,'p' },
            { "flood",      no_argument,        NULL,'f' },
            { "iterations", required_argument,  NULL,'n'},
            { "size",       required_argument,  NULL,'s'},
            { "help",       no_argument,        NULL,'h'},
            { NULL,0,NULL,0}
    };

    int c;
    while((c = getopt_long(argc,argv,"phfn:s:",longopts,NULL)) != -1){
        switch(c){
            case 'p':
                if(options->mode != PERF_MODE_NONE){
                    // Error, multiple modes specified
                    return OPTIONS_PARSE_ERROR;
                }
                options->mode = PERF_MODE_PINGPONG;
                break;
            case 'f':
                if(options->mode != PERF_MODE_NONE){
                    // Error, multiple modes specified
                    return OPTIONS_PARSE_ERROR;
                }
                options->mode = PERF_MODE_FLOOD;
                break;
            case 'n':
                sscanf(optarg,"%u",&(options->iterations));
                break;
            case 's':
                sscanf(optarg,"%u",&(options->payload_size));
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
        options->mode = PERF_MODE_ALL;
    }
    return 0;
};

// Initialize our portal
int portal_initialize(perf_options * options){
    parcSecurity_Init();

    options->factory = setupConsumerFactory();
    options->portal = ccnxPortalFactory_CreatePortal(options->factory, ccnxPortalRTA_Message);

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
    srand(get_now_us());
    options->interest_counter = 100;
    options->prefix = "localhost";
    options->service_name = "ping";
    options->receive_timeout_us = RECEIVE_TIMEOUT_US;
    options->iterations = 10;
    options->interval_us = 1000000;
    options->nonce=rand();
    return 0;
}

int performance_test_run(perf_options * options){
    switch(options->mode){
        case PERF_MODE_ALL:
            perf_ping(options, 100, 0);
            stats_print_average(&options->stats);
            stats_reset(&(options->stats));
            perf_ping(options, 10, 1000000);
            stats_print_average(&options->stats);
            break;
        case PERF_MODE_FLOOD:
            perf_ping(options, options->iterations, 0);
            stats_print_average(&options->stats);
            break;
        case PERF_MODE_PINGPONG:
            perf_ping(options, options->iterations, options->interval_us);
            stats_print_average(&options->stats);
            break;
        case PERF_MODE_NONE:
        default:
            printf("Error, uknown mode");
            break;
    }
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
    data_initialize(&options);
    options_parse_commandline(&options, argc, argv);
    portal_initialize(&options);
    performance_test_run(&options);
    data_print_summary(&options);
    portal_finalize(&options);
    exit(EXIT_SUCCESS);
}
