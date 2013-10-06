#!/usr/bin/env python3

import sys

symbol = '@'
encoding = 'utf-8'
input_file = '/dev/stdin'
output_file = '/dev/stdout'

data = None
with open(input_file, 'rb') as file:
    data = file.read().decode(encoding, 'error').split('\n')

entered = False
bashed = []

def pp(line):
    rc = ''
    symb = False
    for c in line:
        if symb:
            symb = False
            if c == symbol:
                rc += c
        elif c == symbol:
            symb = True
        else:
            rc += c
    return rc

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
        bashed.append('echo %i %s' % (lineno, pp(line)))

