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

require XSLoader;
XSLoader::load('PreludeDB');

use strict;

package PreludeDB;

sub	init
{
    return (prelude_db_init() < 0) ? 0 : 1;
}

sub	shutdown
{
    return (prelude_db_shutdown() < 0) ? 0 : 1;
}

sub	new
{
    my	$class = shift;
    my	$conn_string;
    my	$db;

    if ( @_ == 1 ) {
	$conn_string = $_[0];

    } else {
	my %opt = @_;

	$opt{-iface}  ||= "iface1";
	$opt{-class}  ||= "sql";
	$opt{-type}   ||= "mysql";
	$opt{-format} ||= "classic";
	$opt{-host}   ||= "localhost";
	$opt{-name}   ||= "prelude";
	$opt{-user}   ||= "prelude";
	$opt{-pass}   ||= "prelude";

	$conn_string =
	    "iface=$opt{-iface} " .
	    "class=$opt{-class} ".
	    "type=$opt{-type} " .
	    "host=$opt{-host} " .
	    "name=$opt{-name} " .
	    "user=$opt{-user} " .
	    "pass=$opt{-pass} " .
	    "format=$opt{-format}";
    }

    $db = prelude_db_interface_new_string($conn_string);
    return $db ? bless(\$db, $class) : undef;
}

sub	connect
{
    my	$self = shift;

    return (prelude_db_interface_connect($$self) < 0) ? 0 : 1;
}

sub	disconnect
{
    my	$self = shift;

    return (prelude_db_interface_disconnect($$self) < 0) ? 0 : 1;
}

sub	get_sql_connection
{
    my	$self = shift;
    my	$dbconn;
    my	$sqlconn;

    return undef if ( not defined($dbconn = prelude_db_interface_get_connection($$self)) );

    return undef if ( prelude_db_connection_get_type($dbconn) != $PreludeDB::prelude_db_type_sql );

    $sqlconn = prelude_db_connection_get($dbconn);

    return $sqlconn ? bless(\$sqlconn, "PreludeDBSQL") : undef;
}

sub	get_alert_uident_list
{
     my	$self = shift;
     my	$crit = shift;
     my	$uident_list_handle;
     my	@uident_list;
     my	$uident;

    $uident_list_handle = PreludeDB::prelude_db_interface_get_alert_uident_list($$self, $$crit) or return ();

     while ( ($uident = PreludeDB::prelude_db_interface_get_next_alert_uident($uident_list_handle)) ) {
	 push(@uident_list, $uident);
     }

     PreludeDB::prelude_db_interface_free_alert_uident_list($uident_list_handle);

     return @uident_list;
}

sub	get_heartbeat_uident_list
{
    my	$self = shift;
    my	$crit = shift;
    my	$uident_list_handle;
    my	@uident_list;
    my	$uident;

    $uident_list_handle = PreludeDB::prelude_db_interface_get_heartbeat_uident_list($$self, $$crit) or return ();

    while ( ($uident = PreludeDB::prelude_db_interface_get_next_heartbeat_uident($uident_list_handle)) ) {
	push(@uident_list, $uident);
    }

    PreludeDB::prelude_db_interface_free_heartbeat_uident_list($uident_list_handle);

    return @uident_list;
}

sub	_convert_object_list
{
    my	@object_list = @_;
    my	$object;
    my	$object_list_handle;

    $object_list_handle = Prelude::idmef_selection_new();

    foreach ( @object_list ) {
	$object = Prelude::idmef_object_new_fast($_);
	unless ( $object ) {
	    Prelude::idmef_selection_destroy($object_list_handle);
	    return undef;
	}

	if ( Prelude::idmef_selection_add_object($object_list_handle, $object, $Prelude::function_none) < 0 ) {
	    Prelude::idmef_object_destroy($object);
	    Prelude::idmef_selection_destroy($object_list_handle);
	    return undef;
	}
    }

    return $object_list_handle;
}

sub	get_alert
{
    my	$self = shift;
    my	$uident = shift;
    my	@object_list = @_;
    my	$object_list_handle;
    my	$message;

    if ( @object_list ) {
	$object_list_handle = _convert_object_list(@object_list) or return undef;
    }

    $message = PreludeDB::prelude_db_interface_get_alert($$self, $uident, $object_list_handle);

    Prelude::idmef_selection_destroy($object_list_handle) if ( defined $object_list_handle );

    return $message ? bless(\$message, "IDMEFMessage") : undef;
}

sub	get_heartbeat
{
    my	$self = shift;
    my	$uident = shift;
    my	@object_list = @_;
    my	$object_list_handle;
    my	$message;

    if ( @object_list ) {
	$object_list_handle = _convert_object_list(@object_list) or return undef;
    }

    $message = PreludeDB::prelude_db_interface_get_heartbeat($$self, $uident, $object_list_handle);

    Prelude::idmef_selection_destroy($object_list_handle) if ( defined $object_list_handle );

    return $message ? bless(\$message, "IDMEFMessage") : undef;
}

sub	delete_alert
{
    my	$self = shift;
    my	$uident = shift;

    return (PreludeDB::prelude_db_interface_delete_alert($$self, $uident) < 0) ? 0 : 1;
}

sub	delete_heartbeat
{
    my	$self = shift;
    my	$uident = shift;

    return (PreludeDB::prelude_db_interface_delete_heartbeat($$self, $uident) < 0) ? 0 : 1;
}

sub	DESTROY
{
    my	$self = shift;

    PreludeDB::prelude_db_interface_destroy($$self);
}



package PreludeDBSQL;

sub	errno
{
    my	$self = shift;

    return PreludeDB::prelude_sql_errno($$self);
}

sub	error
{
    my	$self = shift;

    return PreludeDB::prelude_sql_error($$self);
}

sub	query
{
    my	$self = shift;
    my	$query = shift;
    my	$table;

    $table = PreludeDB::prelude_sql_query($$self, $query);

    return $table ? bless(\$table, "PreludeDBSQLTable") : undef;
}

sub	begin
{
    my	$self = shift;

    return (PreludeDB::prelude_sql_begin($$self) < 0) ? 0 : 1;
}

sub	commit
{
    my	$self = shift;

    return (PreludeDB::prelude_sql_commit($$self) < 0) ? 0 : 1;
}

sub	rollback
{
    my	$self = shift;

    return (PreludeDB::prelude_sql_rollback($$self) < 0) ? 0 : 1;
}


sub	close
{
    my	$self = shift;

    return (PreludeDB::prelude_sql_close($$self) < 0) ? 0 : 1;
}



package PreludeDBSQLTable;

sub	field_name
{
    my	$self = shift;
    my	$i = shift;

    return PreludeDB::prelude_sql_field_name($$self, $i);
}

sub	fields_num
{
    my	$self = shift;

    return PreludeDB::prelude_sql_fields_num($$self);
}

sub	rows_num
{
    my	$self = shift;

    return PreludeDB::prelude_sql_rows_num($$self);
}

sub	row_fetch
{
    my	$self = shift;
    my	$row;

    $row = PreludeDB::prelude_sql_row_fetch($$self);

    return $row ? bless(\$row, "PreludeDBSQLRow") : undef;
}

sub	row_fetch_array
{
    my	$self = shift;
    my	$row;
    my	$fields;
    my	$cnt;
    my	@array;

    $row = prelude_sql_row_fetch($$self) or return ();
    $fields = prelude_sql_fields_num($$self);

    for ( $cnt = 0; $cnt < $fields; $cnt++ ) {
	push(@array, PreludeDBSQLRow::prelude_sql_field_fetch($row, $cnt));
    }

    return @array;
}

sub	row_fetch_hash
{
    my	$self = shift;
    my	$row;
    my	$fields;
    my	$cnt;
    my	%hash;

    $row = PreludeDB::prelude_sql_row_fetch($$self) or return ();
    $fields = PreludeDB::prelude_sql_fields_num($$self);

    for ( $cnt = 0; $cnt < $fields; $cnt++ ) {
	$hash{prelude_sql_field_name($$self, $cnt)} = PreludeDBSQLRow::prelude_sql_field_fetch($row, $cnt);
    }

    return %hash;
}

sub	DESTROY
{
    my	$self = shift;

    PreludeDB::prelude_sql_table_free($$self);
}



package PreludeDBSQLRow;

sub	field_fetch
{
    my	$self = shift;
    my	$field_id = shift;
    my	$field;

    $field = ($field_id =~ /^\d+$/ ? 
	      PreludeDB::prelude_sql_field_fetch($$self, $field_id) :
	      PreludeDB::prelude_sql_field_fetch_by_name($$self, $field_id));

    return $field;
}

1;
