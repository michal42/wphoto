#!/usr/bin/perl

use strict;
use warnings;

use HTTP::Daemon;
use HTTP::Response;
use Email::MIME;
use URI::QueryParam;
use Fcntl qw(:seek);


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
	'/CameraConnectedMobile/ObjData' => \&resp_objdata,
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
			eval {
				$handler->($client, $req);
			};
			if ($@) {
				print STDERR $@;
				$client->send_error();
			}
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

sub resp_sendobjinfo {
	my ($client, $req) = @_;

	# thumbnail ignored
	$client->send_status_line(200);
}

my %fragments;
sub resp_objdata {
	my ($client, $req) = @_;

	my $params = $req->uri->query_form_hash;
	for my $p (qw(ObjID Offset TotalSize SendSize)) {
		if (!exists($params->{$p})) {
			die "error: Required parameter $p is missing\n";
		}
	}
	if ($params->{ObjID} !~ /^[a-zA-Z0-9]+$/) {
		die "error: Illegal ObjID: $params->{ObjID}\n";
	}
	if (exists($params->{ObjType}) && $params->{ObjType} !~ /^[a-zA-Z0-9]+$/) {
		die "error: Illegal ObjType for ObjID $params->{ObjID}: $params->{ObjID}\n";
	}
	if (!exists($fragments{$params->{ObjID}})) {
		$fragments{$params->{ObjID}} = {
			size => $params->{TotalSize},
			fragments => {},
		};
	}
	my $obj = $fragments{$params->{ObjID}};
	if ($obj->{size} != $params->{TotalSize}) {
		print STDERR "warning: Size of ObjID $params->{ObjID} changed from $obj->{size} to $params->{TotalSize}\n";
		$obj->{size} = $params->{TotalSize};
	}
	my $str = $req->as_string;
	my $mime = Email::MIME->new($str);
	my $data;
	for my $p ($mime->subparts) {
		next unless $p->content_type =~ m:application/octet-stream.*Object-ID:;
		$data = $p->body;
	}
	if (!$data) {
		die "error: No data found for ObjID $params->{ObjID}\n";
	}
	my $offset = $params->{Offset};
	if (exists($obj->{fragments}{$offset})) {
		print STDERR "warning: overwriting data at offset $offset for ObjID $params->{ObjID}\n";
	}
	$obj->{fragments}{$offset} = [$params->{SendSize}, $data];

	# Now check if we have the complete object
	# FIXME: Handle overlapping fragments
	my $idx = 0;
	while (exists($obj->{fragments}{$idx})) {
		$idx += $obj->{fragments}{$idx}[0];
	}
	if ($idx < $obj->{size}) {
		$client->send_status_line(200);
		return;
	}
	if ($idx > $obj->{size}) {
		print STDERR "warning: ObjID $params->{ObjID} bigger than advertized size ($idx vs. $obj->{size}\n";
	}
	my $fname = $params->{ObjID} . "." . (lc $params->{ObjType} || "jpg");
	print STDERR "Storing $fname\n";
	open(my $fh, '>', $fname) or die "$fname: $!\n";
	for my $i (sort { $a <=> $b } keys(%{$obj->{fragments}})) {
		sysseek($fh, $i, SEEK_SET);
		syswrite($fh, $obj->{fragments}{$i}[1],
			$obj->{fragments}{$i}[0], 0);
	}
	close($fh);
	$client->send_status_line(200);
}


my $server = HTTP::Daemon->new(LocalPort => 8615, ReuseAddr => 1) or die "Cannot create a HTTP Daemon instance: $!\n";
while (my $client = $server->accept) {
	my $req = $client->get_request;
	handle_request($client, $req);
	$client->close;
}
