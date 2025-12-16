
#include "hangul.h"
#include <ibus.h>
#include <stdio.h>
#include <string.h>

static void debug_log(const char *fmt, ...) {
  // Debug logging disabled for production
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
};

G_DEFINE_TYPE(DkstEngine, dkst_engine, IBUS_TYPE_ENGINE)

static void dkst_engine_init(DkstEngine *engine) {
  dkst_hangul_init(&engine->hangul);
  engine->table = ibus_lookup_table_new(10, 0, TRUE, TRUE);
  engine->is_hangul_mode = TRUE;

  engine->shift_mappings =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  engine->enable_custom_shift = FALSE;
  engine->enable_moa_jjiki = TRUE;

  engine->toggle_keys = NULL;
}

static void free_toggle_key(gpointer data) { g_free(data); }

static void dkst_engine_finalize(GObject *object) {
  DkstEngine *engine = (DkstEngine *)object;
  dkst_hangul_free(&engine->hangul);

  if (engine->shift_mappings) {
    g_hash_table_destroy(engine->shift_mappings);
  }

  if (engine->toggle_keys) {
    g_list_free_full(engine->toggle_keys, free_toggle_key);
    engine->toggle_keys = NULL;
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
  } else {
    ibus_engine_hide_preedit_text((IBusEngine *)engine);
  }
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
  }

  // Reset internal state
  dkst_hangul_reset(&engine->hangul);
  // Ensure visual preedit is cleared/updated to match empty state
  update_preedit(engine);

  debug_log("commit_full: Committed '%s', reset state.\n", full->str);

  g_string_free(full, TRUE);
}

static void check_and_commit_pending(DkstEngine *engine) {
  char *pending = dkst_hangul_get_commit_string(&engine->hangul);
  if (pending) {
    commit_string(engine, pending);
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
  }
}

static gboolean dkst_engine_process_key_event(IBusEngine *e, guint keyval,
                                              guint keycode, guint state) {
  DkstEngine *engine = (DkstEngine *)e;

  debug_log("Key: val=%x code=%x state=%x mode=%d\n", keyval, keycode, state,
            engine->is_hangul_mode);

  // Ignore updates on release
  if (state & IBUS_RELEASE_MASK)
    return FALSE;

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
        debug_log("Toggle Key Matched! Toggling mode.\n");
        commit_full(engine);
        engine->is_hangul_mode = !engine->is_hangul_mode;
        update_preedit(engine);
        return TRUE;
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
    // debug_log("English Mode. Pass.\n");
    return FALSE;
  }

  // Backspace
  if (keyval == IBUS_KEY_BackSpace) {
    if (dkst_hangul_backspace(&engine->hangul)) {
      update_preedit(engine);
      return TRUE;
    }
    return FALSE;
  }

  // Space/Enter -> Commit
  if (keyval == IBUS_KEY_space || keyval == IBUS_KEY_Return) {
    commit_full(engine); // Commit everything
    return FALSE;        // Let system handle space
  }

  // Printable char
  if (keyval >= 32 && keyval <= 126) {
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
    ibus_engine_hide_preedit_text(e);
  }

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
