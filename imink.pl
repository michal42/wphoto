#!/usr/bin/perl

use strict;
use warnings;

package IminkServer;

use HTTP::Server::Simple::CGI;
use base qw(HTTP::Server::Simple::CGI);

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
	my $self = shift;
	my $cgi  = shift;
  
	(my $path = $cgi->path_info()) =~ s:/+:/:g;
	my $handler = $dispatch{$path};

	my @params = sort($cgi->url_param());
	my $qs = "?";
	$qs .= "$_=" . $cgi->url_param($_) . "&" for @params;
	if (defined($handler)) {
		print STDERR "Handle: $path$qs\n";
		if (ref($handler) eq "CODE") {
			$handler->($cgi);
		} else {
			send_resp($cgi, 200,
				-type => "text/xml; charset=utf-8", $handler);
		}
	} else {
		print STDERR "Unhandled: $path$qs\n";
		send_resp($cgi, 404, $cgi->start_html('Not found') .
			  $cgi->h1('Not found') .
			  $cgi->end_html);
	}
}

sub resp_usecasestatus {
	my $cgi  = shift;   # CGI.pm object
	return if !ref $cgi;
	
	my $post = $cgi->param('POSTDATA') || "";
	my $status = "Stop";
	if ($post =~ m:<Status>Run</Status>:) {
		$status = "Run";
	}
	print STDERR "Status: $status\n";
	my $res =
"<?xml version=\"1.0\"?>
<ResultSet xmlns=\"urn:schemas-canon-com:service:CameraConnectedMobileService:1\">
  <Status>$status</Status>
  <MajorVersion>0</MajorVersion>
  <MinorVersion>1</MinorVersion>
</ResultSet>";
	send_resp($cgi, 200, -type => "text/xml; charset=utf-8", $res);
}

my $objcounter = 0;
sub resp_sendobjinfo {
	my $cgi  = shift;
	return if !ref $cgi;

	my $post = $cgi->param('POSTDATA') || "None";
	my $fname = "postobj_$objcounter";
	$objcounter++;
	open(my $fh, '>', $fname) or die "$fname: $!\n";
	print STDERR "> $fname\n";
	print $fh $post;
	send_resp($cgi, 200, "");
}


my $pid = IminkServer->new(8615)->run();
