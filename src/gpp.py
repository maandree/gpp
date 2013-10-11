#!/usr/bin/env python3

import os
import sys
import shlex
from subprocess import Popen, PIPE

symbol = '@'
encoding = 'utf-8'
iterations = 1
input_file = '/dev/stdin'
output_file = '/dev/stdout'
unshebang = False

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
    elif arg in ('-u', '--unshebang'):   unshebang   = True; continue
    elif arg in ('-i', '--input'):       input_file  = sys.argv[i]
    elif arg in ('-o', '--output'):      output_file = sys.argv[i]
    elif arg in ('-f', '--file'):
        input_file  = sys.argv[i]
        output_file = sys.argv[i]
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

if unshebang:
    if data[0][:2] == '#!':
        data[:] = data[1:]

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

