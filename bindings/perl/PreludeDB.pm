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

package PreludeDBSelection;

sub	_get_selected_object
{
    my	$string = shift || return undef;
    my	$object;
    my	$selected;
    my	$object_str;
    my	$flags = 0;
    my	$part1;
    my	$part2;
    my	%funcs =
	( 'min' =>		$PreludeDB::PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN,
	  'max' =>		$PreludeDB::PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX,
	  'avg' =>		$PreludeDB::PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG,
	  'std' =>		$PreludeDB::PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD,
	  'count' =>		$PreludeDB::PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT);
    my	%opts =
	( 'group_by' =>		$PreludeDB::PRELUDEDB_SELECTED_OBJECT_GROUP_BY,
	  'order_desc' =>	$PreludeDB::PRELUDEDB_SELECTED_OBJECT_ORDER_DESC,
	  'order_asc' =>	$PreludeDB::PRELUDEDB_SELECTED_OBJECT_ORDER_ASC);

    ($part1, $part2) = split /\//, $string;

    if ( $part1 =~ /^(.+)\((.+)\)$/ ) {
	unless ( defined $funcs{$1} ) {
	    warn "function $1 not recognized";
	    return undef;
	}
	$flags |= $funcs{$1};
	$object_str = $2;

    } else {
	$object_str = $part1;
    }

    if ( $part2 ) {
	foreach ( split /,/, $part2 ) {
	    if ( not defined $opts{$_} ) {
		warn "option $_ not recognized";
		return undef;
	    }

	    $flags |= $opts{$_};
	}
    }

    $object = Prelude::idmef_object_new_fast($object_str) or return undef;

    $selected = PreludeDB::prelude_db_selected_object_new($object, $flags);
    unless ( $selected ) {
	Prelude::idmef_object_destroy($object);
	return undef;
    }

    return $selected;
}

sub	add
{
    my	$self = shift;
    my	@selected_object_list = @_;
    my	$selected;

    foreach ( @selected_object_list ) {

	$selected = _get_selected_object($_) or return 0;

	PreludeDB::prelude_db_object_selection_add($$self, $selected);
    }

    return 1;
}

sub	new
{
    my	$class = shift;
    my	@selected_object_list = @_;
    my	$selection;
    my	$self;

    $selection = PreludeDB::prelude_db_object_selection_new() or return undef;

    $self = bless(\$selection, $class);

    $self->add(@selected_object_list) or return undef;

    return $self;
}

sub	DESTROY
{
    my	$self = shift;

    $$self and PreludeDB::prelude_db_object_selection_destroy($$self);
}



package PreludeDB;

sub	init
{
    return (prelude_db_init() < 0) ? 0 : 1;
}

sub	shutdown
{
    prelude_db_shutdown();
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

sub	enable_message_cache
{
    my	$self = shift;
    my	$directory = shift || return 0;

    return (prelude_db_interface_enable_message_cache($$self, $directory) < 0) ? 0 : 1;
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

sub	get_message_ident_list
{
     my	$self = shift;
     my $get_message_ident_list_func = shift;
     my $message_ident_list_destroy_func = shift;
     my	%opt = @_;
     my	$criteria;
     my	$limit;
     my	$offset;
     my	$ident_list_handle;
     my @ident_list;
     my	$ident_handle;

     if ( defined $opt{-criteria} ) {
	 $criteria = _get_criteria($_[0]) or return ();
     }

     $limit = defined($opt{-limit}) ? $opt{-limit} : -1;
     $offset = defined($opt{-offset}) ? $opt{-offset} : -1;

     $ident_list_handle = &$get_message_ident_list_func($$self, $criteria ? $$criteria : undef, $limit, $offset)
	 or return ();

     while ( $ident_handle = PreludeDB::prelude_db_interface_get_next_alert_ident($ident_list_handle) ) {
	 my $ident = { };

	 $ident->{ident} = PreludeDB::prelude_db_message_ident_get_ident($ident_handle);
	 $ident->{analyzerid} = PreludeDB::prelude_db_message_ident_get_analyzerid($ident_handle);
	 PreludeDB::prelude_db_message_ident_destroy($ident_handle);
	 push(@ident_list, $ident);
     }

     &$message_ident_list_destroy_func($ident_list_handle);

     return @ident_list;
}

sub	get_alert_ident_list
{
    (shift)->get_message_ident_list(\&PreludeDB::prelude_db_interface_get_alert_ident_list,
				    \&PreludeDB::prelude_db_interface_alert_ident_list_destroy,
				    @_);
}

sub	get_heartbeat_ident_list
{
    (shift)->get_message_ident_list(\&PreludeDB::prelude_db_interface_get_heartbeat_ident_list,
				    \&PreludeDB::prelude_db_interface_heartbeat_ident_list_destroy,
				    @_);
}

sub	_build_object_list
{
    my	@object_list = @_;
    my	$object_list_handle;
    my	$object;
    
    $object_list_handle = Prelude::idmef_object_list_new() or return undef;

    foreach ( @object_list ) {
	unless ( $_ ) {
	    Prelude::idmef_object_list_destroy($object_list_handle);
	    return undef;
	}

	$object = Prelude::idmef_object_new_fast($_);
	unless ( $object ) {
	    Prelude::idmef_object_list_destroy($object_list_handle);
	    return undef;
	}

	Prelude::idmef_object_list_add($object_list_handle, $object);
    }

    return $object_list_handle;
}

sub	get_message
{
    my	$self = shift;
    my	$get_message_func = shift;
    my	$ident_hash = shift || return undef;
    my	@object_list = @_;
    my	$object_list_handle;
    my	$ident_handle;
    my	$message;
    my	$ident;

    unless ( defined($ident_hash->{analyzerid}) && defined($ident_hash->{ident}) ) {
	return undef;
    }

    if ( @object_list ) {
	$object_list_handle = _build_object_list(@object_list) or return undef;
    }

    $ident = PreludeDB::prelude_db_message_ident_new($ident_hash->{analyzerid}, $ident_hash->{ident});
    unless ( $ident ) {
	Prelude::idmef_object_list_destroy($object_list_handle) if ( $object_list_handle );
	return undef;
    }

    $message = &$get_message_func($$self, $ident, $object_list_handle);

    PreludeDB::prelude_db_message_ident_destroy($ident);
    Prelude::idmef_object_list_destroy($object_list_handle) if ( $object_list_handle );

    return $message ? bless(\$message, "IDMEFMessage") : undef;
}

sub	get_alert
{
    (shift)->get_message(\&PreludeDB::prelude_db_interface_get_alert, @_);
}

sub	get_heartbeat
{
    (shift)->get_message(\&PreludeDB::prelude_db_interface_get_heartbeat, @_);
}

sub	delete_message
{
    my	$self = shift;
    my	$delete_message_func = shift;
    my	$ident_hash = shift;
    my	$ident;
    my	$retval;

    (defined($ident_hash->{analyzerid}) && defined($ident_hash->{ident})) or return 0;

    $ident = PreludeDB::prelude_db_message_ident_new($ident_hash->{analyzerid}, $ident_hash->{ident}) or return 0;

    $retval = &$delete_message_func($$self, $ident);

    PreludeDB::prelude_db_message_ident_destroy($ident);

    return ($retval < 0) ? 0 : 1;
}

sub	delete_alert
{
    (shift)->delete_message(\&PreludeDB::prelude_db_interface_delete_alert, @_);
}

sub	delete_heartbeat
{
    (shift)->delete_message(\&PreludeDB::prelude_db_interface_delete_heartbeat, @_);
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

    (defined $opt{-selection} && @{ $opt{-selection} } > 0) or return ();
    $selection = new PreludeDBSelection(@{ $opt{-selection} }) or return ();

    if ( defined $opt{-criteria} ) {
	$criteria = _get_criteria($opt{-criteria}) or return ();
    }

    $limit = defined($opt{-limit}) ? $opt{-limit} : -1;

    $distinct = defined($opt{-distinct}) ? $opt{-distinct} : 0;

    $res = PreludeDB::prelude_db_interface_select_values($$self, $$selection, $$criteria, $distinct, $limit) or return ();

    while ( ($objval_list = PreludeDB::prelude_db_interface_get_values($$self, $res, $$selection)) ) {
	my @tmp_list;

	while ( ($objval = Prelude::idmef_object_value_list_get_next($objval_list)) ) {
	    my $tmp;

	    $value = Prelude::idmef_object_value_get_value($objval);
	    unless ( $value ) {
		Prelude::idmef_object_value_list_destroy($objval_list);
		# FIXME: we should also free res, however this is not possible, the API is broken in that way
		return ();
	    }

	    $tmp = Prelude::value2scalar($value);
	    unless ( defined $tmp ) {
		Prelude::idmef_object_value_list_destroy($objval_list);
		return ();
	    }

	    push(@tmp_list, $tmp);
	}

	Prelude::idmef_object_value_list_destroy($objval_list);

	push(@result_list, \@tmp_list);
    }

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
