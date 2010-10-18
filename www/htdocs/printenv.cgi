#!/usr/bin/perl
##
##  printenv -- demo CGI program which just prints its environment
##

print "X-Foo: bar\r\n";
print "Content-type: text/plain; charset=iso-8859-1\r\n\r\n";
foreach $var (sort(keys(%ENV))) {
    $val = $ENV{$var};
    $val =~ s|\n|\\n|g;
    $val =~ s|"|\\"|g;
    print "${var}=\"${val}\"\n";
}

