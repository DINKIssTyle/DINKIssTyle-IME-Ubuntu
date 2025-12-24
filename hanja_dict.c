
#include "hanja_dict.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Debug logging (disabled for production)
static void debug_log(const char *fmt, ...) {
  // Uncomment for debugging:
  // va_list args;
  // va_start(args, fmt);
  // vfprintf(stderr, fmt, args);
  // va_end(args);
}

// Free a GPtrArray of strings stored in hash table
static void free_candidates(gpointer data) {
  GPtrArray *arr = (GPtrArray *)data;
  if (arr) {
    g_ptr_array_unref(arr);
  }
}

// Parse a dictionary file and populate hash table
// Format: hangul:hanja1,hanja2,...
static bool load_dict_file(GHashTable *dict, const char *path) {
  if (!path)
    return false;

  FILE *fp = fopen(path, "r");
  if (!fp) {
    debug_log("Failed to open dictionary: %s\n", path);
    return false;
  }

  char line[1024];
  int count = 0;

  while (fgets(line, sizeof(line), fp)) {
    // Remove newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
      line[len - 1] = '\0';
    if (len > 1 && line[len - 2] == '\r')
      line[len - 2] = '\0';

    // Skip empty lines
    if (line[0] == '\0' || line[0] == '#')
      continue;

    // Find colon separator
    char *colon = strchr(line, ':');
    if (!colon)
      continue;

    // Extract key (hangul)
    *colon = '\0';
    char *key = g_strstrip(g_strdup(line));

    // Extract values (hanja list)
    char *values_str = colon + 1;

    // Split by comma
    gchar **values = g_strsplit(values_str, ",", -1);
    if (!values || !values[0]) {
      g_free(key);
      g_strfreev(values);
      continue;
    }

    // Create array for candidates
    GPtrArray *candidates = g_ptr_array_new_with_free_func(g_free);

    for (int i = 0; values[i] != NULL; i++) {
      gchar *trimmed = g_strstrip(g_strdup(values[i]));
      if (trimmed && *trimmed) {
        g_ptr_array_add(candidates, trimmed);
      } else {
        g_free(trimmed);
      }
    }
    g_strfreev(values);

    if (candidates->len > 0) {
      // If key exists, append to existing candidates
      GPtrArray *existing = (GPtrArray *)g_hash_table_lookup(dict, key);
      if (existing) {
        for (guint i = 0; i < candidates->len; i++) {
          g_ptr_array_add(existing,
                          g_strdup((char *)g_ptr_array_index(candidates, i)));
        }
        g_ptr_array_unref(candidates);
        g_free(key);
      } else {
        g_hash_table_insert(dict, key, candidates);
        count++;
      }
    } else {
      g_ptr_array_unref(candidates);
      g_free(key);
    }
  }

  fclose(fp);
  debug_log("Loaded %d entries from %s\n", count, path);
  return true;
}

bool hanja_dict_init(HanjaDict *dict, const char *system_path,
                     const char *user_path) {
  if (!dict)
    return false;

  // Create hash tables
  dict->system_dict =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_candidates);
  dict->user_dict =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_candidates);

  // Load system dictionary
  if (system_path) {
    load_dict_file(dict->system_dict, system_path);
  }

  // Load user dictionary
  if (user_path) {
    load_dict_file(dict->user_dict, user_path);
  }

  return true;
}

GPtrArray *hanja_dict_lookup(HanjaDict *dict, const char *hangul) {
  if (!dict || !hangul || !*hangul)
    return NULL;

  GPtrArray *result = g_ptr_array_new_with_free_func(g_free);

  // First check user dictionary (higher priority)
  GPtrArray *user_candidates =
      (GPtrArray *)g_hash_table_lookup(dict->user_dict, hangul);
  if (user_candidates) {
    for (guint i = 0; i < user_candidates->len; i++) {
      g_ptr_array_add(result,
                      g_strdup((char *)g_ptr_array_index(user_candidates, i)));
    }
  }

  // Then check system dictionary
  GPtrArray *sys_candidates =
      (GPtrArray *)g_hash_table_lookup(dict->system_dict, hangul);
  if (sys_candidates) {
    for (guint i = 0; i < sys_candidates->len; i++) {
      g_ptr_array_add(result,
                      g_strdup((char *)g_ptr_array_index(sys_candidates, i)));
    }
  }

  // Add original hangul as last option
  g_ptr_array_add(result, g_strdup(hangul));

  if (result->len == 1) {
    // Only has the original hangul, no real candidates found
    // Still return it so user sees feedback
  }

  return result;
}

void hanja_dict_free(HanjaDict *dict) {
  if (!dict)
    return;

  if (dict->system_dict) {
    g_hash_table_destroy(dict->system_dict);
    dict->system_dict = NULL;
  }

  if (dict->user_dict) {
    g_hash_table_destroy(dict->user_dict);
    dict->user_dict = NULL;
  }
}

bool hanja_dict_reload_user(HanjaDict *dict, const char *user_path) {
  if (!dict)
    return false;

  // Clear and reload user dictionary
  if (dict->user_dict) {
    g_hash_table_remove_all(dict->user_dict);
  } else {
    dict->user_dict =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_candidates);
  }

  if (user_path) {
    return load_dict_file(dict->user_dict, user_path);
  }

  return true;
}
