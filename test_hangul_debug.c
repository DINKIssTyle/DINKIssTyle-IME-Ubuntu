
#include "hangul.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>

// Mock main to test logic
int main() {
  DKSTHangul h;
  dkst_hangul_init(&h);

  printf("--- Test 1: 입니.다 ---\n");
  // "입니.다" input: d l q s l e k .
  // d(ㅇ) l(ㅣ) q(ㅂ) -> 입
  // s(ㄴ) l(ㅣ) -> 니
  // e(ㄷ) k(ㅏ) -> 다
  // . -> .
  const char *inputs1 = "dlqslek.";
  for (int i = 0; inputs1[i]; i++) {
    char key = inputs1[i];
    printf("Input '%c': ", key);
    bool consumed = dkst_hangul_process(&h, key);
    printf("Consumed=%d, ", consumed);

    uint32_t current = dkst_hangul_current_syllable(&h);
    if (current) {
      char buf[7] = {0};
      int len = g_unichar_to_utf8(current, buf);
      printf("Current='%s' ", buf);
    } else {
      printf("Current=(none) ");
    }

    char *committed = dkst_hangul_get_commit_string(&h);
    if (committed) {
      printf("COMMITTED='%s'", committed);
      g_free(committed);
    }
    printf("\n");
    if (!consumed && key == '.') {
      printf(" (Period logic triggered)\n");
    }
  }
  // Flush remaining
  dkst_hangul_reset(&h);
  printf("\n");

  printf("--- Test 2: 이ㅆ고 ---\n");
  // "있고" input: d l T r h
  // d(ㅇ) l(ㅣ) -> 이
  // T(ㅆ) -> 있
  // r(ㄱ) -> 있 (commit) + ㄱ
  // h(ㅗ) -> 고
  dkst_hangul_reset(&h);
  const char *inputs2 = "dlTrh";
  for (int i = 0; inputs2[i]; i++) {
    char key = inputs2[i];
    printf("Input '%c': ", key);
    bool consumed = dkst_hangul_process(&h, key);

    uint32_t current = dkst_hangul_current_syllable(&h);
    if (current) {
      char buf[7] = {0};
      int len = g_unichar_to_utf8(current, buf);
      printf("Current='%s' ", buf);
    }

    char *committed = dkst_hangul_get_commit_string(&h);
    if (committed) {
      printf(" COMMITTED='%s'", committed);
      g_free(committed);
    }
    printf("\n");
  }

  printf("--- Test 3: Shift+Space Handling (Engine logic, not hangul.c "
         "directly but simulation) ---\n");
  // This is handled in engine.c, checking hangul.c's behavior on Space
  // If we type Shift+Space, engine handles toggle.
  // If we type Space in Hangul mode:
  dkst_hangul_reset(&h);
  printf("Input 'Space': ");
  bool consumed = dkst_hangul_process(&h, ' ');
  printf("Consumed=%d\n", consumed);

  dkst_hangul_free(&h);
  return 0;
}
