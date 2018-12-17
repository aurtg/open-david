#! /usr/bin/python
# -*- coding: utf-8 -*-

import sys, argparse

from collections import defaultdict
from util import *


STYLE_NORMAL = 0
STYLE_BOLD = 1
STYLE_UNDERLINE = 4

COLOR_FONT_BLACK   = 30
COLOR_FONT_RED     = 31
COLOR_FONT_GREEN   = 32
COLOR_FONT_YELLOW  = 33
COLOR_FONT_BLUE    = 34
COLOR_FONT_PURPLE  = 35
COLOR_FONT_CYAN    = 36
COLOR_FONT_WHITE   = 37

COLOR_BACK_BLACK      = 40
COLOR_BACK_RED        = 41
COLOR_BACK_BROWN      = 43
COLOR_BACK_BLUE       = 44
COLOR_BACK_PURPLE     = 45
COLOR_BACK_CYAN       = 46
COLOR_BACK_LIGHT_GRAY = 47


args = None # Result of argument parsing


# Generates ANSI-escaped string.
def ansi_escape(s, style, colors = []):
    global args

    if args.no_color:
        return s
    
    out = '\033[%d' % style

    for c in colors:
        out += ';%d' % c

    return out + ('m%s\033[0m' % s)


# Colors given string.
def color(s, c):
    return ansi_escape(s, STYLE_NORMAL, [c])


# Modifies given string with strong-format.
def strong(s):
    return ansi_escape(s, STYLE_BOLD, [COLOR_FONT_WHITE])


# Converts an instance of DavidNode.
def convert_node(n, subs):
    pred, terms = n.splitAtom()
    escaped = []

    for i, t in enumerate(terms):
        # SUBSTITUTION
        if t in subs:
            t = subs[t]
                
        t = t.replace('&quot;', '\"')
        if t.startswith('\"'):
            escaped.append(color(t, COLOR_FONT_YELLOW))
        elif t[0].isupper():
            escaped.append(color(t, COLOR_FONT_PURPLE))
        else:
            escaped.append(color(t, COLOR_FONT_BLUE))

    out = color('%d:' % n.index(), COLOR_FONT_CYAN)

    if '=' in pred:
        out += '(%s)' % (' %s ' % pred).join(escaped)
    else:
        out += '%s(%s)' % (pred, ', '.join(escaped))

    return out


# Converts an instance of DavidEdge.
def convert_edge(e, subs):
    out = color('{0:<5}'.format('%d:' % e.index()), COLOR_FONT_CYAN)
    
    if e.type() == 'hypothesize' or e.type() == 'implicate':
        out += ansi_escape('{0:^15}'.format(e.type()), STYLE_NORMAL, [COLOR_BACK_BROWN, COLOR_FONT_BLACK])
    elif e.type() == 'unify':
        out += ansi_escape('{0:^15}'.format(e.type()), STYLE_NORMAL, [COLOR_BACK_BLUE, COLOR_FONT_BLACK])
    else:
        out += ansi_escape('{0:^15}'.format(e.type()), STYLE_NORMAL, [COLOR_BACK_CYAN, COLOR_FONT_BLACK])

    out += '  '

    tail = ' ^ '.join([convert_node(n, subs) for n in e.referTail().referNodes()])
    head = 'None'

    if e.referHead():
        head = ' ^ '.join([convert_node(n, subs) for n in e.referHead().referNodes()])

    if e.type() == 'hypothesize':
        out += '%s <= %s' % (tail, head)
    else:
        out += '%s => %s' % (tail, head)
    
    return out


# Constructs term cluster from given nodes.
def make_term_clusters(nodes):
    out = []
    
    for n in nodes:
        if n.active():
            pred, terms = n.splitAtom()
            if pred == '=':
                idx = []
                
                for i, cls in enumerate(out):
                    if terms[0] in cls or terms[1] in cls:
                        idx.append(i)

                if len(idx) == 0:
                    out.append(set(terms))
                elif len(idx) == 1:
                    out[idx[0]].update(terms)
                else:
                    out[idx[0]].update(terms)

                    for i in reversed(idx[1:]):
                        out[idx[0]].update(out.pop(i))

    return out


# Constructs a map for substitution of arguments.
def make_substitution_map(term_clusters):
    out = {}
    
    is_unbound = lambda s: s[0] == '_'
    is_variable = lambda s: is_unbound(s) or s[0].islower()
    is_constant = lambda s: s[0].isupper() or s[0] == '\"' or s[0] == '\''

    for cls in term_clusters:
        t1 = None

        for t2 in cls:
            if t1 == None:
                t1 = t2
            elif is_unbound(t1):
                if not is_unbound(t2):
                    t1 = t2
            elif is_variable(t1):
                if is_constant(t2):
                    t1 = t2

        for t2 in cls:
            if t1 != t2:
                out[t2] = t1

    return out


# Converts an instance of DavidProofGraph.
def convert_proofgraph(sol):
    global args
    print(strong('nodes:'))

    depth2nodes = defaultdict(list)
    substitution = dict()

    if args.unify:
        substitution = make_substitution_map(make_term_clusters(sol.nodes.values()))
    
    for n in sol.nodes.values():
        depth2nodes[n.depth()].append(n)
    
    depth = 0
    while depth in depth2nodes:
        print(strong('\tdepth %d:' % depth))
        
        nodes = sorted(depth2nodes[depth], key = lambda x: x.index())
        
        for active in [True, False]:
            for n in nodes:
                is_equality = (n.splitAtom()[0] == '=')
                if is_equality and args.unify:
                    continue
                if n.active() == active:
                    prefix = '  ' if n.active() else color('# ', COLOR_FONT_RED)
                    print('\t\t%s%s' % (prefix, convert_node(n, substitution)))
            
        depth += 1
        
    print(strong('edges:'))

    depth2edges = defaultdict(list)
    for e in sol.edges.values():
        d = max([n.depth() for n in e.referTail().referNodes()])
        depth2edges[d].append(e)

    depth = 0
    while depth in depth2edges:        
        edges = sorted(depth2edges[depth], key = lambda x: x.index())
        has_printed_header = False

        for active in [True, False]:
            for e in edges:
                if e.isUnification() and args.unify:
                    continue
                if e.active() == active:
                    if not has_printed_header:
                        print(strong('\tdepth %d:' % depth))
                        has_printed_header = True

                    prefix = '  ' if e.active() else color('# ', COLOR_FONT_RED)
                    print('\t\t%s%s' % (prefix, convert_edge(e, substitution)))
            
        depth += 1


# Returns converted string of given DavidILPVariable instance.
def convert_ilp_variable(v):
    out = color('{0:<7}'.format('%d:' % v.index()), COLOR_FONT_CYAN)
    out += v.name() + color(' * %.2f' % v.coef(), COLOR_FONT_BLUE)

    if v.isFixed():
        out += color(' (fixed at %.2f)' % v.fixedValue(), COLOR_FONT_RED)
    
    return out


# Returns converted string of given DavidILPConstraint instance.
def convert_ilp_constraint(c):
    out = color('{0:<7}'.format('%d:' % c.index()), COLOR_FONT_CYAN)
    out += c.name() + '  '

    terms = []
    for k, v in c.terms():
        coef = color('%.2f * ' % k, COLOR_FONT_BLUE)
        name = color(v.name(), COLOR_FONT_YELLOW)
        terms.append(coef + name)
    out += ' + '.join(terms)
    out += '  ' + color(c.range(), COLOR_FONT_PURPLE)

    if c.lazy():
        out += '  ' + ansi_escape(' lazy ', STYLE_NORMAL, [COLOR_BACK_BROWN, COLOR_FONT_BLACK])
        
    return out


# Converts an instance of DavidILPSolution.
def convert_ilp_solution(sol):
    print(strong('variables:'))
    
    vars = sorted(sol.vars.values(), key = lambda x: x.index())
    for active in [True, False]:
        prefix = '  ' if active else color('# ', COLOR_FONT_RED)
        for v in vars:
            if v.active() == active:
                print('\t\t%s%s' % (prefix, convert_ilp_variable(v)))
    
    print(strong('constraints:'))
    cons = sorted(sol.cons.values(), key = lambda x: x.index())
    for satisfied in [True, False]:
        prefix = '  ' if satisfied else color('# ', COLOR_FONT_RED)
        for c in cons:
            if c.satisfied() == satisfied:
                print('\t\t%s%s' % (prefix, convert_ilp_constraint(c)))

    
# Converts an instance of DavidObject.
def convert(dav):
    is_pg = (dav.outtype() != 'ilp')
    
    for idx, result in dav.results.items():
        print('\n---- result : %d ----\n' % idx)

        if is_pg:
            convert_proofgraph(result.solutions[0])
        else:
            convert_ilp_solution(result.solutions[0])


# Main process.
def main():
    global args
    
    parser = argparse.ArgumentParser(description='Make David\'s output easy to see.')
    parser.add_argument('jsons', nargs='*', help='Paths of JSON files which David output.')
    parser.add_argument('--no-color', action='store_true', default=False, help='Disables coloring.')
    parser.add_argument('-u', '--unify', action='store_true', default=False, help='Applies active unifications to atoms.')
    
    args = parser.parse_args()

    if args.jsons:
        for arg in args.jsons:
            dav = DavidObject()
            dav.load(arg)
            convert(dav)

    else:
        dav = DavidObject()
        dav.read(sys.stdin)
        convert(dav)
            

if __name__=='__main__':
    main()
