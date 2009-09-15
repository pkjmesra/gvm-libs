/* openvase-libraries/omp
 * $Id$
 * Description: OMP client interface.
 *
 * Authors:
 * Matthew Mundell <matt@mundell.ukfsn.org>
 *
 * Copyright:
 * Copyright (C) 2009 Greenbone Networks GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2,
 * or, at your option, any later version as published by the Free
 * Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** @todo Name functions consistently (perhaps omp_*). */

/**
 * @file omp.c
 * @brief OMP client interface.
 *
 * This provides higher level, OMP-aware, facilities for working with with
 * the OpenVAS manager.  The functions are
 *
 * \ref authenticate,
 * \ref env_authenticate,
 * \ref create_task,
 * \ref create_task_from_rc_file,
 * \ref delete_task,
 * \ref start_task,
 * \ref wait_for_task_end and
 * \ref wait_for_task_start.
 *
 * There are examples of using this interface in the openvas-manager tests.
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "omp.h"
#include "xml.h"
#include "openvas_server.h"


/* Local XML interface extension. */

/** @todo Use next_entities and first_entity instead of this. */

/**
 * @brief Do something for each child of an entity.
 *
 * Calling "break" during body exits the loop.
 *
 * @param[in]  entity  The entity.
 * @param[in]  child   Name to use for child variable.
 * @param[in]  temp    Name to use for internal variable.
 * @param[in]  body    The code to run for each child.
 */
#define DO_CHILDREN(entity, child, temp, body)      \
  do                                                \
    {                                               \
      GSList* temp = entity->entities;              \
      while (temp)                                  \
        {                                           \
          entity_t child = temp->data;              \
          {                                         \
            body;                                   \
          }                                         \
          temp = g_slist_next (temp);               \
        }                                           \
    }                                               \
  while (0)

#if 0
/* Lisp version of DO_CHILDREN. */
(defmacro do-children ((entity child) &body body)
  "Do something for each child of an entity."
  (let ((temp (gensym)))
    `(while ((,temp (entity-entities ,entity) (rest ,temp)))
            (,temp)
       ,@body)))
#endif


/* OMP. */

/**
 * @brief Get the task status from an OMP GET_STATUS response.
 *
 * @param[in]  response   GET_STATUS response.
 *
 * @return The entity_text of the status entity if the entity is found, else
 *         NULL.
 */
const char*
task_status (entity_t response)
{
  entity_t task = entity_child (response, "task");
  if (task)
    {
      entity_t status = entity_child (task, "status");
      if (status) return entity_text (status);
    }
  return NULL;
}

/**
 * @brief Authenticate with the manager.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[in]  username  Username.
 * @param[in]  password  Password.
 *
 * @return 0 on success, 1 if manager closed connection, 2 if auth failed,
 *         -1 on error.
 */
int
authenticate (gnutls_session_t* session,
              const char* username,
              const char* password)
{
  entity_t entity;
  const char* status;
  char first;
  gchar* msg;

  /* Send the auth request. */

  msg = g_strdup_printf ("<authenticate><credentials>"
                         "<username>%s</username>"
                         "<password>%s</password>"
                         "</credentials></authenticate>",
                         username,
                         password);
  int ret = openvas_server_send (session, msg);
  g_free (msg);
  if (ret) return ret;

  /* Read the response. */

  entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return 2;
}

/**
 * @brief Authenticate, getting credentials from the environment.
 *
 * Get the user name from environment variable OPENVAS_TEST_USER if that is
 * set, else from USER.  Get the password from OPENVAS_TEST_PASSWORD.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 *
 * @return 0 on success, 1 if manager closed connection, -1 on error.
 */
int
env_authenticate (gnutls_session_t* session)
{
  char* user = getenv ("OPENVAS_TEST_USER");
  if (user == NULL)
    {
      user = getenv ("USER");
      if (user == NULL) return -1;
    }

  char* password = getenv ("OPENVAS_TEST_PASSWORD");
  if (password == NULL) return -1;

  return authenticate (session, user, password);
}

/**
 * @brief Create a task given a config and target.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   name        Task name.
 * @param[in]   config      Task config name.
 * @param[in]   target      Task target name.
 * @param[in]   comment     Task comment.
 * @param[out]  id          Pointer for newly allocated ID of new task.  Only
 *                          set on successful return.
 *
 * @return 0 on success, -1 on error.
 */
int
omp_create_task (gnutls_session_t* session,
                 const char* name,
                 const char* config,
                 const char* target,
                 const char* comment,
                 char** id)
{
  /* Create the OMP request. */

  gchar* new_task_request;
  new_task_request = g_strdup_printf ("<create_task>"
                                      "<config>%s</config>"
                                      "<target>%s</target>"
                                      "<name>%s</name>"
                                      "<comment>%s</comment>"
                                      "</create_task>",
                                      config,
                                      target,
                                      name,
                                      comment);

  /* Send the request. */

  int ret = openvas_server_send (session, new_task_request);
  g_free (new_task_request);
  if (ret) return -1;

  /* Read the response. */

  entity_t entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Get the ID of the new task from the response. */

  entity_t id_entity = entity_child (entity, "task_id");
  if (id_entity == NULL)
    {
      free_entity (entity);
      return -1;
    }
  *id = g_strdup (entity_text (id_entity));
  return 0;
}

/**
 * @brief Create a task, given the task description as an RC file.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   config      Task configuration.
 * @param[in]   config_len  Length of config.
 * @param[in]   name        Task name.
 * @param[in]   comment     Task comment.
 * @param[out]  id          Pointer for newly allocated ID of new task.  Only
 *                          set on successful return.
 *
 * @return 0 on success, -1 on error.
 */
int
create_task (gnutls_session_t* session,
             const char* config,
             unsigned int config_len,
             const char* name,
             const char* comment,
             char** id)
{
  /* Convert the file contents to base64. */

  gchar* new_task_file = strlen (config)
                         ? g_base64_encode ((guchar*) config, config_len)
                         : g_strdup ("");

  /* Create the OMP request. */

  gchar* new_task_request;
  new_task_request = g_strdup_printf ("<create_task>"
                                      "<rcfile>%s</rcfile>"
                                      "<name>%s</name>"
                                      "<comment>%s</comment>"
                                      "</create_task>",
                                      new_task_file,
                                      name,
                                      comment);
  g_free (new_task_file);

  /* Send the request. */

  int ret = openvas_server_send (session, new_task_request);
  g_free (new_task_request);
  if (ret) return -1;

  /* Read the response. */

  entity_t entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Get the ID of the new task from the response. */

  entity_t id_entity = entity_child (entity, "task_id");
  if (id_entity == NULL)
    {
      free_entity (entity);
      return -1;
    }
  *id = g_strdup (entity_text (id_entity));
  return 0;
}

/**
 * @brief Create a task, given the task description as an RC file.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   file_name   Name of the RC file.
 * @param[in]   name        Task name.
 * @param[in]   comment     Task comment.
 * @param[out]  id          ID of new task.
 *
 * @return 0 on success, -1 on error.
 */
int
create_task_from_rc_file (gnutls_session_t* session,
                          const char* file_name,
                          const char* name,
                          const char* comment,
                          char** id)
{
  gchar* new_task_rc = NULL;
  gsize new_task_rc_len;
  GError* error = NULL;

  /* Read in the RC file. */

  g_file_get_contents (file_name,
                       &new_task_rc,
                       &new_task_rc_len,
                       &error);
  if (error)
    {
      g_error_free (error);
      return -1;
    }

  int ret = create_task (session,
                         new_task_rc,
                         new_task_rc_len,
                         name,
                         comment,
                         id);
  g_free (new_task_rc);
  return ret;
}

/**
 * @brief Start a task and read the manager response.
 *
 * @param[in]  session  Pointer to GNUTLS session.
 * @param[in]  id       ID of task.
 *
 * @return 0 on success, -1 on error.
 */
int
start_task (gnutls_session_t* session, const char* id)
{
  if (openvas_server_sendf (session,
                            "<start_task task_id=\"%s\"/>",
                            id)
      == -1)
    return -1;

  /* Read the response. */

  entity_t entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  const char* status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  char first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return -1;
}

/**
 * @brief Wait for a task to start running on the server.
 *
 * @param[in]  session  Pointer to GNUTLS session.
 * @param[in]  id       ID of task.
 *
 * @return 0 on success, 1 on internal error in task, -1 on error.
 */
int
wait_for_task_start (gnutls_session_t* session,
                     const char* id)
{
  while (1)
    {
      if (openvas_server_sendf (session, "<get_status/>") == -1)
        return -1;

      /* Read the response. */

      entity_t entity = NULL;
      if (read_entity (session, &entity)) return -1;

      /* Check the response. */

      const char* status = entity_attribute (entity, "status");
      if (status == NULL)
        {
          free_entity (entity);
          return -1;
        }
      if (strlen (status) == 0)
        {
          free_entity (entity);
          return -1;
        }
      if (status[0] == '2')
        {
          /* Check the running status of the given task. */

          char* run_state = NULL;

#if 0
          /* Lisp version. */
          (do-children (entity child)
            (when (string= (entity-type child) "task")
              (let ((task-id (entity-attribute child "task_id")))
                (fi* task-id
                  (free-entity entity)
                  (return-from wait-for-task-start -1))
                (when (string= task-id id)
                  (let ((status (entity-child child "status")))
                    (fi* status
                      (free-entity entity)
                      (return-from wait-for-task-start -1))
                    (setq run-state (entity-text status)))
                  (return)))))
#endif

          DO_CHILDREN (entity, child, temp,
                       if (strcasecmp (entity_name (child), "task") == 0)
                         {
                           const char* task_id = entity_attribute (child, "id");
                           if (task_id == NULL)
                             {
                               free_entity (entity);
                               return -1;
                             }
                           if (strcasecmp (task_id, id) == 0)
                             {
                               entity_t status = entity_child (child, "status");
                               if (status == NULL)
                                 {
                                   free_entity (entity);
                                   return -1;
                                 }
                               run_state = entity_text (status);
                               break;
                             }
                         });

          if (run_state == NULL)
            {
              free_entity (entity);
              return -1;
            }

          if (strcmp (run_state, "Running") == 0
              || strcmp (run_state, "Done") == 0)
            {
              free_entity (entity);
              return 0;
            }
          if (strcmp (run_state, "Internal Error") == 0)
            {
              free_entity (entity);
              return 1;
            }
          free_entity (entity);
        }

      /** @todo Reconsider this (more below). */
      sleep (1);
    }
}

/**
 * @brief Wait for a task to finish running on the server.
 *
 * @param[in]  session  Pointer to GNUTLS session.
 * @param[in]  id       ID of task.
 *
 * @return 0 on success, 1 on internal error in task, -1 on error.
 */
int
wait_for_task_end (gnutls_session_t* session, const char* id)
{
  while (1)
    {
      if (openvas_server_sendf (session, "<get_status/>") == -1)
        return -1;

      /* Read the response. */

      entity_t entity = NULL;
      if (read_entity (session, &entity)) return -1;

      /* Check the response. */

      const char* status = entity_attribute (entity, "status");
      if (status == NULL)
        {
          free_entity (entity);
          return -1;
        }
      if (strlen (status) == 0)
        {
          free_entity (entity);
          return -1;
        }
      if (status[0] == '2')
        {
          /* Check the running status of the given task. */

          char* run_state = NULL;

#if 0
          /* Lisp version. */
          (do-children (entity child)
            (when (string= (entity-type child) "task")
              (let ((task-id (entity-attribute child "task_id")))
                (fi* task-id
                  (free-entity entity)
                  (return-from wait-for-task-start -1))
                (when (string= task-id id)
                  (let ((status (entity-child child "status")))
                    (fi* status
                      (free-entity entity)
                      (return-from wait-for-task-start -1))
                    (setq run-state (entity-text status)))
                  (return)))))
#endif

          DO_CHILDREN (entity, child, temp,
                       if (strcasecmp (entity_name (child), "task") == 0)
                         {
                           const char* task_id = entity_attribute (child, "id");
                           if (task_id == NULL)
                             {
                               free_entity (entity);
                               return -1;
                             }
                           if (strcasecmp (task_id, id) == 0)
                             {
                               entity_t status = entity_child (child, "status");
                               if (status == NULL)
                                 {
                                   free_entity (entity);
                                   return -1;
                                 }
                               run_state = entity_text (status);
                               break;
                             }
                         });

          if (run_state == NULL)
            {
              free_entity (entity);
              return -1;
            }

          if (strcmp (run_state, "Done") == 0)
            {
              free_entity (entity);
              return 0;
            }
          if (strcmp (run_state, "Internal Error") == 0)
            {
              free_entity (entity);
              return 1;
            }
          if (strcmp (run_state, "Stopped") == 0)
            {
              free_entity (entity);
              return 1;
            }
          free_entity (entity);
        }

      sleep (1);
    }
}

/**
 * @brief Wait for a task to stop on the server.
 *
 * @param[in]  session  Pointer to GNUTLS session.
 * @param[in]  id       ID of task.
 *
 * @return 0 on success, 1 on internal error in task, -1 on error,
 *         -2 on failure to find the task.
 */
int
wait_for_task_stop (gnutls_session_t* session, const char* id)
{
  while (1)
    {
      if (openvas_server_sendf (session, "<get_status/>") == -1)
        return -1;

      /* Read the response. */

      entity_t entity = NULL;
      if (read_entity (session, &entity)) return -1;

      /* Check the response. */

      const char* status = entity_attribute (entity, "status");
      if (status == NULL)
        {
          free_entity (entity);
          return -1;
        }
      if (strlen (status) == 0)
        {
          free_entity (entity);
          return -1;
        }
      if (status[0] == '2')
        {
          /* Check the running status of the given task. */

          char* run_state = NULL;

#if 0
          /* Lisp version. */
          (do-children (entity child)
            (when (string= (entity-type child) "task")
              (let ((task-id (entity-attribute child "task_id")))
                (fi* task-id
                  (free-entity entity)
                  (return-from wait-for-task-start -1))
                (when (string= task-id id)
                  (let ((status (entity-child child "status")))
                    (fi* status
                      (free-entity entity)
                      (return-from wait-for-task-start -1))
                    (setq run-state (entity-text status)))
                  (return)))))
#endif

          DO_CHILDREN (entity, child, temp,
                       if (strcasecmp (entity_name (child), "task") == 0)
                         {
                           const char* task_id = entity_attribute (child, "id");
                           if (task_id == NULL)
                             {
                               free_entity (entity);
                               return -1;
                             }
                           if (strcasecmp (task_id, id) == 0)
                             {
                               entity_t status = entity_child (child, "status");
                               if (status == NULL)
                                 {
                                   free_entity (entity);
                                   return -1;
                                 }
                               run_state = entity_text (status);
                               break;
                             }
                         });

          if (run_state == NULL)
            {
              free_entity (entity);
              return -2;
            }

          if (strcmp (run_state, "Stopped") == 0)
            {
              free_entity (entity);
              return 0;
            }
          if (strcmp (run_state, "Done") == 0)
            {
              free_entity (entity);
              return 0;
            }
          if (strcmp (run_state, "Internal Error") == 0)
            {
              free_entity (entity);
              return 1;
            }
          free_entity (entity);
        }

      sleep (1);
    }
}

/**
 * @brief Wait for the manager to actually remove a task.
 *
 * @param[in]  session  Pointer to GNUTLS session.
 * @param[in]  id       ID of task.
 *
 * @return 0 on success, -1 on error.
 */
int
wait_for_task_delete (gnutls_session_t* session,
                      const char* id)
{
  while (1)
    {
      entity_t entity;
      const char* status;

      if (openvas_server_sendf (session,
                                "<get_status task_id=\"%s\"/>",
                                id)
          == -1)
        return -1;

      entity = NULL;
      if (read_entity (session, &entity)) return -1;

      status = task_status (entity);
      free_entity (entity);
      if (status == NULL) break;

      sleep (1);
    }
  return 0;
}

/**
 * @brief Delete a task and read the manager response.
 *
 * @param[in]  session  Pointer to GNUTLS session.
 * @param[in]  id       ID of task.
 *
 * @return 0 on success, -1 on error.
 */
int
delete_task (gnutls_session_t* session, const char* id)
{
  if (openvas_server_sendf (session,
                            "<delete_task task_id=\"%s\"/>",
                            id)
      == -1)
    return -1;

  /* Read the response. */

  entity_t entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  const char* status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  char first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return -1;
}

/**
 * @brief Get the status of a task.
 *
 * @param[in]  session         Pointer to GNUTLS session.
 * @param[in]  id              ID of task or NULL for all tasks.
 * @param[in]  include_rcfile  Request rcfile in status if true.
 * @param[out] status          Status return.  On success contains GET_STATUS
 *                             response.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_get_status (gnutls_session_t* session, const char* id, int include_rcfile,
                entity_t* status)
{
  const char* status_code;
  int ret;

  if (id == NULL)
    {
      if (openvas_server_sendf (session,
                                "<get_status rcfile=\"%i\"/>",
                                include_rcfile)
          == -1)
        return -1;
    }
  else
    {
      if (openvas_server_sendf (session,
                                "<get_status task_id=\"%s\" rcfile=\"%i\"/>",
                                id,
                                include_rcfile)
          == -1)
        return -1;
    }

  /* Read the response. */

  *status = NULL;
  if (read_entity (session, status)) return -1;

  /* Check the response. */

  status_code = entity_attribute (*status, "status");
  if (status_code == NULL)
    {
      free_entity (*status);
      return -1;
    }
  if (strlen (status_code) == 0)
    {
      free_entity (*status);
      return -1;
    }
  if (status_code[0] == '2') return 0;
  ret = (int) strtol (status_code, NULL, 10);
  free_entity (*status);
  if (errno == ERANGE) return -1;
  return ret;
}

/**
 * @brief Get a report.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[in]  id        ID of report.
 * @param[out] response  Report.  On success contains GET_REPORT response.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_get_report (gnutls_session_t* session, const char* id, entity_t* response)
{
  if (openvas_server_sendf (session,
                            "<get_report format=\"nbe\" report_id=\"%s\"/>",
                            id))
    return -1;

  *response = NULL;
  if (read_entity (session, response)) return -1;

  // FIX check status

  return 0;
}

/**
 * @brief Remove a report.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[in]  id        ID of report.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_delete_report (gnutls_session_t* session, const char* id)
{
  entity_t response;

  if (openvas_server_sendf (session, "<delete_report report_id=\"%s\"/>", id))
    return -1;

  response = NULL;
  if (read_entity (session, &response)) return -1;

  // FIX check status

  free_entity (response);
  return 0;
}

/**
 * @brief Remove a task.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[in]  id        ID of task.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_delete_task (gnutls_session_t* session, const char* id)
{
  entity_t response;

  if (openvas_server_sendf (session, "<delete_task task_id=\"%s\"/>", id))
    return -1;

  response = NULL;
  if (read_entity (session, &response)) return -1;

  // FIX check status

  free_entity (response);
  return 0;
}

/**
 * @brief Modify a task.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[in]  id        ID of task.
 * @param[in]  rcfile    NULL or new RC file (as plain text).
 * @param[in]  name      NULL or new name.
 * @param[in]  comment   NULL or new comment.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_modify_task (gnutls_session_t* session, const char* id,
                 const char* rcfile, const char* name, const char* comment)
{
  entity_t response;

  if (openvas_server_sendf (session, "<modify_task task_id=\"%s\">", id))
    return -1;

  if (rcfile)
    {
      if (strlen (rcfile) == 0)
        {
          if (openvas_server_send (session, "<rcfile></rcfile>"))
            return -1;
        }
      else
        {
          gchar *base64_rc = g_base64_encode ((guchar*) rcfile,
                                              strlen (rcfile));
          int ret = openvas_server_sendf (session,
                                          "<rcfile>%s</rcfile>",
                                          base64_rc);
          g_free (base64_rc);
          if (ret) return -1;
        }
    }

  if (name && openvas_server_sendf (session, "<name>%s</name>", name))
    return -1;

  if (comment
      && openvas_server_sendf (session, "<comment>%s</comment>", comment))
    return -1;

  if (openvas_server_send (session, "</modify_task>"))
    return -1;

  response = NULL;
  if (read_entity (session, &response)) return -1;

  // FIX check status

  free_entity (response);
  return 0;
}

/**
 * @brief Get the manager preferences.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[out] response  On success contains GET_PREFERENCES response.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_get_preferences (gnutls_session_t* session, entity_t* response)
{
  if (openvas_server_send (session, "<get_preferences/>"))
    return -1;

  *response = NULL;
  if (read_entity (session, response)) return -1;

  // FIX check status

  return 0;
}

/**
 * @brief Get the manager certificates.
 *
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[out] response  On success contains GET_CERTIFICATES response.
 *
 * @return 0 on success, -1 or OMP response code on error.
 */
int
omp_get_certificates (gnutls_session_t* session, entity_t* response)
{
  const char* status_code;
  int ret;

  if (openvas_server_send (session, "<get_preferences/>"))
    return -1;

  *response = NULL;
  if (read_entity (session, response)) return -1;

  /* Check the response. */

  status_code = entity_attribute (*response, "status");
  if (status_code == NULL)
    {
      free_entity (*response);
      return -1;
    }
  if (strlen (status_code) == 0)
    {
      free_entity (*response);
      return -1;
    }
  if (status_code[0] == '2') return 0;
  ret = (int) strtol (status_code, NULL, 10);
  free_entity (*response);
  if (errno == ERANGE) return -1;
  return ret;
}

/**
 * @brief Poll an OMP service until it is up.
 *
 * Repeatedly call a function while it returns the value 503.
 *
 * @param[in]  function  Function to call to do polling.
 * @param[in]  session   Pointer to GNUTLS session.
 * @param[out] response  On success contains GET_CERTIFICATES response.
 *
 * @return The value returned from the function.
 */
int
omp_until_up (int (*function) (gnutls_session_t*, entity_t*),
              gnutls_session_t* session,
              entity_t* response)
{
  int ret;
  while ((ret = function (session, response)) == 503);
  return ret;
}

/**
 * @brief Create a target.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   name        Name of target.
 * @param[in]   hosts       Target hosts.
 * @param[in]   comment     Target comment.
 *
 * @return 0 on success, -1 on error.
 */
int
omp_create_target (gnutls_session_t* session,
                   const char* name,
                   const char* hosts,
                   const char* comment)
{
  int ret;
  entity_t entity;
  const char* status;

  /* Create the OMP request. */

  gchar* new_task_request;
  if (comment)
    new_task_request = g_strdup_printf ("<create_target>"
                                        "<name>%s</name>"
                                        "<hosts>%s</hosts>"
                                        "<comment>%s</comment>"
                                        "</create_target>",
                                        name,
                                        hosts,
                                        comment);
  else
    new_task_request = g_strdup_printf ("<create_target>"
                                        "<name>%s</name>"
                                        "<hosts>%s</hosts>"
                                        "</create_target>",
                                        name,
                                        hosts);

  /* Send the request. */

  ret = openvas_server_send (session, new_task_request);
  g_free (new_task_request);
  if (ret) return -1;

  /* Read the response. */

  entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  char first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return -1;
}

/**
 * @brief Delete a target.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   name        Name of target.
 *
 * @return 0 on success, -1 on error.
 */
int
omp_delete_target (gnutls_session_t* session,
                   const char* name)
{
  int ret;
  entity_t entity;
  const char* status;

  /* Create the OMP request. */

  gchar* new_task_request;
  new_task_request = g_strdup_printf ("<delete_target>"
                                      "<name>%s</name>"
                                      "</delete_target>",
                                      name);

  /* Send the request. */

  ret = openvas_server_send (session, new_task_request);
  g_free (new_task_request);
  if (ret) return -1;

  /* Read the response. */

  entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  char first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return -1;
}

/**
 * @brief Create a config, given the config description as a string.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   name        Config name.
 * @param[in]   comment     Config comment.
 * @param[in]   config      Config configuration.
 * @param[in]   config_len  Length of config.
 *
 * @return 0 on success, -1 on error.
 */
int
omp_create_config (gnutls_session_t* session,
                   const char* name,
                   const char* comment,
                   const char* config,
                   unsigned int config_len)
{
  /* Convert the file contents to base64. */

  gchar* new_config_file = strlen (config)
                           ? g_base64_encode ((guchar*) config, config_len)
                           : g_strdup ("");

  /* Create the OMP request. */

  gchar* new_config_request;
  if (comment)
    new_config_request = g_strdup_printf ("<create_config>"
                                          "<name>%s</name>"
                                          "<comment>%s</comment>"
                                          "<rcfile>%s</rcfile>"
                                          "</create_config>",
                                          name,
                                          comment,
                                          new_config_file);
  else
    new_config_request = g_strdup_printf ("<create_config>"
                                          "<name>%s</name>"
                                          "<rcfile>%s</rcfile>"
                                          "</create_config>",
                                          name,
                                          new_config_file);
  g_free (new_config_file);

  /* Send the request. */

  int ret = openvas_server_send (session, new_config_request);
  g_free (new_config_request);
  if (ret) return -1;

  /* Read the response. */

  entity_t entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  const char* status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  char first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return -1;
}

/**
 * @brief Create a config, given the config description as an RC file.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   name        Config name.
 * @param[in]   comment     Config comment.
 * @param[in]   file_name   Name of RC file.
 *
 * @return 0 on success, -1 on error.
 */
int
omp_create_config_from_rc_file (gnutls_session_t* session,
                                const char* name,
                                const char* comment,
                                const char* file_name)
{
  gchar* new_config_rc = NULL;
  gsize new_config_rc_len;
  GError* error = NULL;
  int ret;

  /* Read in the RC file. */

  g_file_get_contents (file_name,
                       &new_config_rc,
                       &new_config_rc_len,
                       &error);
  if (error)
    {
      g_error_free (error);
      return -1;
    }

  ret = omp_create_config (session,
                           name,
                           comment,
                           new_config_rc,
                           new_config_rc_len);
  g_free (new_config_rc);
  return ret;
}

/**
 * @brief Delete a config.
 *
 * @param[in]   session     Pointer to GNUTLS session.
 * @param[in]   name        Name of config.
 *
 * @return 0 on success, -1 on error.
 */
int
omp_delete_config (gnutls_session_t* session,
                   const char* name)
{
  int ret;
  entity_t entity;
  const char* status;

  /* Create the OMP request. */

  gchar* new_task_request;
  new_task_request = g_strdup_printf ("<delete_config>"
                                      "<name>%s</name>"
                                      "</delete_config>",
                                      name);

  /* Send the request. */

  ret = openvas_server_send (session, new_task_request);
  g_free (new_task_request);
  if (ret) return -1;

  /* Read the response. */

  entity = NULL;
  if (read_entity (session, &entity)) return -1;

  /* Check the response. */

  status = entity_attribute (entity, "status");
  if (status == NULL)
    {
      free_entity (entity);
      return -1;
    }
  if (strlen (status) == 0)
    {
      free_entity (entity);
      return -1;
    }
  char first = status[0];
  free_entity (entity);
  if (first == '2') return 0;
  return -1;
}