/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hd-home-view.h"
#include "hd-comp-mgr.h"
#include "hd-home.h"

#include <clutter/clutter.h>

#include <matchbox/core/mb-wm.h>

enum
{
  SIGNAL_THUMBNAIL_CLICKED,
  SIGNAL_BACKGROUND_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_COMP_MGR = 1,
  PROP_HOME,
};

struct _HdHomeViewPrivate
{
  MBWMCompMgrClutter   *comp_mgr;
  HdHome               *home;

  ClutterActor         *background;
  ClutterActor         *mouse_trap;

  gint                  xwidth;
  gint                  xheight;

  gboolean              thumbnail_mode : 1;
  gboolean              active_input   : 1;
};

static void hd_home_view_class_init (HdHomeViewClass *klass);
static void hd_home_view_init       (HdHomeView *self);
static void hd_home_view_dispose    (GObject *object);
static void hd_home_view_finalize   (GObject *object);

static void hd_home_view_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec);

static void hd_home_view_get_property (GObject      *object,
				       guint         prop_id,
				       GValue       *value,
				       GParamSpec   *pspec);

static void hd_home_view_constructed (GObject *object);


G_DEFINE_TYPE (HdHomeView, hd_home_view, CLUTTER_TYPE_GROUP);

static void
hd_home_view_class_init (HdHomeViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdHomeViewPrivate));

  object_class->dispose      = hd_home_view_dispose;
  object_class->finalize     = hd_home_view_finalize;
  object_class->set_property = hd_home_view_set_property;
  object_class->get_property = hd_home_view_get_property;
  object_class->constructed  = hd_home_view_constructed;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_COMP_MGR, pspec);

  pspec = g_param_spec_pointer ("home",
				"HdHome",
				"Parent HdHome object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_HOME, pspec);

  signals[SIGNAL_THUMBNAIL_CLICKED] =
      g_signal_new ("thumbnail-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeViewClass, thumbnail_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__BOXED,
                    G_TYPE_NONE,
                    1,
		    CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[SIGNAL_BACKGROUND_CLICKED] =
      g_signal_new ("background-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeViewClass, background_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__BOXED,
                    G_TYPE_NONE,
                    1,
		    CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

}

static gboolean
hd_home_view_background_clicked (ClutterActor *background,
				 ClutterEvent *event,
				 HdHomeView   *view)
{
  g_debug ("View background clicked");
  g_signal_emit (view, signals[SIGNAL_BACKGROUND_CLICKED], 0, event);
  return TRUE;
}

static gboolean
hd_home_view_mouse_trap_press (ClutterActor       *trap,
			       ClutterButtonEvent *event,
			       HdHomeView         *view)
{
  HdHomeViewPrivate * priv = view->priv;

  g_debug ("Mousetrap pressed, input mode %d", priv->active_input);

  /*
   * If in active input mode, start tracking motion events
   */
  if (priv->active_input)
    {
      g_debug ("In active input mode.");

      hd_home_connect_pan_handler (priv->home, trap, event->x, event->y);

      return TRUE;
    }

  return FALSE;
}

static gboolean
hd_home_view_mouse_trap_release (ClutterActor       *trap,
				 ClutterButtonEvent *event,
				 HdHomeView         *view)
{
  HdHomeViewPrivate * priv = view->priv;

  g_debug ("Mousetrap released, input mode %d", priv->active_input);

  /*
   * If the active input is set, we resend this event, otherwise emit the
   * thumbnail-clicked signal.
   */
  hd_home_disconnect_pan_handler (priv->home);

  if (!priv->active_input)
    g_signal_emit (view, signals[SIGNAL_THUMBNAIL_CLICKED], 0, event);

  return TRUE;
}

static void
hd_home_view_constructed (GObject *object)
{
  ClutterActor        *rect;
  ClutterColor         clr = {0xff, 0, 0, 0xff};
  HdHomeViewPrivate   *priv = HD_HOME_VIEW (object)->priv;
  MBWindowManager     *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;

  /*
   * TODO -- for now, just add a rectangle, so we can see
   * where the home view is. Later we will need to be able to use
   * a texture.
   */

  rect = clutter_rectangle_new_with_color (&clr);

  clutter_actor_set_size (rect, priv->xwidth, priv->xheight);
  clutter_actor_show (rect);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_home_view_background_clicked),
		    object);

  priv->background = rect;

  clr.alpha = 0;
  rect = clutter_rectangle_new_with_color (&clr);

  clutter_actor_set_size (rect, priv->xwidth, priv->xheight);
  clutter_actor_hide (rect);
  clutter_actor_set_reactive (rect, TRUE);

  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_home_view_mouse_trap_release),
		    object);

  g_signal_connect (rect, "button-press-event",
		    G_CALLBACK (hd_home_view_mouse_trap_press),
		    object);

  priv->mouse_trap = rect;
}

static void
hd_home_view_init (HdHomeView *self)
{
  self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, HD_TYPE_HOME_VIEW, HdHomeViewPrivate);
}

static void
hd_home_view_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_home_view_parent_class)->dispose (object);
}

static void
hd_home_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_home_view_parent_class)->finalize (object);
}

static void
hd_home_view_set_property (GObject       *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  HdHomeViewPrivate *priv = HD_HOME_VIEW (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    case PROP_HOME:
      priv->home = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_home_view_get_property (GObject      *object,
			   guint         prop_id,
			   GValue       *value,
			   GParamSpec   *pspec)
{
  HdHomeViewPrivate *priv = HD_HOME_VIEW (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      g_value_set_pointer (value, priv->comp_mgr);
      break;
    case PROP_HOME:
      g_value_set_pointer (value, priv->home);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
hd_home_view_set_background_color (HdHomeView *view, ClutterColor *color)
{
  HdHomeViewPrivate *priv = view->priv;

  if (priv->background && CLUTTER_IS_RECTANGLE (priv->background))
    {
      clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->background), color);
    }
  else
    {
      ClutterActor * background;

      background = clutter_rectangle_new_with_color (color);
      clutter_actor_set_size (background, priv->xwidth, priv->xheight);
      clutter_container_add_actor (CLUTTER_CONTAINER (view), background);

      if (priv->background)
	clutter_actor_destroy (priv->background);

      priv->background = background;
    }
}

void
hd_home_view_set_background_image (HdHomeView *view, const gchar * path)
{
  HdHomeViewPrivate *priv = view->priv;
  ClutterActor      *background;
  GError            *error = NULL;

  background = clutter_texture_new_from_file (path, &error);

  if (error)
    {
      g_warning ("Could not load image %s: %s.",
		 path, error->message);

      g_error_free (error);
      return;
    }

  clutter_actor_set_size (background, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (view), background);

  if (priv->background)
    clutter_actor_destroy (priv->background);

  priv->background = background;
}

/*
 * The thumbnail mode is one in which the view acts as a single actor in
 * which button events are intercepted globally.
 */
void
hd_home_view_set_thumbnail_mode (HdHomeView * view, gboolean on)
{
  HdHomeViewPrivate *priv = view->priv;

  if (priv->thumbnail_mode && !on)
    {
      priv->thumbnail_mode = FALSE;

      clutter_actor_hide (priv->mouse_trap);
    }
  else if (!priv->thumbnail_mode && on)
    {
      priv->thumbnail_mode = TRUE;

      clutter_actor_show (priv->mouse_trap);
      clutter_actor_raise_top (priv->mouse_trap);
    }
}

/*
 * Active input mode is similar to the thumbnail mode (i.e., all events are
 * intercepted globally, but in addition, events get forwarded to
 * children if appropriate.
 */
void
hd_home_view_set_input_mode (HdHomeView * view, gboolean active)
{
  HdHomeViewPrivate *priv = view->priv;

  priv->active_input = active;
  hd_home_view_set_thumbnail_mode (view, active);
}
