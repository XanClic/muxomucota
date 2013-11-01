#!/usr/bin/env ruby
# coding: utf-8

def ex(*args)
    args = args.to_a.flatten
    system(*args)
end

def die(*str)
    $stderr.puts str
    exit 1
end

CC = 'gcc'
CFLAGS = '-std=c11 -O3 -m32 -D_GNU_SOURCE'.split
PLD = 'ld'
PLDFLAGS = '-r -melf_i386'.split
LD = 'gcc'
LDFLAGS = '-m32'
LIBS = []
ARCH = 'i386'
PLATFORM = 'pc'

Dir.entries('.').sort.each do |e|
    next unless e =~ /\d{3}\.c/

    src = e
    obj = e[0..-2] + 'o'
    pro = e[0..-2] + 'par.o'
    exe = e[0..-3]

    capd = ''

    File.open(e) do |f|
        cap = false

        f.each_line do |l|
            if l =~ /^\s*\/\*\s*TEST\s*$/
                cap = true
            elsif cap
                break if l =~ /^\s*\*\/\s*$/
                capd << l
            end
        end
    end

    capd = eval(capd)
    capd = {} unless capd

    hdr = capd[:headers]
    hdr = [] unless hdr

    hdr_par = hdr.map do |h|
        f = ["platform/#{PLATFORM}", "arch/#{ARCH}", 'common'].map do |d|
            "../../src/include/#{d}/#{h}"
        end.find do |f|
            File.file?(f)
        end
        die("Unknown header #{f} (specified in #{e}).") unless f
        f
    end.map do |h|
        ['-include', h]
    end.flatten

    die("Could not compile #{e}.") unless ex(CC, CFLAGS, hdr_par, '-c', src, '-o', obj)

    fls = capd[:files]
    fls = [] unless fls

    fls_par = fls.map do |o|
        f = ['c', 'm', 'cdi'].map do |l|
            ["platform/#{PLATFORM}", "arch/#{ARCH}", 'common'].map do |d|
                ext = File.extname(o)
                "../../src/lib/obj/#{l}_#{"#{d}/#{o.chomp(ext)}".gsub('/', '__')}_#{ext[1..-1]}.o"
            end.find do |f|
                File.file?(f)
            end
        end.find do |f|
            f
        end
        die("Could not find compiled object file for #{o}.") unless f
        f
    end

    die("Could not partial-link #{obj}.") unless ex(PLD, PLDFLAGS, obj, fls_par, '-o', pro)
    die("Could not link against host libc.") unless ex(LD, LDFLAGS, pro, '-o', exe, LIBS)

    File.open("#{exe}.out.log", 'w') do |f|
        IO.popen(["./#{exe}", err: [:child, :out]]) do |io|
            f.puts io.read
        end
    end

    if $?.coredump?
        $stderr.puts "#{exe} dumped core"
    elsif $?.exitstatus != 0
        $stderr.puts "#{exe} failed"
    else
        if File.file?("#{exe}.out") && !system("diff -q #{exe}.out #{exe}.out.log &> /dev/null")
            $stderr.puts "#{exe} failed (output differs from reference)"
        else
            $stderr.puts "#{exe} ok"
        end
    end
end

exit 0
