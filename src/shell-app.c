/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "st.h"
#include "shell-app-private.h"
#include "shell-global.h"
#include "shell-enum-types.h"

#include <string.h>

/**
 * SECTION:shell-app
 * @short_description: Object representing an application
 *
 * This object wraps a #ShellAppInfo, providing methods and signals
 * primarily useful for running applications.
 */
struct _ShellApp
{
  GObject parent;

  ShellAppInfo *info;

  guint workspace_switch_id;

  GSList *windows;

  ShellAppState state;
  gboolean window_sort_stale : 1;
};

G_DEFINE_TYPE (ShellApp, shell_app, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_STATE
};

enum {
  WINDOWS_CHANGED,
  LAST_SIGNAL
};

static guint shell_app_signals[LAST_SIGNAL] = { 0 };

static void
shell_app_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  ShellApp *app = SHELL_APP (gobject);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, app->state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

const char *
shell_app_get_id (ShellApp *app)
{
  return shell_app_info_get_id (app->info);
}

/**
 * shell_app_create_icon_texture:
 *
 * Look up the icon for this application, and create a #ClutterTexture
 * for it at the given size.
 *
 * Return value: (transfer none): A floating #ClutterActor
 */
ClutterActor *
shell_app_create_icon_texture (ShellApp   *app,
                               float size)
{
  return shell_app_info_create_icon_texture (app->info, size);
}
typedef struct {
  ShellApp *app;
  int size;
} CreateFadedIconData;

static CoglHandle
shell_app_create_faded_icon_cpu (StTextureCache *cache,
                                 const char     *key,
                                 void           *datap,
                                 GError        **error)
{
  CreateFadedIconData *data = datap;
  ShellApp *app;
  GdkPixbuf *pixbuf;
  int size;
  CoglHandle texture;
  gint width, height, rowstride;
  guint8 n_channels;
  gboolean have_alpha;
  gint fade_start;
  gint fade_range;
  guint i, j;
  guint pixbuf_byte_size;
  guint8 *orig_pixels;
  guint8 *pixels;
  GIcon *icon;
  GtkIconInfo *info;

  app = data->app;
  size = data->size;

  icon = shell_app_info_get_icon (app->info);
  if (icon == NULL)
    return COGL_INVALID_HANDLE;

  info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                         icon, (int) (size + 0.5),
                                         GTK_ICON_LOOKUP_FORCE_SIZE);
  g_object_unref (icon);
  if (info == NULL)
    return COGL_INVALID_HANDLE;

  pixbuf = gtk_icon_info_load_icon (info, NULL);
  gtk_icon_info_free (info);

  if (pixbuf == NULL)
    return COGL_INVALID_HANDLE;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  orig_pixels = gdk_pixbuf_get_pixels (pixbuf);
  have_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  pixbuf_byte_size = (height - 1) * rowstride +
    + width * ((n_channels * gdk_pixbuf_get_bits_per_sample (pixbuf) + 7) / 8);

  pixels = g_malloc0 (rowstride * height);
  memcpy (pixels, orig_pixels, pixbuf_byte_size);

  fade_start = width / 2;
  fade_range = width - fade_start;
  for (i = fade_start; i < width; i++)
    {
      for (j = 0; j < height; j++)
        {
          guchar *pixel = &pixels[j * rowstride + i * n_channels];
          float fade = 1.0 - ((float) i - fade_start) / fade_range;
          pixel[0] = 0.5 + pixel[0] * fade;
          pixel[1] = 0.5 + pixel[1] * fade;
          pixel[2] = 0.5 + pixel[2] * fade;
          if (have_alpha)
            pixel[3] = 0.5 + pixel[3] * fade;
        }
    }

  texture = cogl_texture_new_from_data (width,
                                        height,
                                        COGL_TEXTURE_NONE,
                                        have_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                                        COGL_PIXEL_FORMAT_ANY,
                                        rowstride,
                                        pixels);
  g_free (pixels);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * shell_app_get_faded_icon:
 * @app: A #ShellApp
 * @size: Size in pixels
 *
 * Return an actor with a horizontally faded look.
 *
 * Return value: (transfer none): A floating #ClutterActor, or %NULL if no icon
 */
ClutterActor *
shell_app_get_faded_icon (ShellApp *app, float size)
{
  MetaWindow *window;
  CoglHandle texture;
  ClutterActor *result;
  char *cache_key;
  CreateFadedIconData data;

  /* Punt for WINDOW types for now...easier to reuse the property tracking bits,
   * and this helps us visually distinguish app-tracked from not.
   */
  window = shell_app_info_get_source_window (app->info);
  if (window)
    {
      return st_texture_cache_bind_pixbuf_property (st_texture_cache_get_default (),
                                                    G_OBJECT (window),
                                                    "icon");
    }

  cache_key = g_strdup_printf ("faded-icon:%s,size=%f", shell_app_get_id (app), size);
  data.app = app;
  data.size = (int) (0.5 + size);
  texture = st_texture_cache_load (st_texture_cache_get_default (),
                                   cache_key,
                                   ST_TEXTURE_CACHE_POLICY_FOREVER,
                                   shell_app_create_faded_icon_cpu,
                                   &data,
                                   NULL);
  g_free (cache_key);

  if (texture != COGL_INVALID_HANDLE)
    {
      result = clutter_texture_new ();
      clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (result), texture);
    }
  else
    {
      result = clutter_texture_new ();
      g_object_set (result, "opacity", 0, "width", size, "height", size, NULL);

    }
  return result;
}

char *
shell_app_get_name (ShellApp *app)
{
  return shell_app_info_get_name (app->info);
}

char *
shell_app_get_description (ShellApp *app)
{
  return shell_app_info_get_description (app->info);
}

gboolean
shell_app_is_transient (ShellApp *app)
{
  return shell_app_info_is_transient (app->info);
}

/**
 * shell_app_activate:
 * @app: a #ShellApp
 *
 * Perform an appropriate default action for operating on this application,
 * dependent on its current state.  For example, if the application is not
 * currently running, launch it.  If it is running, activate the most recently
 * used window.
 */
void
shell_app_activate (ShellApp  *app)
{
  switch (app->state)
    {
      case SHELL_APP_STATE_STOPPED:
        /* TODO sensibly handle this error */
        shell_app_info_launch (app->info, NULL);
        break;
      case SHELL_APP_STATE_STARTING:
        break;
      case SHELL_APP_STATE_RUNNING:
        {
          GSList *windows = shell_app_get_windows (app);
          if (windows)
            {
              ShellGlobal *global = shell_global_get ();
              MetaScreen *screen = shell_global_get_screen (global);
              MetaWorkspace *active = meta_screen_get_active_workspace (screen);
              MetaWindow *window = windows->data;
              MetaWorkspace *workspace = meta_window_get_workspace (window);

              if (active != workspace)
                meta_workspace_activate_with_focus (workspace, window, shell_global_get_current_time (global));
              else
                meta_window_activate (window, shell_global_get_current_time (global));
            }
        }
        break;
    }
}

/**
 * shell_app_open_new_window:
 * @app: a #ShellApp
 *
 * Request that the application create a new window.
 */
void
shell_app_open_new_window (ShellApp *app)
{
  /* Here we just always launch the application again, even if we know
   * it was already running.  For most applications this
   * should have the effect of creating a new window, whether that's
   * a second process (in the case of Calculator) or IPC to existing
   * instance (Firefox).  There are a few less-sensical cases such
   * as say Pidgin.  Ideally, we have the application express to us
   * that it supports an explicit new-window action.
   */
  shell_app_info_launch (app->info, NULL);
}

/**
 * shell_app_get_state:
 * @app: a #ShellApp
 *
 * Returns: State of the application
 */
ShellAppState
shell_app_get_state (ShellApp *app)
{
  return app->state;
}

/**
 * _shell_app_get_info:
 *
 * Returns: (transfer none): Associated app info
 */
ShellAppInfo *
_shell_app_get_info (ShellApp *app)
{
  return app->info;
}

typedef struct {
  ShellApp *app;
  MetaWorkspace *active_workspace;
} CompareWindowsData;

static int
shell_app_compare_windows (gconstpointer   a,
                           gconstpointer   b,
                           gpointer        datap)
{
  MetaWindow *win_a = (gpointer)a;
  MetaWindow *win_b = (gpointer)b;
  CompareWindowsData *data = datap;
  gboolean ws_a, ws_b;
  gboolean vis_a, vis_b;

  ws_a = meta_window_get_workspace (win_a) == data->active_workspace;
  ws_b = meta_window_get_workspace (win_b) == data->active_workspace;

  if (ws_a && !ws_b)
    return -1;
  else if (!ws_a && ws_b)
    return 1;

  vis_a = meta_window_showing_on_its_workspace (win_a);
  vis_b = meta_window_showing_on_its_workspace (win_b);

  if (vis_a && !vis_b)
    return -1;
  else if (!vis_a && vis_b)
    return 1;

  return meta_window_get_user_time (win_b) - meta_window_get_user_time (win_a);
}

/**
 * shell_app_get_windows:
 * @app:
 *
 * Get the toplevel, interesting windows which are associated with this
 * application.  The returned list will be sorted first by whether
 * they're on the active workspace, then by whether they're visible,
 * and finally by the time the user last interacted with them.
 *
 * Returns: (transfer none) (element-type MetaWindow): List of windows
 */
GSList *
shell_app_get_windows (ShellApp *app)
{
  if (app->window_sort_stale)
    {
      CompareWindowsData data;
      data.app = app;
      data.active_workspace = meta_screen_get_active_workspace (shell_global_get_screen (shell_global_get ()));
      app->windows = g_slist_sort_with_data (app->windows, shell_app_compare_windows, &data);
      app->window_sort_stale = FALSE;
    }

  return app->windows;
}

guint
shell_app_get_n_windows (ShellApp *app)
{
  return g_slist_length (app->windows);
}

static gboolean
shell_app_has_visible_windows (ShellApp   *app)
{
  GSList *iter;

  for (iter = app->windows; iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;

      if (meta_window_showing_on_its_workspace (window))
        return TRUE;
    }

  return FALSE;
}

gboolean
shell_app_is_on_workspace (ShellApp *app,
                           MetaWorkspace   *workspace)
{
  GSList *iter;

  for (iter = app->windows; iter; iter = iter->next)
    {
      if (meta_window_get_workspace (iter->data) == workspace)
        return TRUE;
    }

  return FALSE;
}

/**
 * shell_app_compare:
 * @app:
 * @other: A #ShellApp
 *
 * Compare one #ShellApp instance to another, in the following way:
 *   - If one of them has visible windows and the other does not, the one
 *     with visible windows is first.
 *   - If one has no windows at all (i.e. it's not running) and the other
 *     does, the one with windows is first.
 *   - Finally, the application which the user interacted with most recently
 *     compares earlier.
 */
int
shell_app_compare (ShellApp *app,
                   ShellApp *other)
{
  gboolean vis_app, vis_other;
  GSList *windows_app, *windows_other;

  vis_app = shell_app_has_visible_windows (app);
  vis_other = shell_app_has_visible_windows (other);

  if (vis_app && !vis_other)
    return -1;
  else if (!vis_app && vis_other)
    return 1;

  if (app->windows && !other->windows)
    return -1;
  else if (!app->windows && other->windows)
    return 1;

  windows_app = shell_app_get_windows (app);
  windows_other = shell_app_get_windows (other);

  return meta_window_get_user_time (windows_other->data) - meta_window_get_user_time (windows_app->data);
}

ShellApp *
_shell_app_new_for_window (MetaWindow      *window)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);
  app->info = shell_app_system_create_from_window (shell_app_system_get_default (), window);
  _shell_app_system_register_app (shell_app_system_get_default (), app);
  _shell_app_add_window (app, window);

  return app;
}

ShellApp *
_shell_app_new (ShellAppInfo    *info)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);
  app->info = shell_app_info_ref (info);
  _shell_app_system_register_app (shell_app_system_get_default (), app);

  return app;
}

static void
shell_app_state_transition (ShellApp      *app,
                            ShellAppState  state)
{
  if (app->state == state)
    return;
  g_return_if_fail (!(app->state == SHELL_APP_STATE_RUNNING &&
                      state == SHELL_APP_STATE_STARTING));
  app->state = state;
  g_object_notify (G_OBJECT (app), "state");
}

static void
shell_app_on_unmanaged (MetaWindow      *window,
                        ShellApp *app)
{
  _shell_app_remove_window (app, window);
}

static void
shell_app_on_user_time_changed (MetaWindow *window,
                                GParamSpec *pspec,
                                ShellApp   *app)
{
  /* Ideally we don't want to emit windows-changed if the sort order
   * isn't actually changing. This check catches most of those.
   */
  if (window != app->windows->data)
    {
      app->window_sort_stale = TRUE;
      g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
    }
}

static void
shell_app_on_ws_switch (MetaScreen         *screen,
                        int                 from,
                        int                 to,
                        MetaMotionDirection direction,
                        gpointer            data)
{
  ShellApp *self = SHELL_APP (data);
  self->window_sort_stale = TRUE;
  g_signal_emit (self, shell_app_signals[WINDOWS_CHANGED], 0);
}

void
_shell_app_add_window (ShellApp        *app,
                       MetaWindow      *window)
{
  if (g_slist_find (app->windows, window))
    return;

  app->windows = g_slist_prepend (app->windows, g_object_ref (window));
  g_signal_connect (window, "unmanaged", G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_connect (window, "notify::user-time", G_CALLBACK(shell_app_on_user_time_changed), app);
  app->window_sort_stale = TRUE;

  if (app->state != SHELL_APP_STATE_RUNNING)
    shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);

  if (app->workspace_switch_id == 0)
    {
      MetaScreen *screen = shell_global_get_screen (shell_global_get ());

      app->workspace_switch_id =
        g_signal_connect (screen, "workspace-switched", G_CALLBACK(shell_app_on_ws_switch), app);
    }
}

static void
disconnect_workspace_switch (ShellApp  *app)
{
  MetaScreen *screen;

  if (app->workspace_switch_id == 0)
    return;

  screen = shell_global_get_screen (shell_global_get ());
  g_signal_handler_disconnect (screen, app->workspace_switch_id);
  app->workspace_switch_id = 0;
}

void
_shell_app_remove_window (ShellApp   *app,
                          MetaWindow *window)
{
  if (!g_slist_find (app->windows, window))
    return;

  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_user_time_changed), app);
  g_object_unref (window);
  app->windows = g_slist_remove (app->windows, window);

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);

  if (app->windows == NULL)
    {
      disconnect_workspace_switch (app);

      shell_app_state_transition (app, SHELL_APP_STATE_STOPPED);
    }
}

void
_shell_app_set_starting (ShellApp        *app,
                         gboolean         starting)
{
  if (starting && app->state == SHELL_APP_STATE_STOPPED)
    shell_app_state_transition (app, SHELL_APP_STATE_STARTING);
  else if (!starting && app->state == SHELL_APP_STATE_STARTING)
    shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);
}

static void
shell_app_init (ShellApp *self)
{
  self->state = SHELL_APP_STATE_STOPPED;
}

static void
shell_app_dispose (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  if (app->info)
    {
      shell_app_info_unref (app->info);
      app->info = NULL;
    }

  while (app->windows)
    _shell_app_remove_window (app, app->windows->data);

  disconnect_workspace_switch (app);

  G_OBJECT_CLASS(shell_app_parent_class)->dispose (object);
}

static void
shell_app_class_init(ShellAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_app_get_property;
  gobject_class->dispose = shell_app_dispose;

  shell_app_signals[WINDOWS_CHANGED] = g_signal_new ("windows-changed",
                                     SHELL_TYPE_APP,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

  /**
   * ShellApp:state:
   *
   * The high-level state of the application, effectively whether it's
   * running or not, or transitioning between those states.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STATE,
                                   g_param_spec_enum ("state",
                                                      "State",
                                                      "Application state",
                                                      SHELL_TYPE_APP_STATE,
                                                      SHELL_APP_STATE_STOPPED,
                                                      G_PARAM_READABLE));
}
