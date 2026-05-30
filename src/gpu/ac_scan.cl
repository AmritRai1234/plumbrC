/*
 * PlumbrC - OpenCL AC DFA Scan Kernel
 *
 * One work-item per line. Each work-item walks the flat DFA table
 * for its assigned line and records pattern matches.
 *
 * Buffer layout:
 *   dfa[state * 256 + byte] = next_state (int16)
 *   meta_final[state]       = 1 if accepting state (uchar)
 *   meta_pat[state]         = pattern_id (uint)
 *   line_data[]             = packed line bytes
 *   line_offsets[line_id]   = {offset, length} into line_data
 *   results[line_id]        = {num_matches, match_indices[16]}
 */

/* Per-line offset descriptor */
typedef struct {
  uint offset;  /* Byte offset into line_data buffer */
  uint length;  /* Line length in bytes */
} LineDesc;

/* Per-line result (matches found) */
typedef struct {
  uint num_matches;
  uint match_indices[16]; /* Pattern IDs (max 16 matches per line) */
} LineResult;

__kernel void ac_dfa_scan(
    __global const short *dfa,         /* Flat DFA: dfa[state*256+byte] */
    __global const uchar *meta_final,  /* meta_final[state] = is_final */
    __global const uint  *meta_pat,    /* meta_pat[state] = pattern_id */
    __global const uchar *line_data,   /* Packed line bytes */
    __global const LineDesc *line_descs, /* Per-line {offset, length} */
    __global LineResult *results,       /* Per-line match results */
    const uint num_states              /* Total DFA states (for bounds) */
) {
  uint line_id = get_global_id(0);

  /* Load line descriptor */
  uint offset = line_descs[line_id].offset;
  uint length = line_descs[line_id].length;

  /* Initialize result */
  results[line_id].num_matches = 0;

  /* Walk DFA for this line */
  short state = 0;
  uint num_matches = 0;

  for (uint i = 0; i < length; i++) {
    uchar byte = line_data[offset + i];

    /* DFA transition: flat table lookup */
    short next = dfa[(uint)state * 256u + (uint)byte];

    /* Bounds check (defensive — should never trigger with valid DFA) */
    if (next < 0 || (uint)next >= num_states) {
      state = 0;
      continue;
    }
    state = next;

    /* Check for match */
    if (meta_final[(uint)state]) {
      if (num_matches < 16u) {
        results[line_id].match_indices[num_matches] = meta_pat[(uint)state];
        num_matches++;
      }

      /* Follow output chain: check if this state has chained matches.
       * For simplicity, we only record the direct match — the CPU will
       * run PCRE2 verification anyway so we just need to flag the line
       * as having AC hits. */
    }
  }

  results[line_id].num_matches = num_matches;
}
