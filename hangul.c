
#include "hangul.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Jamo Ranges
#define IS_CHO(c) (0x1100 <= (c) && (c) <= 0x1112)
#define IS_JUNG(c) (0x1161 <= (c) && (c) <= 0x1175)
#define IS_JONG(c) (0x11A8 <= (c) && (c) <= 0x11C2)

void dkst_hangul_init(DKSTHangul *h) {
  h->cho = 0;
  h->jung = 0;
  h->jong = 0;
  h->buffer = g_string_new("");
  h->completed = g_string_new("");
  h->moa_jjiki_enabled = true;
  h->backspace_mode = DKST_BACKSPACE_JASO;
}

void dkst_hangul_reset(DKSTHangul *h) {
  h->cho = 0;
  h->jung = 0;
  h->jong = 0;
  // buffer usually tracks raw input history, but logic here is state-based.
  // We clear it for consistency.
  g_string_truncate(h->buffer, 0);
  // Do NOT clear completed here automatically? logic says commit consumes it.
}

void dkst_hangul_free(DKSTHangul *h) {
  if (h->buffer)
    g_string_free(h->buffer, TRUE);
  if (h->completed)
    g_string_free(h->completed, TRUE);
}

// Map char to Jamo
static uint32_t map_key(char c) {
  switch (c) {
  case 'q':
    return 0x1107;
  case 'Q':
    return 0x1108; // ㅂ, ㅃ
  case 'w':
    return 0x110c;
  case 'W':
    return 0x110d; // ㅈ, ㅉ
  case 'e':
    return 0x1103;
  case 'E':
    return 0x1104; // ㄷ, ㄸ
  case 'r':
    return 0x1100;
  case 'R':
    return 0x1101; // ㄱ, ㄲ
  case 't':
    return 0x1109;
  case 'T':
    return 0x110a; // ㅅ, ㅆ
  case 'y':
    return 0x116d;
  case 'Y':
    return 0x116d; // ㅛ
  case 'u':
    return 0x1167;
  case 'U':
    return 0x1167; // ㅕ
  case 'i':
    return 0x1163;
  case 'I':
    return 0x1163; // ㅑ
  case 'o':
    return 0x1162;
  case 'O':
    return 0x1164; // ㅐ, ㅒ
  case 'p':
    return 0x1166;
  case 'P':
    return 0x1168; // ㅔ, ㅖ

  case 'a':
    return 0x1106;
  case 'A':
    return 0x1106; // ㅁ
  case 's':
    return 0x1102;
  case 'S':
    return 0x1102; // ㄴ
  case 'd':
    return 0x110b;
  case 'D':
    return 0x110b; // ㅇ
  case 'f':
    return 0x1105;
  case 'F':
    return 0x1105; // ㄹ
  case 'g':
    return 0x1112;
  case 'G':
    return 0x1112; // ㅎ
  case 'h':
    return 0x1169;
  case 'H':
    return 0x1169; // ㅗ
  case 'j':
    return 0x1165;
  case 'J':
    return 0x1165; // ㅓ
  case 'k':
    return 0x1161;
  case 'K':
    return 0x1161; // ㅏ
  case 'l':
    return 0x1175;
  case 'L':
    return 0x1175; // ㅣ

  case 'z':
    return 0x110f;
  case 'Z':
    return 0x110f; // ㅋ
  case 'x':
    return 0x1110;
  case 'X':
    return 0x1110; // ㅌ
  case 'c':
    return 0x110e;
  case 'C':
    return 0x110e; // ㅊ
  case 'v':
    return 0x1111;
  case 'V':
    return 0x1111; // ㅍ
  case 'b':
    return 0x1172;
  case 'B':
    return 0x1172; // ㅠ
  case 'n':
    return 0x116e;
  case 'N':
    return 0x116e; // ㅜ
  case 'm':
    return 0x1173;
  case 'M':
    return 0x1173; // ㅡ
  default:
    return 0;
  }
}

static uint32_t compatibility_jamo(uint32_t u) {
  if (0x1100 <= u && u <= 0x1112) {
    // Simple offset mapping for Chosung to Compatibility Jamo
    static const uint32_t map[] = {0x3131, 0x3132, 0x3134, 0x3137, 0x3138,
                                   0x3139, 0x3141, 0x3142, 0x3143, 0x3145,
                                   0x3146, 0x3147, 0x3148, 0x3149, 0x314A,
                                   0x314B, 0x314C, 0x314D, 0x314E};
    int idx = u - 0x1100;
    if (idx >= 0 && idx < 19)
      return map[idx];
  }
  if (0x1161 <= u && u <= 0x1175) {
    static const uint32_t map[] = {
        0x314F, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155,
        0x3156, 0x3157, 0x3158, 0x3159, 0x315A, 0x315B, 0x315C,
        0x315D, 0x315E, 0x315F, 0x3160, 0x3161, 0x3162, 0x3163};
    int idx = u - 0x1161;
    if (idx >= 0 && idx < 21)
      return map[idx];
  }
  return u;
}

static int cho_index(uint32_t c) {
  if (0x1100 <= c && c <= 0x1112)
    return c - 0x1100;
  return -1;
}
static int jung_index(uint32_t c) {
  if (0x1161 <= c && c <= 0x1175)
    return c - 0x1161;
  return -1;
}
static int jong_index(uint32_t c) {
  if (0x11A8 <= c && c <= 0x11C2)
    return c - 0x11A8 + 1;
  return 0; // 0 means no jongseong
}

static uint32_t cho_to_jong(uint32_t c) {
  // Mapping table from py
  switch (c) {
  case 0x1100:
    return 0x11A8;
  case 0x1101:
    return 0x11A9;
  case 0x1102:
    return 0x11AB;
  case 0x1103:
    return 0x11AE;
  case 0x1105:
    return 0x11AF;
  case 0x1106:
    return 0x11B7;
  case 0x1107:
    return 0x11B8;
  case 0x1109:
    return 0x11BA;
  case 0x110A:
    return 0x11BB;
  case 0x110B:
    return 0x11BC;
  case 0x110C:
    return 0x11BD;
  case 0x110E:
    return 0x11BE;
  case 0x110F:
    return 0x11BF;
  case 0x1110:
    return 0x11C0;
  case 0x1111:
    return 0x11C1;
  case 0x1112:
    return 0x11C2;
  default:
    return 0;
  }
}
static uint32_t jong_to_cho(uint32_t c) {
  switch (c) {
  case 0x11A8:
    return 0x1100;
  case 0x11A9:
    return 0x1101;
  case 0x11AB:
    return 0x1102;
  case 0x11AE:
    return 0x1103;
  case 0x11AF:
    return 0x1105;
  case 0x11B7:
    return 0x1106;
  case 0x11B8:
    return 0x1107;
  case 0x11BA:
    return 0x1109;
  case 0x11BB:
    return 0x110A;
  case 0x11BC:
    return 0x110B;
  case 0x11BD:
    return 0x110C;
  case 0x11BE:
    return 0x110E;
  case 0x11BF:
    return 0x110F;
  case 0x11C0:
    return 0x1110;
  case 0x11C1:
    return 0x1111;
  case 0x11C2:
    return 0x1112;
  default:
    return 0;
  }
}

static uint32_t combine_jung(uint32_t a, uint32_t b) {
  if (a == 0x1169 && b == 0x1161)
    return 0x116A; // ㅘ
  if (a == 0x1169 && b == 0x1162)
    return 0x116B; // ㅙ
  if (a == 0x1169 && b == 0x1175)
    return 0x116C; // ㅚ
  if (a == 0x116e && b == 0x1165)
    return 0x116F; // ㅝ
  if (a == 0x116e && b == 0x1166)
    return 0x1170; // ㅞ
  if (a == 0x116e && b == 0x1175)
    return 0x1171; // ㅟ
  if (a == 0x1173 && b == 0x1175)
    return 0x1174; // ㅢ
  return 0;
}

static void split_jung(uint32_t c, uint32_t *j1, uint32_t *j2) {
  *j1 = c;
  *j2 = 0;
  if (c == 0x116A) {
    *j1 = 0x1169;
    *j2 = 0x1161;
  } else if (c == 0x116B) {
    *j1 = 0x1169;
    *j2 = 0x1162;
  } else if (c == 0x116C) {
    *j1 = 0x1169;
    *j2 = 0x1175;
  } else if (c == 0x116F) {
    *j1 = 0x116e;
    *j2 = 0x1165;
  } else if (c == 0x1170) {
    *j1 = 0x116e;
    *j2 = 0x1166;
  } else if (c == 0x1171) {
    *j1 = 0x116e;
    *j2 = 0x1175;
  } else if (c == 0x1174) {
    *j1 = 0x1173;
    *j2 = 0x1175;
  }
}

static uint32_t combine_jong(uint32_t a, uint32_t b) {
  if (a == 0x11A8 && b == 0x11BA)
    return 0x11AA; // ㄳ
  if (a == 0x11AB && b == 0x11BD)
    return 0x11AC; // ㄵ
  if (a == 0x11AB && b == 0x11C2)
    return 0x11AD; // ㄶ
  if (a == 0x11AF && b == 0x11A8)
    return 0x11B0; // ㄺ
  if (a == 0x11AF && b == 0x11B7)
    return 0x11B1; // ㄻ
  if (a == 0x11AF && b == 0x11B8)
    return 0x11B2; // ㄼ
  if (a == 0x11AF && b == 0x11BA)
    return 0x11B3; // ㄽ
  if (a == 0x11AF && b == 0x11C0)
    return 0x11B4; // ㄾ
  if (a == 0x11AF && b == 0x11C1)
    return 0x11B5; // ㄿ
  if (a == 0x11AF && b == 0x11C2)
    return 0x11B6; // ㅀ
  if (a == 0x11B8 && b == 0x11BA)
    return 0x11B9; // ㅄ
  return 0;
}

static void split_jong(uint32_t c, uint32_t *j1, uint32_t *j2) {
  *j1 = c;
  *j2 = 0;
  if (c == 0x11AA) {
    *j1 = 0x11A8;
    *j2 = 0x11BA;
  } else if (c == 0x11AC) {
    *j1 = 0x11AB;
    *j2 = 0x11BD;
  } else if (c == 0x11AD) {
    *j1 = 0x11AB;
    *j2 = 0x11C2;
  } else if (c == 0x11B0) {
    *j1 = 0x11AF;
    *j2 = 0x11A8;
  } else if (c == 0x11B1) {
    *j1 = 0x11AF;
    *j2 = 0x11B7;
  } else if (c == 0x11B2) {
    *j1 = 0x11AF;
    *j2 = 0x11B8;
  } else if (c == 0x11B3) {
    *j1 = 0x11AF;
    *j2 = 0x11BA;
  } else if (c == 0x11B4) {
    *j1 = 0x11AF;
    *j2 = 0x11C0;
  } else if (c == 0x11B5) {
    *j1 = 0x11AF;
    *j2 = 0x11C1;
  } else if (c == 0x11B6) {
    *j1 = 0x11AF;
    *j2 = 0x11C2;
  } else if (c == 0x11B9) {
    *j1 = 0x11B8;
    *j2 = 0x11BA;
  }
}

uint32_t dkst_hangul_current_syllable(DKSTHangul *h) {
  if (h->cho == 0 && h->jung == 0 && h->jong == 0)
    return 0;

  // Independent Jamo
  if (h->cho && !h->jung && !h->jong)
    return compatibility_jamo(h->cho);
  if (!h->cho && h->jung && !h->jong)
    return compatibility_jamo(h->jung);

  int c = (h->cho) ? cho_index(h->cho) : -1;
  int j = (h->jung) ? jung_index(h->jung) : -1;
  int k = (h->jong) ? jong_index(h->jong) : 0;

  if (c != -1 && j != -1) {
    return 0xAC00 + (c * 21 * 28) + (j * 28) + k;
  }

  // Moa-jjik-gi partial
  if (j != -1 && c == -1)
    return compatibility_jamo(h->jung);

  return 0; // Should not happen with valid logic
}

bool dkst_hangul_backspace(DKSTHangul *h) {
  if (h->cho == 0 && h->jung == 0 && h->jong == 0)
    return false;

  // Character Mode: Wipe everything
  if (h->backspace_mode == DKST_BACKSPACE_CHAR) {
    h->cho = 0;
    h->jung = 0;
    h->jong = 0;
    return true;
  }

  // Jaso Mode: Detailed breakdown
  if (h->jong != 0) {
    uint32_t j1, j2;
    split_jong(h->jong, &j1, &j2);
    if (j2)
      h->jong = j1;
    else
      h->jong = 0;
    return true;
  }

  if (h->jung != 0) {
    uint32_t j1, j2;
    split_jung(h->jung, &j1, &j2);
    if (j2)
      h->jung = j1;
    else
      h->jung = 0;
    return true;
  }

  if (h->cho != 0) {
    h->cho = 0;
    return true;
  }
  return false;
}

// Append unicode char to buffer string
static void append_unichar(GString *s, uint32_t u) {
  if (u == 0)
    return;
  g_string_append_unichar(s, u);
}

bool dkst_hangul_process(DKSTHangul *h, char key) {
  uint32_t hangul = map_key(key);

  if (hangul == 0) {
    // Not a hangul key. Commit current and return false (not consumed)
    if (h->cho || h->jung || h->jong) {
      uint32_t syl = dkst_hangul_current_syllable(h);
      append_unichar(h->completed, syl);
      dkst_hangul_reset(h);
    }
    return false;
  }

  if (IS_CHO(hangul)) {
    if (h->jung == 0) {
      if (h->cho == 0) {
        h->cho = hangul;
      } else {
        append_unichar(h->completed, compatibility_jamo(h->cho));
        h->cho = hangul;
        // No need to reset others as they are 0
      }
    } else {
      // We have Cho and Jung
      if (h->jong == 0) {
        if (h->cho == 0) {
          // Jung only present.
          if (h->moa_jjiki_enabled) {
            h->cho = hangul;
            return true;
          } else {
            append_unichar(h->completed, dkst_hangul_current_syllable(h));
            dkst_hangul_reset(h);
            h->cho = hangul;
            return true;
          }
        }

        // Standard case: Cho+Jung. Incoming Cho might be Jong.
        uint32_t as_jong = cho_to_jong(hangul);
        if (as_jong) {
          h->jong = as_jong;
        } else {
          append_unichar(h->completed, dkst_hangul_current_syllable(h));
          dkst_hangul_reset(h);
          h->cho = hangul;
        }
      } else {
        // Cho+Jung+Jong. Incoming Cho might combine with Jong.
        uint32_t compound = combine_jong(h->jong, cho_to_jong(hangul));
        if (compound) {
          h->jong = compound;
        } else {
          append_unichar(h->completed, dkst_hangul_current_syllable(h));
          dkst_hangul_reset(h);
          h->cho = hangul;
        }
      }
    }
  } else if (IS_JUNG(hangul)) {
    if (h->jong) {
      uint32_t j1, j2;
      split_jong(h->jong, &j1, &j2);
      if (j2) {
        // Complex jong. Split it. current becomes Cho+Jung+J1. Next is J2(as
        // Cho)+NewJung
        h->jong = j1;
        uint32_t syl = dkst_hangul_current_syllable(h);
        append_unichar(h->completed, syl);

        uint32_t next_cho = jong_to_cho(j2);
        dkst_hangul_reset(h);
        h->cho = next_cho;
        h->jung = hangul;
      } else {
        // Simple jong. It moves to next char as Cho.
        uint32_t next_cho = jong_to_cho(h->jong);
        h->jong = 0;
        uint32_t syl = dkst_hangul_current_syllable(h);
        append_unichar(h->completed, syl);

        dkst_hangul_reset(h); // Clear
        h->cho = next_cho;
        h->jung = hangul;
      }
    } else if (h->jung) {
      uint32_t compound = combine_jung(h->jung, hangul);
      if (compound) {
        h->jung = compound;
      } else {
        append_unichar(h->completed, dkst_hangul_current_syllable(h));
        dkst_hangul_reset(h);
        h->jung = hangul; // Assuming independent jung valid or moa-jjiki start
      }
    } else {
      // Cho might be set or not
      h->jung = hangul;
    }
  }

  return true;
}

char *dkst_hangul_get_commit_string(DKSTHangul *h) {
  if (h->completed->len == 0)
    return NULL;
  char *ret = g_strdup(h->completed->str);
  g_string_truncate(h->completed, 0);
  return ret;
}

bool dkst_hangul_has_composed(DKSTHangul *h) {
  return (h->cho || h->jung || h->jong);
}
