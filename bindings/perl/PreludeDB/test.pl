#!/usr/bin/perl -w

######
#
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
#
######


BEGIN {
    push(@INC, "blib/arch/");    
}

use strict;
use PreludeDB;

my %CONF = (iface =>		'iface1',
	    class =>		'sql',
	    type =>		'pgsql',
	    host =>		'localhost',
	    name =>		'prelude',
	    user =>		'prelude',
	    pass =>		'prelude');
my $QUERY = "select * from Prelude_Alert";

my $db;
my $sql;
my $table;
my $row;
my $field;
my $value;
my @array;
my %hash;
my $ref;
my $i;
my $fields;
my $qstring;

$db = new PreludeDB( "iface=$CONF{iface} " .
		     "class=$CONF{class} ".
		     "type=$CONF{type} " .
		     "host=$CONF{host} " .
		     "name=$CONF{name} " .
		     "user=$CONF{user} " .
		     "pass=$CONF{pass}" )
    or die "new PreludeDB failed";

$db->connect < 0 and die "connect";
$sql = $db->get_sql_connection or die "get_sql_connection failed";
$table = $sql->query($QUERY) or die $sql->error;

# There is more than way to do it (c) Larry Wall ;)

# First way

$fields = $table->fields_num;
while ( $row = $table->row_fetch ) {
    for ( $i = 0; $i < $fields; $i++ ) {
	$field = $row->field_fetch($i) or die $sql->error;
	printf("%s: %s\n", $table->field_name($i), $field);
    }
}


# Second way

# while ( %hash = $table->row_fetch_hash ) {
#     foreach (keys %hash) {
# 	print "$_: $hash{$_}\n";
#     }
# }


# Third way

# while ( @array = $table->row_fetch_array ) {
#     print "@array\n";
# }
