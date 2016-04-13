/*
 * gimmix-playlist.c
 *
 * Copyright (C) 2006-2009 Priyank Gosalia
 *
 * Gimmix is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * Gimmix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with Gimmix; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Priyank Gosalia <priyankmg@gmail.com>
 */

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "gimmix-playlist.h"
#include "gimmix-tagedit.h"

#define GIMMIX_MEDIA_ICON 	"gimmix_logo_small.png"
#define GIMMIX_PLAYLIST_ICON 	"gimmix_playlist.png"
#define LIBMPD_VER		(LIBMPD_VERSION * 10000)

typedef enum {
	COLUMN_TITLE = 0,
	COLUMN_ARTIST,
	COLUMN_ALBUM,
	COLUMN_LENGTH
} GimmixColumnType;

typedef enum {
	SONG = 1,
	DIR,
	PLAYLIST
} GimmixFileType;

enum { TARGET_STRING, TARGET_ROOTWIN };
  	GtkTargetEntry targetentries[] =
    	{
     		{ "text/plain", 0, TARGET_STRING },
      		{ "application/x-rootwindow-drop", 0, TARGET_ROOTWIN }
	};
	guint n_targets = sizeof(targetentries) / sizeof(targetentries[0]);
	
extern MpdObj		*gmo;
extern GladeXML 	*xml;
extern ConfigFile	conf;
extern GtkWidget	*main_window;


static gchar *invalid_dir_error = "You have specified an invalid music directory. Do you want to specify the correct music directory now ?";

GtkWidget 	*search_combo;
GtkWidget	*search_entry;
GtkWidget	*search_box;
	
static GtkWidget	*gimmix_statusbar;
static GtkWidget	*gimmix_statusbox;
static GtkWidget	*button_update;
static GtkWidget	*current_playlist_treeview;
static GtkWidget	*library_treeview;
GtkWidget		*playlists_treeview;
GtkTreeSelection	*current_playlist_selection;
GtkTreeSelection	*library_selection;
GtkWidget		*library_window;
GtkWidget		*current_pl_window;
GtkWidget		*pls_playlist_window;
GtkWidget		*pls_button_add;
GtkWidget		*pls_button_del;
GtkWidget		*pls_button_clear;
GtkWidget		*pls_button_add;
GtkWidget		*pls_button_save;
GtkWidget		*pls_playlists_box;

/* Tree view columns */
static GtkWidget	*cpl_tvw_title_column;
static GtkWidget	*cpl_tvw_artist_column;
static GtkWidget	*cpl_tvw_album_column;
static GtkWidget	*cpl_tvw_length_column;

extern GtkWidget	*tag_editor_window;

gchar			*loaded_playlist;

static void
on_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
	GtkSelectionData *selection_data, guint target_type, guint time, 
	gpointer data)
{
	gchar		*path;
	gboolean	dnd_success = FALSE;
	gboolean	delete_selection_data = FALSE;

	if((selection_data != NULL) && (selection_data->length >= 0))
        {
		switch (target_type)
		{
			case TARGET_STRING:
			{
				path = (gchar*) selection_data->data;
				mpd_playlist_queue_add (gmo, path);
				mpd_playlist_queue_commit (gmo);
				dnd_success = TRUE;
				break;
			}
                                        
			default:
			{
				g_print ("default");
			}
                }
        }

	if (dnd_success == FALSE)
	{
		g_print ("DnD data transfer failed!\n");
	}
        gtk_drag_finish (context, dnd_success, delete_selection_data, time);
	
	return;
}

static void
on_drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data,
	guint target_type, guint time, gpointer user_data)
{
	GtkTreeSelection	*selection = NULL;
	GtkTreeModel		*model = NULL;
	GList			*list = NULL;
	GimmixFileType		type = -1;
	gchar			*path = NULL;
	GtkTreeIter		iter;

        g_assert (selection_data != NULL);
        
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(library_treeview));
	list = gtk_tree_selection_get_selected_rows (library_selection, &model);
	while (list != NULL)
	{
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, 2, &path, 3, &type, -1);
		
		if (type == DIR || type == SONG)
		{
			switch (target_type)
			{
				case TARGET_STRING:	
				{
					gtk_selection_data_set (selection_data, selection_data-> target, 8, (guchar*) path, strlen (path));
					break;
				}
                	
				case TARGET_ROOTWIN:
				{
					g_print ("Dropped on the root window!\n");
					break;
				}
                	
				default:
				{
					g_assert_not_reached ();
				}
			}
			g_free (path);
		}
		list = g_list_next (list);
	}

	return;
}

static gboolean
on_drag_drop (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time,
	gpointer user_data)
{
	gboolean ret = TRUE;

        /* If a target is offerred */
        if (context->targets)
        {
		/* Request the data from the source. */
		gtk_drag_get_data (widget,
				context,
				TARGET_STRING,
				time);
        }
        else
        {
                ret = FALSE;
        }
        
        return  ret;
}

static void		gimmix_search_init (void);
static void		gimmix_library_search (gint, gchar *);
static void		gimmix_library_and_playlists_populate (void);
static void		gimmix_update_library_with_dir (gchar *);
static void		gimmix_current_playlist_popup_menu (void);
static void		gimmix_library_popup_menu (void);
static void		gimmix_playlists_popup_menu (void);
static gchar*		gimmix_path_get_parent_dir (gchar *);
static void		gimmix_load_playlist (gchar *);
static void		gimmix_display_total_playlist_time (MpdObj *);

/* Callbacks */
/* Current playlist callbacks */
static void		cb_current_playlist_double_click (GtkTreeView *treeview);
static void		cb_repeat_menu_toggled (GtkCheckMenuItem *item, gpointer data);
static void		cb_shuffle_menu_toggled (GtkCheckMenuItem *item, gpointer data);
static void		cb_current_playlist_delete_press (GtkWidget *widget, GdkEventKey *event, gpointer data);
static void 		cb_add_button_clicked (GtkWidget *widget, gpointer data);

static void		gimmix_current_playlist_remove_song (void);
static void		gimmix_current_playlist_crop_song (void);
static void		gimmix_current_playlist_song_info (void);
static void		gimmix_current_playlist_clear (void);
static gboolean		gimmix_update_player_status (gpointer data);

/* Library browser callbacks */
static void		cb_library_dir_activated (gpointer data);
static void		gimmix_library_song_info (void);
static void		cb_playlist_activated (GtkTreeView *);
static void		cb_library_right_click (GtkTreeView *treeview, GdkEventButton *event);
static void		cb_library_popup_add_clicked (GtkWidget *widget, gpointer data);
static void		cd_library_popup_replace_clicked (GtkWidget *widget, gpointer data);
static void		cb_search_keypress (GtkWidget *widget, GdkEventKey *event, gpointer data);


/* Playlist browser callbacks */
static void		gimmix_update_playlists_treeview (void);
static void		gimmix_playlist_save_dialog_show (void);
static void		cb_gimmix_playlist_save_response (GtkDialog *dlg, gint arg1, gpointer dialog);
static void		cb_playlists_right_click (GtkTreeView *treeview, GdkEventButton *event);
static bool		cb_all_playlist_button_press (GtkTreeView *treeview, GdkEventButton *event, gpointer data);
static void		cb_gimmix_playlist_remove ();
static void		cb_gimmix_playlist_load ();
static void		cb_playlists_delete_press (GtkWidget *widget, GdkEventKey *event, gpointer data);

static void
gimmix_playlist_search_widgets_init (void)
{
	search_box = glade_xml_get_widget (xml, "search_box");
	search_combo = glade_xml_get_widget (xml, "search_combo");
	search_entry = glade_xml_get_widget (xml, "search_entry");
	
	gtk_combo_box_set_active (GTK_COMBO_BOX(search_combo), 0);
	g_signal_connect (G_OBJECT(search_entry), "key_release_event", G_CALLBACK(cb_search_keypress), NULL);
	
	return;
}

void
gimmix_playlist_setup_current_playlist_tvw (void)
{
	GtkTreeModel		*current_playlist_model;
	GtkListStore		*current_playlist_store;
	GtkCellRenderer		*current_playlist_renderer;
	GtkTreeViewColumn	*current_playlist_column;
	
	current_playlist_treeview = glade_xml_get_widget (xml, "current_playlist_treeview");
	playlists_treeview = glade_xml_get_widget (xml, "playlists_treeview");
	library_treeview = glade_xml_get_widget (xml, "album");
	library_window = glade_xml_get_widget (xml,"library_window");
	
	current_playlist_renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT(current_playlist_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	current_playlist_column = gtk_tree_view_column_new_with_attributes (_("Title"),
										current_playlist_renderer,
										"markup", 0,
										NULL);
	gtk_tree_view_column_set_resizable (current_playlist_column, TRUE);
	gtk_tree_view_column_set_clickable (current_playlist_column, TRUE);
	//gtk_tree_view_column_set_min_width (current_playlist_column, 200);
	g_object_set (G_OBJECT(current_playlist_column), "expand", TRUE, "spacing", 4, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(current_playlist_treeview), current_playlist_column);
	cpl_tvw_title_column = (GtkWidget*)current_playlist_column;
	
	current_playlist_renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT(current_playlist_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	current_playlist_column = gtk_tree_view_column_new_with_attributes (_("Artist"),
										current_playlist_renderer,
										"markup", 3,
										NULL);
	gtk_tree_view_column_set_resizable (current_playlist_column, TRUE);
	gtk_tree_view_column_set_clickable (current_playlist_column, TRUE);
	//gtk_tree_view_column_set_min_width (current_playlist_column, 100);
	g_object_set (G_OBJECT(current_playlist_column), "expand", TRUE, "spacing", 4, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(current_playlist_treeview), current_playlist_column);
	cpl_tvw_artist_column = (GtkWidget*)current_playlist_column;
	
	current_playlist_renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT(current_playlist_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	current_playlist_column = gtk_tree_view_column_new_with_attributes (_("Album"),
										current_playlist_renderer,
										"markup", 4,
										NULL);
	gtk_tree_view_column_set_resizable (current_playlist_column, TRUE);
	gtk_tree_view_column_set_clickable (current_playlist_column, TRUE);
	//gtk_tree_view_column_set_min_width (current_playlist_column, 100);
	g_object_set (G_OBJECT(current_playlist_column), "expand", TRUE, "spacing", 4, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(current_playlist_treeview), current_playlist_column);
	cpl_tvw_album_column = (GtkWidget*)current_playlist_column;
							
	current_playlist_renderer = gtk_cell_renderer_text_new ();
	current_playlist_column = gtk_tree_view_column_new_with_attributes (_("Length"),
										current_playlist_renderer,
										"markup", 5,
										NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(current_playlist_treeview), current_playlist_column);
	cpl_tvw_length_column = (GtkWidget*)current_playlist_column;
	
	current_playlist_store = gtk_list_store_new (6,
						G_TYPE_STRING, 	/* name (0) */
						G_TYPE_STRING, 	/* path (1) */
						G_TYPE_INT,	/* id	(2) */
						G_TYPE_STRING,  /* artist (3) */
						G_TYPE_STRING,  /* album (4) */
						G_TYPE_STRING); /* length (5) */
	current_playlist_model	= GTK_TREE_MODEL (current_playlist_store);
	current_playlist_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(current_playlist_treeview));
	gtk_tree_selection_set_mode (current_playlist_selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (current_playlist_treeview), current_playlist_model);
	
	/* Drag and Drop */
	gtk_drag_dest_set(GTK_WIDGET(current_playlist_treeview),
				GTK_DEST_DEFAULT_ALL,
				targetentries, n_targets - 1,
				GDK_ACTION_COPY|GDK_ACTION_MOVE);
	
	gtk_drag_source_set (GTK_WIDGET(library_treeview),
				GDK_BUTTON1_MASK,
				targetentries, n_targets-1,
				GDK_ACTION_COPY);

	g_signal_connect(GTK_WIDGET(current_playlist_treeview),
			"drag_data_received",
			G_CALLBACK(on_drag_data_received),
			NULL);
	g_signal_connect (current_playlist_treeview,
			"drag-drop",
			G_CALLBACK (on_drag_drop),
			NULL);
	g_signal_connect (library_treeview,
			"drag-data-get",
			G_CALLBACK (on_drag_data_get),
			NULL);
			
	/* load default values */
	gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_title_column),
					gimmix_config_get_bool("pl_column_title_show"));
	gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_artist_column),
					gimmix_config_get_bool("pl_column_artist_show"));
	gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_album_column),
					gimmix_config_get_bool("pl_column_album_show"));
	gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_length_column),
					gimmix_config_get_bool("pl_column_length_show"));
			
	g_object_unref (current_playlist_model);
}

void
gimmix_playlist_widgets_init (void)
{
	gimmix_playlist_setup_current_playlist_tvw ();
	
	gimmix_statusbar = glade_xml_get_widget (xml, "gimmix_status");
	gimmix_statusbox = glade_xml_get_widget (xml, "gimmix_statusbox");
	
	g_signal_connect (current_playlist_treeview, "row-activated", G_CALLBACK(cb_current_playlist_double_click), NULL);
	g_signal_connect (current_playlist_treeview, "button-press-event", G_CALLBACK(cb_all_playlist_button_press), (gpointer)TRUE);
	//g_signal_connect (current_playlist_treeview, "button-release-event", G_CALLBACK (cb_current_playlist_right_click), NULL);
	g_signal_connect (current_playlist_treeview, "key_release_event", G_CALLBACK (cb_current_playlist_delete_press), NULL);
	
	g_signal_connect (playlists_treeview, "button-release-event", G_CALLBACK (cb_playlists_right_click), NULL);
	g_signal_connect (playlists_treeview, "key_release_event", G_CALLBACK (cb_playlists_delete_press), NULL);
	
	/* initialize playlist toolbar */
	/* Add button */
	pls_button_add = glade_xml_get_widget (xml, "button_add");
	g_signal_connect (G_OBJECT(pls_button_add), "clicked", G_CALLBACK(cb_add_button_clicked), (gpointer)glade_xml_get_widget(xml, "library_window"));
	
	/* Remove button */
	pls_button_del = glade_xml_get_widget (xml, "button_remove");
	g_signal_connect (G_OBJECT(pls_button_del), "clicked", G_CALLBACK(gimmix_current_playlist_remove_song), NULL);
	
	/* Clear button */
	pls_button_clear = glade_xml_get_widget (xml, "button_clear");
	g_signal_connect (G_OBJECT(pls_button_clear), "clicked", G_CALLBACK(gimmix_current_playlist_clear), NULL);
	
	/* Update button */
	button_update = glade_xml_get_widget (xml, "button_update");
	g_signal_connect (G_OBJECT(button_update), "clicked", G_CALLBACK(gimmix_library_update), NULL);
	
	/* Save button */
	pls_button_save = glade_xml_get_widget(xml, "button_save");
	g_signal_connect (G_OBJECT (pls_button_save), "clicked", G_CALLBACK(gimmix_playlist_save_dialog_show), NULL);
	
	pls_playlists_box =  glade_xml_get_widget (xml, "pls_playlist_window");
	
	/* Playlist toggle button */
	current_pl_window = glade_xml_get_widget (xml, "current_pl_window");
	pls_playlist_window = glade_xml_get_widget (xml, "pls_playlist_window");

	

	loaded_playlist = NULL;
	
	gimmix_playlist_search_widgets_init ();
}

void
gimmix_playlist_disable_controls (void)
{
	gtk_widget_set_sensitive (pls_button_add, FALSE);
	gtk_widget_set_sensitive (pls_button_del, FALSE);
	gtk_widget_set_sensitive (pls_button_clear, FALSE);
	gtk_widget_set_sensitive (button_update, FALSE);
	gtk_widget_set_sensitive (pls_button_save, FALSE);
	gtk_widget_set_sensitive (pls_playlists_box, FALSE);
	gtk_widget_set_sensitive (current_playlist_treeview, FALSE);
}

void
gimmix_playlist_enable_controls (void)
{
	gtk_widget_set_sensitive (pls_button_add, TRUE);
	gtk_widget_set_sensitive (pls_button_del, TRUE);
	gtk_widget_set_sensitive (pls_button_clear, TRUE);
	gtk_widget_set_sensitive (button_update, TRUE);
	gtk_widget_set_sensitive (pls_button_save, TRUE);
	gtk_widget_set_sensitive (pls_playlists_box, TRUE);
	gtk_widget_set_sensitive (current_playlist_treeview, TRUE);
}

static void
gimmix_search_init (void)
{
	if (!gimmix_config_get_bool("enable_search"))
		gtk_widget_hide (search_box);

	return;
}

void
gimmix_playlist_init (void)
{
	/* populate the file browser */
	gimmix_library_and_playlists_populate ();
	
	/* Initialize playlist search */
	gimmix_search_init ();

	return;
}

void
gimmix_update_current_playlist (MpdObj *mo, MpdData *pdata)
{
	GtkListStore	*current_playlist_store;
	GtkTreeIter	current_playlist_iter;
	gint 		new;
	gint		current_song_id = -1;
	MpdData		*data = pdata;

	if (data!=NULL && mpd_check_connected(mo))
	{
		new = mpd_playlist_get_playlist_id (mo);
		current_song_id = mpd_player_get_current_song_id (mo);
	}
	else
	{
		return;
	}
	current_playlist_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW(current_playlist_treeview)));
	gtk_list_store_clear (current_playlist_store);
	
	while (data != NULL)
	{
		gchar 	*title = NULL;
		gchar	*artist = NULL;
		gchar	*album = NULL;
		gchar 	*ti;
		gchar	time[15];
		
		if (data->song->id == current_song_id)
		{
			if (data->song->title != NULL)
			{
				/*
				if (data->song->artist)
				title = g_markup_printf_escaped ("<span size=\"medium\"weight=\"bold\">%s - %s</span>", data->song->artist, data->song->title);
				else
				*/
				title = g_markup_printf_escaped ("<span size=\"medium\"weight=\"bold\">%s</span>", data->song->title);
			}
			else
			{
				gchar *file = g_path_get_basename (data->song->file);
				gimmix_strip_file_ext (file);
				title = g_markup_printf_escaped ("<span size=\"medium\"weight=\"bold\">%s</span>", file);
				g_free (file);
			}
			if (data->song->artist != NULL)
			{
				artist = g_markup_printf_escaped ("<span size=\"medium\"weight=\"bold\">%s</span>", data->song->artist);
			}
			if (data->song->album != NULL)
			{
				album = g_markup_printf_escaped ("<span size=\"medium\"weight=\"bold\">%s</span>", data->song->album);
			}
			gimmix_get_total_time_for_song (mo, data->song, time);
			ti = g_markup_printf_escaped ("<span size=\"medium\" weight=\"bold\">%s</span>", time);
		}
		else
		{	
			if (data->song->title != NULL)
			{
				/*if (data->song->artist)
				title = g_markup_printf_escaped ("%s - %s", data->song->artist, data->song->title);
				else*/
				title = g_markup_printf_escaped ("%s", data->song->title);
			}
			else
			{
				title = g_markup_printf_escaped (g_path_get_basename(data->song->file));
				gimmix_strip_file_ext (title);
			}
			if (data->song->artist)
			{
				artist = g_markup_printf_escaped ("%s", data->song->artist);
			}
			if (data->song->album)
			{
				album = g_markup_printf_escaped ("%s", data->song->album);
			}
			gimmix_get_total_time_for_song (mo, data->song, time);
			ti = NULL;
		}
		
		gtk_list_store_append (current_playlist_store, &current_playlist_iter);
		gtk_list_store_set (current_playlist_store, 
							&current_playlist_iter,
							0, title,
							5, (ti!=NULL) ? ti : time,
							1, data->song->file,
							2, data->song->id,
							3, artist,
							4, album,
							-1);
		data = mpd_data_get_next (data);
		if (ti)
		g_free (ti);
		g_free (title);
		g_free (album);
		g_free (artist);
	}
	gtk_tree_view_set_model (GTK_TREE_VIEW (current_playlist_treeview), GTK_TREE_MODEL(current_playlist_store));
	gimmix_display_total_playlist_time (mo);
	/*
	if (pdata)
	{
		mpd_data_free (pdata);
	}
	*/
	return;
}

static void
gimmix_display_total_playlist_time (MpdObj *mo)
{
	gchar		*time_string;
	gint		time = 0;
	guint		len = 0;
	MpdData		*data;
	
	len = mpd_playlist_get_playlist_length (mo);
	data = mpd_playlist_get_changes (mo, 0);
	if (!len)
	{
		gtk_widget_hide (gimmix_statusbox);
		return;
	}
	
	while (data != NULL)
	{
		if (data->song)
			time += data->song->time;
		data = mpd_data_get_next (data);
	}
	
	if (time > 0)
	{
		time_string = g_strdup_printf ("%d %s, %s%d %s", len, _("Items"), _("Total Duration: "), time/60, _("minutes"));
		//g_print ("%s\n",time_string);
		gtk_label_set_text (GTK_LABEL(gimmix_statusbar), time_string);
		gtk_widget_show (gimmix_statusbox);
		g_free (time_string);
	}
	/*
	else
	{
		g_print ("called totla\n");
		gtk_widget_hide (gimmix_statusbox);
	}
	*/
	return;
}

static void
gimmix_library_and_playlists_populate (void)
{
	GtkTreeModel 		*dir_model;
	GtkTreeModel		*pls_model;
	GtkTreeIter  		dir_iter;
	GtkTreeIter			pls_iter;
	GtkCellRenderer 	*dir_renderer;
	GtkCellRenderer		*pls_renderer;
	GtkListStore 		*dir_store;
	GtkListStore		*pls_store;
	MpdData 			*data = NULL;
	GdkPixbuf 			*dir_pixbuf;
	GdkPixbuf			*song_pixbuf;
	GdkPixbuf			*pls_pixbuf;
	gchar				*path;

	library_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(library_treeview));
	gtk_tree_selection_set_mode (library_selection, GTK_SELECTION_MULTIPLE);
	dir_renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (library_treeview),
							-1,
							"Icon",
							dir_renderer,
							"pixbuf", 0,
							NULL);
	
	dir_renderer 		= gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (library_treeview),
							-1,
							"Albums",
							dir_renderer,
							"text", 1,
							NULL);
	
	pls_renderer 		= gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playlists_treeview), 
							-1,
							"PlsIcon",
							pls_renderer,
							"pixbuf", 0,
							NULL);
							
	pls_renderer 		= gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playlists_treeview), 
							-1,
							"Playlists",
							pls_renderer,
							"text", 1,
							NULL);
	
	dir_store 	= gtk_list_store_new (5, 
						GDK_TYPE_PIXBUF, 	/* icon (0) */
						G_TYPE_STRING, 		/* name (1) */
						G_TYPE_STRING,		/* path (2) */
						G_TYPE_INT,			/* type DIR/SONG (3) */
						G_TYPE_INT);		/* id (4) */
	
	pls_store 	= gtk_list_store_new (2, 
						GDK_TYPE_PIXBUF, 	/* icon (0) */
						G_TYPE_STRING);		/* name (1) */
	
	path = gimmix_get_full_image_path (GIMMIX_MEDIA_ICON);
	song_pixbuf = gdk_pixbuf_new_from_file_at_size (path, 16, 16, NULL);
	g_free (path);
	dir_pixbuf	= gtk_widget_render_icon (GTK_WIDGET(library_treeview), GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_BUTTON, NULL);
	path = gimmix_get_full_image_path (GIMMIX_PLAYLIST_ICON);
	pls_pixbuf	= gdk_pixbuf_new_from_file_at_size (path, 16, 16, NULL);
	g_free (path);
	
	for (data = mpd_database_get_directory(gmo, NULL); data != NULL; data = mpd_data_get_next(data))
	{	
		if (data->type == MPD_DATA_TYPE_DIRECTORY)
		{
			gtk_list_store_append (dir_store, &dir_iter);
			path = g_path_get_basename (data->directory);
			gtk_list_store_set (dir_store, &dir_iter,
						0, dir_pixbuf,
						1, path,
						2, data->directory,
						3, DIR,
						-1);
			g_free (path);
		}
		else if (data->type == MPD_DATA_TYPE_SONG)
		{
			gchar *title;

			gtk_list_store_append (dir_store, &dir_iter);
			if (data->song->title)
			{
				if (data->song->artist)
					title = g_strdup_printf ("%s - %s", data->song->artist, data->song->title);
				else
					title = g_strdup (data->song->title);
			}
			else
			{
				title = g_path_get_basename (data->song->file);
				gimmix_strip_file_ext (title);
			}
			gtk_list_store_set (dir_store,
						&dir_iter,
						0, song_pixbuf,
						1, title,
						2, data->song->file,
						3, SONG,
						-1);
			g_free (title);
		}
		else if (data->type == MPD_DATA_TYPE_PLAYLIST)
		{
			gchar *name = NULL;
			name = ((mpd_PlaylistFile*)data->playlist)->path;
			//g_print ("ADDING %s\n", name);
			gtk_list_store_append (pls_store, &pls_iter);
			gtk_list_store_set (pls_store,
						&pls_iter,
						0, pls_pixbuf,
						1, name,
						-1);
		}
	}

	mpd_data_free (data);

	dir_model	= GTK_TREE_MODEL (dir_store);
	gtk_tree_view_set_model (GTK_TREE_VIEW (library_treeview), dir_model);
	
	pls_model 	= GTK_TREE_MODEL (pls_store);
	gtk_tree_view_set_model (GTK_TREE_VIEW (playlists_treeview), pls_model);
	
	g_signal_connect (library_treeview, "row-activated", G_CALLBACK(cb_library_dir_activated), NULL);
	g_signal_connect (playlists_treeview, "row-activated", G_CALLBACK(cb_playlist_activated), NULL);
	g_signal_connect (library_treeview, "button-press-event", G_CALLBACK(cb_all_playlist_button_press), NULL);
	g_signal_connect (library_treeview, "button-release-event", G_CALLBACK(cb_library_right_click), NULL);
	g_signal_connect (library_window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	g_object_unref (dir_model);
	g_object_unref (pls_model);
	
	g_object_unref (dir_pixbuf);
	g_object_unref (song_pixbuf);
	g_object_unref (pls_pixbuf);
	
	return;
}

static void
gimmix_library_search (gint type, gchar *text)
{
	if (!text)
		return;

	MpdData 		*data;
	GtkTreeModel	*directory_model;
	GtkListStore	*dir_store;
	GdkPixbuf 		*song_pixbuf;
	GtkTreeIter 	dir_iter;
	gchar 			*path;

	switch (type)
	{
		/* Search by Artist */
		case 0: data = mpd_database_find (gmo, MPD_TABLE_ARTIST, text, FALSE);
				break;
		
		/* Search by Album */
		case 1: data = mpd_database_find (gmo, MPD_TABLE_ALBUM, text, FALSE);
				break;
		
		/* Search by Title */
		case 2: data = mpd_database_find (gmo, MPD_TABLE_TITLE, text, FALSE);
				break;
		
		/* Search by File name */
		case 3: data = mpd_database_find (gmo, MPD_TABLE_FILENAME, text, FALSE);
				break;
				
		default: return;
	}
	
	directory_model = gtk_tree_view_get_model (GTK_TREE_VIEW (library_treeview));
	dir_store 	= GTK_LIST_STORE (directory_model);

	/* Clear the stores */
	gtk_list_store_clear (dir_store);
	
	if (!data)
	{
		GdkPixbuf *icon	= gtk_widget_render_icon (GTK_WIDGET(library_treeview), "gtk-dialog-error",
						GTK_ICON_SIZE_MENU,
						NULL);
		gtk_list_store_append (dir_store, &dir_iter);
		gtk_list_store_set (dir_store, &dir_iter,
								0, icon,
								1, _("No Result"),
								2, NULL,
								3, 0,
								-1);
		directory_model = GTK_TREE_MODEL (dir_store);
		gtk_tree_view_set_model (GTK_TREE_VIEW(library_treeview), directory_model);
		g_object_unref (icon);
		return;
	}

	path = gimmix_get_full_image_path (GIMMIX_MEDIA_ICON);
	song_pixbuf = gdk_pixbuf_new_from_file_at_size (path, 12, 12, NULL);
	g_free (path);

	for (; data!=NULL; data = mpd_data_get_next (data))
	{
		if (data->type == MPD_DATA_TYPE_SONG)
		{
			gchar *title;
			
			title = (data->song->title!=NULL) ? g_strdup (data->song->title) : g_path_get_basename(data->song->file);
			gtk_list_store_append (dir_store, &dir_iter);
			gtk_list_store_set (dir_store, &dir_iter,
								0, song_pixbuf,
								1, title,
								2, data->song->file,
								3, SONG,
								-1);
			g_free (title);
		}
	}
	
	mpd_data_free (data);

	directory_model = GTK_TREE_MODEL (dir_store);
	gtk_tree_view_set_model (GTK_TREE_VIEW(library_treeview), directory_model);
	g_object_unref (song_pixbuf);
	
	return;
}

static void
cb_search_keypress (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	gint		index;
	gchar		text[20];
	
	strcpy (text, gtk_entry_get_text (GTK_ENTRY(search_entry)));
	if ( (strlen (text)) <= 1 )
	{
		gimmix_update_library_with_dir ("/");
		return;
	}
	index = gtk_combo_box_get_active (GTK_COMBO_BOX(search_combo));
	gimmix_library_search (index, text);
	
	return;
}

static void
cb_current_playlist_delete_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval != GDK_Delete)
		return;
	
	gimmix_current_playlist_remove_song ();
	
	return;
}

static void
cb_current_playlist_double_click (GtkTreeView *treeview)
{
	GtkTreeModel 			*model;
	GList				*list;
	GtkTreeIter			iter;
	gint 				id;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	list = gtk_tree_selection_get_selected_rows (current_playlist_selection, &model);
	gtk_tree_model_get_iter (model, &iter, list->data);
	gtk_tree_model_get (model, &iter, 2, &id, -1);
	mpd_player_play_id (gmo, id);
	mpd_status_update (gmo);
	
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}

/* This keeps multiple items selected in case of a right click */
static bool
cb_all_playlist_button_press (GtkTreeView *treeview, GdkEventButton *event, gpointer data)
{
	GtkTreeSelection	*selection;
	gboolean		current_playlist = (gboolean) data;
	
	if (event->button == 3)
	{
		/* check if it's a right click in current playlist treeview */
		if (current_playlist)
		{
			/* if yes, then also show the popup */
			gimmix_current_playlist_popup_menu ();
		}
		GtkTreePath *path;
		
		if (gtk_tree_view_get_path_at_pos(treeview, event->x, event->y, &path, NULL, NULL, NULL))
		{
			selection = gtk_tree_view_get_selection (treeview);
			bool sel = gtk_tree_selection_path_is_selected (selection, path);
			gtk_tree_path_free (path);
			
			if (sel && current_playlist)
				return TRUE;
		}
	}
	
	return FALSE;
}

static void
cb_library_dir_activated (gpointer data)
{
	GtkTreeModel 			*model;
	GtkTreeIter 			iter;
	GList				*list;
	gchar				*path;
	GimmixFileType			type;
	gboolean 			added;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (library_treeview));
	list = gtk_tree_selection_get_selected_rows (library_selection, &model);
	added = false; 

	if (gtk_tree_selection_count_selected_rows (library_selection) == 1)
	{
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, 2, &path, 3, &type, -1);
		
		if (type == DIR)
		{	
			gimmix_update_library_with_dir (path);
		}
		else if (type == SONG)
		{
			mpd_playlist_add (gmo, path);
			added = true;
		}
		
		g_free (path);
	}
	
	if (gimmix_config_get_bool("play_on_add") && (added == true))
	{
		/* If we're adding, there might already be a song playing. */
		int state;

		state = mpd_player_get_state (gmo);

		if (state == MPD_PLAYER_PAUSE || state == MPD_PLAYER_STOP)
		{	
			mpd_player_play (gmo);
		}
	}
	mpd_status_update (gmo);
		
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}

static void
cb_library_popup_add_clicked (GtkWidget *widget, gpointer data)
{
	GtkTreeModel 		*model;
	GtkTreeIter 		iter;
	GList			*list;
	gchar			*path;
	GimmixFileType		type;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (library_treeview));
	list = gtk_tree_selection_get_selected_rows (library_selection, &model);
	
	if (gtk_tree_selection_count_selected_rows (library_selection) == 1)
	{
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, 2, &path, 3, &type, -1);
		
		if (type == DIR)
		{
			mpd_playlist_queue_add (gmo, path);
		}
		else if (type == SONG)
		{
			mpd_playlist_add (gmo, path);
		}
		
		g_free (path);
	}
	else
	while (list != NULL)
	{
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, 2, &path, 3, &type, -1);
	
		
		if (type == DIR)
		{	
			mpd_playlist_queue_add (gmo, path);
			g_free (path);
		}
		
		if (type == SONG)
		{
			mpd_playlist_queue_add (gmo, path);
			g_free (path);
		}
		
		list = g_list_next (list);
	}
	
	mpd_playlist_queue_commit (gmo);
	if (gimmix_config_get_bool("play_on_add"))
	{
		/* If we're adding, there might already be a song playing. */
		int state;

		state = mpd_player_get_state (gmo);

		if (state == MPD_PLAYER_PAUSE || state == MPD_PLAYER_STOP)
		{	
			mpd_player_play (gmo);
		}
	}
	mpd_status_update (gmo);
		
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}

static void
cd_library_popup_replace_clicked (GtkWidget *widget, gpointer data)
{

	GtkTreeModel 		*model;
	GtkTreeIter 		iter;
	GList			*list;
	gchar			*path;
	GimmixFileType		type;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (library_treeview));
	list = gtk_tree_selection_get_selected_rows (library_selection, &model);

	gimmix_current_playlist_clear();	
	mpd_status_update (gmo);

	if (gtk_tree_selection_count_selected_rows (library_selection) == 1)
	{
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, 2, &path, 3, &type, -1);
		
		if (type == DIR)
		{
			mpd_playlist_queue_add (gmo, path);
		}
		else if (type == SONG)
		{
			mpd_playlist_add (gmo, path);
		}
		
		g_free (path);
	}
	else
	while (list != NULL)
	{
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, 2, &path, 3, &type, -1);
	
		
		if (type == DIR)
		{	
			mpd_playlist_queue_add (gmo, path);
			g_free (path);
		}
		
		if (type == SONG)
		{
			mpd_playlist_queue_add (gmo, path);
			g_free (path);
		}
		
		list = g_list_next (list);
	}

	mpd_playlist_queue_commit (gmo);
	
	if (gimmix_config_get_bool("play_on_add"))
	{
		gimmix_play(gmo);
	}
	mpd_status_update (gmo);
		
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}

static void
cb_playlist_activated (GtkTreeView *treeview)
{
	GtkTreeModel 		*model;
	GtkTreeSelection 	*selection;
	GtkTreeIter 		iter;
	gchar 			*pl_name;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if ( gtk_tree_selection_get_selected (selection, &model, &iter) )
		gtk_tree_model_get (model, &iter, 1, &pl_name, -1);
	
	mpd_playlist_queue_load (gmo, pl_name);
	gimmix_current_playlist_clear ();
	mpd_playlist_queue_commit (gmo);
	
	gimmix_load_playlist (pl_name);
	g_free (pl_name);

	return;
}

static void
gimmix_load_playlist (gchar *pls)
{
	/* unload the old playlist */
	if (loaded_playlist != NULL)
		g_free (loaded_playlist);
	
	/* load the new playlist*/
	loaded_playlist = g_strdup (pls);
	
	return;
}

static void
gimmix_update_playlists_treeview (void)
{
	GtkTreeModel	*pls_treemodel;
	GtkTreeIter		pls_treeiter;
	GtkListStore	*pls_liststore;
	GdkPixbuf		*pls_pixbuf;
	MpdData			*data;
	gchar			*name;
	gchar			*path;
	
	pls_treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW(playlists_treeview));
	pls_liststore = GTK_LIST_STORE (pls_treemodel);
	
	gtk_list_store_clear (pls_liststore);
	
	path = gimmix_get_full_image_path (GIMMIX_PLAYLIST_ICON);
	pls_pixbuf	= gdk_pixbuf_new_from_file_at_size (path, 16, 16, NULL);
	g_free (path);
	
	for (data = mpd_database_get_directory(gmo, NULL); data != NULL; data = mpd_data_get_next(data))
	{
		if (data->type == MPD_DATA_TYPE_PLAYLIST)
		{
			gtk_list_store_append (pls_liststore, &pls_treeiter);
			name = (char*) data->playlist;
			gtk_list_store_set (pls_liststore, &pls_treeiter,
								0, pls_pixbuf,
								1, name,
								-1);
		}
	}
	g_object_unref (pls_pixbuf);
	mpd_data_free (data);

	pls_treemodel = GTK_TREE_MODEL (pls_liststore);
	gtk_tree_view_set_model (GTK_TREE_VIEW(playlists_treeview), pls_treemodel);

	return;
}

static void
gimmix_update_library_with_dir (gchar *dir)
{
	GtkTreeModel	*directory_model;
	GtkListStore	*dir_store;
	GtkTreeIter	dir_iter;
	GdkPixbuf	*dir_pixbuf;
	GdkPixbuf	*song_pixbuf;
	MpdData		*data;
	gchar		*parent;
	gchar		*path;
	gchar		*directory;
	
	directory_model = gtk_tree_view_get_model (GTK_TREE_VIEW (library_treeview));
	dir_store 	= GTK_LIST_STORE (directory_model);

	if (!strlen(dir))
		dir = "/";

	/* Clear the stores */
	gtk_list_store_clear (dir_store);

	dir_pixbuf 	= gtk_widget_render_icon (GTK_WIDGET(library_treeview),
							GTK_STOCK_DIRECTORY,
							GTK_ICON_SIZE_BUTTON,
							NULL);
	path = gimmix_get_full_image_path (GIMMIX_MEDIA_ICON);
	song_pixbuf = gdk_pixbuf_new_from_file_at_size (path, 16, 16, NULL);
	g_free (path);
	
	if (strcmp(dir,"/"))
	{	
		parent = gimmix_path_get_parent_dir (dir);
		gtk_list_store_append (dir_store, &dir_iter);
		gtk_list_store_set (dir_store, &dir_iter,
					0, dir_pixbuf,
					1, "..",
					2, parent,
					3, DIR,
					-1);
		g_free (parent);
	}
	
	for (data = mpd_database_get_directory(gmo, dir); data != NULL; data = mpd_data_get_next(data))
	{	
		if (data->type == MPD_DATA_TYPE_DIRECTORY)
		{
			directory = g_path_get_basename(data->directory);
			gtk_list_store_append (dir_store, &dir_iter);
			gtk_list_store_set (dir_store, &dir_iter,
								0, dir_pixbuf,
								1, directory,
								2, data->directory,
								3, DIR,
								-1);
			g_free (directory);
		}
		else if (data->type == MPD_DATA_TYPE_SONG)
		{
			gchar *title;
			
			gtk_list_store_append (dir_store, &dir_iter);
			if (data->song->title)
			{
				if (data->song->artist)
					title = g_strdup_printf ("%s - %s", data->song->artist, data->song->title);
				else
					title = g_strdup (data->song->title);
			}
			else
			{
				title = g_path_get_basename (data->song->file);
				gimmix_strip_file_ext (title);
			}
			gtk_list_store_set (dir_store, &dir_iter,
								0, song_pixbuf,
								1, title,
								2, data->song->file,
								3, SONG,
								-1);
			g_free (title);
		}
		gtk_list_store_set (dir_store, &dir_iter, 4, data->song->id, -1);
	}
	
	mpd_data_free (data);

	directory_model = GTK_TREE_MODEL (dir_store);
	gtk_tree_view_set_model (GTK_TREE_VIEW(library_treeview), directory_model);
	
	g_object_unref (dir_pixbuf);
	g_object_unref (song_pixbuf);
	
	return;
}

static void
cb_library_right_click (GtkTreeView *treeview, GdkEventButton *event)
{	
	if (event->button == 3) /* If right click */
	{
		gimmix_library_popup_menu ();
	}
	
	return;
}

static void
cb_playlists_right_click (GtkTreeView *treeview, GdkEventButton *event)
{
	if (event->button == 3) /* If right click */
	{
		gimmix_playlists_popup_menu ();
	}
	
	return;
}

static void
gimmix_library_song_info (void)
{
	GtkTreeModel		*model;
	GList				*list;
	GtkTreeIter			iter;
	gchar				*path;
	gint				type = -1;
	guint				id;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW(library_treeview));
	
	list = gtk_tree_selection_get_selected_rows (library_selection, &model);
	gtk_tree_model_get_iter (model, &iter, list->data);
	gtk_tree_model_get (model, &iter, 2, &path, 3, &type, 4, &id, -1);

	if (type == DIR)
	{
		g_free (path);
		return;
	}
	#ifdef HAVE_TAGEDITOR
	gchar *song_path = g_strdup_printf ("%s/%s", cfg_get_key_value(conf, "music_directory"), path);
	if (gimmix_tag_editor_populate (song_path))
	{	
		gtk_widget_show (tag_editor_window);
	}
	else
	{
		gimmix_tag_editor_error (invalid_dir_error);
	}
	g_free (song_path);
	#else
	if (gimmix_tag_editor_populate (mpd_database_get_fileinfo(gmo,path)))
	{
		gtk_widget_show (tag_editor_window);
	}
	else
	{
		gimmix_tag_editor_error (_("An error occurred while trying to get song information. Please try again."));
	}
	#endif
	g_free (path);
	
	
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}

static void
gimmix_current_playlist_song_info (void)
{
	GtkTreeModel		*model;
	GList				*list;
	GtkTreeIter			iter;
	gchar				*path;
	guint				id;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW(current_playlist_treeview));
	if (gtk_tree_selection_count_selected_rows(current_playlist_selection) != 1)
	return;
	
	list = gtk_tree_selection_get_selected_rows (current_playlist_selection, &model);
	gtk_tree_model_get_iter (model, &iter, list->data);
	gtk_tree_model_get (model, &iter, 1, &path, 2, &id, -1);
	
	#ifdef HAVE_TAGEDITOR
	gchar *song_path = g_strdup_printf ("%s/%s", cfg_get_key_value(conf, "music_directory"), path);
	if (gimmix_tag_editor_populate (song_path))
	{	
		gtk_widget_show (tag_editor_window);
	}
	else
	{
		gimmix_tag_editor_error (invalid_dir_error);
	}
	g_free (song_path);
	#else
	if (gimmix_tag_editor_populate (mpd_playlist_get_song(gmo,id)))
	{
		gtk_widget_show (tag_editor_window);
	}
	else
	{
		gimmix_tag_editor_error (_("An error occurred while trying to get song information. Please try again."));
	}
	#endif	
	g_free (path);
	
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}

static void
gimmix_current_playlist_remove_song (void)
{
	GtkTreeModel		*current_playlist_model;
	GtkTreeIter			iter;
	GList				*list;
	gint				id;

	current_playlist_model = gtk_tree_view_get_model (GTK_TREE_VIEW(current_playlist_treeview));
	list = gtk_tree_selection_get_selected_rows (current_playlist_selection, &current_playlist_model);
	
	while (list != NULL)
	{
		if (TRUE == gtk_tree_model_get_iter (current_playlist_model, &iter, list->data))
		{
			gtk_tree_model_get (current_playlist_model, &iter, 2, &id, -1);
			mpd_playlist_queue_delete_id (gmo, id);
		}
		list = g_list_next (list);
	}
	mpd_playlist_queue_commit (gmo);
	mpd_status_update (gmo);
	
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	
	return;
}	

static void
gimmix_current_playlist_crop_song (void)
{
	GtkTreeModel			*current_playlist_model;
	GtkTreeIter			iter;
	GList				*list;
	GList				*all;
	gint				id;

	current_playlist_model = gtk_tree_view_get_model (GTK_TREE_VIEW(current_playlist_treeview));
	list = gtk_tree_selection_get_selected_rows (current_playlist_selection, &current_playlist_model);
	gtk_tree_selection_select_all (current_playlist_selection);

	while (list != NULL)
	{
		if (gtk_tree_model_get_iter (current_playlist_model, &iter, list->data))
		{
			gtk_tree_selection_unselect_iter (current_playlist_selection, &iter);
		}
		list = g_list_next (list);
	}

	all = gtk_tree_selection_get_selected_rows (current_playlist_selection, &current_playlist_model);
	while (all != NULL)
	{
		if (TRUE == gtk_tree_model_get_iter (current_playlist_model, &iter, all->data))
		{
			gtk_tree_model_get (current_playlist_model, &iter, 2, &id, -1);
			mpd_playlist_queue_delete_id (gmo, id);
		}
		all = g_list_next (all);
	}
	mpd_playlist_queue_commit (gmo);
	mpd_status_update (gmo);
	
	/* free the list */
	g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (list);
	g_list_foreach (all, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (all);
	
	return;
}	


static void
gimmix_current_playlist_clear (void)
{
	GtkListStore	*current_playlist_store;
	
	current_playlist_store = GTK_LIST_STORE(gtk_tree_view_get_model (GTK_TREE_VIEW(current_playlist_treeview)));
	gtk_list_store_clear (GTK_LIST_STORE(current_playlist_store));
	if (mpd_playlist_get_playlist_length (gmo) != 0)
	{
		mpd_playlist_clear (gmo);
		mpd_status_update (gmo);
	}
	if (loaded_playlist != NULL)
	{
		g_free (loaded_playlist);
		loaded_playlist = NULL;
	}
	return;
}

static void
cb_gimmix_playlist_column_show_toggled (GtkCheckMenuItem *menu_item, gpointer data)
{
	gint		column = (gint) data;
	gchar		*kval;
	gboolean	value;

	if (gtk_check_menu_item_get_active(menu_item))
	{
		kval = "true";
		value = TRUE;
	}
	else
	{
		kval = "false";
		value = FALSE;
	}
	
	switch (column)
	{
		case COLUMN_TITLE:
		{
			cfg_add_key (&conf, "pl_column_title_show", kval);
			gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_title_column), value);
			break;
		}
		case COLUMN_ARTIST:
		{
			cfg_add_key (&conf, "pl_column_artist_show", kval);
			gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_artist_column), value);
			break;
		}
		case COLUMN_ALBUM:
		{
			cfg_add_key (&conf, "pl_column_album_show", kval);
			gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_album_column), value);
			break;
		}
		case COLUMN_LENGTH:
		{
			cfg_add_key (&conf, "pl_column_length_show", kval);
			gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN(cpl_tvw_length_column), value);
			break;
		}
		default:
		break;
	}
	gimmix_config_save ();
}

static void
gimmix_current_playlist_popup_menu (void)
{
	GtkWidget *menu, *menu_item, *image;
	GtkWidget *submenu = NULL;

	menu = gtk_menu_new ();
	image = gtk_image_new_from_stock ("gtk-cut", GTK_ICON_SIZE_MENU); 

	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, NULL);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_current_playlist_remove_song), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_with_label (_("Crop"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), image);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_current_playlist_crop_song), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_INFO, NULL);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_current_playlist_song_info), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, NULL);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_current_playlist_clear), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_check_menu_item_new_with_label (_("Repeat"));
	g_signal_connect (G_OBJECT(menu_item), "toggled", G_CALLBACK(cb_repeat_menu_toggled), NULL);
	if (is_gimmix_repeat(gmo))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_check_menu_item_new_with_label (_("Shuffle"));
	g_signal_connect (G_OBJECT(menu_item), "toggled", G_CALLBACK(cb_shuffle_menu_toggled), NULL);
	if (is_gimmix_shuffle(gmo))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_with_label (_("Save Playlist"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), GTK_WIDGET(get_image ("gtk-save", GTK_ICON_SIZE_MENU)));
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_playlist_save_dialog_show), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	submenu = gtk_menu_new ();
	menu_item = gtk_check_menu_item_new_with_label (_("Title"));
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menu_item);
	if (gimmix_config_get_bool("pl_column_title_show"))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
	g_signal_connect (G_OBJECT (menu_item), "toggled", G_CALLBACK (cb_gimmix_playlist_column_show_toggled), (gpointer)COLUMN_TITLE);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_check_menu_item_new_with_label (_("Artist"));
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menu_item);
	if (gimmix_config_get_bool("pl_column_artist_show"))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
	g_signal_connect (G_OBJECT (menu_item), "toggled", G_CALLBACK (cb_gimmix_playlist_column_show_toggled), (gpointer)COLUMN_ARTIST);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_check_menu_item_new_with_label (_("Album"));
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menu_item);
	if (gimmix_config_get_bool("pl_column_album_show"))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
	g_signal_connect (G_OBJECT (menu_item), "toggled", G_CALLBACK (cb_gimmix_playlist_column_show_toggled), (gpointer)COLUMN_ALBUM);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_check_menu_item_new_with_label (_("Length"));
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menu_item);
	if (gimmix_config_get_bool("pl_column_length_show"))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
	g_signal_connect (G_OBJECT (menu_item), "toggled", G_CALLBACK (cb_gimmix_playlist_column_show_toggled), (gpointer)COLUMN_LENGTH);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_with_label (_("Columns"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), GTK_WIDGET(get_image ("gtk-edit", GTK_ICON_SIZE_MENU)));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu_item), submenu);
	
	gtk_widget_show (menu);
	gtk_menu_popup (GTK_MENU(menu),
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			gtk_get_current_event_time());
	
	return;
}

static void
gimmix_library_popup_menu (void)
{
	GtkWidget 	*menu;
	GtkWidget 	*menu_item;
	GtkWidget	*image;
	GtkWidget	*replace_icon;

	menu = gtk_menu_new ();
	image = gtk_image_new_from_stock ("gtk-refresh", GTK_ICON_SIZE_MENU);
	replace_icon = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
	
	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ADD, NULL);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (cb_library_popup_add_clicked), (gpointer)1);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_with_label (_("Replace"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), replace_icon);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (cd_library_popup_replace_clicked), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	if (gtk_tree_selection_count_selected_rows(library_selection) == 1)
	{
		menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_INFO, NULL);
		g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_library_song_info), NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		gtk_widget_show (menu_item);
	}
	
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_with_label (_("Update Library"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), image);
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (gimmix_library_update), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	gtk_widget_show (menu);
	gtk_menu_popup (GTK_MENU(menu),
			NULL,
			NULL,
			NULL,
			NULL,
			3,
			gtk_get_current_event_time());
			return;
}

static void
gimmix_playlists_popup_menu (void)
{
	GtkWidget *menu, *menu_item;

	menu = gtk_menu_new ();
	
	menu_item = gtk_image_menu_item_new_with_label (_("Load"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), GTK_WIDGET(get_image ("gtk-open", GTK_ICON_SIZE_MENU)));
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (cb_gimmix_playlist_load), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_image_menu_item_new_with_label (_("Delete"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menu_item), GTK_WIDGET(get_image ("gtk-delete", GTK_ICON_SIZE_MENU)));
	g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (cb_gimmix_playlist_remove), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	gtk_widget_show (menu);
	gtk_menu_popup (GTK_MENU(menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					gtk_get_current_event_time());
					
	return;
}

static void
cb_repeat_menu_toggled (GtkCheckMenuItem *item, gpointer data)
{
	gboolean state;
	
	state = gtk_check_menu_item_get_active (item);
	if (state == TRUE)
	{
		mpd_player_set_repeat (gmo, true);
	}
	else if (state == FALSE)
	{
		mpd_player_set_repeat (gmo, false);
	}
	
	return;
}

static void
cb_shuffle_menu_toggled (GtkCheckMenuItem *item, gpointer data)
{
	gboolean state;
	
	state = gtk_check_menu_item_get_active (item);
	if (state == TRUE)
	{
		mpd_player_set_random (gmo, true);
	}
	else if (state == FALSE)
	{
		mpd_player_set_random (gmo, false);
	}
	
	return;
}

void
gimmix_library_update (void)
{
	mpd_database_update_dir (gmo, "/");
	gtk_label_set_text (GTK_LABEL(gimmix_statusbar), _("Updating Library..."));
	gtk_widget_show (gimmix_statusbox);
	/* disable the update button on the toolbar */
	gtk_widget_set_sensitive (button_update, FALSE);
	g_timeout_add (300, (GSourceFunc)gimmix_update_player_status, NULL);

	return;
}

static gboolean
gimmix_update_player_status (gpointer data)
{
	if (mpd_status_db_is_updating (gmo))
		return TRUE;
		
	gimmix_display_total_playlist_time (NULL);		
	gimmix_update_library_with_dir ("/");
	/* re-enable the update button on the toolbar */
	gtk_widget_set_sensitive (button_update, TRUE);	
	
	return FALSE;
}

static gchar *
gimmix_path_get_parent_dir (gchar *path)
{
	gchar *p, buf[128];
	gchar *ret;

	strcpy (buf, path);
	p = strrchr (buf, '/');
	if (p)
	{	
		*p = 0;
		ret = g_strdup (buf);
 	}
 	else
 	ret = g_strdup ("/");
 	
 	return ret;
}

static void
gimmix_playlist_save_dialog_show (void)
{
	GtkWidget 	*dialog;
	GtkWidget 	*label;
	GtkWidget 	*entry;
	static gchar *message = "Enter the name of the playlist: ";
   
	dialog = gtk_dialog_new_with_buttons (_("Save playlist"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_REJECT,
                                         NULL);
	gtk_window_set_resizable (GTK_WINDOW(dialog), FALSE);
	label = gtk_label_new (message);
	entry = gtk_entry_new ();
	if (loaded_playlist != NULL)
		gtk_entry_set_text (GTK_ENTRY(entry), loaded_playlist);
	
	g_signal_connect_swapped (dialog,
                             "response", 
                             G_CALLBACK (cb_gimmix_playlist_save_response),
                             dialog);
	g_signal_connect (dialog, "delete-event", G_CALLBACK(gtk_widget_destroy), dialog);
	gtk_misc_set_padding (GTK_MISC(label), 5, 5);
	gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER((GTK_DIALOG(dialog))->vbox), 10);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), entry);
   
	gtk_widget_show_all (dialog);
   
	return;
}

static void
cb_gimmix_playlist_save_response (GtkDialog *dlg, gint arg1, gpointer dialog)
{
	if (arg1 == GTK_RESPONSE_ACCEPT)
	{	
		if (loaded_playlist != NULL)
		{
			mpd_database_delete_playlist (gmo, loaded_playlist);
			if ((mpd_database_save_playlist (gmo, loaded_playlist)) == MPD_DATABASE_PLAYLIST_EXIST)
				g_print (_("playlist already exists.\n"));
		}
		else
		{
			GList *widget_list;
			const gchar *text;
			
			widget_list = gtk_container_get_children (GTK_CONTAINER(GTK_DIALOG(dialog)->vbox));
			widget_list = widget_list->next;
			text = gtk_entry_get_text (GTK_ENTRY(widget_list->data));
			if ((mpd_database_save_playlist (gmo, (char*)text)) == MPD_DATABASE_PLAYLIST_EXIST)
				g_print (_("playlist already exists.\n"));
			
			g_list_free (widget_list);
			gimmix_load_playlist ((char*)text);
		}
		gimmix_update_playlists_treeview ();
	}	

	gtk_widget_destroy (GTK_WIDGET(dlg));
	
	return;
}

static void
cb_gimmix_playlist_remove ()
{
	GtkTreeModel		*pls_treemodel;
	GtkTreeSelection	*selection;
	GtkTreeIter			iter;
	gchar				*path;

	pls_treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW(playlists_treeview));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(playlists_treeview));
	
	if (!selection)
		return;	
	
	if ( gtk_tree_selection_get_selected (selection, &pls_treemodel, &iter) )
	{
		gtk_tree_model_get (pls_treemodel, &iter,
							1, &path,
							-1);
		mpd_database_delete_playlist (gmo, path);
	}
	
	if ((loaded_playlist != NULL) && (strcmp (path, loaded_playlist) == 0))
			gimmix_current_playlist_clear ();
	
	gimmix_update_playlists_treeview ();
	g_free (path);
	
	return;
}

static void
cb_gimmix_playlist_load ()
{	
	GtkTreeModel		*pls_treemodel;
	GtkTreeSelection	*selection;
	GtkTreeIter			iter;
	gchar				*path;

	pls_treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW(playlists_treeview));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(playlists_treeview));
	
	if ( gtk_tree_selection_get_selected (selection, &pls_treemodel, &iter) )
	{
		gtk_tree_model_get (pls_treemodel, &iter,
							1, &path,
							-1);
		mpd_playlist_queue_load (gmo, path);
		gimmix_current_playlist_clear ();
		mpd_playlist_queue_commit (gmo);
	
		gimmix_load_playlist (path);
		g_free (path);
	}
	
	return;
}

static void
cb_playlists_delete_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval != GDK_Delete)
		return;
	
	cb_gimmix_playlist_remove ();
	
	return;
}

static void
cb_add_button_clicked (GtkWidget *widget, gpointer data)
{
	if (!GTK_WIDGET_VISIBLE(GTK_WINDOW(data)))
		gtk_widget_show (GTK_WIDGET(data));
		
	gtk_window_present (GTK_WINDOW(data));
	return;	
}

