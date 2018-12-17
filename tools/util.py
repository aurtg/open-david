#! /usr/bin/python
# -*- coding: utf-8 -*-

## @file
## @brief Utility classes and functions for david scripts.

import json, sys


## Class of nodes in David's proof-graphs.
class DavidNode:
    def __init__(self, d, pg):
        self.raw = d
        self.pg = pg

    def __str__(self):
        return self.raw['atom']

    def index(self):
        return self.raw['index']

    def atom(self):
        return self.raw['atom']

    def depth(self):
        return self.raw['depth']

    def active(self):
        return self.raw['active']

    def type(self):
        return self.raw['type']

    def predicate(self):
        pred, args = self.splitAtom()
        return '%s/%d' % (pred, len(args))

    def isEquality(self):
        return self.atom().startswith('(')

    def splitAtom(self):
        return splitAtom(self.atom())


## Class of hypernodes in David's proof-graphs.
class DavidHypernode:
    def __init__(self, d, pg):
        self.raw = d
        self.pg = pg

    def __str__(self):
        return ' ^ '.join([str(n) for n in self.referNodes()])

    def index(self):
        return self.raw['index']

    def nodes(self):
        return self.raw['nodes']

    def active(self):
        return self.raw['active']
    
    def referNodes(self):
        return [self.pg.nodes[x] for x in self.nodes()]


## Class of edges in David's proof-graphs.
class DavidEdge:
    def __init__(self, d, pg):
        self.raw = d
        self.pg = pg

    def __str__(self):
        return '%s(%s -> %s)' % (self.type(), self.referTail(), self.referHead())

    def index(self):
        return self.raw['index']

    def type(self):
        return self.raw['type']

    def tail(self):
        return self.raw['tail']

    def head(self):
        return self.raw['head']

    def rule(self):
        try:
            return self.raw['rule']
        except KeyError:
            return None

    def active(self):
        return self.raw['active']

    def referTail(self):
        return self.pg.hypernodes[self.tail()]

    def referHead(self):
        if self.head() >= 0:
            return self.pg.hypernodes[self.head()]
        else:
            return None

    def referRule(self):
        try:
            return self.pg.rules[self.rule()]
        except KeyError:
            return None

    def isUnification(self):
        return self.type() == 'unify'


## Class of rules used in David.
class DavidRule:
    def __init__(self, d):
        self.raw = d

    def rid(self):
        return self.raw['rid']

    def name(self):
        return self.raw['name']
    
    def lhs(self):
        return [splitAtom(x) for x in self.raw['left']['atoms']]
    
    def rhs(self):
        return [splitAtom(x) for x in self.raw['right']['atoms']]

    def string(self):
        lhs = self.raw['left']['atoms']
        rhs = self.raw['right']['atoms']
        return '%s => %s' % (
            ' ^ '.join(lhs) if lhs else 'false',
            ' ^ '.join(rhs) if rhs else 'false')
    

## Class of proof-graphs from David.
class DavidProofGraph:
    def __init__(self, d):
        self.raw = d
        
        self.nodes      = {}
        self.hypernodes = {}
        self.edges      = {}
        self.rules      = {}

        if 'active' in self.raw:
            self.__parse(self.raw['active'], True)
            self.__parse(self.raw['not-active'], False)
        else:
            self.__parse(self.raw, True)

        for dd in self.raw['rules']:
            r = DavidRule(dd)
            self.rules[r.rid()] = r            

    def __parse(self, obj, is_active):
        for d in obj['nodes']:
            n = DavidNode(d, self)
            n.raw['active'] = is_active
            self.nodes[n.index()] = n

        for d in obj['hypernodes']:
            h = DavidHypernode(d, self)
            h.raw['active'] = is_active
            self.hypernodes[h.index()] = h

        for d in obj['edges']:
            e = DavidEdge(d, self)
            e.raw['active'] = is_active
            self.edges[e.index()] = e
            
    def objective(self):
        return float(self.raw['objective'])

    def dump(self, level=1, file=sys.stderr):
        print("num of nodes={}".format(len(self.nodes)), file=file)
        if level >= 1:
            for i, n in sorted(self.nodes.items())  :
                print("    nodes[{}].index={}, .atom={}, .depth={}, .active={} .type={}"
                      .format(i, n.index(), n.atom(), n.depth(), n.active(), n.type()),
                      file=file)
                if level >= 2:
                    print("        nodes[{}].splitAtom()={}".format(i,  n.splitAtom()),
                          file=file)
                
        print("num of hypernodes={}".format(len(self.hypernodes)), file=file)
        if level >= 1:
            for i, h in sorted(self.hypernodes.items()):
                print("    hypernodes[{}].index={}, .nodes={}, .active={}"
                      .format(i, h.index(), h.nodes(), h.active()),
                      file=file)
                if level >= 2:
                    print("        hypernodes[{}].referNodes()={}"
                          .format(i, [n.index() for n in h.referNodes()]),
                          file=file)

        print("num of edges={}".format(len(self.edges)), file=file)
        if level >= 1:
            for i, e in sorted(self.edges.items()):
                print("    edges[{}].index={}, .type={}, .tail={}, .head={}, .rule={}, .active={} .isUnification()={}"
                      .format(i, e.index(), e.type(), e.tail(), e.head(), e.rule(),
                              e.active(), e.isUnification()),
                      file=file)
                if level >= 2:
                    rid = None if e.referRule() is None else e.referRule().rid()
                    print("        edges[{}].referTail()={}, .referHead()={}, .referRule().rid()={}"
                          .format(i, e.referTail().index(), e.referHead().index(), rid),
                          file=file)

        print("num of rules={}".format(len(self.rules)), file=file)
        if level >= 1:
            for i , r in sorted(self.rules.items()):
                print("    rules[{}].rid={}, .name={}, .lhs={}, .rhs={}"
                      .format(i, r.rid(), r.name(),  r.lhs(), r.rhs()),
                      file=file)


## Class of ILP-variables in David.
class DavidILPVariable:
    def __init__(self, d, sol):
        self.raw = d
        self.sol = sol

    def index(self):
        return self.raw['index']

    def name(self):
        return self.raw['name']

    def coef(self):
        return self.raw['coefficient']

    def active(self):
        return self.raw['active']

    def isFixed(self):
        return ('fixed' in self.raw)

    def fixedValue(self):
        return self.raw['fixed']


## Class of ILP-constraints in David.
class DavidILPConstraint:
    def __init__(self, d, sol):
        self.raw = d
        self.sol = sol
        
    def index(self):
        return self.raw['index']
    
    def name(self):
        return self.raw['name']

    def terms(self):
        return [(float(x[0]), self.sol.vars[int(x[1][1:-1])]) for x in
                    [t.split('*') for t in self.raw['terms']]]

    def range(self):
        return self.raw['range']

    def lazy(self):
        return self.raw['lazy']

    def satisfied(self):
        return self.raw['satisfied']
    

## Class of ILP-Solutions from David.
class DavidILPSolution:
    def __init__(self, d):
        self.raw = d
        self.vars = {}
        self.cons = {}

        vars = self.raw['variables']
        self.__parseVariables(vars['positive'], True)
        self.__parseVariables(vars['negative'], False)

        cons = self.raw['constraints']
        self.__parseConstraints(cons['satisfied'], True)
        self.__parseConstraints(cons['violated'], False)

    def __parseVariables(self, d, is_active):
        for obj in d:
            v = DavidILPVariable(obj, self)
            v.raw['active'] = is_active
            self.vars[v.index()] = v

    def __parseConstraints(self, d, is_satisfied):
        for obj in d:
            c = DavidILPConstraint(obj, self)
            c.raw['satisfied'] = is_satisfied
            self.cons[c.index()] = c

    def dump(self, level=1, file=sys.stderr):
        print("num of variables={}".format(len(self.vars)), file=file)
        # TODO: Print each variables's information
        print("num of constraints={}".format(len(self.cons)), file=file)
        # TODO: Print each constraint's information

            
## Class of results of abductive inference by David.
class DavidResult:
    def __init__(self, d, otype):
        self.raw = d
        self.solutions = []
        
        assert(otype in ('mini', 'full', 'ilp'))

        if otype != 'ilp':
            if 'solution' in self.raw:
                self.solutions.append(DavidProofGraph(self.raw['solution']))
            
            if 'solutions' in self.raw:
                for d in self.raw['solutions']:
                    self.solutions.append(DavidProofGraph(d))

        else:
            if 'solution' in self.raw:
                self.solutions.append(DavidILPSolution(self.raw['solution']))
            
            if 'solutions' in self.raw:
                for d in self.raw['solutions']:
                    self.solutions.append(DavidILPSolution(d))

    def index(self):
        return self.raw['index']

    def name(self):
        name = self.raw['name']
        return name if bool(name) else 'NoName'

    def time_elapsed(self):
        return self.raw['elapsed-time']
    
    def dump(self, level=1, file=sys.stderr):
        print("num of solutions={}".format(len(self.solutions)), file=file)
        if level >= 1:
            for i, sol in enumerate(self.solutions):
                print("------------------------------solution[{}]".format(i), file=file)
                sol.dump(level,file)
                print("------------------------------", file=file)


## Class of outputs from David.
class DavidObject:
    def __init__(self):
        self.raw = None
        self.results = {}

    def load(self, path):
        with open(path, 'r') as fi:
            self.read(fi)

    def loads(self, string):
        self.raw = json.loads(string)
        self.__construct()

    def read(self, stream):
        self.raw = json.load(stream)
        self.__construct()

    def __construct(self):
        for r in self.raw['results']:
            idx = r['index']
            self.results[idx] = DavidResult(r, self.outtype())
            
    def outtype(self):
        return self.raw['output-type']

    def version(self):
        return self.raw['kernel']['version']

    def time_executed(self):
        return self.raw['kernel']['executed']

    def dump(self, title=None, level=1, file=sys.stderr):
        print("DavidObject.proofgraph dump:level={}: {}"
              .format(level,title if title is not None else ""),
              file=file)

        print("num of results={}".format(len(self.results)), file=file)
        if level >= 1:
            for i, r in self.results.items():
                print("---------------results[{}] .index={}, .name={}"
                      .format(i,r.index(), r.name()), file=file)
                r.dump(level,file)
                print("---------------", file=file)
            

## Splits a string of FOL formulae into its predicate and arguments.
def splitAtom(atom):
    is_equality = atom.startswith('(')
    is_in_quot = False
    i = 0
    splitted = []
        
    for j, c in enumerate(atom):
        if c in ('\'', '\"'):
            is_in_quot = not is_in_quot
            
        if not is_in_quot:
            if c in (' ', ',', '(', ')'):
                s = atom[i:j].strip()
                if s:
                    splitted.append(s)
                i = j + 1
            if c == ')':
                break

    if is_equality:
        assert (len(splitted) == 3)
        assert (splitted[1] in ('=', '!='))
        return splitted[1], (splitted[0], splitted[2])
    else:
        return splitted[0], tuple(splitted[1:])


## Returns whether the string element is a constant or not(i.e. variable name).
def isConst(elem):
    if elem[0] == '_':
        return isConst(elem[1:])
    if elem[0] == '"' or elem[0] == "'":
        return True
    if not elem[0].islower():
        return True
    return False
