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

sub	_get_criteria
{
    my	$value = shift || return undef;

    return (ref $value eq "IDMEFCriteria") ? $value : new IDMEFCriteria($value);
}

sub	new
{
    my	$class = shift;
    my	$conn_string;
    my	$db;

    if ( @_ == 1 ) {
	$conn_string = $_[0];
	return undef unless ( $conn_string );

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

    $sqlconn = _prelude_db_connection_get($dbconn);

    return $sqlconn ? bless(\$sqlconn, "PreludeDBSQL") : undef;
}

sub	get_alert_uident_list
{
     my	$self = shift;
     my	$criteria;
     my	$uident_list_handle;
     my	@uident_list;
     my	$uident;

     if ( defined $_[0] ) {
	 $criteria = _get_criteria($_[0]) or return();
     }

     $uident_list_handle = PreludeDB::prelude_db_interface_get_alert_uident_list($$self, $$criteria) or return ();

     while ( ($uident = PreludeDB::prelude_db_interface_get_next_alert_uident($uident_list_handle)) ) {
	 push(@uident_list, $uident);
     }

     PreludeDB::prelude_db_interface_free_alert_uident_list($uident_list_handle);

     return @uident_list;
}

sub	get_heartbeat_uident_list
{
    my	$self = shift;
    my	$criteria;
    my	$uident_list_handle;
    my	@uident_list;
    my	$uident;

    if ( defined $_[0] ) {
	$criteria = _get_criteria($_[0]) or return();
    }

    $uident_list_handle = PreludeDB::prelude_db_interface_get_heartbeat_uident_list($$self, $$criteria) or return ();

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
    my	$selection;

    $selection = Prelude::idmef_selection_new() or return undef;

    foreach ( @object_list ) {
	$object = Prelude::idmef_object_new_fast($_);
	unless ( $object ) {
	    Prelude::idmef_selection_destroy($selection);
	    return undef;
	}

	if ( Prelude::idmef_selection_add_object($selection, $object) < 0 ) {
	    Prelude::idmef_object_destroy($object);
	    Prelude::idmef_selection_destroy($selection);
	    return undef;
	}
    }

    return $selection;
}

sub	_convert_selected_object_list
{
    my	@selected_list = @_;
    my	$selection;

    $selection = Prelude::idmef_selection_new() or return undef;

    foreach ( @selected_list ) {

	if ( ref eq "HASH" ) {
	    my $selected;
	    my $object;

	    unless ( $_->{-object} ) {
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }

	    $object = Prelude::idmef_object_new_fast($_->{-object});
	    unless ( $object ) {
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }

	    $selected = Prelude::idmef_selected_object_new
		($object,
		 defined($_->{-function}) ? $_->{-function} : $Prelude::function_none,
		 defined($_->{-group}) ? $_->{-group} : $Prelude::group_by_none,
		 defined($_->{-order}) ? $_->{-order} : $Prelude::order_none);

	    unless ( $selected ) {
		Prelude::idmef_object_destroy($object);
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }

	    if ( Prelude::idmef_selection_add_selected_object($selection, $selected) < 0 ) {
		Prelude::idmef_selected_object_destroy($selected);
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }

	} else {
	    my $object;

	    unless ( $_ ) {
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }

	    $object = Prelude::idmef_object_new_fast($_);
	    unless ( $object ) {
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }

	    if ( Prelude::idmef_selection_add_object($selection, $object) < 0 ) {
		Prelude::idmef_object_destroy($object);
		Prelude::idmef_selection_destroy($selection);
		return undef;
	    }
	}
    }
    
    return $selection;
}

sub	get_alert
{
    my	$self = shift;
    my	$uident = shift || return undef;
    my	@object_list = @_;
    my	$object_list_handle;
    my	$message;

    if ( @object_list ) {
	$object_list_handle = _convert_object_list(@object_list) or return undef;
    }

    $message = PreludeDB::prelude_db_interface_get_alert($$self, $uident, $object_list_handle);

    Prelude::idmef_selection_destroy($object_list_handle) if ( $object_list_handle );

    return $message ? bless(\$message, "IDMEFMessage") : undef;
}

sub	get_heartbeat
{
    my	$self = shift;
    my	$uident = shift || return undef;
    my	@object_list = @_;
    my	$object_list_handle;
    my	$message;

    if ( @object_list ) {
	$object_list_handle = _convert_object_list(@object_list) or return undef;
    }

    $message = PreludeDB::prelude_db_interface_get_heartbeat($$self, $uident, $object_list_handle);

    Prelude::idmef_selection_destroy($object_list_handle) if ( $object_list_handle );

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

sub	get_values
{
    my	$self = shift;
    my	%opt = @_;
    my	$selection;
    my	$criteria;
    my	$distinct;
    my	$limit;
    my	$res;
    my	$objval_list;
    my	$objval;
    my	$value;
    my	@result_list;

    (defined $opt{-object_list} && @{ $opt{-object_list} } > 0) or return ();
    $selection = _convert_selected_object_list(@{ $opt{-object_list} }) or return ();

    if ( defined $opt{-criteria} ) {
	$criteria = _get_criteria($opt{-criteria});
	unless ( $criteria ) {
	    Prelude::idmef_selection_destroy($selection);
	    return ();
	}
    }

    $limit = defined($opt{-limit}) ? $opt{-limit} : -1;

    $distinct = defined($opt{-distinct}) ? $opt{-distinct} : 0;

    $res = PreludeDB::prelude_db_interface_select_values($$self, $selection, $$criteria, $distinct, $limit);
    unless ( $res ) {
	Prelude::idmef_selection_destroy($selection);
	return ();
    }

    while ( ($objval_list = PreludeDB::prelude_db_interface_get_values($$self, $res, $selection)) ) {
	my @tmp_list;

	while ( ($objval = Prelude::idmef_object_value_list_get_next($objval_list)) ) {
	    my $tmp;

	    $value = Prelude::idmef_object_value_get_value($objval);
	    unless ( $value ) {
		Prelude::idmef_selection_destroy($selection);
		Prelude::idmef_object_value_list_destroy($objval_list);
		# FIXME: we should also free res, however this is not possible, the API is broken in that way
		return ();
	    }

	    $tmp = Prelude::value2scalar($value);
	    unless ( defined $tmp ) {
		Prelude::idmef_selection_destroy($selection);
		Prelude::idmef_object_value_list_destroy($objval_list);
		return ();
	    }

	    push(@tmp_list, $tmp);
	}

	Prelude::idmef_object_value_list_destroy($objval_list);

	push(@result_list, \@tmp_list);
    }

    Prelude::idmef_selection_destroy($selection);

    return @result_list;
}

sub	DESTROY
{
    my	$self = shift;

    $$self and PreludeDB::prelude_db_interface_destroy($$self);
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
    my	$i = shift || return undef;

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

    $row = PreludeDB::prelude_sql_row_fetch($$self) or return ();
    $fields = PreludeDB::prelude_sql_fields_num($$self);

    for ( $cnt = 0; $cnt < $fields; $cnt++ ) {
	push(@array, PreludeDB::prelude_sql_field_fetch($row, $cnt));
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
	$hash{PreludeDB::prelude_sql_field_name($$self, $cnt)} = PreludeDB::prelude_sql_field_fetch($row, $cnt);
    }

    return %hash;
}

sub	DESTROY
{
    my	$self = shift;

    $$self and PreludeDB::prelude_sql_table_free($$self);
}



package PreludeDBSQLRow;

sub	field_fetch
{
    my	$self = shift;
    my	$field_id = shift || return undef;
    my	$field;

    $field = ($field_id =~ /^\d+$/ ? 
	      PreludeDB::prelude_sql_field_fetch($$self, $field_id) :
	      PreludeDB::prelude_sql_field_fetch_by_name($$self, $field_id));

    return $field;
}

1;
