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
import _prelude
import _preludedb


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



# class DBNoResult(Exception):
#     pass



class PreludeDBMessageIdentList:
    """Internal class, should not be used directly"""
    
    def __init__(self, parent, res, destroy_list_func, get_next_ident_func, get_ident_list_len_func):
        self.parent = parent
        self.res = res
        self.destroy_list_func = destroy_list_func
        self.get_next_ident_func = get_next_ident_func
        self.get_ident_list_len_func = get_ident_list_len_func

    def __del__(self):
        self.destroy_list_func(self.res)

    def __len__(self):
        return self.get_ident_list_len_func(self.res)

    def __iter__(self):
        return self

    def next(self):
        ident_handle = self.get_next_ident_func(self.res)
        if not ident_handle:
            raise StopIteration
        
        analyzerid = _preludedb.prelude_db_message_ident_get_analyzerid(ident_handle)
        ident = _preludedb.prelude_db_message_ident_get_ident(ident_handle)

        _preludedb.prelude_db_message_ident_destroy(ident_handle)

        return analyzerid, ident
        

class PreludeDB:
    """preludedb main class"""

    def init():
        """Static method, MUST be called before any instantiation of PreludeDB"""
        if _preludedb.prelude_db_init() < 0:
            raise Error()
    init = staticmethod(init)

    def __init__(self,
                 iface="iface1",
                 class_="sql",
                 type="mysql",
                 format="classic",
                 host="localhost",
                 port=0,
                 name="prelude",
                 user="prelude",
                 password="prelude"):
        """PreludeDB constructor"""
        conn_string = "iface=%s class=%s type=%s format=%s host=%s port=%s name=%s user=%s pass=%s" % \
                      (iface, class_, type, format, host, port, name, user, password)
        self.res = _preludedb.prelude_db_interface_new_string(conn_string)
        if not self.res:
            raise DBBadInterface(conn_string)

    def __del__(self):
        """PreludeDB destructor"""
        _preludedb.prelude_db_interface_destroy(self.res)

    def errno(self):
        """Get the current libpreludedb errno.

        The errno is the errno of the underlying database"""
        errno = _preludedb.prelude_db_interface_errno(self.res)
        if errno < 0:
            raise Error()

        return errno

    def error(self):
        """Get the current libpreludedb error message.

        The error message is the error message of the underlying database"""
        error = _preludedb.prelude_db_interface_error(self.res)
        if not error:
            raise Error()

        return error

    def connect(self):
        """Connect to the database."""
        if _preludedb.prelude_db_interface_connect(self.res) < 0:
            raise DBConnectionFailed(self)

    def disconnect(self):
        """Disconnect from the database."""
        if _preludedb.prelude_db_interface_disconnect(self.res) < 0:
            raise DBError(self)

    def enable_message_cache(self, directory):
        """Enable libpreludedb message cache."""
        if _preludedb.prelude_db_interface_enable_message_cache(self.res, directory) < 0:
            raise Error()

    def sql(self):
        """Get the sql handle of the underlying sql database."""
        dbconn = _preludedb.prelude_db_interface_get_connection(self.res)
        if not dbconn:
            raise Error()

        sqlconn = _preludedb.prelude_db_connection_get_sql(dbconn)
        if not sqlconn:
            raise Error()

        return PreludeDBSQL(sqlconn)

    def get_alert_ident_list(self, criteria=None, limit=-1, offset=-1):
        """Get the alert ident list.

        Return an object to be iterated with a for in
        each iteration will return two integers: analyzerid and ident
        
        Return None if no results are available.
        """
        if criteria:
            criteria_res = criteria.res
        else:
            criteria_res = None
        
        ident_list_handle = _preludedb.prelude_db_interface_get_alert_ident_list(self.res, criteria_res, limit, offset)
        if not ident_list_handle:
            return None

        # we must pass the current db object (self) to the PreludeDBMessageIdentList constructor, this constructor
        # will store the reference to this "parent" object to ensure that the top db object will still exist when
        # the PreludeDBMessageIdentList will be destroyed (this destructor needs the db top object, and without
        # this reference trick, the db object could be destroyed before the PreludeDBMessageIdentList object, that will
        # lead to a crash)
        return PreludeDBMessageIdentList(self,
                                         ident_list_handle,
                                         _preludedb.prelude_db_interface_alert_ident_list_destroy,
                                         _preludedb.prelude_db_interface_get_next_alert_ident,
                                         _preludedb.prelude_db_interface_get_alert_ident_list_len)

    def get_heartbeat_ident_list(self, criteria=None, limit=-1, offset=-1):
        """Get the heartbeat ident list.

        Return an object to be iterated with a for in
        each iteration will return two integers: analyzerid and ident
        
        Return None if no results are available.
        """
        if criteria:
            criteria_res = criteria.res
        else:
            criteria_res = None
        
        ident_list_handle = _preludedb.prelude_db_interface_get_heartbeat_ident_list(self.res, criteria_res, limit, offset)
        if not ident_list_handle:
            return None
        
        return PreludeDBMessageIdentList(ident_list_handle,
                                         _preludedb.prelude_db_interface_heartbeat_ident_list_destroy,
                                         _preludedb.prelude_db_interface_get_next_heartbeat_ident,
                                         _preludedb.prelude_db_interface_get_heartbeat_ident_list_len)
        
    def __get_message(self, analyzerid, ident, get_message_func):
        ident_handle = _preludedb.prelude_db_message_ident_new(long(analyzerid), long(ident))
        if not ident_handle:
            raise Error()

        message_handle = get_message_func(self.res, ident_handle, None)
        
        _preludedb.prelude_db_message_ident_destroy(ident_handle)

        if not message_handle:
            return None

        return IDMEFMessage(message_handle)

    def get_alert(self, analyzerid, ident):
        """Get an alert."""
        return self.__get_message(analyzerid, ident, _preludedb.prelude_db_interface_get_alert)

    def get_heartbeat(self, analyzerid, ident):
        """Get a heartbeat."""
        return self.__get_message(analyzerid, ident, _preludedb.prelude_db_interface_get_heartbeat)

    def __delete_message(self, analyzerid, ident, delete_message_func):
        ident_handle = _preludedb.prelude_db_message_ident_new(long(analyzerid), long(ident))
        if not ident_handle:
            raise Error()

        ret = delete_message_func(self.res, ident_handle)
        
        _preludedb.prelude_db_message_ident_destroy(ident_handle)

        if ret < 0:
            raise DBError(self)

    def delete_alert(self, analyzerid, ident):
        """Delete an alert."""
        self.__delete_message(analyzerid, ident, _preludedb.prelude_db_interface_delete_alert)

    def delete_heartbeat(self, analyzerid, ident):
        """Delete a heartbeat."""
        self.__delete_message(analyzerid, ident, _preludedb.prelude_db_interface_delete_heartbeat)

    def insert(self, message):
        "Insert an IDMEF message in the db."
        if _preludedb.prelude_db_interface_insert_idmef_message(self.res, message.res) < 0:
            raise DBError(self)

    def get_values(self, selection, criteria=None, distinct=0, limit=-1, offset=-1):
        """Get object values from the database."""
        selection_handle = _preludedb.prelude_db_object_selection_new()
        if not selection_handle:
            raise Error()

        for selected in selection:
            selected_handle = _preludedb.prelude_db_selected_object_new_string(selected)
            if not selected_handle:
                _preludedb.prelude_db_object_selection_destroy(selection_handle)
                raise Error()

            _preludedb.prelude_db_object_selection_add(selection_handle, selected_handle)

        if criteria:
            criteria_res = criteria.res
        else:
            criteria_res = None

        res = _preludedb.prelude_db_interface_select_values(self.res, selection_handle, criteria_res, distinct, limit, offset)
        if not res:
            _preludedb.prelude_db_object_selection_destroy(selection_handle)
            if self.errno():
                raise DBError(self)
            return None

        row_list = [ ]

        while True:
            objval_list = _preludedb.prelude_db_interface_get_values(self.res, res, selection_handle)
            if not objval_list:
                break

            row = [ ]

            while True:
                objval = _prelude.idmef_object_value_list_get_next(objval_list)
                if not objval:
                    break

                value = _prelude.idmef_object_value_get_value(objval)
                if not value:
                    _preludedb.prelude_db_object_selection_destroy(selection_handle)
                    _prelude.idmef_object_value_list_destroy(objval_list)
                    raise Error()

                tmp = idmef_value_c_to_python(value)
                if tmp is None:
                    _preludedb.prelude_db_object_selection_destroy(selection_handle)
                    _prelude.idmef_object_value_list_destroy(objval_list)
                    raise Error()

                row.append(tmp)
                
            _prelude.idmef_object_value_list_destroy(objval_list)

            row_list.append(row)

        _preludedb.prelude_db_object_selection_destroy(selection_handle)

        return row_list



class PreludeDBSQL:
    """libpreludedb sql class.

    You can get a PreludeDBSQL object by calling the sql method on a PreludeDB
    object.
    """
    def __init__(self, res):
        self.res = res

    def errno(self):
        """Get current errno."""
        return _preludedb.prelude_sql_errno(self.res)

    def error(self):
        """Get current error message."""
        return _preludedb.prelude_sql_error(self.res)

    def query(self, query):
        """Execute a query.

        Return a PreludeDBSQLTable object if a result a available, None otherwise."""
        table = _preludedb.prelude_sql_query(self.res, query)
        if not table:
            if self.errno():
                raise SQLError(self)
            return None

        return PreludeDBSQLTable(table)

    def begin(self):
        """Execute an sql begin query."""
        if _preludedb.prelude_sql_begin(self.res) < 0:
            raise SQLError(self)

    def commit(self):
        """Execute an sql commit query."""
        if _preludedb.prelude_sql_commit(self.res) < 0:
            raise SQLError(self)

    def rollback(self):
        """Execute an sql rollback query."""
        if _preludedb.prelude_sql_rollback(self.res) < 0:
            raise SQLError(self)



def _prelude_sql_field_to_python(field):
     type = _preludedb.prelude_sql_field_info_type(field)

     if type == _preludedb.dbtype_int32:
         return _preludedb.prelude_sql_field_value_int32(field)
     
     if type == _preludedb.dbtype_uint32:
         return _preludedb.prelude_sql_field_value_uint32(field)
     
     if type == _preludedb.dbtype_int64:
         return _preludedb.prelude_sql_field_value_int64(field)
     
     if type == _preludedb.dbtype_uint64:
         return _preludedb.prelude_sql_field_value_uint64(field)
     
     if type == _preludedb.dbtype_float:
         return _preludedb.prelude_sql_field_value_float(field)
     
     if type == _preludedb.dbtype_double:
         return _preludedb.prelude_sql_field_value_double(field)
     
     if type == _preludedb.dbtype_string:
         return _preludedb.prelude_sql_field_value(field)

     raise Error()



class PreludeDBSQLTable:
    """PreludeDB sql table class.

    Contain the result of the query method of a PreludeDBSQL object."""
    
    def __init__(self, res):
        self.res = res
        self.rows = self.rows_num()

    def __del__(self):
        _preludedb.prelude_sql_table_free(self.res)

    def __list__(self):
        """Get the the list of rows contained by the table."""
        return [row for row in self]

    def __repr__(self):
        return str(list(self))

    def field_name(self, i):
        """Get the name of the field number represented by i."""
        return _preludedb.prelude_sql_field_name(self.res, i)

    def fields_num(self):
        """Get the number of fields (columns) in the table."""
        return _preludedb.prelude_sql_fields_num(self.res)

    def rows_num(self):
        """Get the number of rows in the table."""
        return _preludedb.prelude_sql_rows_num(self.res)

    def row_fetch(self):
        """Fetch the next row from the table.

        Return a PreludeDBSQLRow object containing the row or
        None if there is no more rows to fetch"""
        row = _preludedb.prelude_sql_row_fetch(self.res)
        if not row:
            return None

        return PreludeDBSQLRow(self, row)

    def __iter__(self):
        """Return an object to iterate on the rows of the table."""
        return self

    def next(self):
        if self.rows == 0:
            raise StopIteration
        self.rows -= 1

        return self.row_fetch()



class PreludeDBSQLRow:
    """PreludeDB sql row class.

    Contain the result of a row returned by a PreludeDBSQLTable object.
    """
    def __init__(self, table, res):
        self.table = table
        self.res = res

    def __list__(self):
        """Get the the list of fields contained by the row."""
        return [field for field in self]

    def __repr__(self):
        return str(list(self))

    def __getitem__(self, index):
        """Get a field from the row.

        The field can be either identified by its field number or its field name.
        """
        
        if type(index) is int:
            field = _preludedb.prelude_sql_field_fetch(self.res, index)
        elif type(index) is str:
            field = _preludedb.prelude_sql_field_fetch_by_name(self.res, index)
        else:
            raise TypeError()

        return _prelude_sql_field_to_python(field)

    def __iter__(self):
        """Return an object to iterate on the fields of the row."""
        self.index = 0
        return self

    def next(self):
        if self.index == self.table.fields_num():
            raise StopIteration

        ret = self[self.index]
        self.index += 1

        return ret
