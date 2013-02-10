#!/usr/bin/env ruby
# coding: utf-8

Dir.chdir('build')

ENV['MTOOLSRC'] = 'myxomycota.cfg'
ENV['PATH'] += ':/usr/local/sbin:/usr/sbin:/sbin'

system('mkdir -p root/boot/grub')

IO.write(ENV['MTOOLSRC'], 'drive x:
  file="images/floppy.img" cylinders=80 heads=2 sectors=18 filter')

puts('%-8s%s' % ['DD', '/dev/zero -> floppy.img'])

exit 1 unless system('dd if=/dev/zero of=images/floppy.img bs=512 count=2880 &> /dev/null')

puts("STRIP'n'ZIP")

Dir.entries('root') do |f|
    next unless File.file?(f)

    system("strip -s 'root/#{f}'")
    system("gzip -9 'root/#{f}'")

    File.rename("root/#{f}.gz", "root/#{f}")
end

system('cp scripts/i386-pc-default-menu.lst root/boot/grub/menu.lst')
system('cp scripts/i386-pc-default-stage1 root/boot/grub/stage1')
system('cp scripts/i386-pc-default-stage2 root/boot/grub/stage2')

puts('%-8s%s' % ['MFMT', 'floppy.img'])
exit 1 unless system('mformat -l MYXOMYCOTA x: > /dev/null')

puts('%-8s%s' % ['CP', 'root/* -> floppy.img/'])
exit 1 unless system('mcopy -s root/* x:/ > /dev/null')

puts('%-8s%s' % ['GRUB', 'floppy.img'])
exit 1 unless system("echo -e 'device (fd0) images/floppy.img\nroot (fd0)\nsetup (fd0)\nquit' | grub --batch &> /dev/null")

system("rm -rf '#{ENV['MTOOLSRC']}'")
