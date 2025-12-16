

#include "hangul.h"
#include <ibus.h>
#include <stdio.h>

static void debug_log(const char *fmt, ...) {
  // Use a user-specific temp file to avoid permission issues
  char log_path[256];
  const char *user = g_get_user_name();
  snprintf(log_path, sizeof(log_path), "/tmp/dkst_debug_%s.log",
           user ? user : "default");

  FILE *f = fopen(log_path, "a");
  if (f) {
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
  } else {
    // Fallback to stderr if file cannot be opened
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(IBusEngine, g_object_unref)

// --- GObject Boilerplate ---
#define DKST_TYPE_ENGINE (dkst_engine_get_type())
G_DECLARE_FINAL_TYPE(DkstEngine, dkst_engine, DKST, ENGINE, IBusEngine)

struct _DkstEngine {
  IBusEngine parent;

  DKSTHangul hangul;
  IBusLookupTable *table;
  gboolean is_hangul_mode;
};

G_DEFINE_TYPE(DkstEngine, dkst_engine, IBUS_TYPE_ENGINE)

static void dkst_engine_init(DkstEngine *engine) {
  dkst_hangul_init(&engine->hangul);
  engine->table = ibus_lookup_table_new(10, 0, TRUE, TRUE);
  engine->is_hangul_mode = TRUE;
  // Config loading could go here (e.g. gsettings)
}

static void dkst_engine_finalize(GObject *object) {
  DkstEngine *engine = (DkstEngine *)object;
  dkst_hangul_free(&engine->hangul);
  // g_object_unref(engine->table); // IBus might handle this? check docs.
  // explicit unref is safer usually.
  G_OBJECT_CLASS(dkst_engine_parent_class)->finalize(object);
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
    ibus_engine_update_preedit_text((IBusEngine *)engine, text,
                                    ibus_text_get_length(text), TRUE);
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

  if (full->len > 0) {
    commit_string(engine, full->str);
  }
  g_string_free(full, TRUE);

  dkst_hangul_reset(&engine->hangul);
  ibus_engine_hide_preedit_text((IBusEngine *)engine);
}

static void check_and_commit_pending(DkstEngine *engine) {
  char *pending = dkst_hangul_get_commit_string(&engine->hangul);
  if (pending) {
    commit_string(engine, pending);
    g_free(pending);
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

  // Check for Modifier Keys itself being pressed (not just holding modifier)
  // If the key IS a modifier, we should assume it doesn't commit the
  // composition.
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

  // Modifier check
  // Exception: Shift+Space for mode toggle
  gboolean is_shift = (state & IBUS_SHIFT_MASK) != 0;
  gboolean is_space = (keyval == IBUS_KEY_space);

  // Check for Shift+Space
  if (is_shift && is_space) {
    debug_log("Shift+Space Detected. Toggling mode.\n");
    commit_full(engine); // Commit any pending composition before switching
    engine->is_hangul_mode = !engine->is_hangul_mode;
    update_preedit(engine);
    return TRUE; // Consume event
  }

  // Also support Hangul key if present on keyboard
  if (keyval == IBUS_KEY_Hangul) {
    debug_log("Hangul Key Detected. Toggling mode.\n");
    commit_full(engine);
    engine->is_hangul_mode = !engine->is_hangul_mode;
    update_preedit(engine);
    return TRUE;
  }

  // Allow only Shift to pass through for typing (e.g. upper case)
  // But if Ctrl/Alt/Super are pressed, ignore.
  if (state & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK)) {
    debug_log("Modifier pressed, ignoring.\n");
    // If we have composition, commit it? Usually yes if hotkey is triggered.
    if (dkst_hangul_has_composed(&engine->hangul)) {
      commit_full(engine);
    }
    return FALSE;
  }

  // English Mode Pass-through
  if (!engine->is_hangul_mode) {
    debug_log("English Mode. Pass.\n");
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
      // Processed.
      // Check for any completed partials (e.g. from moa-jjik-gi or auto-commit)
      check_and_commit_pending(engine);
      update_preedit(engine);
      return TRUE;
    } else {
      // Not processed (e.g. non-mapped key like punctuation).
      // CRITICAL FIX: Even if not processed, dkst_hangul_process might have
      // pushed the current composition to 'completed' queue and reset state.
      // We MUST check for pending commits here.

      check_and_commit_pending(engine); // Fix for "Double Finale"

      // Fix for "Phantom Character" (User reported "다" remains visible)
      // We must clear/update the preedit because the state inside hangul has
      // likely reset but the IBus client still shows the old preedit.
      update_preedit(engine);

      // Also commit any remaining active composition (rare if process returns
      // false logic is correct, but safe)
      if (dkst_hangul_has_composed(&engine->hangul)) {
        commit_full(engine);
      }

      return FALSE; // Let system handle the raw key (e.g. period)
    }
  }

  // Other keys
  if (dkst_hangul_has_composed(&engine->hangul)) {
    commit_full(engine);
    return FALSE;
  }

  return FALSE;
}

static void dkst_engine_focus_in(IBusEngine *engine) {
  // ibus_engine_register_properties(engine, props);
}

static void dkst_engine_focus_out(IBusEngine *e) {
  DkstEngine *engine = (DkstEngine *)e;
  commit_full(engine);
}

static void dkst_engine_reset(IBusEngine *e) {
  DkstEngine *engine = (DkstEngine *)e;
  commit_full(engine);
  dkst_hangul_reset(&engine->hangul);
}

static void dkst_engine_class_init(DkstEngineClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

  object_class->finalize = dkst_engine_finalize;

  engine_class->process_key_event = dkst_engine_process_key_event;
  engine_class->focus_in = dkst_engine_focus_in;
  engine_class->focus_out = dkst_engine_focus_out;
  engine_class->reset = dkst_engine_reset;
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

  if (ibus_bus_request_name(bus, "com.dinkisstyle.inputmethod", 0) == 0) {
    g_warning("Failed to get name: com.dinkisstyle.inputmethod");
    exit(1);
  }
}

int main(int argc, char **argv) {
  init();
  ibus_main();
  return 0;
}
