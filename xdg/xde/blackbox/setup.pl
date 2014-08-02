#!/usr/bin/perl

use Glib qw(TRUE FALSE);
use Gtk2 qw(-init);
use File::Path qw(make_path);
use File::Which qw(which where);
use File::Temp qw(tempfile);
use File::Copy qw(copy move);
use File::Compare qw(compare_text);
use Sys::Hostname;
use Cwd qw(realpath);
use strict;
use warnings;

$ENV{XDE_USE_XDG_HOME} = 1;
$ENV{XDE_NOEXEC} = 1;

my $here = realpath($0); $here =~ s{/[^/]*$}{};

my $name = "blackbox";
my @files = qw(version keys menu rc setup.pl start.pl);
my @progs = qw(start);
my @udirs = qw(backgrounds styles);

my $XDG_CONFIG_HOME = $ENV{XDG_CONFIG_HOME};
$XDG_CONFIG_HOME = "$ENV{HOME}/.config" unless $XDG_CONFIG_HOME;
my $XDG_DATA_HOME   = $ENV{XDG_DATA_HOME};
$XDG_DATA_HOME   = "$ENV{HOME}/.local/share" unless $XDG_DATA_HOME;
my $XDG_CONFIG_DIRS = $ENV{XDG_CONFIG_DIRS};
$XDG_CONFIG_DIRS = "/etc/xdg" unless $XDG_CONFIG_DIRS;
my $XDG_DATA_DIRS   = $ENV{XDG_DATA_DIRS};
$XDG_DATA_DIRS   = "/usr/local/share:/usr/share" unless $XDG_DATA_DIRS;

my $prog = $ARGV[0];
$prog = $ENV{XDE_WM_TYPE} unless $prog;
$prog = $name unless $prog;

my $exec = which($prog) or die "ERROR: cannot find usable $prog program";

my $vers = $ARGV[1];
$vers = $ENV{XDE_WM_VERSION} unless $vers;
chomp($vers = `LANG= $prog -version 2>/dev/null|awk '/Blackbox/{print\$2;exit}'`) unless $vers;
$vers = "0.70.2" unless $vers;

my $sdir = $ENV{XDE_WM_CONFIG_SDIR};
$sdir = "/usr/share/$prog" unless $sdir;
my $path = $ENV{XDE_USE_XDG_HOME} ? "$XDG_CONFIG_HOME/$prog" : "$ENV{HOME}/.$prog";

$ENV{XDE_WM_TYPE} = "$prog";
$ENV{XDE_WM_VERSION} = "$vers";
$ENV{XDE_WM_CONFIG_HOME} = "$path";
$ENV{XDE_WM_CONFIG_SDIR} = "$sdir";
$ENV{XDE_WM_CONFIG_EDIR} = "$here";

foreach (qw(TYPE VERSION CONFIG_HOME CONFIG_SDIR CONFIG_EDIR)) {
	printf STDERR "XDE_WM_$_ => %s\n", $ENV{"XDE_WM_$_"};
}

sub backup {
	my($dir,$file) = @_;
	return unless -f "$dir/$file";
	my $th = File::Temp->new(UNLINK=>0,TEMPLATE=>"${file}.XXXXXX",SUFFIX=>".bak",DIR=>"$dir");
	my $temp = $th->filename;
	close($th);
	printf STDERR "backup: %s -> %s\n", "$dir/$file", $temp;
	move("$dir/$file", $temp);
}

sub editfile {
	my($fname) = @_;
	my($oh,$temp) = tempfile();
	if (open(my $ih,"<","$fname")) {
		while(my $line = <$ih>) {
			chomp($line);
			$line =~ s{~/\.$name}{$path}g;
			$line =~ s{\$HOME/\.$name}{$path}g;
			$line =~ s{~/}{$ENV{HOME}/}g;
			print $oh "$line\n";
		}
		close($ih);
	}
	close($oh);
	return $temp;
}

sub makelink {
	my($dir,$name,$targ) = @_;
	my $file = "$dir/$name";
	if (-l $file) {
		if (readlink($file) ne $targ) {
			printf STDERR "unlink: %s\n", $file;
			unlink $file;
			printf STDERR "symlink: %s -> %s\n", $file, $targ;
			symlink $targ, $file;
		}
	} elsif (-s $file) {
		backup($dir, $name);
		printf STDERR "symlink: %s -> %s\n", $file, $targ;
		symlink $targ, $file;
	}
}

if ( -s "$here/version" ) {
	chomp(my $cver = `head -1 "$here/version" 2>/dev/null`);
	chomp(my $iver = `head -1 "$path/version" 2>/dev/null`);
	if ($cver ne $iver) {
		if ($iver) {
			print STDERR "Directory $path needs to be updated...\n";
			my $dialog = Gtk2::MessageDialog->new(undef,
					'destroy-with-parent', 'question', 'yes-no',
					"X Desktop Environment:\n\nUpdate $path from version $iver to $cver?");
			$dialog->set_position('center-always');
			$dialog->set_default_response('yes');
			if ($dialog->run eq 'no') {
				exit 1;
			}
		} else {
			print STDERR "Directory $path needs to be initialized...\n";
			if (-d $path) {
				my $dialog = Gtk2::MessageDialog->new(undef,
					'destroy-with-parent', 'question', 'yes-no',
					"X Desktop Environment:\n\nInitialize $path for version $cver?\n(Existing files will be backed up.)");
				$dialog->set_position('center-always');
				$dialog->set_default_response('yes');
				if ($dialog->run eq 'no') {
					exit 1;
				}
			} else {
				printf STDERR "mkdir: %s\n", "$path";
				make_path($path);
			}
		}
		my %progs = (map{$_=>1}qw(version));
		foreach my $d (qw(backgrounds styles)) {
			next if -d "$path/$d";
			printf STDERR "mkdir: %s\n", "$path/$d";
			make_path("$path/$d");
		}
		foreach my $f (qw(version keys menu rc xde-rc)) {
			next unless -f "$here/$f";
			if ($progs{$f}) {
				if (not -f "$path/$f" or compare_text("$here/$f", "$path/$f")) {
					backup($path, $f) unless $iver;
					printf STDERR "copy: %s -> %s\n", "$here/$f", "$path/$f";
					copy("$here/$f", "$path/$f");
				}
			} else {
				my $temp = editfile "$here/$f";
				if (not -f "$path/$f" or compare_text($temp, "$path/$f")) {
					backup($path, $f) unless $iver;
					printf STDERR "edit: %s -> %s\n", "$here/$f", "$path/$f";
					copy($temp, "$path/$f");
				}
				unlink $temp;
			}
		}
		unless ($iver) {
			my $desktop = "\U$name\E";
			if (which('xdg-menugen')) {
				system("xdg-menugen --format $name --desktop $desktop -o '$path/menu' &");
			} elsif (which('xde-menugen')) {
				system("xde-menugen --format $name --desktop $desktop >'$path/menu' &");
			}
		}
	}
}

makelink($ENV{HOME},".blackboxrc","$path/rc");
makelink($ENV{HOME},".bbmenu","$path/menu");
makelink($ENV{HOME},".bbkeysrc","$path/keys");

unless ($ENV{XDE_NOEXEC}) {

	my @command = ($prog, '-rc', "$path/rc");

	my $root = Gtk2::Gdk::Screen->get_default->get_root_window;
	$root->property_change(Gtk2::Gdk::Atom->new("_NET_WM_PID"),
			Gtk2::Gdk::Atom->new("CARDINAL"),
			32, replace=>$$);
	$root->property_change(Gtk2::Gdk::Atom->new("_NET_WM_NAME"),
			Gtk2::Gdk::Atom->new("UTF8_STRING"),
			8, replace=>"$name $vers");
	$root->property_change(Gtk2::Gdk::Atom->new("WM_NAME"),
			Gtk2::Gdk::Atom->new("STRING"),
			8, replace=>"$name $vers");
	$root->property_change(Gtk2::Gdk::Atom->new("WM_COMMAND"),
			Gtk2::Gdk::Atom->new("STRING"),
			8, replace=>join("\0",@command));
	$root->property_change(Gtk2::Gdk::Atom->new("WM_CLIENT_MACHINE"),
			Gtk2::Gdk::Atom->new("STRING"),
			8, replace=>hostname);

	Gtk2->main_iteration while Gtk2->events_pending;

	exec $prog;

	#exec "$prog -rc \"$path/rc\"";

	exit 1;
}

exit 0

