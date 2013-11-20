#!/usr/bin/env ruby
# coding: utf-8

# Copyright (C) 2013 by Max Reitz
#
# This file is part of µxoµcota.
#
# µxoµcota is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# µxoµcota is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.


require 'find'


cwd = Dir.pwd

['.', 'src/kernel', 'src/progs', 'src/lib'].each do |dir|
    Dir.chdir(dir)

    exit 1 unless system('rm -f cscope.{out,files}')

    if !ARGV[0] || ARGV[0] != 'clean'
        IO.write('cscope.files', Find.find('.').select { |f| File.file?(f) && f =~ /\.[ch]$/ } * "\n")
        exit 1 unless system('cscope -b')
    end

    Dir.chdir(cwd)
end
