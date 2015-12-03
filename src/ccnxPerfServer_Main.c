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
#include <LongBow/runtime.h>

#include <time.h>

#include <ccnx/api/ccnx_Portal/ccnx_Portal.h>
#include <ccnx/api/ccnx_Portal/ccnx_PortalRTA.h>

#include <parc/security/parc_Security.h>
#include <parc/security/parc_PublicKeySignerPkcs12Store.h>
#include <parc/security/parc_IdentityFile.h>

#include <ccnx/common/ccnx_Name.h>

PARCIdentity *
createAndGetIdentity(void)
{
    const char *keystoreName = "producer_keystore";
    const char *keystorePassword = "keystore_password";
    unsigned int keyLength = 1024;
    unsigned int validityDays = 30;
    char *subjectName = "producer";

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
setupPortalFactory(void)
{
    PARCIdentity *identity = createAndGetIdentity();

    CCNxPortalFactory *factory = ccnxPortalFactory_Create(identity);
    parcIdentity_Release(&identity);
    return factory;
}

PARCBuffer *
makePayload(void)
{
    time_t theTime = time(0);

    PARCBufferComposer *composer = parcBufferComposer_Create();
    parcBufferComposer_Format(composer, "Hello World. The time is %s", ctime(&theTime));
    PARCBuffer *payload = parcBufferComposer_ProduceBuffer(composer);
    parcBufferComposer_Release(&composer);

    return payload;
}

int
producer(void)
{
    parcSecurity_Init();

    CCNxPortalFactory *factory = setupPortalFactory();

    CCNxPortal *portal = ccnxPortalFactory_CreatePortal(factory, ccnxPortalRTA_Message, &ccnxPortalAttributes_Blocking);
    
    assertNotNull(portal, "Expected a non-null CCNxPortal pointer.");

    CCNxName *listenName = ccnxName_CreateFromURI("lci:/localhost/ping");
    CCNxName *goodbye = ccnxName_CreateFromURI("lci:/Hello/Goodbye%21");
    CCNxName *contentName = ccnxName_CreateFromURI("lci:/localhost/ping");

    if (ccnxPortal_Listen(portal, listenName, CCNxTransportStackTimeout_Never)) {
        while (true) {
            CCNxMetaMessage *request = ccnxPortal_Receive(portal, CCNxTransportStackTimeout_Never);

            if (request == NULL) {
                break;
            }

            CCNxInterest *interest = ccnxMetaMessage_GetInterest(request);

            if (interest != NULL) {
                CCNxName *interestName = ccnxInterest_GetName(interest);

                //if (ccnxName_Equals(interestName, contentName)) {

                    PARCBuffer *payload = makePayload();

                    CCNxContentObject *contentObject = ccnxContentObject_CreateWithDataPayload(interestName, payload);

                    CCNxMetaMessage *message = ccnxMetaMessage_CreateFromContentObject(contentObject);

                    if (ccnxPortal_Send(portal, message, CCNxTransportStackTimeout_Never) == false) {
                        fprintf(stderr, "ccnxPortal_Send failed: %d\n", ccnxPortal_GetError(portal));
                    }

                    ccnxMetaMessage_Release(&message);

                    parcBuffer_Release(&payload);
                //} else if (ccnxName_Equals(interestName, goodbye)) {
                //    break;
                //}
            }
            ccnxMetaMessage_Release(&request);
        }
    }

    ccnxName_Release(&listenName);
    ccnxName_Release(&goodbye);
    ccnxName_Release(&contentName);

    ccnxPortal_Release(&portal);

    ccnxPortalFactory_Release(&factory);

    parcSecurity_Fini();

    return 0;
}

int
main(int argc, const char *argv[argc])
{
    return producer();
}
