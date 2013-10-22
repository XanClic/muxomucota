#!/usr/bin/env ruby
# coding: utf-8

require 'find'


Dir.chdir('build')

ENV['PATH'] += ':/usr/local/sbin:/usr/sbin:/sbin'

system('mkdir -p root/boot/grub')
system('mkdir -p root-unstripped')


relevant_files = []

File.open('scripts/i386-pc-hd-initrd-menu.lst') do |f|
    is_relevant = false

    f.each_line do |line|
        next if line.strip.empty?

        if line =~ /^\s*title\s+/
            break if is_relevant

            is_relevant = true
        end

        next if !is_relevant

        match = /^\s*(kernel|module)\s+\/(\S+)/.match(line)
        next if !match

        relevant_files << match[2]
    end
end


initrd_files = Dir.entries('root').reject { |f| ['initrd.tar', 'boot', '.', '..'].include?(f) }


puts('STRIP')

Find.find('root').select { |f| File.file?(f) }.each do |f|
    system("cp #{f} root-unstripped")
    system("strip -s '#{f}' 2> /dev/null") unless f == 'root/boot/kernel' # Kernel wegen Backtraces nicht strippen
end


# Damit die relativen Pfadangaben im tar korrekt sind
Dir.chdir('root')

puts('%-8s%s' % ['TAR', '>initrd.tar'])
exit 1 unless system("tar cf boot/initrd.tar #{initrd_files.map { |f| "'#{f}'" } * ' '}")

Dir.chdir('..')


system('cp scripts/i386-pc-hd-initrd-menu.lst root/boot/grub/menu.lst')
system('cp scripts/i386-pc-default-stage1 root/boot/grub/stage1')
system('cp scripts/i386-pc-default-stage2 root/boot/grub/stage2')


puts('%-8s%s' % ['DD', '/dev/zero -> hd.img'])
exit 1 unless system('dd if=/dev/zero of=images/hd.img bs=1048576 count=64 &> /dev/null')

puts('%-8s%s' % ['PARTED', 'hd.img'])
exit 1 unless system("echo -e 'mktable msdos\\nmkpart primary ext2 63s #{64 * 2048 - 1}s\\nquit' | parted images/hd.img &> /dev/null")

puts('%-8s%s' % ['LOSETUP', 'hd.img#p1'])
loop_dev = `sudo losetup -f images/hd.img -o #{63 * 512} --show`.strip
exit 1 unless $?.exitstatus == 0

puts('%-8s%s' % ['MKFS', 'hd.img#p1'])
exit 1 unless system("sudo mkfs.ext2 -F -q '#{loop_dev}'")

puts('%-8s%s' % ['MOUNT', 'hd.img#p1 -> mp'])
exit 1 unless system("mkdir -p mp && sudo mount '#{loop_dev}' -t ext2 mp && sudo chmod ugo+rwx -R mp")

puts('%-8s%s' % ['CP', 'root/... -> mp/'])
exit 1 unless system("cp -r root/{bin,boot,etc} mp")

puts('%-8s%s' % ['UMOUNT', 'mp'])
exit 1 unless system("sudo umount mp && sudo losetup -d '#{loop_dev}' && rmdir mp")

puts('%-8s%s' % ['GRUB', 'hd.img'])
exit 1 unless system("echo -e 'device (hd0) images/hd.img\nroot (hd0,0)\nsetup (hd0)\nquit' | grub --batch &> /dev/null")
