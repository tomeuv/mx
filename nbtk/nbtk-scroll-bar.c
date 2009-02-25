/* nbtk-scroll-bar.c: Scroll bar actor
 *
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>
 * Port to Nbtk by: Robert Staudinger <robsta@openedhand.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter.h>

#include "nbtk-scroll-bar.h"
#include "nbtk-button.h"
#include "nbtk-marshal.h"
#include "nbtk-stylable.h"
#include "nbtk-tile.h"
#include "nbtk-enum-types.h"
#include "nbtk-private.h"

static void nbtk_stylable_iface_init (NbtkStylableIface *iface);

G_DEFINE_TYPE_WITH_CODE (NbtkScrollBar, nbtk_scroll_bar, NBTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (NBTK_TYPE_STYLABLE,
                                                nbtk_stylable_iface_init))

#define NBTK_SCROLL_BAR_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), NBTK_TYPE_SCROLL_BAR, NbtkScrollBarPrivate))

struct _NbtkScrollBarPrivate
{
  NbtkAdjustment  *adjustment;
  guint            refresh_source;

  gulong           motion_handler;
  gulong           release_handler;
  ClutterUnit      x_origin;

  ClutterActor    *bw_stepper;
  ClutterActor    *fw_stepper;
  ClutterActor    *trough;
  ClutterActor    *handle;
};

enum
{
  PROP_0,

  PROP_ADJUSTMENT
};

static gboolean
button_press_event_cb (ClutterActor       *actor,
                       ClutterButtonEvent *event,
                       NbtkScrollBar      *bar);

static void
nbtk_scroll_bar_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  NbtkScrollBarPrivate *priv = NBTK_SCROLL_BAR (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_scroll_bar_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  NbtkScrollBar *bar = NBTK_SCROLL_BAR (gobject);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      nbtk_scroll_bar_set_adjustment (bar, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_scroll_bar_dispose (GObject *gobject)
{
  NbtkScrollBar *bar = NBTK_SCROLL_BAR (gobject);
  NbtkScrollBarPrivate *priv = bar->priv;

  if (priv->refresh_source)
    {
      g_source_remove (priv->refresh_source);
      priv->refresh_source = 0;
    }

  if (priv->adjustment)
    nbtk_scroll_bar_set_adjustment (bar, NULL);

  if (priv->handle)
    {
      g_signal_handlers_disconnect_by_func (priv->handle,
                                            G_CALLBACK (button_press_event_cb),
                                            bar);
      clutter_actor_unparent (priv->handle);
      priv->handle = NULL;
    }

  clutter_actor_unparent (priv->bw_stepper);
  priv->bw_stepper = NULL;

  clutter_actor_unparent (priv->fw_stepper);
  priv->fw_stepper = NULL;

  clutter_actor_unparent (priv->trough);
  priv->trough = NULL;

  G_OBJECT_CLASS (nbtk_scroll_bar_parent_class)->dispose (gobject);
}

static void
nbtk_scroll_bar_paint (ClutterActor *actor)
{
  NbtkScrollBarPrivate *priv = NBTK_SCROLL_BAR (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_scroll_bar_parent_class)->paint (actor);

  clutter_actor_paint (priv->bw_stepper);

  clutter_actor_paint (priv->fw_stepper);

  clutter_actor_paint (priv->trough);

  if (priv->handle && CLUTTER_ACTOR_IS_VISIBLE (priv->handle))
    clutter_actor_paint (priv->handle);
}

static void
nbtk_scroll_bar_pick (ClutterActor       *actor,
                      const ClutterColor *pick_color)
{
  NbtkScrollBarPrivate *priv = NBTK_SCROLL_BAR (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_scroll_bar_parent_class)->pick (actor, pick_color);

  clutter_actor_paint (priv->bw_stepper);

  clutter_actor_paint (priv->fw_stepper);

  clutter_actor_paint (priv->trough);

  if (priv->handle &&
      CLUTTER_ACTOR_IS_VISIBLE (priv->handle) &&
      priv->adjustment)
    clutter_actor_paint (priv->handle);
}

static void
nbtk_scroll_bar_allocate (ClutterActor          *actor,
                          const ClutterActorBox *box,
                          gboolean               absolute_origin_changed)
{
  NbtkScrollBarPrivate *priv = NBTK_SCROLL_BAR (actor)->priv;
  NbtkPadding padding;
  ClutterActorBox bw_box, fw_box, trough_box;
  ClutterUnit inner_height;

  /* Chain up */
  CLUTTER_ACTOR_CLASS (nbtk_scroll_bar_parent_class)->
    allocate (actor, box, absolute_origin_changed);

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  inner_height = clutter_actor_get_heightu (actor) - padding.top - padding.bottom;

  /* Backward stepper */
  bw_box.x1 = padding.left;
  bw_box.y1 = padding.top;
  bw_box.x2 = bw_box.x1 + inner_height;
  bw_box.y2 = bw_box.y1 + inner_height;
  clutter_actor_allocate (priv->bw_stepper, &bw_box, absolute_origin_changed);

  /* Forward stepper */
  fw_box.x1 = clutter_actor_get_widthu (actor) - padding.right - inner_height;
  fw_box.y1 = padding.top;
  fw_box.x2 = fw_box.x1 + inner_height;
  fw_box.y2 = fw_box.y1 + inner_height;
  clutter_actor_allocate (priv->fw_stepper, &fw_box, absolute_origin_changed);

  /* Trough. */
  trough_box.x1 = bw_box.x2;
  trough_box.y1 = padding.top;
  trough_box.x2 = fw_box.x1;
  trough_box.y2 = trough_box.y1 + inner_height;
  clutter_actor_allocate (priv->trough, &trough_box, absolute_origin_changed);

  if (priv->adjustment)
    {
      ClutterUnit real_width, real_height, min_sizeu, max_sizeu;
      ClutterFixed lower, upper, page_size, size, increment;
      ClutterActorBox handle_box;
      guint min_size, max_size;

      nbtk_adjustment_get_valuesx (priv->adjustment,
                                   NULL,
                                   &lower,
                                   &upper,
                                   NULL,
                                   NULL,
                                   &page_size);

      real_width = trough_box.x2 - trough_box.x1;
      real_height = trough_box.y2 - trough_box.y1;

      if (upper == lower)
        increment = CFX_ONE;
      else
        increment = clutter_qdivx (page_size, upper - lower);

      size = clutter_qmulx (CLUTTER_UNITS_TO_FIXED (real_width), increment);
      if (size > real_width) size = real_width;

      nbtk_stylable_get (NBTK_STYLABLE (actor),
                         "min-size", &min_size,
                         "max-size", &max_size,
                         NULL);
      min_sizeu = CLUTTER_UNITS_FROM_INT (min_size);
      max_sizeu = CLUTTER_UNITS_FROM_INT (max_size);

      clutter_actor_get_positionu (priv->handle, &handle_box.x1, &handle_box.y1);

      /* Get initial position right.
       * Need to account for the fact that the handle is only a "clutter"-child
       * of the trough, not an "nbtk" one. */
      if (handle_box.x1 <= 0.)
        handle_box.x1 = trough_box.x1;

      handle_box.y1 = clutter_actor_get_yu (priv->trough);

      handle_box.x2 = handle_box.x1 +
                      MIN (max_sizeu,
                           MAX (min_sizeu,
                                CLUTTER_UNITS_FROM_FIXED (size)));
      handle_box.y2 = handle_box.y1 + real_height;

      clutter_actor_allocate (priv->handle,
                              &handle_box,
                              absolute_origin_changed);
    }
}

static void
on_style_change (NbtkStyle     *style,
                 NbtkScrollBar *bar)
{
  NbtkScrollBarPrivate *priv = bar->priv;
  ClutterColor *color = NULL;

  if (CLUTTER_IS_RECTANGLE (priv->handle))
    {
      nbtk_stylable_get (NBTK_STYLABLE (bar), "color", &color, NULL);
      if (color)
        {
          clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->handle), color);
          clutter_color_free (color);
        }
    }
}

static void
bar_reactive_notify_cb (GObject *gobject,
                        GParamSpec *arg1,
                        gpointer user_data)
{
  NbtkScrollBar *bar = NBTK_SCROLL_BAR (gobject);

  clutter_actor_set_reactive (bar->priv->handle,
                              clutter_actor_get_reactive (CLUTTER_ACTOR (bar)));
}

static GObject*
nbtk_scroll_bar_constructor (GType                  type,
                             guint                  n_properties,
                             GObjectConstructParam *properties)
{
  GObjectClass         *gobject_class;
  GObject              *obj;
  NbtkScrollBar        *bar;
  NbtkScrollBarPrivate *priv;

  gobject_class = G_OBJECT_CLASS (nbtk_scroll_bar_parent_class);
  obj = gobject_class->constructor (type, n_properties, properties);

  bar  = NBTK_SCROLL_BAR (obj);
  priv = NBTK_SCROLL_BAR_GET_PRIVATE (bar);

  g_signal_connect (bar, "notify::reactive",
                    G_CALLBACK (bar_reactive_notify_cb), NULL);
  g_signal_connect (nbtk_stylable_get_style (NBTK_STYLABLE (bar)),
                    "changed", G_CALLBACK (on_style_change),
                    bar);

  /* Manually call on_style_change */
  on_style_change (NULL, bar);

  return obj;
}

static void
nbtk_scroll_bar_class_init (NbtkScrollBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (NbtkScrollBarPrivate));

  object_class->get_property = nbtk_scroll_bar_get_property;
  object_class->set_property = nbtk_scroll_bar_set_property;
  object_class->dispose      = nbtk_scroll_bar_dispose;
  object_class->constructor  = nbtk_scroll_bar_constructor;

  actor_class->allocate       = nbtk_scroll_bar_allocate;
  actor_class->paint          = nbtk_scroll_bar_paint;
  actor_class->pick           = nbtk_scroll_bar_pick;

  g_object_class_install_property
           (object_class,
            PROP_ADJUSTMENT,
            g_param_spec_object ("adjustment",
                                 "Adjustment",
                                 "The adjustment",
                                 NBTK_TYPE_ADJUSTMENT,
                                 NBTK_PARAM_READWRITE));
}

static void
nbtk_stylable_iface_init (NbtkStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {
      GParamSpec *pspec;

      pspec = g_param_spec_uint ("min-size",
                                 "Minimum grabber size",
                                 "Minimum size of the scroll grabber, in px",
                                 0, G_MAXUINT, 32,
                                 G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface,
                                            NBTK_TYPE_SCROLL_BAR, pspec);

      pspec = g_param_spec_uint ("max-size",
                                 "Maximum grabber size",
                                 "Maximum size of the scroll grabber, in px",
                                 0, G_MAXINT16, G_MAXINT16,
                                 G_PARAM_READWRITE);
      nbtk_stylable_iface_install_property (iface,
                                            NBTK_TYPE_SCROLL_BAR, pspec);
    }
}

static void
move_slider (NbtkScrollBar *bar, gint x, gint y, gboolean interpolate)
{
  ClutterFixed position, lower, upper, page_size;
  ClutterUnit ux, width;

  NbtkScrollBarPrivate *priv = bar->priv;

  if (!priv->adjustment)
    return;

  if (!clutter_actor_transform_stage_point (priv->trough,
                                            CLUTTER_UNITS_FROM_DEVICE(x),
                                            CLUTTER_UNITS_FROM_DEVICE(y),
                                            &ux, NULL))
    return;

  width = clutter_actor_get_widthu (priv->trough) -
          clutter_actor_get_widthu (priv->handle);

  if (width == 0)
    return;

  ux -= priv->x_origin;
  ux = CLAMP (ux, 0, width);

  nbtk_adjustment_get_valuesx (priv->adjustment,
                               NULL,
                               &lower,
                               &upper,
                               NULL,
                               NULL,
                               &page_size);

  position =
    clutter_qmulx (clutter_qdivx (CLUTTER_UNITS_TO_FIXED (ux),
                                  CLUTTER_UNITS_TO_FIXED (width)),
                   upper - lower - page_size) + lower;

  if (interpolate)
    {
      guint mfreq = clutter_get_motion_events_frequency ();
      guint fps = clutter_get_default_frame_rate ();
      guint n_frames = fps / mfreq;

      nbtk_adjustment_interpolatex (priv->adjustment,
                                    position,
                                    n_frames,
                                    fps);
      return;
    }

  nbtk_adjustment_set_valuex (priv->adjustment, position);
}

static gboolean
motion_event_cb (ClutterActor *trough,
                 ClutterMotionEvent *event,
                 NbtkScrollBar *bar)
{
  move_slider (bar, event->x, event->y, FALSE);

  return TRUE;
}

static gboolean
button_release_event_cb (ClutterActor *trough,
                         ClutterButtonEvent *event,
                         NbtkScrollBar *bar)
{
  if (event->button != 1)
    return FALSE;

  if (bar->priv->motion_handler)
    {
      g_signal_handler_disconnect (bar->priv->trough, bar->priv->motion_handler);
      bar->priv->motion_handler = 0;
    }

  if (bar->priv->release_handler)
    {
      g_signal_handler_disconnect (bar->priv->trough, bar->priv->release_handler);
      bar->priv->release_handler = 0;
    }

  move_slider (bar, event->x, event->y, FALSE);

  clutter_ungrab_pointer ();

  return TRUE;
}

static gboolean
button_press_event_cb (ClutterActor       *actor,
                       ClutterButtonEvent *event,
                       NbtkScrollBar      *bar)
{
  NbtkScrollBarPrivate *priv = bar->priv;

  if (event->button != 1)
    return FALSE;

  if (!clutter_actor_transform_stage_point (priv->trough,
                                            CLUTTER_UNITS_FROM_DEVICE(event->x),
                                            CLUTTER_UNITS_FROM_DEVICE(event->y),
                                            &priv->x_origin, NULL))
    return FALSE;

  priv->motion_handler = g_signal_connect_after (priv->trough,
                                                 "motion-event",
                                                 G_CALLBACK (motion_event_cb),
                                                 bar);
  priv->release_handler = g_signal_connect_after (priv->trough,
                                                  "button-release-event",
                                                  G_CALLBACK (button_release_event_cb),
                                                  bar);

  clutter_grab_pointer (priv->trough);

  return TRUE;
}

static void
nbtk_scroll_bar_init (NbtkScrollBar *self)
{
  self->priv = NBTK_SCROLL_BAR_GET_PRIVATE (self);

  self->priv->bw_stepper = (ClutterActor *) nbtk_button_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (self->priv->bw_stepper), "backward-stepper");
  clutter_actor_set_parent (CLUTTER_ACTOR (self->priv->bw_stepper),
                            CLUTTER_ACTOR (self));
  /* TODO hook up "clicked" */

  self->priv->fw_stepper = (ClutterActor *) nbtk_button_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (self->priv->fw_stepper), "forward-stepper");
  clutter_actor_set_parent (CLUTTER_ACTOR (self->priv->fw_stepper),
                            CLUTTER_ACTOR (self));
  /* TODO hook up "clicked" */

  self->priv->trough = (ClutterActor *) nbtk_tile_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (self->priv->trough), "trough");
  clutter_actor_set_parent (CLUTTER_ACTOR (self->priv->trough),
                            CLUTTER_ACTOR (self));
  /* TODO hook up "clicked" */

  self->priv->handle = (ClutterActor *) nbtk_tile_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (self->priv->handle), "handle");
  clutter_actor_set_parent (CLUTTER_ACTOR (self->priv->handle),
                            self->priv->trough);
  g_signal_connect (self->priv->handle, "button-press-event",
                    G_CALLBACK (button_press_event_cb), self);
}

ClutterActor *
nbtk_scroll_bar_new (NbtkAdjustment *adjustment)
{
  return g_object_new (NBTK_TYPE_SCROLL_BAR,
                       "adjustment", adjustment,
                       NULL);
}

static gboolean
nbtk_scroll_bar_refresh (NbtkScrollBar *bar)
{
  ClutterActor *actor = CLUTTER_ACTOR (bar);
  NbtkScrollBarPrivate *priv = bar->priv;
  ClutterUnit width, button_width, handle_offset_x, handle_offset_y;
  ClutterFixed lower, upper, value, page_size;
  ClutterFixed x, position;

  /* Work out scroll handle size */
  nbtk_adjustment_get_valuesx (priv->adjustment,
                               &value,
                               &lower,
                               &upper,
                               NULL,
                               NULL,
                               &page_size);

  /* Need to account for the fact that the handle is only a "clutter"-child
   * of the trough, not an "nbtk" one. */
  handle_offset_x = clutter_actor_get_xu (priv->trough);
  handle_offset_y = clutter_actor_get_yu (priv->trough);

  if (upper - page_size <= lower)
    {
      clutter_actor_set_position (CLUTTER_ACTOR (priv->handle),
                                  handle_offset_x,
                                  handle_offset_y);
      priv->refresh_source = 0;
      return FALSE;
    }

  width = clutter_actor_get_widthu (priv->trough);
  button_width = clutter_actor_get_widthu (priv->handle);

  position = clutter_qdivx (value - lower, upper - lower - page_size);

  /* Set padding on trough */
  x = clutter_qmulx (position, CLUTTER_UNITS_TO_FIXED (width - button_width));
  clutter_actor_set_positionu (CLUTTER_ACTOR (priv->handle),
                               CLUTTER_UNITS_FROM_FIXED (x) + handle_offset_x,
                               handle_offset_y);

  clutter_actor_queue_redraw (actor);

  priv->refresh_source = 0;
  return FALSE;
}

static void
adjustment_changed_cb (NbtkScrollBar *bar)
{
  NbtkScrollBarPrivate *priv = bar->priv;

  if (!priv->refresh_source)
    priv->refresh_source = g_idle_add_full (CLUTTER_PRIORITY_EVENTS,
                                            (GSourceFunc)
                                            nbtk_scroll_bar_refresh,
                                            bar,
                                            NULL);
}

void
nbtk_scroll_bar_set_adjustment (NbtkScrollBar *bar,
                                NbtkAdjustment *adjustment)
{
  NbtkScrollBarPrivate *priv;

  g_return_if_fail (NBTK_IS_SCROLL_BAR (bar));

  priv = bar->priv;
  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            clutter_actor_queue_relayout,
                                            bar);
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            adjustment_changed_cb,
                                            bar);
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }

  if (adjustment)
    {
      priv->adjustment = g_object_ref (adjustment);

      g_signal_connect_swapped (priv->adjustment, "notify::value",
                                G_CALLBACK (adjustment_changed_cb),
                                bar);
      g_signal_connect_swapped (priv->adjustment, "changed",
				G_CALLBACK (clutter_actor_queue_relayout),
				bar);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (bar));
    }
}

NbtkAdjustment *
nbtk_scroll_bar_get_adjustment (NbtkScrollBar *bar)
{
  g_return_val_if_fail (NBTK_IS_SCROLL_BAR (bar), NULL);

  return bar->priv->adjustment;
}
