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

//#include <parc/algol/parc_Memory.h>


enum options_errors {
    OPTIONS_PARSE_ERROR = -1
};

enum perf_mode {
    PERF_MODE_NONE = 0,
    PERF_MODE_LATENCY,
    PERF_MODE_PINGPONG
};

typedef struct Perf_Options {
    enum perf_mode mode;
} perf_options;


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

int
consumer(void)
{
    parcSecurity_Init();
    
    CCNxPortalFactory *factory = setupConsumerFactory();
   
    CCNxPortal *portal = ccnxPortalFactory_CreatePortal(factory, ccnxPortalRTA_Message, &ccnxPortalAttributes_NonBlocking);

    assertNotNull(portal, "Expected a non-null CCNxPortal pointer.");

    char nameBuffer[100];

    sprintf(nameBuffer,"lci:/%s/ping/%08lu","localhost",(long)100);
    printf("Using name: %s\n",nameBuffer);

    CCNxName *name = ccnxName_CreateFromURI(nameBuffer);

    //CCNxNameSegment * name_sequence;
    //name_sequence = ccnxNameSegment_CreateTypeValue(CCNxNameLabelType_App(10),);
    //ccnxName_Append(name,);

    CCNxInterest *interest = ccnxInterest_CreateSimple(name);
    ccnxName_Release(&name);

    CCNxMetaMessage *message = ccnxMetaMessage_CreateFromInterest(interest);

#define MAX_PINGS 100
    long total_rtt = 0;
    long rtt;

    struct timeval tv_send, tv_receive;
    for(int pings = 0; pings < MAX_PINGS; pings++) {
        gettimeofday(&tv_send,NULL);
        if (ccnxPortal_Send(portal, message, CCNxTransportStackTimeout_Never)) {
            //usleep(10000);
            while (ccnxPortal_IsError(portal) == false) {
                CCNxMetaMessage *response = ccnxPortal_Receive(portal,CCNxTransportStackTimeout_MicroSeconds(100));
                gettimeofday(&tv_receive,NULL);
                if (response != NULL) {
                    if (ccnxMetaMessage_IsContentObject(response)) {
                        CCNxContentObject *contentObject = ccnxMetaMessage_GetContentObject(response);

                        //PARCBuffer *payload = ccnxContentObject_GetPayload(contentObject);

                        //char *string = parcBuffer_ToString(payload);
                        //printf("%s\n", string);
                        //parcMemory_Deallocate((void **) &string);

                        rtt = (tv_receive.tv_sec - tv_send.tv_sec) * 1000000 +
                                tv_receive.tv_usec - tv_send.tv_usec;

                        total_rtt = total_rtt + rtt;

                        char * nameOfInterest = ccnxName_ToString(ccnxContentObject_GetName(contentObject));

                        printf("%s %luus\n",nameOfInterest,rtt);

                        break;
                    }
                    ccnxMetaMessage_Release(&response);
                }
                else {
                    // Didn't receive response in time
                    printf("some/name 000\n");
                }
            }
        }
        if(!(pings % 5)) {
            usleep(100);
        }

    }

    printf("Avg: %lu  (%lu)\n",total_rtt/MAX_PINGS,(long)MAX_PINGS);

    ccnxPortal_Release(&portal);

    ccnxPortalFactory_Release(&factory);
    
    parcSecurity_Fini();
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

int
main(int argc, char *argv[argc])
{
    perf_options options;
    options_init(&options);
    options_parse_commandline(&options, argc, argv);
    return consumer();
}
