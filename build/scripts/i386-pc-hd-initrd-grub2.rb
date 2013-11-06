#!/usr/bin/env ruby
# coding: utf-8

require 'find'


Dir.chdir('build')

ENV['PATH'] += ':/usr/local/sbin:/usr/sbin:/sbin'

if ENV['PATH'].split(':').find { |p| File.file?("#{p}/grub2-install") }
    grub_base = 'grub2'
else
    grub_base = 'grub'
end

system("mkdir -p root/boot/#{grub_base}")
system('mkdir -p root-unstripped')


relevant_files = []

File.open('scripts/i386-pc-hd-initrd-grub2-grub.cfg') do |f|
    is_relevant = false

    f.each_line do |line|
        next if line.strip!.empty?

        if line =~ /^menuentry\s+/
            break if is_relevant

            is_relevant = true
        end

        next if !is_relevant

        match = /^\s*(multiboot|module)\s+\/(\S+)/.match(line)
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


system("cp scripts/i386-pc-hd-initrd-grub2-grub.cfg root/boot/#{grub_base}/grub.cfg")


puts('%-8s%s' % ['DD', '/dev/zero -> hd.img'])
exit 1 unless system('dd if=/dev/zero of=images/hd.img bs=1048576 count=64 &> /dev/null')

puts('%-8s%s' % ['PARTED', 'hd.img'])
exit 1 unless system("echo -e 'mktable msdos\\nmkpart primary ext2 63s #{64 * 2048 - 1}s\\nquit' | parted images/hd.img &> /dev/null")

puts('%-8s%s' % ['LOSETUP', 'hd.img#p1'])
loop_dev = `sudo losetup -f images/hd.img -o #{63 * 512} --show`.strip
exit 1 unless $?.exitstatus == 0

begin
    puts('%-8s%s' % ['MKFS', 'hd.img#p1'])
    raise 42 unless system("sudo mkfs.ext2 -F -q '#{loop_dev}'")

    puts('%-8s%s' % ['MOUNT', 'hd.img#p1 -> mp'])
    raise 42 unless system("mkdir -p mp && sudo mount '#{loop_dev}' -t ext2 mp && sudo chmod ugo+rwx -R mp")

    puts('%-8s%s' % ['CP', 'root/... -> mp/'])
    raise 42 unless system("cp -r root/{bin,boot,etc} mp")

    puts('%-8s%s' % ['!@#$', 'mp/boot/kernel'])
    File.open('mp/boot/kernel', 'r+b') do |f|
        f.seek(0x2a)
        phentsize = f.read(2).unpack('S')[0]
        phnum = f.read(2).unpack('S')[0]
        f.seek(0x1c)
        f.seek(f.read(4).unpack('L')[0] + 8)
        phnum.times do |phi|
            naddr = f.read(4).unpack('L')[0] & 0x0fffffff
            f.seek(-4, IO::SEEK_CUR)
            f.write([naddr].pack('L'))
            f.seek(phentsize - 4, IO::SEEK_CUR)
        end
    end

    puts('%-8s%s' % ['GRUB2', 'hd.img'])
    raise 42 unless system("sudo #{grub_base}-install --boot-directory=mp/boot --target=i386-pc --modules='part_msdos ext2 biosdisk' images/hd.img &> /dev/null")
rescue
    puts('%-8s%s' % ['UMOUNT', 'mp'])
    system("sudo umount mp; sudo losetup -d '#{loop_dev}'; rmdir mp")
    exit 1
end
puts('%-8s%s' % ['UMOUNT', 'mp'])
exit 1 unless system("sudo umount mp && sudo losetup -d '#{loop_dev}' && rmdir mp")
