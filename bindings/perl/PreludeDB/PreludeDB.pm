package PreludeDB;

use 5.008;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use PreludeDB ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw( PRELUDE_DB_TYPE_INVALID
		  PRELUDE_DB_TYPE_SQL

		  ERR_PLUGIN_DB_SUCCESS
		  ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED
		  ERR_PLUGIN_DB_SESSIONS_EXHAUSTED
		  ERR_PLUGIN_DB_NOT_CONNECTED
		  ERR_PLUGIN_DB_QUERY_ERROR
		  ERR_PLUGIN_DB_NO_TRANSACTION
		  ERR_PLUGIN_DB_CONNECTION_FAILED
		  ERR_PLUGIN_DB_RESULT_TABLE_ERROR
		  ERR_PLUGIN_DB_RESULT_ROW_ERROR
		  ERR_PLUGIN_DB_RESULT_FIELD_ERROR
		  ERR_PLUGIN_DB_INCORRECT_PARAMETERS
		  ERR_PLUGIN_DB_ALREADY_CONNECTED
		  ERR_PLUGIN_DB_MEMORY_EXHAUSTED
		  ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM
		  ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME
		  ERR_PLUGIN_DB_CONNECTION_LOST
		  ERR_PLUGIN_DB_DOUBLE_QUERY
		  );

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('PreludeDB', $VERSION);

# Preloaded methods go here.

sub	PRELUDE_DB_TYPE_INVALID		{ 0 }
sub	PRELUDE_DB_TYPE_SQL		{ 1 }

sub ERR_PLUGIN_DB_SUCCESS		{ 0 }
sub ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED	{ 1 }
sub ERR_PLUGIN_DB_SESSIONS_EXHAUSTED	{ 2 }
sub ERR_PLUGIN_DB_NOT_CONNECTED		{ 3 }
sub ERR_PLUGIN_DB_QUERY_ERROR		{ 4 }
sub ERR_PLUGIN_DB_NO_TRANSACTION	{ 5 }
sub ERR_PLUGIN_DB_CONNECTION_FAILED	{ 6 }
sub ERR_PLUGIN_DB_RESULT_TABLE_ERROR	{ 7 }
sub ERR_PLUGIN_DB_RESULT_ROW_ERROR	{ 8 }
sub ERR_PLUGIN_DB_RESULT_FIELD_ERROR	{ 9 }
sub ERR_PLUGIN_DB_INCORRECT_PARAMETERS	{ 10 }
sub ERR_PLUGIN_DB_ALREADY_CONNECTED	{ 11 }
sub ERR_PLUGIN_DB_MEMORY_EXHAUSTED	{ 12 }
sub ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM	{ 13 }
sub ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME	{ 14 }
sub ERR_PLUGIN_DB_CONNECTION_LOST	{ 15 }
sub ERR_PLUGIN_DB_DOUBLE_QUERY		{ 16 }


1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

PreludeDB - Perl extension for blah blah blah

=head1 SYNOPSIS

  use PreludeDB;
  blah blah blah

=head1 ABSTRACT

  This should be the abstract for PreludeDB.
  The abstract is used when making PPD (Perl Package Description) files.
  If you don't want an ABSTRACT you should also edit Makefile.PL to
  remove the ABSTRACT_FROM option.

=head1 DESCRIPTION

Stub documentation for PreludeDB, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.



=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

root, E<lt>root@internal.cyberhqz.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2003 by root

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
