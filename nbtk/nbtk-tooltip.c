/* nbtk-tooltip.c: Plain tooltip actor
 *
 * Copyright 2008 Intel Corporation
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
 * Written by: Thomas Wood <thomas@linux.intel.com>
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>
#include <clutter/clutter-container.h>

#include "nbtk-tooltip.h"

#include "nbtk-widget.h"

enum
{
  PROP_0,

  PROP_LABEL
};

#define NBTK_TOOLTIP_GET_PRIVATE(obj)    \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NBTK_TYPE_TOOLTIP, NbtkTooltipPrivate))

struct _NbtkTooltipPrivate
{
  ClutterActor *label;
  ClutterActor *widget;
};

G_DEFINE_TYPE (NbtkTooltip, nbtk_tooltip, NBTK_TYPE_WIDGET)

static void
nbtk_tooltip_set_property (GObject      *gobject,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  NbtkTooltip *tooltip = NBTK_TOOLTIP (gobject);

  switch (prop_id)
    {
    case PROP_LABEL:
      nbtk_tooltip_set_label (tooltip, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_tooltip_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  NbtkTooltipPrivate *priv = NBTK_TOOLTIP (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, clutter_label_get_text (CLUTTER_LABEL (priv->label)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_tooltip_class_init (NbtkTooltipClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (NbtkTooltipPrivate));

  gobject_class->set_property = nbtk_tooltip_set_property;
  gobject_class->get_property = nbtk_tooltip_get_property;

  pspec = g_param_spec_string ("label",
                               "Label",
                               "Label of the tooltip",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LABEL, pspec);

}

static void
nbtk_tooltip_init (NbtkTooltip *tooltip)
{
  tooltip->priv = NBTK_TOOLTIP_GET_PRIVATE (tooltip);

  tooltip->priv->label = g_object_new (CLUTTER_TYPE_LABEL,
                                       "alignment", PANGO_ALIGN_CENTER,
                                       "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                                       "use-markup", TRUE,
                                       "wrap", FALSE,
                                       NULL);

  clutter_container_add (CLUTTER_CONTAINER (tooltip), tooltip->priv->label, NULL);
}


static void
nbtk_tooltip_weak_ref_notify (gpointer tooltip, GObject *obj)
{
  g_object_ref_sink (G_OBJECT (tooltip));
  g_object_unref (G_OBJECT (tooltip));
}

/**
 * nbtk_tooltip_new:
 * @widget: actor the tooltip is attached to
 * @text: text to set the label to
 *
 * Create a new #NbtkTooltip with the specified label
 *
 * Returns: a new #NbtkTooltip
 */
NbtkWidget *
nbtk_tooltip_new (NbtkWidget *widget, const gchar *text)
{
  NbtkTooltip  *tooltip;
  NbtkPadding   padding = {
      CLUTTER_UNITS_FROM_INT (5),
      CLUTTER_UNITS_FROM_INT (10),
      CLUTTER_UNITS_FROM_INT (5),
      CLUTTER_UNITS_FROM_INT (10)
  };

  /* add the tooltip to the stage, but don't allow it to be visible */
  tooltip = g_object_new (NBTK_TYPE_TOOLTIP,
                          "label", text,
                          "show-on-set-parent", FALSE,
                          "padding", &padding,
                          NULL);

  /* remember the associated widget */
  tooltip->priv->widget = CLUTTER_ACTOR (widget);

  g_object_weak_ref (G_OBJECT (widget), nbtk_tooltip_weak_ref_notify, tooltip);

  return NBTK_WIDGET (tooltip);
}

/**
 * nbtk_tooltip_get_label:
 * @tooltip: a #NbtkTooltip
 *
 * Get the text displayed on the tooltip
 *
 * Returns: the text for the tooltip. This must not be freed by the application
 */
G_CONST_RETURN gchar *
nbtk_tooltip_get_label (NbtkTooltip *tooltip)
{
  g_return_val_if_fail (NBTK_IS_TOOLTIP (tooltip), NULL);

  return clutter_label_get_text (CLUTTER_LABEL (tooltip->priv->label));
}

/**
 * nbtk_tooltip_set_label:
 * @tooltip: a #NbtkTooltip
 * @text: text to set the label to
 *
 * Sets the text displayed on the tooltip
 */
void
nbtk_tooltip_set_label (NbtkTooltip *tooltip,
                        const gchar *text)
{
  NbtkTooltipPrivate *priv;

  g_return_if_fail (NBTK_IS_TOOLTIP (tooltip));

  priv = tooltip->priv;

  clutter_label_set_text (CLUTTER_LABEL (priv->label), text);

  g_object_notify (G_OBJECT (tooltip), "label");
}

/**
 * nbtk_tooltip_show:
 * @tooltip: a #NbtkTooltip
 *
 * Show the tooltip relative to the associated widget.
 */
void
nbtk_tooltip_show (NbtkTooltip *tooltip)
{
  ClutterActor *parent;
  ClutterActor *stage;
  gint x, y;
  guint w, h;

  g_return_if_fail (NBTK_TOOLTIP (tooltip));

  parent = clutter_actor_get_parent (CLUTTER_ACTOR (tooltip));
  stage = clutter_actor_get_stage (tooltip->priv->widget);

  /* make sure we're parented on the stage */
  if (G_UNLIKELY (parent != stage))
    {
      if (parent)
        {
          g_warning ("NbtkTooltip must be parented directly on the stage");
        }
      clutter_actor_reparent (CLUTTER_ACTOR (tooltip), stage);
    }

  /* raise the tooltip to the top */
  clutter_container_raise_child (CLUTTER_CONTAINER (stage),
                                 CLUTTER_ACTOR (tooltip),
                                 NULL);

  /* place the tooltip under the associated actor */
  clutter_actor_get_transformed_position (tooltip->priv->widget, &x, &y);
  clutter_actor_get_transformed_size (tooltip->priv->widget, &w, &h);

  clutter_actor_set_position (CLUTTER_ACTOR (tooltip),
                              (x + w / 2) - clutter_actor_get_width (CLUTTER_ACTOR (tooltip)) / 2,
                              y + h);

  /* finally show the tooltip! */
  clutter_actor_show (CLUTTER_ACTOR (tooltip));
}

/**
 * nbtk_tooltip_hide:
 * tooltip: a #NbtkTooltip
 *
 * Hide the tooltip
 */
void
nbtk_tooltip_hide (NbtkTooltip *tooltip)
{
  g_return_if_fail (NBTK_TOOLTIP (tooltip));

  clutter_actor_hide (CLUTTER_ACTOR (tooltip));
}