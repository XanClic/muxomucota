#!/bin/mruby
# coding: utf-8


class String
    def clear
        sub!(self, '')
    end

    def tok
        lstrip!

        space = index(' ')
        unless space
            val = String.new(self)
            clear()
            return val
        end

        val = self[0..(space - 1)]
        sub!(val, '')
        lstrip!

        return val
    end
end


class Array
    def [](pos)
        if pos.kind_of?(Range)
            b = (pos.begin < 0) ? (length + pos.begin) : pos.begin
            e = (pos.end   < 0) ? (length + pos.end  ) : pos.end

            (b..e).map { |i| at(i) }
        else
            at(pos)
        end
    end
end


def process_line(line)
    if line[0] == ':'
        prefixed = true
        line = line[1..-1]
    else
        prefixed = false
    end

    trailing = line.index(' :')
    if trailing
        if trailing > 0
            processed = line[0..(trailing - 1)].strip.split(' ') + [line[(trailing + 2)..-1]]
        else
            processed = line[2..-1]
        end
    else
        processed = line.strip.split(' ')
    end

    if prefixed
        source = processed.shift.split('!')[0]
    else
        source = $server
    end


    return if processed.empty?


    cmd = processed.shift.downcase
    case cmd
    when 'ping'
        $con.send("PONG :#{processed.shift}\r\n")

    when 'notice'
        processed.shift # target
        puts("[Notice] -#{source}- #{processed.shift}")

    when 'privmsg'
        target = processed.shift
        msg = processed.shift

        if msg[0] == "\1" # CTCP
            msg = msg[1..-2]

            if msg.downcase[0..6] == 'action '
                puts("* #{source} \e[1;34m#{msg[7..-1].strip}\e[0m")
            else
                puts("*** Got CTCP #{msg} request from #{source}.")
            end
        else
            if %w[# & + !].include?(target[0])
                puts("<#{source}> #{msg}")
            else
                puts("\e[1m<#{source}>\e[0m #{msg}") # Query
            end
        end

    when 'nick'
        puts("*** #{source} changes his nick to #{processed.shift}.")

    when 'quit'
        puts("*** #{source} has quit the server#{processed[0] ? " (#{processed.shift})" : ''}.")

    when 'join'
        puts("*** #{source} has joined #{processed.shift}.")

    when 'part'
        puts("*** #{source} has left #{processed.shift}#{processed[0] ? " (#{processed.shift})" : ''}.")

    when 'mode'
        target = processed.shift
        mode = processed.shift

        mode[1..-1].each_char do |m|
            case m
            when 'v', 'h', 'o', 'a', 'q'
                privs = { 'v' => 'voice', 'h' => 'half-op', 'o' => 'op', 'a' => 'admin', 'q' => 'founder' }
                if mode[0] == '+'
                    puts("*** #{source} gives #{privs[m]} to #{processed.shift} in #{target}.")
                else
                    puts("*** #{source} takes #{privs[m]} from #{processed.shift} in #{target}.")
                end

            when 'l'
                puts("*** #{source} sets user limit of #{target} to #{processed.shift}.")

            when 'k'
                puts("*** #{source} sets the channel key of #{target} to #{processed.shift}.")

            when 'b'
                puts("*** #{source} #{mode[0] == '+' ? "bans" : "unbans"} #{processed.shift} in #{target}.")

            else
                puts("*** #{source} sets mode #{mode[0]}#{m} of #{target}.")
            end
        end

    when 'topic'
        puts("*** #{source} changes topic of #{processed.shift} to “#{processed.shift}”.") if processed[1]

    when 'invite'
        puts("*** #{source} has invited #{processed.shift} into #{processed.shift}.")

    when 'kick'
        channel = processed.shift
        user = processed.shift
        puts("*** #{source} has kicked #{user} from #{channel}#{processed[0] ? " (#{processed.shift})" : ''}.")

    when '352' # who reply
        $who_list << processed[4]
    when '315' # end of who
        puts("*** Who: #{$who_list * ' '}")
        $who_list = []

    when '353' # names reply
        $names_list[processed[0]] = [] unless $names_list[processed[0]]
        $names_list[processed[0]] << processed[1..-1].map { |n| n.split(' ') }
    when '366' # end of names
        puts("*** Users in #{processed[0]}: #{$names_list[processed[0]] * ' '}") if $names_list[processed[0]]
        $names_list[processed[0]] = nil

    when '372', '375', '376' # motd / start of motd / end of motd
        processed.shift # target
        puts("*** [MOTD] #{processed.shift}")

    when '332' # topic reply
        processed.shift
        $current_channel = processed.shift
        puts("*** The topic of #{$current_channel} is: “#{processed.shift}”.")

    when '331' # no topic
        processed.shift
        $current_channel = processed.shift
        puts("*** #{$current_channel} has not topic.")

    else
        puts("[#{cmd}] -#{source}- #{processed * ' '}")
    end
end


inpipe  = Pipe.from_fd(0)
outpipe = Pipe.from_fd(1)


if ARGV.length != 1
    $stderr.puts('Usage: palirc <server>')
    exit 1
end

$server = ARGV[0]

puts("Resolving #{$server}...")

dnspipe = Pipe.create("(dns)/#{$server}")
if !dnspipe
    $stderr.puts('Could not connect to DNS server.')
    exit 1
end

if dnspipe.readable == 0
    $stderr.puts("Could not resolve #{$server}.")
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


$nick = 'palirc-user'

$con.send("NICK #{$nick}\r\n")
$con.send("USER palirc palirc xanclic.is-a-geek.org palirc\r\n")


inpipe.echo = 0


quit = false
input_buffer = ''
incomplete_line = nil

$who_list = []
$names_list = {}
$current_channel = nil


while !quit
    printf("\e[s\e[1;1f\e[44m%-80s\e[0m\e[u", input_buffer)
    outpipe.flush = 1

    while ($con.pressure == 0) && (inpipe.pressure == 0)
        Thread.pass()
    end

    if inpipe.pressure > 0
        input_buffer += inpipe.receive()

        while input_buffer.include?("\b")
            idx = input_buffer.index("\b")

            if idx == 0
                input_buffer = input_buffer[1..-1]
            elsif idx == 1
                input_buffer = input_buffer[2..-1]
            else
                input_buffer = input_buffer[0..(idx-2)] + input_buffer[(idx+1)..-1]
            end
        end

        if input_buffer.include?("\n")
            split = input_buffer.split("\n")
            input = split[0]
            input_buffer = split[1] ? split[1] : ''

            next if input == '/'


            from = $nick

            if (input[0] == '/') && (input[1] != '/')
                cmd = input.tok.downcase[1..-1]
                param = input


                case cmd
                when 'notice'
                    sendstr = "NOTICE #{param.tok} :#{param}"

                when 'msg'
                    sendstr = "PRIVMSG #{param.tok} :#{param}"

                when 'join'
                    if $current_channel
                        puts("*** You have already joined a channel (#{$current_channel}).")
                        next
                    end
                    sendstr = "JOIN #{param}"

                when 'kick'
                    sendstr = "KICK #{param.tok} #{param.tok}#{param.empty? ? '' : " :#{param}"}"

                when 'part', 'leave'
                    if param.empty? && $current_channel
                        sendstr = "PART #{$current_channel}"
                        $current_channel = nil
                    else
                        channel = param.tok
                        sendstr = "PART #{channel}#{param.empty? ? '' : " #{param}"}"
                        $current_channel = nil if channel == $current_channel
                    end

                when 'quit'
                    sendstr = "QUIT#{param.empty? ? '' : " :#{param}"}"
                    quit = true

                when 'me'
                    sendstr = "PRIVMSG #{$current_channel} :\1ACTION #{param}\1"

                when 'nick'
                    sendstr = "NICK #{param}"
                    $nick = param.tok

                else
                    sendstr = "#{cmd.upcase} #{param}"
                end
            else
                input = input[1..-1] if input[0] == '/'

                if !$current_channel
                    puts('*** You must join a channel first.')
                    sendstr = nil
                else
                    sendstr = "PRIVMSG #{$current_channel} :#{input}"
                end
            end

            if sendstr
                $con.send("#{sendstr}\r\n")
                process_line(":#{from} #{sendstr}")
            end
        end
    else
        recbuf = $con.receive()
        incomplete = (recbuf[-1] != "\n")

        recbuf = recbuf.split("\n")
        next if recbuf.size < 1

        recbuf[0] = incomplete_line + recbuf[0] if incomplete_line

        if incomplete
            incomplete_line = recbuf.pop
            next if recbuf.size < 1
        else
            incomplete_line = nil
        end

        recbuf.each do |line|
            process_line(line)
        end
    end
end


inpipe.echo = 1

$con.destroy()
