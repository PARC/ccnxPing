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

#include "ccnxPerf_Common.h"

#include <LongBow/runtime.h>

#include <parc/security/parc_Security.h>
#include <parc/security/parc_Pkcs12KeyStore.h>
#include <parc/security/parc_IdentityFile.h>

const size_t ccnxPerf_DefaultReceiveTimeoutInUs = 1000000;
const size_t ccnxPerf_DefaultPayloadSize = 4096;
const size_t mediumNumberOfPings = 100;
const size_t smallNumberOfPings = 10;

static PARCIdentity *
_ccnxPerfCommon_CreateAndGetIdentity(const char *keystoreName,
                                     const char *keystorePassword,
                                     const char *subjectName)
{
    parcSecurity_Init();

    unsigned int keyLength = 1024;
    unsigned int validityDays = 30;

    bool success = parcPkcs12KeyStore_CreateFile(keystoreName, keystorePassword, subjectName, keyLength, validityDays);
    assertTrue(success,
               "parcPkcs12KeyStore_CreateFile('%s', '%s', '%s', %d, %d) failed.",
               keystoreName, keystorePassword, subjectName, keyLength, validityDays);

    PARCIdentityFile *identityFile = parcIdentityFile_Create(keystoreName, keystorePassword);
    PARCIdentity *result = parcIdentity_Create(identityFile, PARCIdentityFileAsPARCIdentity);
    parcIdentityFile_Release(&identityFile);

    parcSecurity_Fini();

    return result;
}

CCNxPortalFactory *
ccnxPerfCommon_SetupPortalFactory(const char *keystoreName, const char *keystorePassword, const char *subjectName)
{
    PARCIdentity *identity = _ccnxPerfCommon_CreateAndGetIdentity(keystoreName, keystorePassword, subjectName);
    CCNxPortalFactory *result = ccnxPortalFactory_Create(identity);
    parcIdentity_Release(&identity);

    return result;
}
