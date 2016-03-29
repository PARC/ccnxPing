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
#ifndef ccnxPerf_Stats_h
#define ccnxPerf_Stats_h

/**
 * Structure to collect and display the performance statistics.
 */
struct perf_stats;
typedef struct perf_stats CCNxPerfStats;

/**
 * Create an empty `CCNxPerfStats` instance.
 *
 * The returned result must be freed via {@link ccnxPerfStats_Release}
 *
 * @return A newly allocated `CCNxPerfStats`.
 *
 * Example
 * @code
 * {
 *     CCNxPerfStats *stats = ccnxPerfStats_Create();
 * }
 * @endcode
 */
CCNxPerfStats *ccnxPerfStats_Create(void);

/**
 * Increase the number of references to a `CCNxPerfStats`.
 *
 * Note that new `CCNxPerfStats` is not created,
 * only that the given `CCNxPerfStats` reference count is incremented.
 * Discard the reference by invoking `ccnxPerfStats_Release`.
 *
 * @param [in] clock A pointer to a `CCNxPerfStats` instance.
 *
 * @return The input `CCNxPerfStats` pointer.
 *
 * Example:
 * @code
 * {
 *     CCNxPerfStats *stats = ccnxPerfStats_Create();
 *     CCNxPerfStats *copy = ccnxPerfStats_Acquire(stats);
 *     ccnxPerfStats_Release(&stats);
 *     ccnxPerfStats_Release(&copy);
 * }
 * @endcode
 */
CCNxPerfStats *ccnxPerfStats_Acquire(const CCNxPerfStats *stats);

/**
 * Release a previously acquired reference to the specified instance,
 * decrementing the reference count for the instance.
 *
 * The pointer to the instance is set to NULL as a side-effect of this function.
 *
 * If the invocation causes the last reference to the instance to be released,
 * the instance is deallocated and the instance's implementation will perform
 * additional cleanup and release other privately held references.
 *
 * @param [in,out] clockPtr A pointer to a pointer to the instance to release.
 *
 * Example:
 * @code
 * {
 *     CCNxPerfStats *stats = ccnxPerfStats_Create();
 *     CCNxPerfStats *copy = ccnxPerfStats_Acquire(stats);
 *     ccnxPerfStats_Release(&stats);
 *     ccnxPerfStats_Release(&copy);
 * }
 * @endcode
 */
void ccnxPerfStats_Release(CCNxPerfStats **statsPtr);

/**
 * Record the name and time for a request (e.g., interest).
 *
 * @param [in] stats The `CCNxPerfStats` instance.
 * @param [in] name The `CCNxName` name structure.
 * @param [in] timeInUs The send time (in microseconds).
 */
void ccnxPerfStats_RecordRequest(CCNxPerfStats *stats, CCNxName *name, uint64_t timeInUs);

/**
 * Record the name and time for a response (e.g., content object).
 *
 * @param [in] stats The `CCNxPerfStats` instance.
 * @param [in] name The `CCNxName` name structure.
 * @param [in] timeInUs The send time (in microseconds).
 * @param [in] message The response `CCNxMetaMessage`.
 *
 * @return The delta between the request and response (in microseconds).
 */
size_t ccnxPerfStats_RecordResponse(CCNxPerfStats *stats, CCNxName *name, uint64_t timeInUs, CCNxMetaMessage *message);

/**
 * Display the average statistics stored in this `CCNxPerfStats` instance.
 *
 * @param [in] stats The `CCNxPerfStats` instance from which to draw the average data.
 *
 * @retval true If the stats were displayed correctly
 * @retval false Otherwise
 */
bool ccnxPerfStats_Display(CCNxPerfStats *stats);
#endif // ccnxPerf_Stats_h
