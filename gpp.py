#!@{SHEBANG}
# -*- coding: utf-8 -*-
copyright = '''
gpp – Bash-based general-purpose preprocessor

Copyright © 2013, 2014, 2015, 2017  Mattias Andrée (maandree@member.fsf.org)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
'''
VERSION="@{VERSION}"

import os
import sys
import shlex
from subprocess import Popen, PIPE

if sys.version_info.major < 3:
    def bytes(string):
        r = bytearray(len(string))
        b = buffer(r)
        r[:] = string
        return r

if sys.version_info.major < 3:
    def bytelist(string):
        return [ord(c) for c in string]
else:
    bytelist = list

symbol = '@'
encoding = sys.getdefaultencoding()
iterations = 1
input_file = '/dev/stdin'
output_file = '/dev/stdout'
unshebang = 0

args = sys.argv

# If the file is executed, the first command line argument will be the first argument in the shebang,
# the second will be the rest of the arguments in the command line as is and the third and final will
# be the executed file.
if len(args) == 3:
    if os.path.exists(args[2]) and os.path.isfile(args[2]) and ((os.stat(args[2]).st_mode & 0o111) != 0):
        args[1 : 2] = shlex.split(args[1])

for i in range(1, len(args)):
    arg = args[i]
    i += 1
    if   arg in ('-s', '--symbol'):      symbol      = sys.argv[i]
    elif arg in ('-e', '--encoding'):    encoding    = sys.argv[i]
    elif arg in ('-n', '--iterations'):  iterations  = int(sys.argv[i])
    elif arg in ('-u', '--unshebang'):   unshebang   += 1; continue
    elif arg in ('-i', '--input'):       input_file  = sys.argv[i]
    elif arg in ('-o', '--output'):      output_file = sys.argv[i]
    elif arg in ('-f', '--file'):
        input_file  = sys.argv[i]
        output_file = sys.argv[i]
    elif arg in ('-D', '--export'):
        export = sys.argv[i]
        if '=' not in export:
            export += '=1'
        export = (export.split('=')[0], '='.join(export.split('=')[1:]))
        os.putenv(export[0], export[1])
    elif arg in ('-v', '--version'):
        print('gpp ' + VERSION)
        sys.exit(0)
    elif arg in ('-c', '--copying'):
        print(copyright[1:-1])
        sys.exit(0)
    else:
        continue
    i += 1

if input_file == '-':   input_file = '/dev/stdin'
if output_file == '-':  output_file = '/dev/stdout'

symbol = bytelist(symbol.encode(encoding))
symlen = len(symbol)

if iterations < 1:
    if input_file != output_file:
        data = None
        with open(input_file, 'rb') as file:
            data = file.read()
        with open(output_file, 'wb') as file:
            file.write(data)
            file.flush()
    sys.exit(0)

def linesplit(bs):
    rc = []
    elem = []
    for b in bs:
        if b == 10:
            rc.append(elem)
            elem = []
        else:
            elem.append(b)
    rc.append(elem)
    return rc

def linejoin(bss):
    rc = []
    if len(bss) > 0:
        rc += bss[0]
    for bs in bss[1:]:
        rc.append(10)
        rc += bs
    return rc

data = None
with open(input_file, 'rb') as file:
    data = file.read()
data = linesplit(bytelist(data))

if unshebang == 1:
    if (len(data[0]) >= 2) and (data[0][0] == ord('#')) and (data[0][1] == ord('!')):
        data[0] = []

if unshebang >= 2:
    if (len(data[0]) >= 2) and (data[0][0] == ord('#')) and (data[0][1] == ord('!')):
        data[0] = data[1]
        data[1] = []

def pp(line):
    rc = []
    symb = False
    brackets = 0
    esc = False
    dollar = False
    quote = []
    n = len(line)
    i = 0
    rc.append(ord('\''))
    while i < n:
        c = line[i]
        i += 1
        if brackets > 0:
            if esc:
                esc = False
            elif len(quote) > 0:
                if dollar:
                    dollar = False
                    if c == ord('('):
                        quote.append(ord(')'))
                    elif c == ord('{'):
                        quote.append(ord('}'))
                elif c == quote[-1]:
                    quote[:] = quote[:-1]
                elif (quote[-1] in (ord(')'), ord('}'))) and (c in (ord('"'), ord('\''), ord('`'))):
                    quote.append(c)
                elif (c == ord('\\')) and (quote[-1] != ord('\'')):
                    esc = True
                elif c == ord('$'):
                    dollar = True
            elif c in (ord('"'), ord('\''), ord('`')):
                quote.append(c)
            elif c in (ord(')'), ord('}')):
                brackets -= 1
                if brackets == 0:
                    rc.append(c)
                    rc.append(ord('"'))
                    rc.append(ord('\''))
                    continue
            elif c in (ord('('), ord('{')):
                brackets += 1
            elif c == ord('\\'):
                esc = True
            rc.append(c)
        elif line[i - 1 : i + symlen - 1] == symbol:
            if symb:
                rc += symbol
            symb = not symb
            i += symlen - 1
        elif symb:
            symb = False
            if c in (ord('('), ord('{')):
                brackets += 1
                rc.append(ord('\''))
                rc.append(ord('"'))
                rc.append(ord('$'))
            else:
                rc += symbol
            if c == ord('\''):
                rc.append(c)
                rc.append(ord('\\'))
                rc.append(c)
            rc.append(c)
        elif c == ord('\''):
            rc.append(c)
            rc.append(ord('\\'))
            rc.append(c)
            rc.append(c)
        else:
            rc.append(c)
    rc.append(ord('\''))
    return rc

for _ in range(iterations):
    entered = False
    bashed = []
    
    for lineno in range(len(data)):
        line = data[lineno]
        if (len(line) > symlen) and (line[:symlen] == symbol) and (line[symlen] in (ord('<'), ord('>'))):
            bashed.append(line[symlen + 1:])
            entered = line[symlen] == ord('<')
        elif entered:
            bashed.append(line)
        else:
            buf = bytelist(('echo $\'\\e%i\\e\'' % lineno).encode())
            bashed.append(buf + pp(line))
    
    bashed = bytes(linejoin(bashed))
    bash = Popen(["bash"], stdin = PIPE, stdout = PIPE, stderr = sys.stderr)
    bashed = bash.communicate(bashed)[0]
    
    if bash.returncode != 0:
        sys.exit(bash.returncode)
    
    bashed = linesplit(bytelist(bashed))
    data = []
    lineno = -1
    
    for line in bashed:
        no = -1
        if (len(line) > 0) and (line[0] == 0o33):
            no = 0
            for i in range(1, len(line)):
                if line[i] == 0o33:
                    line = line[i + 1:]
                    break
                no = no * 10 + (line[i] - ord('0'))
        if no > lineno:
            while no != lineno + 1:
                data.append([])
                lineno += 1
        data.append(line)
        lineno += 1

data = bytes(linejoin(data))
with open(output_file, 'wb') as file:
    file.write(data)
    file.flush()
