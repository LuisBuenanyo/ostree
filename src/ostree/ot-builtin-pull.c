/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2011,2013 Colin Walters <walters@verbum.org>
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
 * Author: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include "ot-builtins.h"
#include "ostree.h"
#include "otutil.h"

gboolean opt_metadata;
#ifdef HAVE_GPGME
gboolean opt_verifysignatures;
#endif

static GOptionEntry options[] = {
  { "metadata", 'm', 0, G_OPTION_ARG_NONE, &opt_metadata, "Download only the metadata", NULL },
#ifdef HAVE_GPGME
  { "verify-commits", 0, 0, G_OPTION_ARG_NONE, &opt_verifysignatures, "Verify commits with gpg signatures", NULL },
#endif
  { NULL }
};

gboolean
ostree_builtin_pull (int argc, char **argv, OstreeRepo *repo, GCancellable *cancellable, GError **error)
{
  GOptionContext *context;
  gboolean ret = FALSE;
  const char *remote;
  OstreeRepoPullFlags pullflags = 0;
  gs_unref_ptrarray GPtrArray *refs_to_fetch = NULL;

  context = g_option_context_new ("REMOTE [BRANCH...] - Download data from remote repository");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  if (argc < 2)
    {
      ot_util_usage_error (context, "REMOTE must be specified", error);
      goto out;
    }
  remote = argv[1];

  if (argc > 2)
    {
      int i;
      refs_to_fetch = g_ptr_array_new ();
      for (i = 2; i < argc; i++)
        g_ptr_array_add (refs_to_fetch, argv[i]);
      g_ptr_array_add (refs_to_fetch, NULL);
    }

  if (opt_metadata)
    pullflags |= OSTREE_REPO_PULL_FLAGS_METADATA;

#ifdef HAVE_GPGME
  if (opt_verifysignatures)
    pullflags |= OSTREE_REPO_PULL_FLAGS_VERIFY;
#endif

  if (!ostree_repo_pull (repo, remote, refs_to_fetch ? (char**)refs_to_fetch->pdata : NULL,
                    pullflags, cancellable, error))
    goto out;
 
  ret = TRUE;
 out:
  if (context)
    g_option_context_free (context);
  return ret;
}
