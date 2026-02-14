/*
 * PlumbrC - SSE 4.2 Trigger Pre-filter
 * Uses PCMPESTRI to quickly reject lines without trigger characters
 */

#ifndef PLUMBR_SSE42_FILTER_H
#define PLUMBR_SSE42_FILTER_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Check if SSE 4.2 is available at runtime
 */
bool sse42_available(void);

/*
 * Fast check if a line contains any trigger character using SSE 4.2
 * PCMPESTRI instruction. Processes 16 bytes per cycle.
 *
 * triggers: string of trigger chars (max 16)
 * trigger_count: number of trigger chars
 * line: the line to scan
 * len: length of the line
 *
 * Returns true if any trigger character found (line needs AC scan).
 * Returns false if no triggers found (line can be skipped).
 */
bool sse42_has_triggers(const char *triggers, size_t trigger_count,
                        const char *line, size_t len);

/*
 * Build trigger string from AC automaton root transitions.
 * Returns the number of unique trigger characters found.
 */
size_t sse42_build_triggers(const void *automaton, char *triggers,
                            size_t max_triggers);

#endif /* PLUMBR_SSE42_FILTER_H */
