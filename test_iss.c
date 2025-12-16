
#include "hangul.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>

// We need to access internal static functions for deep debugging,
// but since they are static in hangul.c, we can't link them directly.
// Instead, we will simulate the logic or rely on black-box testing with very
// granular prints. Or, we can include hangul.c source directly to access
// statics (dirty but effective for debug).

#include "hangul.c"

int main() {
  printf("--- Debugging 'Iss' (있) ---\n");

  // 1. Verify Map Key
  uint32_t t_map = map_key('T');
  printf("map_key('T') = 0x%X (Expected 0x110A)\n", t_map);

  // 2. Verify Cho to Jong
  uint32_t jong_mapped = cho_to_jong(t_map);
  printf("cho_to_jong(0x%X) = 0x%X (Expected 0x11BB)\n", t_map, jong_mapped);

  // 3. Trace Process
  DKSTHangul h;
  dkst_hangul_init(&h);

  // 'd'
  dkst_hangul_process(&h, 'd');
  printf("After 'd': Cho=%X Jung=%X Jong=%X\n", h.cho, h.jung, h.jong);

  // 'l'
  dkst_hangul_process(&h, 'l');
  printf("After 'l': Cho=%X Jung=%X Jong=%X\n", h.cho, h.jung, h.jong);

  // 'T'
  printf("Processing 'T'...\n");
  // Manual step simulation
  uint32_t hangul = map_key('T'); // 0x110A
  if (IS_CHO(hangul)) {
    printf("  IS_CHO is true.\n");
    printf("  h->jung is %X (should be non-zero)\n", h.jung);
    if (h.jung) {
      printf("  h->jong is %X (should be 0)\n", h.jong);
      if (h.jong == 0) {
        uint32_t as_jong = cho_to_jong(hangul);
        printf("  cho_to_jong(%X) returned %X\n", hangul, as_jong);
        if (as_jong) {
          printf("  SUCCESS PATH: h->jong will be set.\n");
        } else {
          printf("  FAILURE PATH: as_jong is 0. Commit triggers.\n");
        }
      }
    }
  }

  bool res = dkst_hangul_process(&h, 'T');
  printf("dkst_hangul_process('T') returned %d\n", res);
  printf("After 'T': Cho=%X Jung=%X Jong=%X\n", h.cho, h.jung, h.jong);

  uint32_t syl = dkst_hangul_current_syllable(&h);
  char buf[10] = {0};
  g_unichar_to_utf8(syl, buf);
  printf("Result Syllable: %s (Hex: %X)\n", buf, syl);

  return 0;
}
