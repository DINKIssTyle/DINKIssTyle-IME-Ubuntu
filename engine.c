
#include "hangul.h"
#include "hanja_dict.h"
#include <ibus.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void debug_log(const char *fmt, ...) {
  // Debug logging disabled for production
  (void)fmt;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(IBusEngine, g_object_unref)

// --- GObject Boilerplate ---
#define DKST_TYPE_ENGINE (dkst_engine_get_type())
G_DECLARE_FINAL_TYPE(DkstEngine, dkst_engine, DKST, ENGINE, IBusEngine)

typedef struct {
  guint keyval;
  guint modifiers;
} ToggleKey;

struct _DkstEngine {
  IBusEngine parent;

  DKSTHangul hangul;
  IBusLookupTable *table;
  gboolean is_hangul_mode;

  // Settings
  GHashTable *shift_mappings;
  gboolean enable_custom_shift;
  gboolean enable_moa_jjiki;

  // Toggle Keys
  GList *toggle_keys;

  // Indicator
  guint indicator_timeout_id;
  gboolean showing_indicator;
  gboolean enable_indicator;

  // Hanja feature
  gboolean hanja_mode;         // True when showing hanja candidates
  GPtrArray *hanja_candidates; // Current candidate list
  gchar *hanja_source;         // Original hangul being converted
  GList *hanja_keys;           // Configurable hanja trigger keys
  gchar *word_buffer;          // Buffer for multi-char word conversion
};

// Static hanja dictionary (shared across all engine instances)
static HanjaDict g_hanja_dict = {NULL, NULL};
static gboolean g_hanja_dict_loaded = FALSE;

G_DEFINE_TYPE(DkstEngine, dkst_engine, IBUS_TYPE_ENGINE)

static void dkst_engine_init(DkstEngine *engine) {
  dkst_hangul_init(&engine->hangul);

  // Create lookup table and sink the floating reference
  engine->table = ibus_lookup_table_new(10, 0, TRUE, TRUE);
  g_object_ref_sink(engine->table);

  engine->is_hangul_mode = TRUE;

  engine->shift_mappings =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  engine->enable_custom_shift = FALSE;
  engine->enable_moa_jjiki = TRUE;

  engine->toggle_keys = NULL;

  engine->indicator_timeout_id = 0;
  engine->showing_indicator = FALSE;
  engine->enable_indicator = TRUE;

  // Hanja feature initialization
  engine->hanja_mode = FALSE;
  engine->hanja_candidates = NULL;
  engine->hanja_source = NULL;

  // Load hanja dictionary (once, shared)
  if (!g_hanja_dict_loaded) {
    gchar *user_dict_path = g_build_filename(
        g_get_user_config_dir(), "ibus-dkst", "hanja_user.txt", NULL);
    hanja_dict_init(&g_hanja_dict, "/usr/share/ibus-dkst/hanja.txt",
                    user_dict_path);
    g_free(user_dict_path);
    g_hanja_dict_loaded = TRUE;
  }
}

static void free_toggle_key(gpointer data) { g_free(data); }

static void dkst_engine_finalize(GObject *object) {
  DkstEngine *engine = (DkstEngine *)object;

  if (engine->indicator_timeout_id > 0) {
    g_source_remove(engine->indicator_timeout_id);
    engine->indicator_timeout_id = 0;
  }

  dkst_hangul_free(&engine->hangul);

  if (engine->shift_mappings) {
    g_hash_table_destroy(engine->shift_mappings);
  }

  if (engine->toggle_keys) {
    g_list_free_full(engine->toggle_keys, free_toggle_key);
    engine->toggle_keys = NULL;
  }

  // Hanja cleanup
  if (engine->hanja_candidates) {
    g_ptr_array_unref(engine->hanja_candidates);
    engine->hanja_candidates = NULL;
  }
  if (engine->hanja_source) {
    g_free(engine->hanja_source);
    engine->hanja_source = NULL;
  }

  G_OBJECT_CLASS(dkst_engine_parent_class)->finalize(object);
}

// Helper to parse key string like "Shift+space"
static void add_toggle_key(DkstEngine *engine, const gchar *keystr) {
  guint keyval = 0;
  guint modifiers = 0;

  // Split by '+'
  gchar **parts = g_strsplit(keystr, "+", -1);
  guint len = g_strv_length(parts);

  if (len > 0) {
    // Last part is key name
    keyval = ibus_keyval_from_name(parts[len - 1]);

    // Previous parts are modifiers
    for (guint i = 0; i < len - 1; i++) {
      if (g_ascii_strcasecmp(parts[i], "Shift") == 0)
        modifiers |= IBUS_SHIFT_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Control") == 0)
        modifiers |= IBUS_CONTROL_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Alt") == 0)
        modifiers |= IBUS_MOD1_MASK; // Alt is usually Mod1
      else if (g_ascii_strcasecmp(parts[i], "Super") == 0)
        modifiers |= IBUS_SUPER_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Meta") == 0)
        modifiers |= IBUS_META_MASK;
    }
  }
  g_strfreev(parts);

  if (keyval != 0) {
    ToggleKey *tk = g_new(ToggleKey, 1);
    tk->keyval = keyval;
    tk->modifiers = modifiers;
    engine->toggle_keys = g_list_append(engine->toggle_keys, tk);
    debug_log("Added Toggle Key: Val=%x Mods=%x (from %s)\n", keyval, modifiers,
              keystr);
  }
}

// Helper to parse hanja key string like "Alt+Return"
static void add_hanja_key(DkstEngine *engine, const gchar *keystr) {
  guint keyval = 0;
  guint modifiers = 0;

  // Split by '+'
  gchar **parts = g_strsplit(keystr, "+", -1);
  guint len = g_strv_length(parts);

  if (len > 0) {
    // Last part is key name
    keyval = ibus_keyval_from_name(parts[len - 1]);

    // Previous parts are modifiers
    for (guint i = 0; i < len - 1; i++) {
      if (g_ascii_strcasecmp(parts[i], "Shift") == 0)
        modifiers |= IBUS_SHIFT_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Control") == 0)
        modifiers |= IBUS_CONTROL_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Alt") == 0)
        modifiers |= IBUS_MOD1_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Super") == 0)
        modifiers |= IBUS_SUPER_MASK;
      else if (g_ascii_strcasecmp(parts[i], "Meta") == 0)
        modifiers |= IBUS_META_MASK;
    }
  }
  g_strfreev(parts);

  if (keyval != 0) {
    ToggleKey *hk = g_new(ToggleKey, 1);
    hk->keyval = keyval;
    hk->modifiers = modifiers;
    engine->hanja_keys = g_list_append(engine->hanja_keys, hk);
    debug_log("Added Hanja Key: Val=%x Mods=%x (from %s)\n", keyval, modifiers,
              keystr);
  }
}

static void load_config(DkstEngine *engine) {
  gchar *config_path = g_build_filename(g_get_user_config_dir(), "ibus-dkst",
                                        "config.ini", NULL);
  GKeyFile *key_file = g_key_file_new();

  // Clear existing toggle keys before loading
  if (engine->toggle_keys) {
    g_list_free_full(engine->toggle_keys, free_toggle_key);
    engine->toggle_keys = NULL;
  }

  if (g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, NULL)) {
    // Moa-jjik-gi
    if (g_key_file_has_key(key_file, "Settings", "EnableMoaJjiki", NULL)) {
      engine->enable_moa_jjiki =
          g_key_file_get_boolean(key_file, "Settings", "EnableMoaJjiki", NULL);
      engine->hangul.moa_jjiki_enabled = engine->enable_moa_jjiki;
    }

    // Backspace Mode
    if (g_key_file_has_key(key_file, "Settings", "BackspaceMode", NULL)) {
      gchar *mode_str =
          g_key_file_get_string(key_file, "Settings", "BackspaceMode", NULL);
      if (g_strcmp0(mode_str, "CHAR") == 0) {
        engine->hangul.backspace_mode = DKST_BACKSPACE_CHAR;
      } else {
        engine->hangul.backspace_mode = DKST_BACKSPACE_JASO;
      }
      g_free(mode_str);
    }

    // Indicator
    if (g_key_file_has_key(key_file, "Settings", "EnableIndicator", NULL)) {
      engine->enable_indicator =
          g_key_file_get_boolean(key_file, "Settings", "EnableIndicator", NULL);
    }

    // Custom Shift
    if (g_key_file_has_key(key_file, "Settings", "EnableCustomShift", NULL)) {
      engine->enable_custom_shift = g_key_file_get_boolean(
          key_file, "Settings", "EnableCustomShift", NULL);
    }

    // Toggle Keys
    if (g_key_file_has_key(key_file, "ToggleKeys", "Keys", NULL)) {
      gchar *keys_str =
          g_key_file_get_string(key_file, "ToggleKeys", "Keys", NULL);
      if (keys_str) {
        gchar **keys = g_strsplit(keys_str, ";", -1);
        for (int i = 0; keys[i] != NULL; i++) {
          if (strlen(keys[i]) > 0) {
            add_toggle_key(engine, keys[i]);
          }
        }
        g_strfreev(keys);
        g_free(keys_str);
      }
    }

    // Hanja Keys
    if (engine->hanja_keys) {
      g_list_free_full(engine->hanja_keys, free_toggle_key);
      engine->hanja_keys = NULL;
    }
    if (g_key_file_has_key(key_file, "HanjaKeys", "Keys", NULL)) {
      gchar *keys_str =
          g_key_file_get_string(key_file, "HanjaKeys", "Keys", NULL);
      if (keys_str) {
        gchar **keys = g_strsplit(keys_str, ";", -1);
        for (int i = 0; keys[i] != NULL; i++) {
          if (strlen(keys[i]) > 0) {
            add_hanja_key(engine, keys[i]);
          }
        }
        g_strfreev(keys);
        g_free(keys_str);
      }
    }

    // Load Mappings
    g_hash_table_remove_all(engine->shift_mappings);
    if (engine->enable_custom_shift) {
      gsize length = 0;
      gchar **keys =
          g_key_file_get_keys(key_file, "CustomShift", &length, NULL);
      if (keys) {
        for (gsize i = 0; i < length; i++) {
          gchar *val =
              g_key_file_get_string(key_file, "CustomShift", keys[i], NULL);
          if (val) {
            g_hash_table_insert(engine->shift_mappings, g_strdup(keys[i]), val);
          }
        }
        g_strfreev(keys);
      }
    }

    debug_log("Config Loaded: Moa=%d, Backspace=%d, Shift=%d\n",
              engine->enable_moa_jjiki, engine->hangul.backspace_mode,
              engine->enable_custom_shift);
  }

  // Fallback if no toggle keys loaded? Add defaults.
  if (engine->toggle_keys == NULL) {
    add_toggle_key(engine, "Shift+space");
    add_toggle_key(engine, "Hangul");
  }

  // Fallback if no hanja keys loaded? Add defaults.
  if (engine->hanja_keys == NULL) {
    add_hanja_key(engine, "Alt+Return");
    add_hanja_key(engine, "Hangul_Hanja");
  }

  g_key_file_free(key_file);
  g_free(config_path);
}

// Helper to update preedit text
static void update_preedit(DkstEngine *engine) {
  uint32_t syl = dkst_hangul_current_syllable(&engine->hangul);
  if (syl > 0) {
    IBusText *text = ibus_text_new_from_unichar(syl);
    ibus_text_set_attributes(text, ibus_attr_list_new());
    ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE,
                               IBUS_ATTR_UNDERLINE_SINGLE, 0,
                               ibus_text_get_length(text));

    // Attempt to force visibility in Sublime Text by adding a background color
    // attribute. Using a neutral light gray (0xDDDDDD). IBUS colors are
    // typically simple integers (RGB). Note: This might look weird in dark
    // modes, but we need to verify visibility first. 0x00RRGGBB
    ibus_text_append_attribute(text, IBUS_ATTR_TYPE_BACKGROUND, 0x00666666, 0,
                               ibus_text_get_length(text));

    // Use PREEDIT_COMMIT mode to ensure text stays at original position if
    // committed automatically
    ibus_engine_update_preedit_text_with_mode((IBusEngine *)engine, text,
                                              ibus_text_get_length(text), TRUE,
                                              IBUS_ENGINE_PREEDIT_COMMIT);
  } else if (engine->showing_indicator) {
    // Show "한" or "EN"
    const char *indicator_str = engine->is_hangul_mode ? "한" : "영";
    IBusText *text = ibus_text_new_from_string(indicator_str);
    ibus_text_set_attributes(text, ibus_attr_list_new());
    // Use a different color or style for indicator?
    // Maybe just underline for now to be safe.
    // ibus_text_append_attribute(text, IBUS_ATTR_TYPE_FOREGROUND, 0x0000FF00,
    // 0,
    //                            ibus_text_get_length(text)); // Green?

    ibus_engine_update_preedit_text_with_mode((IBusEngine *)engine, text,
                                              ibus_text_get_length(text), TRUE,
                                              IBUS_ENGINE_PREEDIT_CLEAR);
  } else {
    ibus_engine_hide_preedit_text((IBusEngine *)engine);
  }
}

static void clear_indicator(DkstEngine *engine) {
  if (engine->indicator_timeout_id > 0) {
    g_source_remove(engine->indicator_timeout_id);
    engine->indicator_timeout_id = 0;
  }
  if (engine->showing_indicator) {
    engine->showing_indicator = FALSE;
    // Don't call update_preedit here immediately if we are inside
    // process_key_event usually update_preedit is called at end of
    // process_key_event anyway. But if called efficiently? Let's just update.
    update_preedit(engine);
  }
}

static gboolean on_indicator_timeout(gpointer data) {
  DkstEngine *engine = (DkstEngine *)data;
  engine->indicator_timeout_id = 0;
  engine->showing_indicator = FALSE;
  update_preedit(engine);
  return G_SOURCE_REMOVE;
}

static void show_indicator(DkstEngine *engine) {
  if (!engine->enable_indicator)
    return;

  if (engine->indicator_timeout_id > 0) {
    g_source_remove(engine->indicator_timeout_id);
  }
  engine->showing_indicator = TRUE;
  // 1 second timeout
  engine->indicator_timeout_id =
      g_timeout_add(1000, on_indicator_timeout, engine);
  update_preedit(engine);
}

// Forward declaration for commit_string (used by select_hanja_candidate)
static void commit_string(DkstEngine *engine, const char *str);

// --- Hanja Feature ---
static void hide_hanja_candidates(DkstEngine *engine) {
  if (engine->hanja_mode) {
    engine->hanja_mode = FALSE;
    ibus_engine_hide_lookup_table((IBusEngine *)engine);
    ibus_lookup_table_clear(engine->table);
  }
  if (engine->hanja_candidates) {
    g_ptr_array_unref(engine->hanja_candidates);
    engine->hanja_candidates = NULL;
  }
  if (engine->hanja_source) {
    g_free(engine->hanja_source);
    engine->hanja_source = NULL;
  }
}

static void show_hanja_candidates(DkstEngine *engine) {
  debug_log("show_hanja_candidates: ENTER\n");

  // Get current composed text
  uint32_t syl = dkst_hangul_current_syllable(&engine->hangul);
  debug_log("show_hanja_candidates: syl=%x, word_buffer=%s\n", syl,
            engine->word_buffer ? engine->word_buffer : "(null)");

  // Build lookup string
  GString *word = g_string_new("");
  gchar cur_char[7] = "";

  if (engine->word_buffer && strlen(engine->word_buffer) > 0) {
    g_string_append(word, engine->word_buffer);
  }

  if (syl != 0) {
    // Convert current syllable to UTF-8
    gint cur_len = g_unichar_to_utf8(syl, cur_char);
    cur_char[cur_len] = '\0';
    g_string_append(word, cur_char);
  }

  // If nothing to look up, return
  if (word->len == 0) {
    debug_log("show_hanja_candidates: nothing to look up\n");
    g_string_free(word, TRUE);
    return;
  }

  debug_log("show_hanja_candidates: looking up word '%s'\n", word->str);

  // Try word lookup first
  GPtrArray *candidates = hanja_dict_lookup(&g_hanja_dict, word->str);
  gboolean is_word_match = FALSE;
  glong word_len = g_utf8_strlen(word->str, -1);

  // If word is 2+ chars and lookup found results, use word match
  if (word_len >= 2 && candidates && candidates->len > 0) {
    is_word_match = TRUE;
    debug_log(
        "show_hanja_candidates: word match found (%ld chars), %u candidates\n",
        word_len, candidates->len);
  } else if (cur_char[0] != '\0') {
    // Fall back to single character lookup
    if (candidates)
      g_ptr_array_unref(candidates);
    debug_log("show_hanja_candidates: no word match, trying single char '%s'\n",
              cur_char);
    candidates = hanja_dict_lookup(&g_hanja_dict, cur_char);
  }

  debug_log("show_hanja_candidates: candidates=%p\n", (void *)candidates);
  if (!candidates || candidates->len == 0) {
    debug_log("show_hanja_candidates: no candidates found\n");
    if (candidates)
      g_ptr_array_unref(candidates);
    g_string_free(word, TRUE);
    return;
  }
  debug_log("show_hanja_candidates: found %u candidates\n", candidates->len);

  // Store source text for later
  if (is_word_match) {
    engine->hanja_source = g_strdup(word->str);
  } else {
    engine->hanja_source = g_strdup(cur_char);
  }
  g_string_free(word, TRUE);

  engine->hanja_candidates = candidates;
  engine->hanja_mode = TRUE;

  // Populate lookup table
  ibus_lookup_table_clear(engine->table);
  for (guint i = 0; i < candidates->len; i++) {
    const gchar *candidate = g_ptr_array_index(candidates, i);
    IBusText *text = ibus_text_new_from_string(candidate);
    ibus_lookup_table_append_candidate(engine->table, text);
  }

  // Show lookup table
  debug_log("show_hanja_candidates: showing table with %u entries\n",
            ibus_lookup_table_get_number_of_candidates(engine->table));
  ibus_engine_update_lookup_table((IBusEngine *)engine, engine->table, TRUE);
  debug_log("show_hanja_candidates: EXIT success\n");
}

static void select_hanja_candidate(DkstEngine *engine, guint index) {
  if (!engine->hanja_mode || !engine->hanja_candidates)
    return;

  if (index >= engine->hanja_candidates->len)
    return;

  const gchar *selected = g_ptr_array_index(engine->hanja_candidates, index);

  // Extract just the character (before any parenthesis)
  // Format may be "韓 (한국 한)" - we want just "韓"
  gchar *commit_str = g_strdup(selected);
  gchar *space = strchr(commit_str, ' ');
  if (space)
    *space = '\0';

  // Clear word buffer when hanja is selected (word is replaced)
  if (engine->word_buffer) {
    g_free(engine->word_buffer);
    engine->word_buffer = NULL;
  }

  // Clear composed text
  dkst_hangul_reset(&engine->hangul);
  ibus_engine_hide_preedit_text((IBusEngine *)engine);

  // Commit selected hanja
  commit_string(engine, commit_str);
  g_free(commit_str);

  // Cleanup
  hide_hanja_candidates(engine);
}

static void commit_string(DkstEngine *engine, const char *str) {
  if (str && *str) {
    IBusText *text = ibus_text_new_from_string(str);
    ibus_engine_commit_text((IBusEngine *)engine, text);
  }
}

static void commit_full(DkstEngine *engine) {
  // Current composed
  uint32_t syl = dkst_hangul_current_syllable(&engine->hangul);

  // Any pending commit
  char *pending = dkst_hangul_get_commit_string(&engine->hangul);

  GString *full = g_string_new("");
  if (pending) {
    g_string_append(full, pending);
    g_free(pending);
  }
  if (syl) {
    g_string_append_unichar(full, syl);
  }

  // Use PREEDIT_COMMIT mode to commit text at the PREEDIT position (original
  // cursor) This prevents the text from jumping to a new cursor position (e.g.
  // on mouse click)
  // Use ibus_engine_commit_text for explicit commits (Space, Enter, etc.)
  // PREEDIT_COMMIT mode in update_preedit handles the focus_out case
  // automatically.
  if (full->len > 0) {
    IBusText *text = ibus_text_new_from_string(full->str);
    ibus_engine_commit_text((IBusEngine *)engine, text);
    // Note: commit_text takes ownership of text or refcounts?
    // Usually we unref if we created it? IBus docs say: "text: An IBusText to
    // be committed." Most examples show just passing it. IBus bindings usually
    // handle ref/sink. Actually ibus_engine_commit_text usually consumes the
    // text object float ref. But checking ibus-hangul: text = ...;
    // ibus_engine_commit_text(...); It does not free 'text' manually if IBus
    // takes it. g_object_ref_sink logic usually applies.

    // Accumulate committed Hangul into word_buffer for multi-char hanja lookup
    if (engine->word_buffer == NULL) {
      engine->word_buffer = g_strdup(full->str);
    } else {
      gchar *new_buffer = g_strconcat(engine->word_buffer, full->str, NULL);
      g_free(engine->word_buffer);
      engine->word_buffer = new_buffer;
    }
    // Limit word_buffer size to prevent unbounded growth (max ~20 chars)
    if (engine->word_buffer && g_utf8_strlen(engine->word_buffer, -1) > 20) {
      g_free(engine->word_buffer);
      engine->word_buffer = NULL;
    }
  }

  // Reset internal state
  dkst_hangul_reset(&engine->hangul);
  // Ensure visual preedit is cleared/updated to match empty state
  update_preedit(engine);

  debug_log("commit_full: Committed '%s', word_buffer='%s'.\n", full->str,
            engine->word_buffer ? engine->word_buffer : "");

  g_string_free(full, TRUE);
}

static void check_and_commit_pending(DkstEngine *engine) {
  char *pending = dkst_hangul_get_commit_string(&engine->hangul);
  if (pending) {
    commit_string(engine, pending);

    // Also accumulate to word_buffer for multi-char hanja lookup
    if (engine->word_buffer == NULL) {
      engine->word_buffer = g_strdup(pending);
    } else {
      gchar *new_buffer = g_strconcat(engine->word_buffer, pending, NULL);
      g_free(engine->word_buffer);
      engine->word_buffer = new_buffer;
    }
    // Limit word_buffer size
    if (engine->word_buffer && g_utf8_strlen(engine->word_buffer, -1) > 20) {
      g_free(engine->word_buffer);
      engine->word_buffer = NULL;
    }

    g_free(pending);
  }
}

// --- Properties & Setup ---
static void dkst_engine_register_props(DkstEngine *engine) {
  IBusPropList *props = ibus_prop_list_new();

  // Settings Property
  IBusProperty *prop_setup =
      ibus_property_new("Setup", PROP_TYPE_NORMAL,
                        ibus_text_new_from_string("환경설정 (Settings)"),
                        "gtk-preferences", // Standard icon
                        ibus_text_new_from_string("Open Settings"), TRUE, TRUE,
                        PROP_STATE_UNCHECKED, NULL);
  ibus_prop_list_append(props, prop_setup);

  // Dictionary Editor Property
  IBusProperty *prop_hanja_editor = ibus_property_new(
      "HanjaEditor", PROP_TYPE_NORMAL,
      ibus_text_new_from_string("사전 편집기 (Dictionary Editor)"),
      "accessories-dictionary",
      ibus_text_new_from_string("Edit Hanja Dictionary"), TRUE, TRUE,
      PROP_STATE_UNCHECKED, NULL);
  ibus_prop_list_append(props, prop_hanja_editor);

  ibus_engine_register_properties((IBusEngine *)engine, props);
}

static void dkst_engine_property_activate(IBusEngine *e, const gchar *prop_name,
                                          guint prop_state) {
  debug_log("Property Activate: %s\n", prop_name);

  if (g_strcmp0(prop_name, "Setup") == 0) {
    // Launch setup.py
    gchar *argv[] = {"/usr/share/ibus-dkst/setup.py", NULL};
    GError *error = NULL;
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                  &error);
    if (error) {
      debug_log("Failed to launch setup: %s\n", error->message);
      g_error_free(error);
    }
  } else if (g_strcmp0(prop_name, "HanjaEditor") == 0) {
    // Launch hanja_editor.py
    gchar *argv[] = {"/usr/share/ibus-dkst/hanja_editor.py", NULL};
    GError *error = NULL;
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                  &error);
    if (error) {
      debug_log("Failed to launch hanja editor: %s\n", error->message);
      g_error_free(error);
    }
  }
}

static gboolean dkst_engine_process_key_event(IBusEngine *e, guint keyval,
                                              guint keycode, guint state) {
  DkstEngine *engine = (DkstEngine *)e;

  debug_log("Key: val=%x code=%x state=%x mode=%d hanja=%d\n", keyval, keycode,
            state, engine->is_hangul_mode, engine->hanja_mode);

  // Ignore updates on release
  if (state & IBUS_RELEASE_MASK)
    return FALSE;

  // --- Hanja Mode Key Handling ---
  if (engine->hanja_mode) {
    debug_log("Hanja mode: handling key %x\n", keyval);

    // Safety check
    if (!engine->table || !IBUS_IS_LOOKUP_TABLE(engine->table)) {
      debug_log("ERROR: engine->table is invalid! ptr=%p\n",
                (void *)engine->table);
      engine->hanja_mode = FALSE;
      return FALSE;
    }

    // Handle candidate selection when in hanja mode
    debug_log("Hanja mode: calling get_cursor_pos...\n");
    guint cursor = ibus_lookup_table_get_cursor_pos(engine->table);
    debug_log("Hanja mode: cursor=%u\n", cursor);

    switch (keyval) {
    case IBUS_KEY_Up:
    case IBUS_KEY_KP_Up:
      debug_log("Hanja: cursor up\n");
      ibus_lookup_table_cursor_up(engine->table);
      ibus_engine_update_lookup_table(e, engine->table, TRUE);
      return TRUE;

    case IBUS_KEY_Down:
    case IBUS_KEY_KP_Down:
      debug_log("Hanja: cursor down\n");
      ibus_lookup_table_cursor_down(engine->table);
      ibus_engine_update_lookup_table(e, engine->table, TRUE);
      return TRUE;

    case IBUS_KEY_Page_Up:
      ibus_lookup_table_page_up(engine->table);
      ibus_engine_update_lookup_table(e, engine->table, TRUE);
      return TRUE;

    case IBUS_KEY_Page_Down:
      ibus_lookup_table_page_down(engine->table);
      ibus_engine_update_lookup_table(e, engine->table, TRUE);
      return TRUE;

    case IBUS_KEY_Return:
    case IBUS_KEY_KP_Enter:
      cursor = ibus_lookup_table_get_cursor_pos(engine->table);
      select_hanja_candidate(engine, cursor);
      return TRUE;

    case IBUS_KEY_Escape:
      hide_hanja_candidates(engine);
      return TRUE;

    // Number keys 1-9 for direct selection
    case IBUS_KEY_1:
    case IBUS_KEY_2:
    case IBUS_KEY_3:
    case IBUS_KEY_4:
    case IBUS_KEY_5:
    case IBUS_KEY_6:
    case IBUS_KEY_7:
    case IBUS_KEY_8:
    case IBUS_KEY_9: {
      guint page_size = ibus_lookup_table_get_page_size(engine->table);
      guint page_start =
          (ibus_lookup_table_get_cursor_pos(engine->table) / page_size) *
          page_size;
      guint index = page_start + (keyval - IBUS_KEY_1);
      if (index < engine->hanja_candidates->len) {
        select_hanja_candidate(engine, index);
      }
    }
      return TRUE;

    default:
      // Any other key cancels hanja mode
      hide_hanja_candidates(engine);
      // Fall through to normal processing
      break;
    }
  }

  // --- Hanja Trigger Keys (from config) ---
  if (engine->hanja_keys) {
    guint mask = IBUS_SHIFT_MASK | IBUS_CONTROL_MASK | IBUS_MOD1_MASK |
                 IBUS_SUPER_MASK | IBUS_META_MASK;
    guint current_mods = state & mask;

    GList *l;
    for (l = engine->hanja_keys; l != NULL; l = l->next) {
      ToggleKey *hk = (ToggleKey *)l->data;
      if (keyval == hk->keyval && current_mods == hk->modifiers) {
        // Allow hanja conversion if there's composed text OR word_buffer
        if (dkst_hangul_has_composed(&engine->hangul) ||
            (engine->word_buffer && strlen(engine->word_buffer) > 0)) {
          show_hanja_candidates(engine);
          return TRUE;
        }
        return FALSE;
      }
    }
  }

  // Check against toggle keys
  // For each registered toggle key, check if keyval and modifiers (excluding
  // Locks) match Relevant modifiers for comparison: Shift, Control, Alt, Super,
  // Meta
  guint mask = IBUS_SHIFT_MASK | IBUS_CONTROL_MASK | IBUS_MOD1_MASK |
               IBUS_SUPER_MASK | IBUS_META_MASK;
  guint current_mods = state & mask;

  if (engine->toggle_keys) {
    GList *l;
    for (l = engine->toggle_keys; l != NULL; l = l->next) {
      ToggleKey *tk = (ToggleKey *)l->data;

      if (keyval == tk->keyval && current_mods == tk->modifiers) {
        if (keyval == tk->keyval && current_mods == tk->modifiers) {
          debug_log("Toggle Key Matched! Toggling mode.\n");
          commit_full(engine);
          engine->is_hangul_mode = !engine->is_hangul_mode;
          // update_preedit(engine); // Called inside show_indicator
          show_indicator(engine);
          return TRUE;
        }
      }
    }
  }

  // Check for Modifier Keys themselves being pressed (not just holding
  // modifier)
  switch (keyval) {
  case IBUS_KEY_Shift_L:
  case IBUS_KEY_Shift_R:
  case IBUS_KEY_Control_L:
  case IBUS_KEY_Control_R:
  case IBUS_KEY_Alt_L:
  case IBUS_KEY_Alt_R:
  case IBUS_KEY_Meta_L:
  case IBUS_KEY_Meta_R:
  case IBUS_KEY_Super_L:
  case IBUS_KEY_Super_R:
  case IBUS_KEY_Caps_Lock:
    return FALSE;
  }

  // Custom Shift Handling
  gboolean is_shift = (state & IBUS_SHIFT_MASK) != 0;
  if (engine->enable_custom_shift && is_shift && engine->is_hangul_mode) {
    const gchar *key_name = ibus_keyval_name(keyval);
    if (key_name) {
      gchar *mapped = g_hash_table_lookup(engine->shift_mappings, key_name);
      if (mapped) {
        clear_indicator(engine); // Clear if typing
        commit_full(engine);
        commit_string(engine, mapped);
        return TRUE;
      }
    }
  }

  // Allow only Shift to pass through for typing (e.g. upper case)
  // But if Ctrl/Alt/Super are pressed, ignore.
  if (state & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK)) {
    debug_log("Modifier pressed, ignoring.\n");
    if (dkst_hangul_has_composed(&engine->hangul)) {
      commit_full(engine);
    }
    return FALSE;
  }

  // English Mode Pass-through
  if (!engine->is_hangul_mode) {
    if (engine->showing_indicator)
      clear_indicator(engine);
    // debug_log("English Mode. Pass.\n");
    return FALSE;
  }

  // Backspace
  // Backspace
  // Backspace
  if (keyval == IBUS_KEY_BackSpace) {
    // if (engine->showing_indicator) clear_indicator(engine); // Handled above
    // implicitly? No, wait. Wait, if we are here we are in Hangul mode. If
    // indicator was showing, we should clear it if we start interacting.
    if (engine->showing_indicator)
      clear_indicator(engine);

    if (dkst_hangul_backspace(&engine->hangul)) {
      update_preedit(engine);
      return TRUE;
    }
    return FALSE;
  }

  // Space/Enter -> Commit
  if (keyval == IBUS_KEY_space || keyval == IBUS_KEY_Return) {
    if (engine->showing_indicator)
      clear_indicator(engine);
    commit_full(engine); // Commit everything

    // Reset word buffer on space/enter (word boundary)
    if (engine->word_buffer) {
      g_free(engine->word_buffer);
      engine->word_buffer = NULL;
    }

    return FALSE; // Let system handle space
  }

  // Printable char
  if (keyval >= 32 && keyval <= 126) {
    if (engine->showing_indicator)
      clear_indicator(engine);
    char c = (char)keyval;
    if (dkst_hangul_process(&engine->hangul, c)) {
      check_and_commit_pending(engine);
      update_preedit(engine);
      return TRUE;
    } else {
      check_and_commit_pending(engine);
      update_preedit(engine);
      if (dkst_hangul_has_composed(&engine->hangul)) {
        commit_full(engine);
      }
      return FALSE;
    }
  }

  // Other keys
  if (dkst_hangul_has_composed(&engine->hangul)) {
    commit_full(engine);
    return FALSE;
  }

  return FALSE;
}

static void dkst_engine_focus_in(IBusEngine *e) {
  DkstEngine *engine = (DkstEngine *)e;
  debug_log("Focus In\n");

  // Safety: Ensure no leftover state from previous interactions
  if (dkst_hangul_has_composed(&engine->hangul)) {
    debug_log("Focus In: Cleansing leftover state. (Cho=%x Jung=%x Jong=%x)\n",
              engine->hangul.cho, engine->hangul.jung, engine->hangul.jong);
    dkst_hangul_reset(&engine->hangul);
    dkst_hangul_reset(&engine->hangul);
    ibus_engine_hide_preedit_text(e);
  }

  // Also clear indicator on focus in, just in case
  clear_indicator(engine);

  // Refresh config on focus in
  load_config(engine);
  // Register Properties
  dkst_engine_register_props(engine);
}

static void dkst_engine_focus_out(IBusEngine *e) {
  DkstEngine *engine = (DkstEngine *)e;
  debug_log("Focus Out: Starting... (Hangul Mode: %d)\n",
            engine->is_hangul_mode);

  if (dkst_hangul_has_composed(&engine->hangul)) {
    debug_log("Focus Out: Has composed text. Resetting internal state "
              "(auto-commit expected).\n");
    // Do NOT commit manually here. ibus_engine_update_preedit_text_with_mode
    // (used in update_preedit) handles the commit at the correct position
    // automatically.
    dkst_hangul_reset(&engine->hangul);
    // Clearing the preedit buffer logic:
    // ibus-hangul does: hangul_ic_reset + ustring_clear
    // We just reset our hangul state.
  } else {
    debug_log("Focus Out: No composed text.\n");
  }

  // Clear indicator on focus out
  clear_indicator(engine);
  debug_log("Focus Out: Finished.\n");
}

static void dkst_engine_reset(IBusEngine *e) {
  DkstEngine *engine = (DkstEngine *)e;
  debug_log("Reset: Starting...\n");
  // Similarly, reset signal should rely on PREEDIT_COMMIT auto-behavior
  dkst_hangul_reset(&engine->hangul);
  debug_log("Reset: Finished.\n");
}

static void dkst_engine_disable(IBusEngine *e) {
  DkstEngine *engine = (DkstEngine *)e;
  debug_log("Disable\n");
  commit_full(engine);
}

static void dkst_engine_set_capabilities(IBusEngine *e, guint caps) {
  // Log the capabilities reported by the client application
  debug_log("set_capabilities: %x\n", caps);

  if (caps & IBUS_CAP_PREEDIT_TEXT) {
    debug_log("Client supports IBUS_CAP_PREEDIT_TEXT\n");
  } else {
    debug_log("Client DOES NOT support IBUS_CAP_PREEDIT_TEXT\n");
  }
}

static void dkst_engine_class_init(DkstEngineClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

  object_class->finalize = dkst_engine_finalize;

  engine_class->process_key_event = dkst_engine_process_key_event;
  engine_class->focus_in = dkst_engine_focus_in;
  engine_class->focus_out = dkst_engine_focus_out;
  engine_class->reset = dkst_engine_reset;
  engine_class->disable = dkst_engine_disable;
  engine_class->set_capabilities = dkst_engine_set_capabilities;

  // Register property activate handler
  engine_class->property_activate = dkst_engine_property_activate;
}

// --- Main ---

static IBusBus *bus = NULL;
static IBusFactory *factory = NULL;

static void ibus_disconnected_cb(IBusBus *bus, gpointer user_data) {
  ibus_quit();
}

static void init(void) {
  ibus_init();

  bus = ibus_bus_new();
  g_signal_connect(bus, "disconnected", G_CALLBACK(ibus_disconnected_cb), NULL);

  factory = ibus_factory_new(ibus_bus_get_connection(bus));
  ibus_factory_add_engine(factory, "dinkisstyle", DKST_TYPE_ENGINE);

  if (ibus_bus_request_name(bus, "com.dkst.inputmethod", 0) == 0) {
    g_warning("Failed to get name: com.dkst.inputmethod");
    exit(1);
  }
}

int main(int argc, char **argv) {
  init();
  ibus_main();
  return 0;
}
