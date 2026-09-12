#!/usr/bin/env perl
# offset.pl - compute struct field offsets from Apple's dyld_cache_format.h
# so we never guess a layout again. Sums field sizes in declaration order.
use strict; use warnings;

my $file = shift // '/tmp/apex/dc.h';
open my $fh, '<', $file or die "open $file: $!";
my @lines = <$fh>; close $fh;

my %size = (uint8_t=>1, int8_t=>1, char=>1, uint16_t=>2, int16_t=>2,
            uint32_t=>4, int32_t=>4, uint64_t=>8, int64_t=>8, bool=>1);
my %align = %size;
$align{$_} = 8 for qw(uint64_t int64_t);

my $in = 0; my $off = 0; my %want;
my @want_fields = qw(mappingOffset mappingCount imagesOffsetOld imagesCountOld
                     imagesTextOffset imagesTextCount sharedRegionStart
                     sharedRegionSize maxSlide subCacheArrayOffset);
%want = map { $_ => 1 } @want_fields;

for my $l (@lines) {
    if ($l =~ /^struct dyld_cache_header/) { $in = 1; $off = 0; next; }
    next unless $in;
    if ($l =~ /^\};/) { last; }
    # skip comments and blank
    $l =~ s{//.*$}{};
    next unless $l =~ /\S/;
    # field:  [uint32_t foo : 8, bar : 4;]  or  uint64_t foo;
    if ($l =~ /^\s*([A-Za-z_][A-Za-z0-9_]*)\s+(.+);/) {
        my ($type, $rest) = ($1, $2);
        $rest =~ s/^\s+|\s+$//g;
        my $sz = $size{$type} // 0;
        if ($sz == 0) { warn "unknown type $type\n"; next; }
        # bitfields: "foo : 8, bar : 24" -> total bits
        if ($rest =~ /:/) {
            my $bits = 0; $bits += $1 while $rest =~ /:\s*(\d+)/g;
            my $bytes = int(($bits + 7) / 8);
            $bytes = 4 if $bytes <= 4;   # bitfields pack into the base type
            printf "  +0x%03x  %-22s %s (bitfield %d bits)\n", $off, "?", $type, $bits if 0;
            $off += $bytes;
            next;
        }
        # arrays of one name
        my @names = split /\s*,\s*/, $rest;
        my ($nm) = $names[0] =~ /^([A-Za-z_][A-Za-z0-9_]*)/;
        # align
        my $a = $align{$type};
        $off = int(($off + $a - 1) / $a) * $a;
        if ($want{$nm}) {
            printf "  +0x%03x  %-22s %s  (size %d)\n", $off, $nm, $type, $sz;
        }
        $off += $sz * scalar(@names);
        # array suffix like uuid[16]
        if ($rest =~ /\[\s*(\d+)\s*\]/) { $off += $sz * ($1 - 1); }
    }
}
printf "  total header size ≈ 0x%x (%d bytes)\n", $off, $off;
