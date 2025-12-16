
#ifndef HANGUL_H
#define HANGUL_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum { DKST_BACKSPACE_JASO, DKST_BACKSPACE_CHAR } DKSTBackspaceMode;

typedef struct {
  uint32_t cho;
  uint32_t jung;
  uint32_t jong;
  GString *buffer;    // Internal buffer for raw keys if needed, though we track
                      // state mostly
  GString *completed; // Queue of completed characters to commit
  bool moa_jjiki_enabled;
  DKSTBackspaceMode backspace_mode;
} DKSTHangul;

// Initialize
void dkst_hangul_init(DKSTHangul *h);

// Reset state
void dkst_hangul_reset(DKSTHangul *h);

// Cleanup (free memory)
void dkst_hangul_free(DKSTHangul *h);

// Process a key code (ascii char). Returns true if consumed, false otherwise.
bool dkst_hangul_process(DKSTHangul *h, char key);

// Get the current composed character (0 if none)
uint32_t dkst_hangul_current_syllable(DKSTHangul *h);

// Get pending committed string (caller must free)
char *dkst_hangul_get_commit_string(DKSTHangul *h);

// Backspace handling. Returns true if state changed.
bool dkst_hangul_backspace(DKSTHangul *h);

// Helper to check if string has any composed content
bool dkst_hangul_has_composed(DKSTHangul *h);

#endif
