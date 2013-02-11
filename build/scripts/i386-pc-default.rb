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


relevant_files = []

File.open('scripts/i386-pc-default-menu.lst') do |f|
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


# Damit die relativen Pfadangaben im tar korrekt sind
Dir.chdir('root')

puts('%-8s%s' % ['TAR', '>initrd.tar'])
exit 1 unless system("tar cf initrd.tar #{initrd_files.map { |f| "'#{f}'" } * ' '}")

Dir.chdir('..')


puts("STRIP'n'ZIP")

relevant_files.map { |f| "root/#{f}" }.each do |f|
    next unless File.file?(f)

    #system("strip -s '#{f}' 2> /dev/null")
    system("gzip -9 '#{f}'")

    File.rename("#{f}.gz", f)
end


system('cp scripts/i386-pc-default-menu.lst root/boot/grub/menu.lst')
system('cp scripts/i386-pc-default-stage1 root/boot/grub/stage1')
system('cp scripts/i386-pc-default-stage2 root/boot/grub/stage2')

puts('%-8s%s' % ['MFMT', 'floppy.img'])
exit 1 unless system('mformat -l MYXOMYCOTA x: > /dev/null')

puts('%-8s%s' % ['CP', 'root/... -> floppy.img/'])
exit 1 unless system("mcopy -s root/boot #{relevant_files.map { |f| "'root/#{f}'" } * ' '} x:/ > /dev/null")

puts('%-8s%s' % ['GRUB', 'floppy.img'])
exit 1 unless system("echo -e 'device (fd0) images/floppy.img\nroot (fd0)\nsetup (fd0)\nquit' | grub --batch &> /dev/null")

system("rm -rf '#{ENV['MTOOLSRC']}'")
