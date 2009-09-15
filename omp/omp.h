/* openvase-libraries/omp
 * $Id$
 * Description: Header for OMP client interface.
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

#ifndef _OPENVAS_LIBRARIES_OMP_H
#define _OPENVAS_LIBRARIES_OMP_H

#include "xml.h"

const char*
task_status (entity_t status_response);

int
authenticate (gnutls_session_t* session,
              const char* username,
              const char* password);

int
env_authenticate (gnutls_session_t* session);

int
create_task (gnutls_session_t*, const char*, unsigned int,
             const char*, const char*, char**);

int
omp_create_task (gnutls_session_t*, const char*, const char*,
                 const char*, const char*, char**);

int
create_task_from_rc_file (gnutls_session_t*, const char*, const char*,
                          const char*, char**);

int
delete_task (gnutls_session_t*, const char*);

int
start_task (gnutls_session_t* , const char*);

int
wait_for_task_end (gnutls_session_t*, const char*);

int
wait_for_task_start (gnutls_session_t*, const char*);

int
wait_for_task_stop (gnutls_session_t*, const char*);

int
wait_for_task_delete (gnutls_session_t*, const char*);

int
omp_get_status (gnutls_session_t*, const char*, int, entity_t*);

int
omp_get_report (gnutls_session_t*, const char*, entity_t*);

int
omp_delete_report (gnutls_session_t*, const char*);

int
omp_delete_task (gnutls_session_t*, const char*);

int
omp_modify_task (gnutls_session_t*, const char*,
                 const char*, const char*, const char*);

int
omp_get_preferences (gnutls_session_t*, entity_t*);

int
omp_get_certificates (gnutls_session_t*, entity_t*);

int
omp_until_up (int (*) (gnutls_session_t*, entity_t*),
              gnutls_session_t*,
              entity_t*);

int
omp_create_target (gnutls_session_t*, const char*, const char*, const char*);

int
omp_delete_target (gnutls_session_t*, const char*);

int
omp_create_config (gnutls_session_t*, const char*, const char*, const char*,
                   unsigned int);

int
omp_create_config_from_rc_file (gnutls_session_t*, const char*, const char*,
                                const char*);

int
omp_delete_config (gnutls_session_t*, const char*);

#endif /* not _OPENVAS_LIBRARIES_OMP_H */