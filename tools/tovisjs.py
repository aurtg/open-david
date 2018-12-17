#! /usr/bin/python
# -*- coding: utf-8 -*-

## @file tovisjs.py
## @brief
##   visualize.py の出力ファイルを読み込み、
##   visjsを用いた可視化を含むHTMLファイルに変換して出力する。

import argparse, json, sys, os

VERSION_STRING = "1.0.0"


class Visjs:
    def __init__(self, input_json_path):
        self.path_in = input_json_path
        
        if self.path_in:
            self.path_out = self.path_in[:-5] + '.html'
            with open(self.path_in, 'r') as fi:
                self.jsobj = json.load(fi)
        else:
            self.path_out = 'out.html'
            self.jsobj = json.load(sys.stdin)

    def write(self, do_out_stdout):
        if do_out_stdout:
            print(self.tohtml())
        else:
            with open(self.path_out, 'w') as fo:
                fo.write(self.tohtml())        

    def tohtml(self):
        html = ''
        with open(os.path.dirname(__file__) + '/visjs.html') as fi:
            html = fi.read()

        subs = {
            '__NAME__' : self.jsobj['info']['name'],
            '__EXECUTED__' : self.jsobj['info']['executed'],
            '__VERSION__' : self.jsobj['info']['version'],
            '__LHS_TIME__' : self.jsobj['info']['elapsed']['lhs'],
            '__CNV_TIME__' : self.jsobj['info']['elapsed']['cnv'],
            '__SOL_TIME__' : self.jsobj['info']['elapsed']['sol'],
            '__ALL_TIME__' : self.jsobj['info']['elapsed']['all'],
            '__NODES__' : self.__nodes(),
            '__EDGES__' : self.__edges(),
            '__RULES__' : json.dumps(self.jsobj['rules']) }
            
        for (key, val) in subs.items():
            html = html.replace(key, str(val))
        return html

    def __depth2color(self, d):
        r = 100 + d * 15
        g = 220 - d * 20
        b = 255 - d * 20
        return 'rgba(%d,%d,%d,1.0)' % (r, g, b)

    ## Convert nodes object in JSON into ones for visjs.
    def __nodes(self):
        nodes = self.jsobj['nodes']
        objs = []
        for n in nodes.values():
            n['color'] = self.__depth2color(float(n['depth'])) if n['active'] else 'lightgray'
            n['shape'] = 'box' if (n['type'] != 'junction') else 'dot'
            n['level'] = n['depth']
            if n['type'] == 'junction':
                n['size'] = 5
                del n['label']
            objs.append(n)
        return json.dumps(objs)

    ## Convert edge object in JSON into ones for visjs.
    def __edges(self):
        edges = self.jsobj['edges']
        objs = []
        for idx, e in edges.items():
            obj = {
                'id' : int(idx),
                'type' : e['type'],
                'eid' : int(e['eid']),
                'rid' : int(e['rid']),
                'from' : e['tail'],
                'to' : e['head'],
                'label' : e['label'],
                'color' : {} }
            
            if e['type'] == 'hypothesize':
                obj['color']['color'] = 'blue' if e['active'] else 'lightgray'
                obj['arrows'] = 'to'
            elif e['type'] == 'implicate':
                obj['color']['color'] = 'red' if e['active'] else 'lightgray'
                obj['arrows'] = 'to'
            else:
                obj['color']['color'] = 'green' if e['active'] else 'lightgray'
                obj['dashes'] = True
            obj['color']['highlight'] = obj['color']['color']
            
            objs.append(obj)
        return json.dumps(objs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "input_json_file", nargs='?', default="",
        help='directed graph file output by david.visualize.\n'
        + 'output file\'s extention is automatically set to ".dot"\n'
        + 'stdin used if not specified (and output to stdout).\n')
    parser.add_argument("-f", help="output to file. (always stdout without this options)",
                        action="store_true")
    parser.add_argument("-r", help="don't output ranks to dot file.",
                        action="store_true")
    parser.add_argument("--version", action="version", version="%(prog)s " + VERSION_STRING)

    args = parser.parse_args()

    visjs = Visjs(args.input_json_file)
    visjs.write(args.f)


if __name__=='__main__':
    main()
