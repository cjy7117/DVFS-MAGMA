#!/usr/bin/env perl
#
# parses Doxygen output_err and prints a more readable summary of it.
# Usage: ./summarize_errors.pl [output_err]
#
# @author Mark Gates

use strict;

my $file     = '';
my $func     = '';
my @notfound = ();
my @notdoc   = ();
my $notdoc   = 0;
my $count    = 0;

print <<EOT;
"not found"      means \@param FOO exists in the doxygen documentation,
                 but the function has no argument FOO. Often this is a
                 spelling or capitalization error.

"not documented" means a function has an argument FOO,
                 but the doxygen documentation has no \@param FOO for it.
EOT


# --------------------------------------------------
sub output
{
	$count += 1;
	print "$file: $func()\n";
	if ( @notfound ) {
		print "    not found:      ", join( ", ", @notfound ), "\n";
	}
	if ( @notdoc ) {
		print "    not documented: ", join( ", ", @notdoc   ), "\n";
	}
	print "\n";
	$file     = '';
	@notfound = ();
	@notdoc   = ();
}


# --------------------------------------------------
sub pathsplit
{
	my( $path ) = @_;
	my($dir, $file) = $path =~ m|^(\S+)/([^ /]+)$|;
	return ($dir, $file);
}


# --------------------------------------------------
my $errfile = shift || 'output_err';
open( FILE, $errfile ) or die( $! );
while( <FILE> ) {
	if ( $notdoc and m/^ +parameter '(\w+)'/ ) {
		#print "$.: param\n";
		push @notdoc, $1;
	}
	elsif ( m/^(\S+):\d+: warning: argument '(\w+)' of command \@param is not found in the argument list of (\w+)/ ) {
		#print "$.: not found\n";
		my $path = $1;
		my $arg  = $2;
		my $newfunc = $3;
		my ($dir, $newfile) = pathsplit( $path );
		if ( $newfile ne $file or $newfunc ne $func ) {
			output();
			$file = $newfile;
			$func = $newfunc;
		}
		push @notfound, $arg;
		$notdoc = 0;
	}
	elsif ( m/^(\S+):\d+: warning: The following parameters of (\w+).* are not documented:/ ) {
		#print "$.: not doc\n";
		my $path = $1;
		my $newfunc = $2;
		my ($dir, $newfile) = pathsplit( $path );
		if ( $newfile ne $file or $newfunc ne $func ) {
			output();
			$file = $newfile;
			$func = $newfunc;
		}
		$notdoc = 1;
	}
	else {
		print "Unmatched: $.: $_";
	}
}

#print "count $count\n";
