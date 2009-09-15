/* OpenVAS
 *
 * $Id$
 * Description: NASL API implementation for WMI support
 *
 * Authors:
 * Chandrashekhar B <bchandra@secpod.com>
 *
 * Copyright:
 * Copyright (c) 2009 Greenbone Networks GmbH, http://www.greenbone.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * (or any later version), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file nasl_wmi.c
 *
 * @brief NASL WMI functions
 *
 * Provides WMI (Windows Management Instrumentation) functionalities via calling
 * functions of a appropriate library.
 * The API offers three groups of functions:
 * 1. WMI_FUNCTIONS
 * 2. WMI_RSOP_FUNCTIONS (RSOP = Resultant Set of Policy)
 * 3. WMI_REGISTRY_FUNCTIONS
 */

/**
 * @TODO Check for memleak and document reference counting in tree cells.
 *       In some cases, after a tree_cell (typically retc) has been allocated
 *       with alloc_tree_cell, it is not later freed or deref_tree_cell'ed. It
 *       has to evaluated if that is okay or leads to memory leaks.
 */

#include <stdio.h>
#include <string.h>

#include "nasl_wmi.h"
#include "openvas_wmi_interface.h"

/**
 * @brief Get a version string of the WMI implementation.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL in case no implementation is present.
 *         Else a tree_cell with the version as string.
 */
tree_cell *
nasl_wmi_versioninfo (lex_ctxt * lexic)
{
  char * version = wmi_versioninfo ();
  tree_cell * retc = alloc_tree_cell (0, NULL);

  if (!retc)
    return NULL;

  if (!version)
    {
      return NULL;
    }

  retc->type = CONST_DATA;
  retc->x.str_val = strdup (version);
  retc->size = strlen (version);

  return retc;
}

/*
################################################################################
 WMI_FUNCTIONS
################################################################################
*/

/**
 * @brief Connect to a WMI service and  return a handle for it.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL in case the connection could not be established.
 *         Else a tree_cell with the handle.
 *
 * Retrieves local variables "host", "username", "password" and "ns"
 * from the lexical context, performs and connects to this given
 * WMI service returning a handle for the service as integer.
 */
tree_cell *
nasl_wmi_connect (lex_ctxt * lexic)
{
  char *host = get_str_local_var_by_name (lexic, "host");
  char *username = get_str_local_var_by_name (lexic, "username");
  char *password = get_str_local_var_by_name (lexic, "password");
  char *namespace = get_str_local_var_by_name (lexic, "ns");
  WMI_HANDLE handle;
  int value;

  if (namespace == NULL)
    namespace = "root\\cimv2";

  if ((host == NULL) || (username == NULL) || (password == NULL))
    {
      fprintf (stderr, "nasl_wmi_connect: Invalid input arguments\n");
      return NULL;
    }

  if ((strlen (password) == 0) || (strlen (username) == 0)
      || strlen (host) == 0)
    {
      fprintf (stderr, "nasl_wmi_connect: Invalid input arguments\n");
      return NULL;
    }

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_INT;
  value = wmi_connect (username, password, host, namespace, &handle);
  if (value == -1)
    {
      fprintf (stderr, "nasl_wmi_connect: WMI Connect failed\n");
      return NULL;
    }

  retc->x.i_val = (int) handle;
  return retc;
}

/**
 * @brief Close WMI service handle.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL in case of a serious problem. Else returns a
 *         treecell with integer == 1.
 *
 * Retrieves local variable "wmi_handle" from the lexical context
 * and closes the respective handle.
 */
tree_cell *
nasl_wmi_close (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);
  if (!handle)
    return NULL;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_INT;

  if (wmi_close (handle) == 0);
  {
    retc->x.i_val = 1;
    return retc;
  }
  return NULL;
}

/**
 * @brief Perform WQL query.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL in case the query can not be executed properly.
 *         Else a tree_cell with the result of the query as string.
 *
 * Retrieves local variables "wmi_handle" and "query" from the lexical
 * context, performs a WMI query on the given handle and returns the
 * result as a string.
 */
tree_cell *
nasl_wmi_query (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);
  char *query = get_str_local_var_by_name (lexic, "query");
  char *res = NULL;
  int value;

  if (!handle)
    return NULL;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_query (handle, query, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "wmi_query: WMI query failed '%s'\n", query);
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);

  return retc;
}

/*
################################################################################
 WMI_RSOP_FUNCTIONS
################################################################################
*/

/**
 * @brief Connect to a WMI RSOP service and return a handle for it.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL in case the connection could not be established.
 *         Else a tree_cell with the handle.
 *
 * Retrieves local variables "host", "username", "password"
 * from the lexical context, performs and connects to this given
 * WMI service returning a handle for the service as integer.
 */
tree_cell *
nasl_wmi_connect_rsop (lex_ctxt * lexic)
{
  char *host = get_str_local_var_by_name (lexic, "host");
  char *username = get_str_local_var_by_name (lexic, "username");
  char *password = get_str_local_var_by_name (lexic, "password");
  WMI_HANDLE handle;
  int value;

  if ((host == NULL) || (username == NULL) || (password == NULL))
    {
      fprintf (stderr, "nasl_wmi_connect_rsop: Invalid input arguments\n");
      return NULL;
    }

  if ((strlen (password) == 0) || (strlen (username) == 0)
      || strlen (host) == 0)
    {
      fprintf (stderr, "nasl_wmi_connect_rsop: Invalid input arguments\n");
      return NULL;
    }

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_INT;
  value = wmi_connect_rsop (username, password, host, &handle);
  if (value == -1)
    {
      fprintf (stderr, "nasl_wmi_connect_rsop: WMI RSOP Connect failed\n");
      return NULL;
    }

  retc->x.i_val = (int) handle;
  return retc;
}

/**
 * @brief WMI RSOP query.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL on failure, 1 on success
 *
 * Retrieves local variables "wmi_handle", "query"
 * from the lexical context, performs the RSOP query returning
 * results in string format.
 */
tree_cell *
nasl_wmi_query_rsop (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);
  if (!handle)
    return NULL;

  char *query = get_str_local_var_by_name (lexic, "query");     // WQL query
  char *res = NULL;
  int value;
  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_query_rsop (handle, query, &res);
  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "wmi_query_rsop: WMI query failed\n");
      return NULL;
    }
  retc->x.str_val = strdup (res);
  retc->size = strlen (res);

  return retc;
}

/*
################################################################################
 WMI_REGISTRY_FUNCTIONS
################################################################################
*/

/**
 * @brief Connect to a WMI Registry service and return a handle for it.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL in case the connection could not be established.
 *         Else a tree_cell with the handle.
 *
 * Retrieves local variables "host", "username", "password"
 * from the lexical context, performs and connects to this given
 * WMI service returning a handle for the service as integer.
 */
tree_cell *
nasl_wmi_connect_reg (lex_ctxt * lexic)
{
  char *host = get_str_local_var_by_name (lexic, "host");
  char *username = get_str_local_var_by_name (lexic, "username");
  char *password = get_str_local_var_by_name (lexic, "password");
  WMI_HANDLE handle;
  int value;

  if ((host == NULL) || (username == NULL) || (password == NULL))
    {
      fprintf (stderr, "nasl_wmi_connect_reg: Invalid input arguments\n");
      return NULL;
    }

  if ((strlen (password) == 0) || (strlen (username) == 0)
      || strlen (host) == 0)
    {
      fprintf (stderr, "nasl_wmi_connect_reg: Invalid input arguments\n");
      return NULL;
    }

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_INT;
  value = wmi_connect_reg (username, password, host, &handle);
  if (value == -1)
    {
      fprintf (stderr, "nasl_wmi_connect_reg: WMI REGISTRY Connect failed\n");
      return NULL;
    }

  retc->x.i_val = (int) handle;
  return retc;
}

/**
 * @brief Get string value from Registry.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL if the query fails.
 *         Else a tree_cell with the Registry value.
 *
 * Retrieves local variables "wmi_handle", "key", "key_name"
 * from the lexical context, performs the registry query
 * returning a string value.
 */
tree_cell *
nasl_wmi_reg_get_sz (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY
  char *key_name = get_str_local_var_by_name (lexic, "key_name");       // REGISTRY value name

  char *res = NULL;
  int value;
  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_get_sz (handle, key, key_name, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_get_sz: WMI Registry get failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);

  return retc;
}

/**
 * @brief Enumerate registry values.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL if the query fails.
 *         Else a tree_cell with the Registry values.
 *
 * Retrieves local variables "wmi_handle", "key"
 * from the lexical context, performs the registry query
 * returning a string value.
 */
tree_cell *
nasl_wmi_reg_enum_value (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY

  char *res = NULL;
  int value;
  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_enum_value (handle, key, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_enum_value: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);

  return retc;
}

/**
 * @brief Enumerate registry keys.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL if the query fails.
 *         Else a tree_cell with the Registry keys.
 *
 * Retrieves local variables "wmi_handle", "key"
 * from the lexical context, performs the registry query
 * returning a string value.
 */
tree_cell *
nasl_wmi_reg_enum_key (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY

  char *res = NULL;
  int value;
  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_enum_key (handle, key, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_enum_key: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);

  return retc;
}

/**
 * @brief Get registry binary value.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL on failure, else tree_cell containing string
 *         representation of binary value
 *
 * Retrieves local variables "wmi_handle", "key", "val_name"
 * from the lexical context, performs the registry operation
 * querying binary value.
 */
tree_cell *
nasl_wmi_reg_get_bin_val (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY
  char *val_name = get_str_local_var_by_name (lexic, "val_name");       // REGISTRY VALUE NAME

  char *res = NULL;
  int value;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_get_bin_val (handle, key, val_name, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_get_bin_val: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);
  return retc;
}

/**
 * @brief Get registry DWORD value.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL on failure, else tree_cell containing string
 *         representation of DWORD value
 *
 * Retrieves local variables "wmi_handle", "key", "val_name"
 * from the lexical context, performs the registry operation
 * querying DWORD value.
 */
tree_cell *
nasl_wmi_reg_get_dword_val (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY
  char *val_name = get_str_local_var_by_name (lexic, "val_name");       // REGISTRY VALUE NAME

  char *res = NULL;
  int value;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_get_dword_val (handle, key, val_name, &res);

  if ((value == 0) && (res == 0))
    res = "0";

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_get_dword_val: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);
  return retc;
}

/**
 * @brief Get registry expanded string value.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL on failure, else tree_cell containing string
 *         representation of Expanded String value
 *
 * Retrieves local variables "wmi_handle", "key", "val_name"
 * from the lexical context, performs the registry operation
 * querying Expanded string value.
 */
tree_cell *
nasl_wmi_reg_get_ex_string_val (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY
  char *val_name = get_str_local_var_by_name (lexic, "val_name");       // REGISTRY VALUE NAME

  char *res = NULL;
  int value;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_get_ex_string_val (handle, key, val_name, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_get_ex_string_val: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);
  return retc;
}

/**
 * @brief Get registry multi valued strings.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL on failure, else tree_cell containing string
 *         representation of multi valued strings
 *
 * Retrieves local variables "wmi_handle", "key", "val_name"
 * from the lexical context, performs the registry operation
 * querying Expanded string value.
 */
tree_cell *
nasl_wmi_reg_get_mul_string_val (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY
  char *val_name = get_str_local_var_by_name (lexic, "val_name");       // REGISTRY VALUE NAME

  char *res = NULL;
  int value;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_get_mul_string_val (handle, key, val_name, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_get_mul_string_val: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);
  return retc;
}

/**
 * @brief Get registry QWORD value.
 *
 * @param[in] lexic Lexical context of NASL interpreter.
 *
 * @return NULL on failure, else tree_cell containing string
 *         representation of QWORD value
 *
 * Retrieves local variables "wmi_handle", "key", "val_name"
 * from the lexical context, performs the registry operation
 * querying Expanded string value.
 */
tree_cell *
nasl_wmi_reg_get_qword_val (lex_ctxt * lexic)
{
  WMI_HANDLE handle =
    (WMI_HANDLE) get_int_local_var_by_name (lexic, "wmi_handle", 0);

  if (!handle)
    return NULL;

  char *key = get_str_local_var_by_name (lexic, "key"); // REGISTRY KEY
  char *val_name = get_str_local_var_by_name (lexic, "val_name");       // REGISTRY VALUE NAME

  char *res = NULL;
  int value;

  tree_cell *retc = alloc_tree_cell (0, NULL);
  if (!retc)
    return NULL;

  retc->type = CONST_DATA;
  retc->x.str_val = NULL;
  retc->size = 0;

  value = wmi_reg_get_qword_val (handle, key, val_name, &res);

  if ((value == -1) || (res == NULL))
    {
      fprintf (stderr, "nasl_wmi_reg_get_qword_val: WMI query failed\n");
      return NULL;
    }

  retc->x.str_val = strdup (res);
  retc->size = strlen (res);
  return retc;
}