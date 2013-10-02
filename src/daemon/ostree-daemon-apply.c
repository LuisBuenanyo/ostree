/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2013 Collabora Ltd.
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
 * Author: Vivek Dasmohapatra <vivek@etla.org>
 */

#include "ostree-daemon-apply.h"
#include "ot-deployment.h"
#include "ot-admin-functions.h"
#include "ot-admin-deploy.h"
#include <ostree.h>

static void
apply_finished (GObject *object,
                GAsyncResult *res,
                gpointer user_data)
{
  OTDOSTree *ostree = OTD_OSTREE (object);
  GTask *task;
  GError *error = NULL;

  gs_unref_object OstreeRepo *repo = OSTREE_REPO (user_data);
  gboolean bootver_changed = FALSE;

  if (!g_task_is_valid (res, object))
    goto invalid_task;

  task = G_TASK (res);
  bootver_changed = g_task_propagate_boolean (task, &error);

  if (!bootver_changed)
    message ("System redeployed same boot version");

  if (error)
    {
      ostree_daemon_set_error (ostree, error);
      g_clear_error (&error);
    }
  else
    {
      otd_ostree_set_error_code (ostree, 0);
      otd_ostree_set_error_message (ostree, "");
      ostree_daemon_set_state (ostree, OTD_STATE_UPDATE_APPLIED);
    }

  return;

 invalid_task:
  // Either the threading or the memory management is shafted. Or both.
  // We're boned. Log an error and activate the self destruct mechanism:
  g_error ("Invalid async task object when returning from Poll() thread!");
  g_assert_not_reached ();
}

static void
apply (GTask *task,
       gpointer object,
       gpointer task_data,
       GCancellable *cancel)
{
  OTDOSTree *ostree = OTD_OSTREE (object);
  GError *error = NULL;
  GMainContext *task_context = g_main_context_new ();
  const gchar *update_id = otd_ostree_get_update_id (ostree);
  gint bootversion = 0;
  gint newbootver = 0;
  gs_unref_ptrarray GPtrArray *deployed = NULL;
  gs_unref_object GFile *root = g_file_new_for_path ("/");
  gs_unref_object OtDeployment *booted_deployment = NULL;
  gs_unref_object OtDeployment *merge_deployment = NULL;
  const gchar *osname;
  GKeyFile *origin;

  g_main_context_push_thread_default (task_context);

  if (!ot_admin_list_deployments (root, &bootversion, &deployed, cancel, &error))
    goto error;

  if (!ot_admin_require_deployment_or_osname (root, deployed, NULL,
                                              &booted_deployment,
                                              cancel, &error))
    goto error;

  osname = ot_deployment_get_osname (booted_deployment);
  merge_deployment =
    ot_admin_get_merge_deployment (deployed, osname, booted_deployment);
  origin = ot_deployment_get_origin (merge_deployment);

  if (!ot_admin_deploy (root, bootversion, deployed, osname, update_id, origin,
                        NULL, FALSE, booted_deployment, merge_deployment,
                        NULL, &newbootver, NULL, // out args
                        cancel, &error))
    goto error;

  g_task_return_boolean (task, bootversion != newbootver);
  goto cleanup;

 error:
  g_task_return_error (task, error);

 cleanup:
  g_main_context_pop_thread_default (task_context);
  g_main_context_unref (task_context);
  return;
}

gboolean
handle_apply (OTDOSTree             *ostree,
              GDBusMethodInvocation *call,
              gpointer               user_data)
{
  OstreeRepo *repo = OSTREE_REPO (user_data);
  GTask *task = NULL;
  OTDState state = otd_ostree_get_state (ostree);

  switch (state)
    {
    case OTD_STATE_UPDATE_READY:
      break;
    default:
      g_dbus_method_invocation_return_error (call,
        OTD_ERROR, OTD_ERROR_WRONG_STATE,
        "Can't call Apply() while in state %s", otd_state_to_string (state));
      goto bail;
    }

  ostree_daemon_set_state (ostree, OTD_STATE_APPLYING_UPDATE);
  task = g_task_new (ostree, NULL, apply_finished, g_object_ref (repo));
  g_task_set_task_data (task, g_object_ref (repo), g_object_unref);
  g_task_run_in_thread (task, apply);

  otd_ostree_complete_apply (ostree, call);

bail:
  return TRUE;
}
