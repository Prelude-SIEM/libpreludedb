# Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
# All Rights Reserved
#
# This file is part of the Prelude program.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.


from prelude import *
from _preludedb import *

class Error(Exception):
    def __init__(self, desc="libpreludedb internal error"):
        self.desc = desc

    def __str__(self):
        return self.desc



class SQLError(Error):
    def __init__(self, sql):
        self.errno = sql.errno()
        self.error = sql.error()

    def __str__(self):
        return "%s (errno: %d)" % (self.error, self.errno)



class DBError(SQLError):
    def __init__(self, db):
        SQLError.__init__(self, db.sql())



class DBBadInterface(Error):
    def __init__(self, interface):
        self.interface = interface

    def __str__(self):
        return "Bad interface: " % self.interface



class DBConnectionFailed(Error):
    def __str__(self):
        return "Connection failed"



class DBMessageDeletionFailed(Error):
    def __init__(self, analyzerid, ident):
        self.analyzerid = analyzerid
        self.ident = ident



class PreludeDBMessageIdentList:
    def __init__(self, res, destroy_list_func, get_next_ident_func):
        self.res = res
        self.destroy_list_func = destroy_list_func
        self.get_next_ident_func = get_next_ident_func        

    def __del__(self):
        self.destroy_list_func(self.res)

    def __iter__(self):
        return self

    def next(self):
        ident_handle = self.get_next_ident_func(self.res)
        if not ident_handle:
            raise StopIteration
        
        analyzerid = prelude_db_message_ident_get_analyzerid(ident_handle)
        ident = prelude_db_message_ident_get_ident(ident_handle)

        prelude_db_message_ident_destroy(ident_handle)

        return analyzerid, ident
        

class PreludeDB:
    def init():
        if prelude_db_init() < 0:
            raise Error()
    init = staticmethod(init)

    def __init__(self,
                 iface="iface1",
                 class_="sql",
                 type="mysql",
                 format="classic",
                 host="localhost",
                 name="prelude",
                 user="prelude",
                 password="prelude"):
        conn_string = "iface=%s class=%s type=%s format=%s host=%s name=%s user=%s pass=%s" % \
                      (iface, class_, type, format, host, name, user, password)
        self.res = prelude_db_interface_new_string(conn_string)
        if not self.res:
            raise DBBadInterface(conn_string)

    def __del__(self):
        prelude_db_interface_destroy(self.res)

    def errno(self):
        errno = prelude_db_interface_errno(self.res)
        if errno < 0:
            raise Error()

        return errno

    def error(self):
        error = prelude_db_interface_error(self.res)
        if not error:
            raise Error()

        return error

    def connect(self):
        if prelude_db_interface_connect(self.res) < 0:
            raise DBConnectionFailed(self)

    def disconnect(self):
        if prelude_db_interface_disconnect(self.res) < 0:
            raise DBError(self)

    def enable_message_cache(self, directory):
        if prelude_db_interface_enable_message_cache(self.res, directory) < 0:
            raise Error()

    def sql(self):
        dbconn = prelude_db_interface_get_connection(self.res)
        if not dbconn:
            raise Error()

        sqlconn = prelude_db_connection_get_sql(dbconn)
        if not sqlconn:
            raise Error()

        return PreludeDBSQL(sqlconn)

    def get_alert_ident_list(self, criteria=None, limit=-1, offset=-1):
        ident_list_handle = prelude_db_interface_get_alert_ident_list(self.res, criteria, limit, offset)
        return PreludeDBMessageIdentList(ident_list_handle,
                                         prelude_db_interface_alert_ident_list_destroy,
                                         prelude_db_interface_get_next_alert_ident)

    def get_heartbeat_ident_list(self, criteria=None, limit=-1, offset=-1):
        ident_list_handle = prelude_db_interface_get_heartbeat_ident_list(self.res, criteria, limit, offset)
        return PreludeDBMessageIdentList(ident_list_handle,
                                         prelude_db_interface_heartbeat_ident_list_destroy,
                                         prelude_db_interface_get_next_heartbeat_ident)
        
    def get_message(self, analyzerid, ident, get_message_func):
        ident_handle = prelude_db_message_ident_new(long(analyzerid), long(ident))
        if not ident_handle:
            raise Error()

        message_handle = get_message_func(self.res, ident_handle, None)
        
        prelude_db_message_ident_destroy(ident_handle)

        return IDMEFMessage(message_handle)

    def get_alert(self, analyzerid, ident):
        return self.get_message(analyzerid, ident, prelude_db_interface_get_alert)

    def get_heartbeat(self, analyzerid, ident):
        return self.get_message(analyzerid, ident, prelude_db_interface_get_heartbeat)

    def delete_message(self, analyzerid, ident, delete_message_func):
        ident_handle = prelude_db_message_ident_new(long(analyzerid), long(ident))
        if not ident_handle:
            raise Error()

        ret = delete_message_func(self.res, ident_handle)
        
        prelude_db_message_ident_destroy(ident_handle)

        if ret < 0:
            raise DBError(self)

    def delete_alert(self, analyzerid, ident):
        self.delete_message(analyzerid, ident, prelude_db_interface_delete_alert)

    def delete_heartbeat(self, analyzerid, ident):
        self.delete_message(analyzerid, ident, prelude_db_interface_delete_heartbeat)

    def get_values(self, selection, criteria=None, distinct=0, limit=-1, offset=-1):
        selection_handle = prelude_db_object_selection_new()
        if not selection_handle:
            raise Error()

        for selected in selection:
            selected_handle = prelude_db_selected_object_new_string(selected)
            if not selected_handle:
                prelude_db_object_selection_destroy(selection_handle)
                raise Error()

            prelude_db_object_selection_add(selection_handle, selected_handle)

        if criteria:
            criteria_res = criteria.res
        else:
            criteria_res = None

        res = prelude_db_interface_select_values(self.res, selection_handle, criteria_res, distinct, limit, offset)
        if not res:
            prelude_db_object_selection_destroy(selection_handle)
            if self.errno():
                raise DBError(self)
            return None

        row_list = [ ]

        while True:
            objval_list = prelude_db_interface_get_values(self.res, res, selection_handle)
            if not objval_list:
                break

            row = [ ]

            while True:
                objval = idmef_object_value_list_get_next(objval_list)
                if not objval:
                    break

                value = idmef_object_value_get_value(objval)
                if not value:
                    prelude_db_object_selection_destroy(selection_handle)
                    idmef_object_value_list_destroy(objval)
                    raise Error()

                tmp = idmef_value_c_to_python(value)
                if not tmp:
                    prelude_db_object_selection_destroy(selection_handle)
                    idmef_object_value_list_destroy(objval)
                    raise Error()

                row.append(tmp)
                
            idmef_object_value_list_destroy(objval_list)

            row_list.append(row)

        prelude_db_object_selection_destroy(selection_handle)

        return row_list



class PreludeDBSQL:
    def __init__(self, res):
        self.res = res

    def errno(self):
        return prelude_sql_errno(self.res)

    def error(self):
        return prelude_sql_error(self.res)

    def query(self, query):
        table = prelude_sql_query(self.res, query)
        if not table:
            if self.errno():
                raise SQLError(self)
            return None

        return PreludeDBSQLTable(table)

    def begin(self):
        if prelude_sql_begin(self.res) < 0:
            raise SQLError(self)

    def commit(self):
        if prelude_sql_commit(self.res) < 0:
            raise SQLError(self)

    def rollback(self):
        if prelude_sql_rollback(self.res) < 0:
            raise SQLError(self)

    def close(self):
        if prelude_sql_close(self.res) < 0:
            raise SQLError(self)


def _prelude_sql_field_to_python(field):
     type = prelude_sql_field_info_type(field)

     if type == dbtype_int32:
         return prelude_sql_field_value_int32(field)
     if type == dbtype_uint32:
         return prelude_sql_field_value_uint32(field)
     if type == prelude_int64:
         return prelude_sql_field_value_int64(field)
     if type == prelude_uint64:
         return prelude_sql_field_value_uint64(field)
     if type == dbtype_float:
         return prelude_sql_field_value_float(field)
     if type == dbtype_double:
         return prelude_sql_field_value_double(field)
     if type == dbtype_string:
         return prelude_sql_field_value(field)

     raise Error()



class PreludeDBSQLTable:
    def __init__(self, res):
        self.res = res
        self.rows = self.rows_num()

    def __del__(self):
        prelude_sql_table_free(self.res)

    def __list__(self):
        return [row for row in self]

    def __repr__(self):
        return str(list(self))

    def field_name(self, i):
        return prelude_sql_field_name(self.res, i)

    def fields_num(self):
        return prelude_sql_fields_num(self.res)

    def rows_num(self):
        return prelude_sql_rows_num(self.res)

    def row_fetch(self):
        row = prelude_sql_row_fetch(self.res)
        if not row:
            return

        return PreludeDBSQLRow(self, row)

    def __iter__(self):
        return self

    def next(self):
        if self.rows == 0:
            raise StopIteration
        self.rows -= 1

        return self.row_fetch()



class PreludeDBSQLRow:
    def __init__(self, table, res):
        self.table = table
        self.res = res

    def __list__(self):
        return [field for field in self]

    def __repr__(self):
        return str(list(self))

    def __getitem__(self, index):
        if type(index) is int:
            field = prelude_sql_field_fetch(self.res, index)
        elif type(index) is str:
            field = prelude_sql_field_fetch_by_name(self.res, index)
        else:
            raise TypeError()

        return _prelude_sql_field_to_python(field)

    def __iter__(self):
        self.index = 0
        return self

    def next(self):
        if self.index == self.table.fields_num():
            raise StopIteration

        ret = self[self.index]
        self.index += 1

        return ret
        

        
