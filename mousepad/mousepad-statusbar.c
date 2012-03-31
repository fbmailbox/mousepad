/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-util.h>



static gboolean mousepad_statusbar_overwrite_clicked (GtkWidget         *widget,
                                                      GdkEventButton    *event,
                                                      MousepadStatusbar *statusbar);

static gboolean mousepad_statusbar_filetype_clicked  (GtkWidget         *widget,
                                                      GdkEventButton    *event,
                                                      MousepadStatusbar *statusbar);



enum
{
  ENABLE_OVERWRITE,
  POPULATE_FILETYPE_POPUP,
  LAST_SIGNAL,
};

struct _MousepadStatusbarClass
{
  GtkStatusbarClass __parent__;
};

struct _MousepadStatusbar
{
  GtkStatusbar        __parent__;

  /* whether overwrite is enabled */
  guint               overwrite_enabled : 1;

  /* extra labels in the statusbar */
  GtkWidget          *language;
  GtkWidget          *position;
  GtkWidget          *overwrite;
};



static guint statusbar_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadStatusbar, mousepad_statusbar, GTK_TYPE_STATUSBAR);



GtkWidget *
mousepad_statusbar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_STATUSBAR, NULL);
}



static void
mousepad_statusbar_class_init (MousepadStatusbarClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  statusbar_signals[ENABLE_OVERWRITE] =
    g_signal_new (I_("enable-overwrite"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  statusbar_signals[POPULATE_FILETYPE_POPUP] =
    g_signal_new (I_("populate-filetype-popup"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_MENU);
}



static void
mousepad_statusbar_init (MousepadStatusbar *statusbar)
{
  GtkWidget    *ebox, *box, *separator, *label;
  GtkStatusbar *bar = GTK_STATUSBAR (statusbar);
  GList *frame;

  /* init statusbar */
  gtk_statusbar_set_has_resize_grip (bar, TRUE);

  /* create a new horizontal box */
  box = gtk_hbox_new (FALSE, 8);
  gtk_widget_show (box);

  /* reorder the gtk statusbar */
  frame = gtk_container_get_children (GTK_CONTAINER (bar));
  gtk_frame_set_shadow_type (GTK_FRAME (frame->data), GTK_SHADOW_NONE);
  label = gtk_bin_get_child (GTK_BIN (frame->data));
  g_object_ref (label);
  gtk_container_remove (GTK_CONTAINER (frame->data), label);
  gtk_container_add (GTK_CONTAINER (frame->data), box);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  g_object_unref (label);
  g_list_free (frame);

  /* separator */
  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  /* language/filetype event box */
  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, TRUE, 0);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  mousepad_widget_set_tooltip_text (ebox, _("Choose a filetype"));
  g_signal_connect (G_OBJECT (ebox), "button-press-event", G_CALLBACK (mousepad_statusbar_filetype_clicked), statusbar);
  gtk_widget_show (ebox);

  /* language/filetype */
  statusbar->language = gtk_label_new (_("Filetype: None"));
  gtk_container_add (GTK_CONTAINER (ebox), statusbar->language);
  gtk_widget_show (statusbar->language);

  /* separator */
  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  /* line and column numbers */
  statusbar->position = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), statusbar->position, FALSE, TRUE, 0);
  gtk_widget_show (statusbar->position);

  /* separator */
  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  /* overwrite event box */
  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, TRUE, 0);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  mousepad_widget_set_tooltip_text (ebox, _("Toggle the overwrite mode"));
  g_signal_connect (G_OBJECT (ebox), "button-press-event", G_CALLBACK (mousepad_statusbar_overwrite_clicked), statusbar);
  gtk_widget_show (ebox);

  /* overwrite label */
  statusbar->overwrite = gtk_label_new (_("OVR"));
  gtk_container_add (GTK_CONTAINER (ebox), statusbar->overwrite);
  gtk_widget_show (statusbar->overwrite);
}



static gboolean
mousepad_statusbar_overwrite_clicked (GtkWidget         *widget,
                                      GdkEventButton    *event,
                                      MousepadStatusbar *statusbar)
{
  mousepad_return_val_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar), FALSE);

  /* only respond on the left button click */
  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  /* swap the overwrite mode */
  statusbar->overwrite_enabled = !statusbar->overwrite_enabled;

  /* send the signal */
  g_signal_emit (G_OBJECT (statusbar), statusbar_signals[ENABLE_OVERWRITE], 0, statusbar->overwrite_enabled);

  return TRUE;
}



static gboolean
mousepad_statusbar_filetype_clicked (GtkWidget         *widget,
                                     GdkEventButton    *event,
                                     MousepadStatusbar *statusbar)
{
  GtkMenu *menu;
  GList   *children;
  gint     n_children = 0;

  mousepad_return_val_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar), FALSE);

  /* only respond on the left button click */
  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  /* create the popup menu */
  menu = GTK_MENU (gtk_menu_new ());

  /* send the signal to fill the menu */
  g_signal_emit (G_OBJECT (statusbar), statusbar_signals[POPULATE_FILETYPE_POPUP], 0, menu);

  /* get the number of items in the menu */
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  n_children = g_list_length (children);
  g_list_free (children);

  /* make sure there's at least one item in the menu */
  if (n_children)
    {
      /* cleanup the menu once a selection is made or the menu is cancelled */
      g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

      /* show the menu */
      gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
    }
  else
    {
      /* since the menu wasn't shown, just destroy it straight-away */
      gtk_widget_destroy (GTK_WIDGET (menu));
    }

  return TRUE;
}



void
mousepad_statusbar_set_language (MousepadStatusbar *statusbar,
                                 GtkSourceLanguage *language)
{
  gchar *label;

  mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  if (language == NULL)
      gtk_label_set_text (GTK_LABEL (statusbar->language), _("Filetype: None"));
  else
    {
      label = g_strdup_printf (_("Filetype: %s"), gtk_source_language_get_name (language));
      gtk_label_set_text (GTK_LABEL (statusbar->language), label);
      g_free (label);
    }
}



void
mousepad_statusbar_set_cursor_position (MousepadStatusbar *statusbar,
                                        gint               line,
                                        gint               column,
                                        gint               selection)
{
  gchar string[64];

  mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  /* create printable string */
  if (G_UNLIKELY (selection > 0))
    g_snprintf (string, sizeof (string), _("Line: %d Column: %d Selection: %d"), line, column, selection);
  else
    g_snprintf (string, sizeof (string), _("Line: %d Column: %d"), line, column);

  /* set label */
  gtk_label_set_text (GTK_LABEL (statusbar->position), string);
}



void
mousepad_statusbar_set_overwrite (MousepadStatusbar *statusbar,
                                  gboolean           overwrite)
{
  mousepad_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  gtk_widget_set_sensitive (statusbar->overwrite, overwrite);

  statusbar->overwrite_enabled = overwrite;
}
