#!/usr/bin/env ruby
# coding: utf-8

require 'find'
require 'trollop'


arch = `uname -m`.chomp

if arch == 'x86'
    arch = 'i386'
elsif arch == 'x86_64'
    arch = File.directory?('src/kernel/arch/x64') ? 'x64' : 'i386'
elsif arch[0..2] == 'arm'
    arch = 'arm'
end


case arch
when 'i386', 'x64'
    platform = 'pc'
when 'arm'
    platform = 'rpi'
else
    platform = 'default'
end


def image_types
    Dir.entries('build/scripts').select { |e| e =~ /^.*-.*-.*\.sh$/ }.map { |e| e.chomp('.sh').split('-') }
end

def platform_output
    arch_plat = Hash.new
    image_types.each { |e| arch_plat[e[0]] = Array.new unless arch_plat[e[0]]; arch_plat[e[0]] << e[1] }

    maxlen = 0
    arch_plat.each_key { |a| maxlen = a.length if a.length > maxlen }

    arch_plat.keys.map { |a| "    %*s: #{arch_plat[a].uniq * ', '}" % [maxlen, a] } * "\n"
end

def image_type_output
    hash = Hash.new
    image_types.map { |e| [e[0..1] * '-', e[2]] }.each { |e| hash[e[0]] = Array.new unless hash[e[0]]; hash[e[0]] << e[1] }

    maxlen = 0
    hash.each_key { |ap| maxlen = ap.length if ap.length > maxlen }

    hash.keys.map { |ap| "    %*s: #{hash[ap].uniq * ', '}" % [maxlen, ap] } * "\n"
end


opts = Trollop::options do
    version('µxoµcota build system 0.0.1 (c) 2012/13 Hanna Reitz')
    banner <<-EOS
µxoµcota build system

Supported target architectures:
    #{image_types.map { |e| e[0] }.uniq * ', '}

Supported target platforms:
#{platform_output}

Supported image types:
#{image_type_output}

Usage:
    ./build.sh [options] [target]

where [options] are:
    EOS

    opt(:arch, 'Target architecture', short: 'a', default: arch)
    opt(:platform, 'Target platform', short: 'p', default: platform)
    opt(:image, 'Image type to be built', short: 'i', default: 'default')
end


arch     = opts[:arch]
platform = opts[:platform]
image    = opts[:image]


Trollop::die('Unsupported architecture') unless File.directory?("src/kernel/arch/#{arch}")
Trollop::die('Unsupported platform')     unless File.directory?("src/kernel/platform/#{platform}")
Trollop::die('Unsupported image type')   unless File.file?("build/scripts/#{arch}-#{platform}-#{image}.sh")


target = ARGV[0] ? ARGV[0] : 'all'


Trollop::die('Unsupported target') unless ['clean', 'all'].include?(target)


exts = ['.c', '.asm', '.S', '.incbin']


$inc_dirs = ['common', "arch/#{arch}", "platform/#{platform}"].map { |d| "#{Dir.pwd}/src/include/#{d}" }


cores = `cat /proc/cpuinfo | grep -c 'processor[[:space:]]*:'`
cores = cores.to_i if cores

cores = 4 unless cores && (cores > 0)

threads = Array.new


['kernel', 'lib', 'progs'].each do |dir|
    puts("——— #{dir} ———") unless target == 'clean'

    pushed = Dir.pwd
    Dir.chdir("src/#{dir}")

    if target == 'clean'
        system("rm -f obj/* #{{ 'kernel' => 'kernel*', 'lib' => 'crt/*.o libc.a libm.a', 'progs' => 'progs/*' }[dir]}")
        Dir.chdir(pushed)
        next
    end


    pars = eval(IO.read("arch/#{arch}/build-vars.rb"))

    objdir = "#{Dir.pwd}/obj"
    exit 1 unless system("mkdir -p '#{objdir}'")

    progdir = { 'progs' => "#{Dir.pwd}/progs" }[dir]
    exit 1 unless system("mkdir -p '#{progdir}'") if progdir


    subdirs = Array.new

    Find.find('.') do |path|
        subdirs << File.dirname(path)[2..-1] if exts.include?(File.extname(path))
    end


    operations = Array.new
    retirement = Array.new

    retirement << '.' if dir == 'progs'


    subdirs.uniq.each do |sd|
        if dir == 'lib'
            objprefix = "#{(File.basename(sd) == 'math') ? 'm' : 'c'}_#{sd.gsub('/', '__')}"
        else
            objprefix = sd.gsub('/', '__')
        end

        operations << sd

        Dir.new(sd).each do |file|
            ext = File.extname(file)

            next unless exts.include?(ext)

            if (dir == 'lib') && (File.basename(sd) == 'crt')
                output = "#{Dir.pwd}/crt/#{File.basename(sd, ext)}.o"
            else
                output = "#{objdir}/#{objprefix}__#{file.gsub('.', '_')}.o"
            end

            next if File.file?(output) && (File.mtime("#{sd}/#{file}") < File.mtime(output))

            case ext
            when '.c'
                operations << [['CC', file], "#{pars[:cc][:cmd]} #{pars[:cc][:flags]} -c '#{sd}/#{file}' -o '#{output}'"]
            when '.asm', '.S'
                if pars[:asm][:quiet]
                    operations << [['ASM', file], "#{pars[:asm][:cmd]} #{pars[:asm][:flags]} '#{sd}/#{file}' #{pars[:asm][:out]} '#{output}' &> /dev/null"]
                else
                    operations << [['ASM', file], "#{pars[:asm][:cmd]} #{pars[:asm][:flags]} '#{sd}/#{file}' #{pars[:asm][:out]} '#{output}'"]
                end
            when '.incbin'
                operations << [['OBJCP', file], "pushd '#{sd}' &> /dev/null; #{pars[:objcp][:cmd]} #{pars[:objcp][:bin2elf]} '#{file}' '#{output}' && popd &> /dev/null"]
            end
        end


        if dir == 'progs'
            ret = [['LD', ">#{sd}"], "#{pars[:ld][:cmd]} #{pars[:ld][:flags]} #{objdir}/#{objprefix}__*.o ../lib/crt/*.o -L../lib -o '#{progdir}/#{File.basename(sd)}' -\\( -lc #{pars[:ld][:libs]} -\\)"]

            ret += " && #{pars[:strip][:cmd]} #{pars[:strip][:flags]} #{progdir}/#{File.basename(sd)}" if pars[:strip]

            retirement << ret
        end
    end


    print_mtx = Mutex.new

    [operations, retirement].each do |queue|
        queue.each do |op|
            if op.kind_of?(String)
                print_mtx.lock()
                puts("<<< #{op} >>>")
                print_mtx.unlock()
                next
            end

            while threads.size > cores
                threads.keep_if { |thr| thr.alive? }
                Thread.pass
            end

            threads << Thread.new(op) do |op|
                print_mtx.lock()
                puts('%-8s%s' % op[0])
                print_mtx.unlock()

                exit 1 unless system(op[1])
            end
        end


        threads.each do |thr|
            thr.join()
        end


        threads = []
    end


    case dir
    when 'kernel'
        puts('<<< . >>>')
        puts('%-8s%s' % ['LD', '>kernel'])
        exit 1 unless system("#{pars[:ld][:cmd]} #{pars[:ld][:flags]} obj/*.o -o kernel #{pars[:ld][:libs]}")
    when 'lib'
        puts('<<< . >>>')
        [ 'c', 'm' ].each do |lib|
            puts('%-8s%s' % ['AR', ">lib#{lib}.a"])
            exit 1 unless system("#{pars[:ar][:cmd]} #{pars[:ar][:flags]} lib#{lib}.a obj/#{lib}_*.o")
        end
    end


    Dir.chdir(pushed)
end


exit 1 unless system("mkdir -p build/images")

if target == 'all'
    # build initrd here
    puts("——— image ———")
    exit 1 unless system("build/scripts/#{arch}-#{platform}-#{image}.sh")
else
    system("rm -rf build/images/*")
end
