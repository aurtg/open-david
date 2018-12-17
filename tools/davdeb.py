#! /usr/bin/python
# -*- coding: utf-8 -*-

import sys, subprocess, os, collections
from util import *
from functools import partial


## Decorates string with ANSI color code.
def ansi_color(code, text, is_bold=False):
    if is_bold:
        code = ('1;' + code)
    return '\033[%sm%s\033[0m' % (code, text)

ansi_red    = partial(ansi_color, '31')
ansi_green  = partial(ansi_color, '32')
ansi_yellow = partial(ansi_color, '33')
ansi_blue   = partial(ansi_color, '34')
ansi_pink   = partial(ansi_color, '35')
ansi_cyan   = partial(ansi_color, '36')
ansi_silver = partial(ansi_color, '37')
ansi_gray   = partial(ansi_color, '90')

def print_log(s):
    print(ansi_cyan('# ' + s))


Output = collections.namedtuple('Output', ('stdout', 'stderr'))
objs = []

PATH_OUT_FULL_1 = '_davdeb.tmp.%d.1.full.json' % os.getpid()
PATH_OUT_FULL_2 = '_davdeb.tmp.%d.2.full.json' % os.getpid()

STATES = {
    'error1' : 'David has thrown some error in normal inference.',
    'error2' : 'David has thrown some error in pseudo-positive inference.',
    'error3' : 'David has thrown some error in inference with --set-const-true option.',
    'ok' : 'David has provided the ideal result.',
    'tie_score' : 'The ideal hypothesis has the best score, but another one was selected as the output.',
    'not_best' : 'The ideal hypothesis is contained in the search-space, but does not have the best score.',
    'impossible' : 'The search-space does not contain the ideal hypothesis.'
    }


## Runs david and returns the outputs.
def run_david(args):
    david = os.path.dirname(__file__) + '/../bin/david'
    cmd = [david, 'infer'] + args
    print_log('    $ ' + ' '.join(cmd))
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    return Output(proc.stdout.decode('utf-8'), proc.stderr.decode('utf-8'))


## Compares the normal output and the pseudo-positive output.
def test1(opts, prob):
    print_log('Test 1:')

    opts1 = ['-o', 'full:' + PATH_OUT_FULL_1]
    opts2 = ['-o', 'full:' + PATH_OUT_FULL_2, '--pseudo-positive']
    
    print_log('  > Running david ...')
    out1 = run_david(opts + opts1 + [prob])    
    print_log('  > Parsing output ...')
    obj1 = DavidObject()

    try:
        obj1.load(PATH_OUT_FULL_1)
    except: 
        import traceback
        print('\n'.join(filter(lambda s: 'ERROR' in s, out1.stderr.split('\n'))))
        traceback.print_exc()
        return 'error1'
    
    print_log('  > Running david ...')
    out2 = run_david(opts + opts2 + [prob])
    print_log('  > Parsing output ...')
    obj2 = DavidObject()
    
    try:
        obj2.load(PATH_OUT_FULL_2)
    except:
        import traceback
        print('\n'.join(filter(lambda s: 'ERROR' in s, out2.stderr.split('\n'))))
        traceback.print_exc()
        return 'error2'

    sol1 = obj1.results[0].solutions[0]
    sol2 = obj2.results[0].solutions[0]

    req1 = sol1.raw['requirement']['satisfied']
    req2 = sol2.raw['requirement']['satisfied']
    eval1 = sol1.objective()
    eval2 = sol2.objective()

    print('satisfy-requirement[normal]   = %s' % str(req1))
    print('satisfy-requirement[positive] = %s' % str(req2))
    print('evaluation[normal]   = %f' % eval1)
    print('evaluation[positive] = %f' % eval2)
    state = 'impossible'

    if req1:
        state = 'ok'
    elif not req1 and req2:
        if abs(eval1 - eval2) < 0.1:
            state = 'tie_score'
        else:
            state = 'not_best'

    for req in sol1.raw['requirement']['detail']:
        if req['state'] == 'unsatisfiable':
            print('\"%s\" is unsatisfiable!' % req['literal'])

    return state


## Main function.
def main():
    opts = sys.argv[1:-1]
    prob = sys.argv[-1]

    print_log('Options: %s' % str(opts))
    print_log('Input: %s' % prob)
    
    state1 = test1(opts, prob)
    print(ansi_yellow('[%s] %s' % (state1, STATES[state1])))


if __name__=='__main__':
    main()
