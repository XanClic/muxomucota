#!/bin/mruby
# coding: utf-8

if ARGV.length != 1
    $stderr.puts('Usage: palirc <server>')
    exit 1
end

server = ARGV[0]

puts("Resolving #{server}...")

dnspipe = Pipe.create("(dns)/#{server}")
if !dnspipe
    $stderr.puts('Could not connect to DNS server.')
    exit 1
end

if dnspipe.readable == 0
    $stderr.puts("Could not resolve #{server}.")
    exit 1
end

ip = dnspipe.receive()
dnspipe.destroy()

puts("Resolved to #{ip}, connecting...")


$con = Pipe.create("(tcp)/#{ip}:6667", Pipe::O_RDWR)
if !$con
    $stderr.puts('Could not connect.')
    exit 1
end

$con.destroy()
