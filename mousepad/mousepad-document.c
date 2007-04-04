/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 *
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-types.h>
#include <mousepad/mousepad-exo.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-window.h>



#define DEFAULT_SEARCH_FLAGS  (GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY)



static void mousepad_document_class_init              (MousepadDocumentClass  *klass);
static void mousepad_document_init                    (MousepadDocument       *document);
static void mousepad_document_get_property            (GObject                *object,
                                                       guint                   prop_id,
                                                       GValue                 *value,
                                                       GParamSpec             *pspec);
static void mousepad_document_set_property            (GObject                *object,
                                                       guint                   prop_id,
                                                       const GValue           *value,
                                                       GParamSpec             *pspec);
static void mousepad_document_finalize                (GObject                *object);
static void mousepad_document_modified_changed        (GtkTextBuffer          *buffer,
                                                       MousepadDocument       *document);
static void mousepad_document_notify_has_selection    (GtkTextBuffer          *buffer,
                                                       GParamSpec             *pspec,
                                                       MousepadDocument       *document);
static void mousepad_document_notify_cursor_position  (GtkTextBuffer          *buffer,
                                                       GParamSpec             *pspec,
                                                       MousepadDocument       *document);
static void mousepad_document_toggle_overwrite        (GtkTextView            *textview,
                                                       GParamSpec             *pspec,
                                                       MousepadDocument       *document);
static void mousepad_document_scroll_to_visible_area  (MousepadDocument       *document);
static void mousepad_document_set_font                (MousepadDocument       *document,
                                                       const gchar            *font_name);
static void mousepad_document_tab_set_tooltip         (MousepadDocument       *document,
                                                       GParamSpec             *pspec,
                                                       GtkWidget              *ebox);
static void mousepad_document_tab_button_clicked      (GtkWidget              *widget,
                                                       MousepadDocument       *document);



enum
{
  PROP_0,
  PROP_FILENAME,
  PROP_FONT_NAME,
  PROP_TITLE,
};

enum
{
  CLOSE_TAB,
  SELECTION_CHANGED,
  MODIFIED_CHANGED,
  CURSOR_CHANGED,
  OVERWRITE_CHANGED,
  LAST_SIGNAL,
};

struct _MousepadDocumentClass
{
  GtkScrolledWindowClass __parent__;
};

struct _MousepadDocument
{
  GtkScrolledWindow  __parent__;

  /* text view */
  GtkTextView       *textview;
  GtkTextBuffer     *buffer;

  /* the highlight tag */
  GtkTextTag        *tag;

  /* absolute path of the file */
  gchar             *filename;

  /* name of the file used for the titles */
  gchar             *display_name;

  /* last document modified time */
  time_t             mtime;

  /* settings */
  guint              word_wrap : 1;
  guint              line_numbers : 1;
  guint              auto_indent : 1;
};



static GObjectClass *mousepad_document_parent_class;
static guint         document_signals[LAST_SIGNAL];



GtkWidget *
mousepad_document_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_DOCUMENT, NULL);
}



GType
mousepad_document_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (GTK_TYPE_SCROLLED_WINDOW,
                                            I_("MousepadDocument"),
                                            sizeof (MousepadDocumentClass),
                                            (GClassInitFunc) mousepad_document_class_init,
                                            sizeof (MousepadDocument),
                                            (GInstanceInitFunc) mousepad_document_init,
                                            0);
    }

  return type;
}



static void
mousepad_document_class_init (MousepadDocumentClass *klass)
{
  GObjectClass *gobject_class;

  mousepad_document_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_document_finalize;
  gobject_class->get_property = mousepad_document_get_property;
  gobject_class->set_property = mousepad_document_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        "filename",
                                                        "filename",
                                                        NULL,
                                                        MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        "font-name",
                                                        "font-name",
                                                        NULL,
                                                        MOUSEPAD_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "title",
                                                        "title",
                                                        NULL,
                                                        MOUSEPAD_PARAM_READWRITE));

  document_signals[CLOSE_TAB] =
    g_signal_new (I_("close-tab"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  document_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  document_signals[MODIFIED_CHANGED] =
    g_signal_new (I_("modified-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  document_signals[CURSOR_CHANGED] =
    g_signal_new (I_("cursor-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  document_signals[OVERWRITE_CHANGED] =
    g_signal_new (I_("overwrite-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
mousepad_document_init (MousepadDocument *document)
{
  /* initialize the variables */
  document->filename     = NULL;
  document->display_name = NULL;
  document->mtime        = 0;

  /* setup the scolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (document), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (document), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (document), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (document), NULL);

  /* create a textbuffer */
  document->buffer = gtk_text_buffer_new (NULL);

  /* create the highlight tag */
  document->tag = gtk_text_buffer_create_tag (document->buffer, NULL, "background", "#ffff78", NULL);

  /* setup the textview */
  document->textview = g_object_new (MOUSEPAD_TYPE_VIEW, "buffer", document->buffer, NULL);
  gtk_container_add (GTK_CONTAINER (document), GTK_WIDGET (document->textview));
  gtk_widget_show (GTK_WIDGET (document->textview));

  /* attach signals to the text view and buffer */
  g_signal_connect (G_OBJECT (document->buffer), "modified-changed",
                    G_CALLBACK (mousepad_document_modified_changed), document);
  g_signal_connect (G_OBJECT (document->buffer), "notify::has-selection",
                    G_CALLBACK (mousepad_document_notify_has_selection), document);
  g_signal_connect (G_OBJECT (document->buffer), "notify::cursor-position",
                    G_CALLBACK (mousepad_document_notify_cursor_position), document);
  g_signal_connect (G_OBJECT (document->textview), "notify::overwrite",
                    G_CALLBACK (mousepad_document_toggle_overwrite), document);
}



static void
mousepad_document_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (object);

  switch (prop_id)
    {
      case PROP_FILENAME:
        g_value_set_static_string (value, mousepad_document_get_filename (document));
        break;

      case PROP_TITLE:
        g_value_set_static_string (value, mousepad_document_get_title (document, FALSE));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
mousepad_document_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (object);

  switch (prop_id)
    {
      case PROP_FONT_NAME:
        mousepad_document_set_font (document, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
mousepad_document_finalize (GObject *object)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (object);

  /* cleanup */
  g_free (document->filename);
  g_free (document->display_name);

  /* release our reference from the buffer */
  g_object_unref (G_OBJECT (document->buffer));

  (*G_OBJECT_CLASS (mousepad_document_parent_class)->finalize) (object);
}



static void
mousepad_document_modified_changed (GtkTextBuffer    *buffer,
                                    MousepadDocument *document)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[MODIFIED_CHANGED], 0);
}



static void
mousepad_document_notify_has_selection (GtkTextBuffer    *buffer,
                                        GParamSpec       *pspec,
                                        MousepadDocument *document)
{
  gboolean has_selection;

  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* check if we have selected text or not */
  has_selection = mousepad_document_get_has_selection (document);

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[SELECTION_CHANGED], 0, has_selection);
}



static void
mousepad_document_notify_cursor_position (GtkTextBuffer    *buffer,
                                          GParamSpec       *pspec,
                                          MousepadDocument *document)
{
  GtkTextIter iter, start;
  guint       line, column = 0;

  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* get the current iter position */
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));

  /* get the current line numbr */
  line = gtk_text_iter_get_line (&iter) + 1;

  /* get the column */
  start = iter;
  gtk_text_iter_set_line_offset (&start, 0);

  while (!gtk_text_iter_equal (&start, &iter))
    {
      if (gtk_text_iter_get_char (&start) == '\t')
        column += (8 - (column % 8));
      else
        ++column;

      gtk_text_iter_forward_char (&start);
    }

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[CURSOR_CHANGED], 0, line, column + 1);
}



static void
mousepad_document_toggle_overwrite (GtkTextView      *textview,
                                    GParamSpec       *pspec,
                                    MousepadDocument *document)
{
  gboolean overwrite;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (GTK_IS_TEXT_VIEW (textview));

  /* whether overwrite is enabled */
  overwrite = gtk_text_view_get_overwrite (textview);

  /* emit the signal */
  g_signal_emit (G_OBJECT (document), document_signals[OVERWRITE_CHANGED], 0, overwrite);
}



static void
mousepad_document_scroll_to_visible_area (MousepadDocument *document)
{
	/* scroll to visible area */
  gtk_text_view_scroll_to_mark (document->textview,
                                gtk_text_buffer_get_insert (document->buffer),
                                0.25, FALSE, 0.0, 0.0);
}



gboolean
mousepad_document_reload (MousepadDocument  *document,
                          GError           **error)
{
  GtkTextIter  start, end;
  gchar       *filename;
  gboolean     succeed = FALSE;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* remove the content of the textview */
  gtk_text_buffer_get_bounds (document->buffer, &start, &end);
  gtk_text_buffer_delete (document->buffer, &start, &end);

  /* we have to copy the filename, because mousepad_screen_open_file (resets) the name */
  filename = g_strdup (document->filename);

  /* reload the document */
  succeed = mousepad_document_open_file (document, filename, error);

  /* cleanup */
  g_free (filename);

  return succeed;
}



gboolean
mousepad_document_save_file (MousepadDocument  *document,
                             const gchar       *filename,
                             GError           **error)
{
  gchar       *content;
  gchar       *converted = NULL;
  GtkTextIter  start, end;
  gsize        bytes;
  gint         new_mtime;
  gboolean     succeed = FALSE;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (filename != NULL, FALSE);

  /* get the textview content */
  gtk_text_buffer_get_bounds (document->buffer, &start, &end);
  content = gtk_text_buffer_get_slice (document->buffer, &start, &end, TRUE);

  if (G_LIKELY (content != NULL))
    {
      /* TODO: fix the file encoding here, basic utf8 for testing */
      converted = g_convert (content,
                             strlen (content),
                             "UTF-8",
                             "UTF-8",
                             NULL,
                             &bytes,
                             error);

      /* cleanup */
      g_free (content);

      if (G_LIKELY (converted != NULL))
        {
          succeed = mousepad_file_save_data (filename, converted, bytes,
                                             &new_mtime, error);

          /* cleanup */
          g_free (converted);

          /* saving work w/o problems */
          if (G_LIKELY (succeed))
            {
              /* set the new mtime */
              document->mtime = new_mtime;

              /* nothing happend */
              gtk_text_buffer_set_modified (document->buffer, FALSE);

              /* we were allowed to write */
              gtk_text_view_set_editable (document->textview, TRUE);

              /* emit the modified-changed signal */
              mousepad_document_modified_changed (NULL, document);
            }
        }
    }

  return succeed;
}



void
mousepad_document_set_filename (MousepadDocument *document,
                                const gchar      *filename)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (filename != NULL);

  /* cleanup the old names */
  g_free (document->filename);
  g_free (document->display_name);

  /* create the new names */
  document->filename = g_strdup (filename);
  document->display_name = g_filename_display_basename (filename);

  /* tell the listeners */
  g_object_notify (G_OBJECT (document), "title");
}



void
mousepad_document_set_auto_indent (MousepadDocument *document,
                                   gboolean          auto_indent)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* store the setting */
  document->auto_indent = auto_indent;

  mousepad_view_set_auto_indent (MOUSEPAD_VIEW (document->textview), auto_indent);
}



void
mousepad_document_set_line_numbers (MousepadDocument *document,
                                    gboolean          line_numbers)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* store the setting */
  document->line_numbers = line_numbers;

  mousepad_view_set_show_line_numbers (MOUSEPAD_VIEW (document->textview), line_numbers);
}



void
mousepad_document_set_word_wrap (MousepadDocument *document,
                                 gboolean          word_wrap)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* store the setting */
  document->word_wrap = word_wrap;

  /* set the wrapping mode */
  gtk_text_view_set_wrap_mode (document->textview,
                               word_wrap ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}



static void
mousepad_document_set_font (MousepadDocument *document,
                            const gchar      *font_name)
{
  PangoFontDescription *font_desc;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  if (G_LIKELY (font_name))
    {
      font_desc = pango_font_description_from_string (font_name);
      gtk_widget_modify_font (GTK_WIDGET (document->textview), font_desc);
      pango_font_description_free (font_desc);
    }
}



gboolean
mousepad_document_open_file (MousepadDocument  *document,
                             const gchar       *filename,
                             GError           **error)
{
  GtkTextIter iter;
  gboolean    succeed = FALSE;
  gboolean    readonly;
  gint        new_mtime;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _mousepad_return_val_if_fail (filename != NULL, FALSE);

  /* we're going to add the file content */
  gtk_text_buffer_begin_user_action (document->buffer);

  /* insert the file content */
  if (mousepad_file_read_to_buffer (filename,
                                    document->buffer,
                                    &new_mtime,
                                    &readonly,
                                    error))
    {
      /* set the new filename */
      mousepad_document_set_filename (document, filename);

      /* set the new mtime */
      document->mtime = new_mtime;

      /* whether the textview is editable */
      gtk_text_view_set_editable (document->textview, !readonly);

      /* move the cursors to the first place and pretend nothing happend */
      gtk_text_buffer_get_start_iter (document->buffer, &iter);
      gtk_text_buffer_place_cursor (document->buffer, &iter);
      gtk_text_buffer_set_modified (document->buffer, FALSE);
      gtk_text_view_scroll_to_iter (document->textview, &iter, 0, FALSE, 0, 0);

      /* it worked out very well */
      succeed = TRUE;
    }

  /* and we're done */
  gtk_text_buffer_end_user_action (document->buffer);

  return succeed;
}



gboolean
mousepad_document_find (MousepadDocument    *document,
                        const gchar         *string,
                        MousepadSearchFlags  flags)
{
  gboolean    found;
  gboolean    already_wrapped = FALSE;
  GtkTextIter doc_start, doc_end;
  GtkTextIter sel_start, sel_end;
  GtkTextIter match_start, match_end;
  GtkTextIter start, end;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  _mousepad_return_val_if_fail (GTK_IS_TEXT_BUFFER (document->buffer), FALSE);

  /* get the bounds */
  gtk_text_buffer_get_bounds (document->buffer, &doc_start, &doc_end);
  gtk_text_buffer_get_selection_bounds (document->buffer, &sel_start, &sel_end);

  if (flags & MOUSEPAD_SEARCH_FORWARDS)
    {
      start = sel_end;
      end   = doc_end;
    }
  else if (flags & MOUSEPAD_SEARCH_BACKWARDS)
    {
      start = sel_start;
      end   = doc_start;
    }
  else /* type-ahead */
    {
      start = sel_start;
      end   = doc_end;
    }

search:
  /* try to find the next occurence of the string */
  if (flags & MOUSEPAD_SEARCH_BACKWARDS)
    found = gtk_text_iter_backward_search (&start, string, DEFAULT_SEARCH_FLAGS, &match_start, &match_end, &end);
  else
    found = gtk_text_iter_forward_search (&start, string, DEFAULT_SEARCH_FLAGS, &match_start, &match_end, &end);

  /* select the occurence */
  if (found)
    {
      /* set the cursor in from of the matched iter */
      gtk_text_buffer_place_cursor (document->buffer, &match_start);

      /* select the match */
      gtk_text_buffer_move_mark_by_name (document->buffer, "insert", &match_end);

      /* scroll document so the cursor is visible */
      mousepad_document_scroll_to_visible_area (document);
    }
  /* wrap around */
  else if (already_wrapped == FALSE)
    {
      /* set the new start and end iter */
      if (flags & MOUSEPAD_SEARCH_BACKWARDS)
        {
          end   = start;
          start = doc_end;
        }
      else
        {
          end   = start;
          start = doc_start;
        }

      /* set we did the wrap, so we don't end up in a loop */
      already_wrapped = TRUE;

      /* search again */
      goto search;
    }

  return found;
}



void
mousepad_document_replace (MousepadDocument *document)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));


}



void
mousepad_document_highlight_all (MousepadDocument    *document,
                                 const gchar         *string,
                                 MousepadSearchFlags  flags)
{
  GtkTextIter iter;
  GtkTextIter doc_start, doc_end;
  GtkTextIter match_start, match_end;
  gboolean    found;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  _mousepad_return_if_fail (GTK_IS_TEXT_BUFFER (document->buffer));

  /* get the document bounds */
  gtk_text_buffer_get_bounds (document->buffer, &doc_start, &doc_end);

  /* remove all the highlight tags */
  gtk_text_buffer_remove_tag (document->buffer, document->tag, &doc_start, &doc_end);

  /* highlight the new string */
  if (G_LIKELY (string != NULL))
    {
      /* set the iter to the beginning of the document */
      iter = doc_start;

      /* highlight all the occurences of the strings */
      do
      {
        /* search for the next occurence of the string */
        found = gtk_text_iter_forward_search (&iter, string, DEFAULT_SEARCH_FLAGS, &match_start, &match_end, NULL);

        if (G_LIKELY (found))
          {
             /* highlight the found occurence */
             gtk_text_buffer_apply_tag (document->buffer, document->tag, &match_start, &match_end);

             /* jump to the end of the highlighted string and continue searching */
             iter = match_end;
          }
      } while (found);
    }
}



void
mousepad_document_cut_selection (MousepadDocument *document)
{
  GtkClipboard *clipboard;

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (document->textview), GDK_SELECTION_CLIPBOARD);

  /* cut the text */
  gtk_text_buffer_cut_clipboard (document->buffer, clipboard, gtk_text_view_get_editable (document->textview));

  /* make sure the cursor is in the visible area */
  mousepad_document_scroll_to_visible_area (document);
}





void
mousepad_document_copy_selection (MousepadDocument *document)
{
  GtkClipboard *clipboard;

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (document->textview), GDK_SELECTION_CLIPBOARD);

  /* copy the selected text */
  gtk_text_buffer_copy_clipboard (document->buffer, clipboard);
}



void
mousepad_document_paste_clipboard (MousepadDocument *document)
{
  GtkClipboard *clipboard;

  /* get the clipboard */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (document->textview), GDK_SELECTION_CLIPBOARD);

  /* paste the clipboard content */
  gtk_text_buffer_paste_clipboard (document->buffer, clipboard, NULL, gtk_text_view_get_editable (document->textview));

  /* make sure the cursor is in the visible area */
  mousepad_document_scroll_to_visible_area (document);
}



void
mousepad_document_delete_selection (MousepadDocument *document)
{
  /* delete the selected text */
  gtk_text_buffer_delete_selection (document->buffer, TRUE, gtk_text_view_get_editable (document->textview));

  /* make sure the cursor is in the visible area */
  mousepad_document_scroll_to_visible_area (document);
}



void
mousepad_document_select_all (MousepadDocument *document)
{
	GtkTextIter start, end;

	/* get the start and end iter */
  gtk_text_buffer_get_bounds (document->buffer, &start, &end);

  /* select everything between those iters */
  gtk_text_buffer_select_range (document->buffer, &start, &end);
}



void
mousepad_document_focus_textview (MousepadDocument *document)
{
	_mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

	/* focus the textview */
	gtk_widget_grab_focus (GTK_WIDGET (document->textview));
}



void
mousepad_document_jump_to_line (MousepadDocument *document,
                                gint              line_number)
{
	GtkTextIter iter;

	_mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

	/* move the cursor */
	gtk_text_buffer_get_iter_at_line (document->buffer, &iter, line_number-1);
  gtk_text_buffer_place_cursor (document->buffer, &iter);

  /* make sure the cursor is in the visible area */
  mousepad_document_scroll_to_visible_area (document);
}



void
mousepad_document_send_statusbar_signals (MousepadDocument *document)
{
  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* re-send the cursor changed signal */
  mousepad_document_notify_cursor_position (document->buffer, NULL, document);

  /* re-send the overwrite signal */
  mousepad_document_toggle_overwrite (document->textview, NULL, document);
}



void
mousepad_document_line_numbers (MousepadDocument *document,
                                gint             *current_line,
                                gint             *last_line)
{
  GtkTextIter iter;

  _mousepad_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* get the current line number */
  gtk_text_buffer_get_iter_at_mark (document->buffer, &iter, gtk_text_buffer_get_insert (document->buffer));
  *current_line = gtk_text_iter_get_line (&iter) + 1;

  /* get the last line number */
	gtk_text_buffer_get_end_iter (document->buffer, &iter);
	*last_line = gtk_text_iter_get_line (&iter) + 1;
}



gboolean
mousepad_document_get_externally_modified (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);
  _mousepad_return_val_if_fail (document->filename != NULL, FALSE);

  /* return whether the file has been externally modified */
  return mousepad_file_get_externally_modified (document->filename, document->mtime);
}



const gchar *
mousepad_document_get_filename (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  return document->filename;
}



gboolean
mousepad_document_get_has_selection (MousepadDocument *document)
{
  gboolean       has_selection;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* check if we have selected text or not */
  g_object_get (G_OBJECT (document->buffer), "has-selection", &has_selection, NULL);

  return has_selection;
}



gboolean
mousepad_document_get_modified (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* return whether the buffer has been modified */
  return gtk_text_buffer_get_modified (document->buffer);
}



gboolean
mousepad_document_get_readonly (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  return !gtk_text_view_get_editable (document->textview);
}



GtkWidget *
mousepad_document_get_tab_label (MousepadDocument *document)
{
  GtkWidget *hbox;
  GtkWidget *label, *ebox;
  GtkWidget *button, *image;

  /* create the box */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);

  /* the ebox */
  ebox = g_object_new (GTK_TYPE_EVENT_BOX, "border-width", 2, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), ebox, TRUE, TRUE, 0);
  gtk_widget_show (ebox);
  mousepad_document_tab_set_tooltip (document, NULL, ebox);
  g_signal_connect (G_OBJECT (document), "notify::title",
                    G_CALLBACK (mousepad_document_tab_set_tooltip), ebox);

  /* create the label */
  label = g_object_new (GTK_TYPE_LABEL,
                        "selectable", FALSE,
                        "xalign", 0.0, NULL);
  gtk_container_add (GTK_CONTAINER (ebox), label);
  exo_binding_new (G_OBJECT (document), "title", G_OBJECT (label), "label");
  gtk_widget_show (label);

  /* create the button */
  button = g_object_new (GTK_TYPE_BUTTON,
                         "relief", GTK_RELIEF_NONE,
                         "focus-on-click", FALSE,
                         "border-width", 0,
                         "can-default", FALSE,
                         "can-focus", FALSE, NULL);
  mousepad_gtk_set_tooltip (button, _("Close this tab"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mousepad_document_tab_button_clicked), document);
  gtk_widget_show (button);

  /* button image */
  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  return hbox;
}



static void
mousepad_document_tab_set_tooltip (MousepadDocument *document,
                                   GParamSpec       *pspec,
                                   GtkWidget        *ebox)
{
  mousepad_gtk_set_tooltip (ebox, document->filename);
}



static void
mousepad_document_tab_button_clicked (GtkWidget        *widget,
                                      MousepadDocument *document)
{
  g_signal_emit (G_OBJECT (document), document_signals[CLOSE_TAB], 0);
}



const gchar *
mousepad_document_get_title (MousepadDocument *document,
                             gboolean          show_full_path)
{
  const gchar  *title;
  static guint  untitled_counter = 0;

  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  if (G_UNLIKELY (show_full_path && document->filename))
    {
      /* return the filename */
      title = document->filename;
    }
  else
    {
      /* check if the document is still untitled, if so, fix it */
      if (G_UNLIKELY (document->display_name == NULL))
        document->display_name = g_strdup_printf ("%s %d", _("Untitled"), ++untitled_counter);

      /* return the display_name */
      title = document->display_name;
    }

  return title;
}



gboolean
mousepad_document_get_word_wrap (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  return document->word_wrap;
}



gboolean
mousepad_document_get_line_numbers (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  return document->line_numbers;
}



gboolean
mousepad_document_get_auto_indent (MousepadDocument *document)
{
  _mousepad_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  return document->auto_indent;
}