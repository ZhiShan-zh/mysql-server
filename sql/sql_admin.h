/* Copyright (c) 2010 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_TABLE_MAINTENANCE_H
#define SQL_TABLE_MAINTENANCE_H


bool mysql_assign_to_keycache(THD* thd, TABLE_LIST* table_list,
                              LEX_STRING *key_cache_name);
bool mysql_preload_keys(THD* thd, TABLE_LIST* table_list);
int reassign_keycache_tables(THD* thd, KEY_CACHE *src_cache,
                             KEY_CACHE *dst_cache);

/**
  Analyze_statement represents the ANALYZE TABLE statement.
*/
class Analyze_table_statement : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a ANALYZE TABLE statement.
  */
  Analyze_table_statement()
  {}

  ~Analyze_table_statement()
  {}

  /**
    Execute a ANALYZE TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_ANALYZE;
  }
};



/**
  Check_table_statement represents the CHECK TABLE statement.
*/
class Check_table_statement : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a CHECK TABLE statement.
  */
  Check_table_statement()
  {}

  ~Check_table_statement()
  {}

  /**
    Execute a CHECK TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CHECK;
  }
};


/**
  Optimize_table_statement represents the OPTIMIZE TABLE statement.
*/
class Optimize_table_statement : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a OPTIMIZE TABLE statement.
  */
  Optimize_table_statement()
  {}

  ~Optimize_table_statement()
  {}

  /**
    Execute a OPTIMIZE TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_OPTIMIZE;
  }
};



/**
  Repair_table_statement represents the REPAIR TABLE statement.
*/
class Repair_table_statement : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a REPAIR TABLE statement.
  */
  Repair_table_statement()
  {}

  ~Repair_table_statement()
  {}

  /**
    Execute a REPAIR TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_REPAIR;
  }
};

#endif
