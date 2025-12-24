
#ifndef HANJA_DICT_H
#define HANJA_DICT_H

#include <glib.h>
#include <stdbool.h>

// Hanja dictionary structure
typedef struct {
  GHashTable *system_dict; // System dictionary (read-only)
  GHashTable *user_dict;   // User dictionary (editable)
} HanjaDict;

// Initialize and load dictionaries
// system_path: /usr/share/ibus-dkst/hanja.txt
// user_path: ~/.config/ibus-dkst/hanja_user.txt
bool hanja_dict_init(HanjaDict *dict, const char *system_path,
                     const char *user_path);

// Lookup hanja candidates for a hangul string
// Returns GPtrArray of strings (caller must free with g_ptr_array_unref)
// Returns NULL if not found
GPtrArray *hanja_dict_lookup(HanjaDict *dict, const char *hangul);

// Free dictionary resources
void hanja_dict_free(HanjaDict *dict);

// Reload user dictionary (after editing)
bool hanja_dict_reload_user(HanjaDict *dict, const char *user_path);

#endif
