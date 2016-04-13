#ifdef HAVE_LYRICS

#ifndef GIMMIX_LYRICS_H
#define GIMMIX_LYRICS_H

#include "gimmix-core.h"
#include "gimmix.h"
#include "wejpconfig.h"
#include <gtk/gtk.h>

typedef struct lnode
{
	char artist[80];
	char title[80];
	char hid[16];
	char writer[80];
	gboolean match;
	char *lyrics;
} LYRICS_NODE;

/* Initialize the gimmix lyrics plugin */
void gimmix_lyrics_plugin_init (void);

/* Free memory used by lyrics plugin */
void gimmix_lyrics_plugin_cleanup (void);

/* update lyrics for current song */
void gimmix_lyrics_plugin_update_lyrics (void);

void lyrics_set_artist (const char *artist);
void lyrics_set_songtitle (const char *title);
LYRICS_NODE* lyrics_search (void);
void gimmix_lyrics_populate_textview (LYRICS_NODE *node);

#endif

#endif
