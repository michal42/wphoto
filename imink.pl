#!/usr/bin/perl

use strict;
use warnings;

use HTTP::Daemon;
use HTTP::Response;


my $resp_capabilityinfo = "";
my $resp_objrecvcapability =
'<?xml version="1.0"?>
<ResultSet xmlns="urn:schemas-canon-com:service:CameraConnectedMobileService:1">
  <TotalBufferSize>9223372036854775807</TotalBufferSize>
  <MaxDataSize>2147483647</MaxDataSize>
</ResultSet>';

my %dispatch = (
	'/CameraConnectedMobile/CapabilityInfo' => $resp_capabilityinfo,
	'/CameraConnectedMobile/UsecaseStatus' => \&resp_usecasestatus,
	'/CameraConnectedMobile/ObjRecvCapability' => $resp_objrecvcapability,
	'/CameraConnectedMobile/SendObjInfo' => \&resp_sendobjinfo,
	'/CameraConnectedMobile/ObjData' => \&resp_sendobjinfo,
);

my %codes = (
	200 => "OK",
	404 => "Not found",
);

sub send_resp {
	my $cgi = shift;
	my $code = shift;
	my $data = pop;
	my @headers = @_;

	my $desc = $codes{$code} || "xxx";
	my $res = "HTTP/1.2 $code $desc\r\n";
	$res .= $cgi->header(@headers, -Content_length => length($data));
	$res .= $data;
	#print STDERR $res, "\n";
	print $res;
}

sub handle_request {
	my ($client, $req) = @_;
	my $uri = $req->uri;
  
	(my $path = $uri->path) =~ s:/+:/:g;
	my $handler = $dispatch{$path};

	if (defined($handler)) {
		print STDERR "Handle: " . $uri->path_query . "\n";
		if (ref($handler) eq "CODE") {
			$handler->($client, $req);
		} else {
			my $res = HTTP::Response->new(200);
			$res->header("Content-type", "text/xml; charset=utf-8");
			$res->content($handler);
			$client->send_response($res);
		}
	} else {
		print STDERR "Unhandled: " . $uri->path_query . "\n";
		$client->send_error(404, "Not found");
	}
}

sub resp_usecasestatus {
	my ($client, $req) = @_;
	
	my $content = $req->content;
	my $status = "Stop";
	if ($content =~ m:<Status>Run</Status>:) {
		$status = "Run";
	}
	print STDERR "Status: $status\n";
	my $xml =
"<?xml version=\"1.0\"?>
<ResultSet xmlns=\"urn:schemas-canon-com:service:CameraConnectedMobileService:1\">
  <Status>$status</Status>
  <MajorVersion>0</MajorVersion>
  <MinorVersion>1</MinorVersion>
</ResultSet>";
	my $res = HTTP::Response->new(200);
	$res->header("Content-type", "text/xml; charset=utf-8");
	$res->content($xml);
	$client->send_response($res);
}

my $objcounter = 0;
sub resp_sendobjinfo {
	my ($client, $req) = @_;

	my $content = $req->content;
	my $fname = "postobj_$objcounter";
	$objcounter++;
	open(my $fh, '>', $fname) or die "$fname: $!\n";
	print STDERR "> $fname\n";
	print $fh $content;
	$client->send_status_line(200);
}


my $server = HTTP::Daemon->new(LocalPort => 8615, ReuseAddr => 1) or die "Cannot create a HTTP Daemon instance: $!\n";
while (my $client = $server->accept) {
	my $req = $client->get_request;
	handle_request($client, $req);
	$client->close;
}
