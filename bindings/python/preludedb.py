# Copyright (C) 2003-2005 Nicolas Delon <nicolas@prelude-ids.org>
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
    def __init__(self, desc="libpreludedb error"):
        self.desc = desc

    def __str__(self):
        return self.desc



class PreludeDB:
    """preludedb main class"""

    def init():
        """Static method, MUST be called before any instantiation of PreludeDB"""
        pass # nop
    init = staticmethod(init)

    def __init__(self,
                 type="mysql",
                 format=None,
                 host=None,
                 port=None,
                 name=None,
                 user=None,
                 password=None,
                 log=None):
        """PreludeDB constructor"""
        settings = _preludedb.preludedb_sql_settings_new()
        if host:
            _preludedb.preludedb_sql_settings_set_host(settings, host)
        if port:
            _preludedb.preludedb_sql_settings_set_port(settings, str(port))
        if name:
            _preludedb.preludedb_sql_settings_set_name(settings, name)
        if user:
            _preludedb.preludedb_sql_settings_set_user(settings, user)
        if password:
            _preludedb.preludedb_sql_settings_set_pass(settings, password)

        sql = _preludedb.preludedb_sql_new(type, settings)
        if not sql:
            _preludedb.preludedb_sql_settings_destroy(settings)
            raise Error("cannot create sql object")

        if log:
            ret = _preludedb.preludedb_sql_enable_query_logging(sql, log)
            if ret < 0:
                _preludedb.preludedb_sql_destroy(sql)
                raise Error("cannot enable query logging object")
        
        self.res = _preludedb.preludedb_new(sql, None)
        if not self.res:
            raise Error("cannot create preludedb object")

    def __del__(self):
        """PreludeDB destructor"""
        if self.res:
            _preludedb.preludedb_destroy(self.res)

    def connect(self):
        """Connect to the database."""
        if _preludedb.preludedb_connect(self.res) < 0:
            raise Error("cannot connect database")

    def disconnect(self):
        """Disconnect from the database."""
        _preludedb.preludedb_disconnect()
        
    def _get_message_idents(self, get_idents, criteria, limit, offset):
        if criteria:
            criteria_res = criteria.res
        else:
            criteria_res = None

        result = get_idents(self.res, criteria_res, limit, offset,
                            _preludedb.PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_DESC)
        if not result:
            return None

        idents = [ ]
        while True:
            ident = _preludedb.preludedb_result_idents_get_next(result)
            if ident is None:
                break

            idents.append(ident)

        _preludedb.preludedb_result_idents_destroy(result)

        return idents

    def get_alert_idents(self, criteria, limit, offset):
        return self._get_message_idents(_preludedb.preludedb_get_alert_idents, criteria, limit, offset)

    def get_heartbeat_idents(self, criteria, limit, offset):
        return self._get_message_idents(_preludedb.preludedb_get_heartbeat_idents, criteria, limit, offset)

    def _get_message(self, get_message, ident):
        message_handle = get_message(self.res, ident)
        if not message_handle:
            raise Error("cannot retrieve IDMEF message")

        return IDMEFMessage(message_handle)

    def get_alert(self, ident):
        """Get an alert."""
        return self._get_message(_preludedb.preludedb_get_alert, ident)

    def get_heartbeat(self, ident):
        """Get a heartbeat."""
        return self._get_message(_preludedb.preludedb_get_heartbeat, ident)

    def _delete_message(self, delete_message_func, ident):
        ret = delete_message_func(self.res, ident)
        if ret < 0:
            raise Error("cannot delete IDMEF message")

    def delete_alert(self, ident):
        """Delete an alert."""
        return self._delete_message(_preludedb.preludedb_delete_alert, ident)

    def delete_heartbeat(self, ident):
        """Delete a heartbeat."""
        return self._delete_message(_preludedb.preludedb_delete_heartbeat, ident)

    def get_values(self, selection, criteria=None, distinct=0, limit=-1, offset=-1):
        """Get object values from the database."""
        selection_handle = _preludedb.preludedb_object_selection_new()
        if not selection_handle:
            raise Error()

        for selected in selection:
            selected_handle = _preludedb.preludedb_selected_object_new_string(selected)
            if not selected_handle:
                _preludedb.preludedb_object_selection_destroy(selection_handle)
                raise Error()

            _preludedb.preludedb_object_selection_add(selection_handle, selected_handle)

        if criteria:
            criteria_res = criteria.res
        else:
            criteria_res = None

        result = _preludedb.preludedb_get_values(self.res, selection_handle, criteria_res, distinct, limit, offset)
        if not result:
            _preludedb.preludedb_object_selection_destroy(selection_handle)
            return None

        rows = [ ]

        while True:
            values = _preludedb.preludedb_result_values_get_next(result)
            if not values:
                break
            row = [ ]
            rows.append(row)
            for value in values:
                row.append(idmef_value_c_to_python(value))
                _prelude.idmef_value_destroy(value)

        _preludedb.preludedb_result_values_destroy(result)
        _preludedb.preludedb_object_selection_destroy(selection_handle)

        return rows
