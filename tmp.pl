#!/usr/bin/perl

use Glib qw(TRUE FALSE);
use Net::DBus;
use Net::DBus::GLib;

my $bus = Net::DBus::GLib->system();
my $service = $bus->get_service("org.freedesktop.login1");
my $object = $service->get_object("/org/freedesktop/login1");

my $sessions = $object->ListSessions();

foreach my $sess (@$sessions) {
	print STDOUT "Session:\n";
	foreach my $val (@$sess) {
		printf STDOUT "\tval = %s\n", $val;
	}
	my $obj = $service->get_object($sess->[4]);
	my $prop = $obj->GetAll("org.freedesktop.login1.Session");
	while (my ($key,$val) = each(%$prop)) {
		if (ref($val) eq 'ARRAY') {
			printf STDOUT "\t%s = [", $key;
			foreach my $v (@$val) {
				printf STDOUT " '%s',", $v;
			}
			printf STDOUT " ]\n";
		} else {
			printf STDOUT "\t%s = %s\n", $key, $val;
		}
	}
}
