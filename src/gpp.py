#!/usr/bin/env python3
#!@{SHEBANG}
'''
gpp – Bash based general purpose preprocessor

Copyright © 2013  Mattias Andrée (maandree@member.fsf.org)

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

symbol = '@'
encoding = 'utf-8'
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
        print('gpp – Bash based general purpose preprocessor')
        print('')
        print('Copyright © 2013  Mattias Andrée (maandree@member.fsf.org)')
        print('')
        print('This program is free software: you can redistribute it and/or modify')
        print('it under the terms of the GNU General Public License as published by')
        print('the Free Software Foundation, either version 3 of the License, or')
        print('(at your option) any later version.')
        print('')
        print('This program is distributed in the hope that it will be useful,')
        print('but WITHOUT ANY WARRANTY; without even the implied warranty of')
        print('MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the')
        print('GNU General Public License for more details.')
        print('')
        print('You should have received a copy of the GNU General Public License')
        print('along with this program.  If not, see <http://www.gnu.org/licenses/>.')
        sys.exit(0)
    else:
        continue
    i += 1

if input_file == '-':   input_file = '/dev/stdin'
if output_file == '-':  output_file = '/dev/stdout'

if iterations < 1:
    if input_file != output_file:
        data = None
        with open(input_file, 'rb') as file:
            data = file.read()
        with open(write_file, 'wb') as file:
            file.write(data)
            file.flush()
    sys.exit(0)

data = None
with open(input_file, 'rb') as file:
    data = file.read().decode(encoding, 'error').split('\n')

if unshebang == 1:
    if data[0][:2] == '#!':
        data[0] = ''

if unshebang >= 2:
    if data[0][:2] == '#!':
        data[0] = data[1]
        data[1] = ''

def pp(line):
    rc = ''
    symb = False
    brackets = 0
    esc = False
    dollar = False
    quote = []
    for c in line:
        if brackets > 0:
            if esc:
                esc = False
            elif (c == ')') or (c == '}'):
                brackets -= 1
                if brackets == 0:
                    rc += c + '"\''
                    continue
            elif (c == '(') or (c == '{'):
                brackets += 1
            elif c == '\\':
                esc = True
            rc += c
        elif symb:
            symb = False
            if (c == '(') or (c == '{'):
                brackets += 1
                rc += '\'"$' + c
            else:
                rc += c
        elif c == symbol:
            symb = True
        elif len(quote) > 0:
            if esc:
                esc = False
            elif dollar:
                dollar = False
                if c == '(':
                    quote.append(')')
                elif c == '{':
                    quote.append('}')
            elif c == quote[-1]:
                quote[:] = quote[:-1]
            elif (quote[-1] in ')}') and (c in '\'"`'):
                quote.append(c)
            elif (c == '\\') and (quote[-1] != "'"):
                esc = True
            elif c == '$':
                dollar = True
            rc += c
        elif (c == '"') or (c == "'") or (c == '`'):
            quote.append(c)
            rc += c
        else:
            rc += c
    return rc

for _ in range(iterations):
    entered = False
    bashed = []
    
    for lineno in range(len(data)):
        line = data[lineno]
        if line.startswith(symbol + '<'):
            bashed.append(line[2:])
            entered = True
        elif line.startswith(symbol + '>'):
            bashed.append(line[2:])
            entered = False
        elif entered:
            bashed.append(line)
        else:
            line = '\'%s\'' % line.replace('\'', '\'\\\'\'')
            bashed.append('echo $\'\\e%i\\e\'%s' % (lineno, pp(line)))
    
    bashed = '\n'.join(bashed).encode(encoding)
    bash = Popen(["bash"], stdin = PIPE, stdout = PIPE, stderr = sys.stderr)
    bashed = bash.communicate(bashed)[0]
    
    if bash.returncode != 0:
        sys.exit(bash.returncode)
    
    bashed = bashed.decode(encoding, 'error').split('\n')
    data = []
    lineno = -1
    
    for line in bashed:
        no = -1
        if line.startswith('\033'):
            no = int(line[1:].split('\033')[0])
            line = '\033'.join(line[1:].split('\033')[1:])
        if no > lineno:
            while no != lineno + 1:
                data.append('')
                lineno += 1
        data.append(line)
        lineno += 1

with open(output_file, 'wb') as file:
    file.write('\n'.join(data).encode(encoding))
    file.flush()

