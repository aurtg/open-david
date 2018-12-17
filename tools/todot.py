#! /usr/bin/python3
# -*- coding: utf-8 -*-

## @file todot.py
#
#  @brief visualize.pyの出力したノードを単位としたエッジにより表現された有向グラフ構造を読み込み、
#  DOTフォーマットに変換して出力する。
#

import argparse
import json
#import pdb # pdb.set_trace()用
import sys

## @name このスクリプトのバージョン文字列
##@{
VERSION_STRING = "1.0.0"
##@}

## DOT出力の細部をコントロールするためのラッパークラス
#
class DotForm():

    ## インスタンスの初期化時の処理。
    #
    #  @param fp 出力先ファイル
    #  @param stderr_ エラーメッセージ出力先を変更する際に指定する
    #  
    def __init__(self,fp,stderr_=sys.stderr):
        ## インデントのレベル
        self.__indent = 0
        ## 出力先ファイル
        self.__fp = fp
        ## ノードのshape属性。(非デバッグモード時。中継ノードは点にする。他はデフォルトを使う）
        self.__node_shape = { "junction": "point" }
        ## ノードのshpae属性。（デバッグモード時。中継ノード内にもラベルを表示する）
        self.__node_shape_debug = { "junction": "rectangle" }
        ## ノードの線のスタイル。ノード種別とactiveによって変化させる。
        self.__node_style = { "junction": { True: "bold", False: "dotted" },
                              "observable": { True: "solid", False: "dotted" },
                              "hypothesized": { True: "solid", False: "dotted" } }
        ## ノードの線の色。ノード種別とactiveによって変化させる。
        self.__node_color = { "observable":   { True: "#808080", False: "#8f8f8f" },
                              "hypothesized": { True: "#608080", False: "#6f8f8f" } }
        ## ノードの背景の色。depthとactiveによって変化させる。
        self.__node_bgcolor = { 0: { True: "#f0f0f0", False: "#ffffff" },
                                1: { True: "#f0f0d0", False: "#ffffdf" },
                                2: { True: "#f0f0c0", False: "#ffffcf" } }
        ## エッジの線のスタイル。エッジ種別とactiveによって変化させる。
        self.__edge_style = { "unify":   { True: "dashed", False: "dotted" },
                              "hypothesize": { True: "bold", False: "dotted" } }
        ## エッジの線の色。エッジ種別とactiveによって変化させる。
        self.__edge_color = { "unify":   { True: "#707070", False: "#7f7f7f" },
                              "hypothesize": { True: "#808060", False: "#8f8f6f" } }

        ## エラー／デバッグメッセージ出力先
        self.stderr_ = stderr_
        
    ## 現在のインデント分の空白文字を返す。
    def indents(self):
        return "    " * self.__indent

    ## インデントを進める
    def indent(self):
        self.__indent += 1

    ## インデントを戻す
    def dedent(self):
        if self.__indent > 0:
            self.__indent -= 1

    ## インデントとともにファイルへ指定された文字列を出力する
    def print(self,*args,**kwds):
        print(self.indents(), *args, sep='', file=self.__fp, **kwds)

    ## ノードのshapeを表す文字列を返す。
    #
    # @param type_ ノード種別(文字列)
    # @param debug_mode デバッグモードの時True、参照先のテーブルが変わる。
    #
    def node_shape(self,type_,debug_mode=False):
        try:
            if debug_mode:
                return self.__node_shape_debug[type_]
            return self.__node_shape[type_] 
        except KeyError:
            return "ellipse"

    ## ノードの線のスタイルを表す文字列を返す。
    #
    # @param type_ ノード種別(文字列)
    # @param active アクティブなノードの時True
    #
    def node_style(self,type_,active):
        try:
            return self.__node_style[type_][active]
        except KeyError:
            return "solid" if active else "dotted"

    ## ノードの線の色を表す文字列を返す。
    #
    # @param type_ ノード種別(文字列)
    # @param active アクティブなノードの時True
    #
    def node_color(self,type_,active):
        try:
            return self.__node_color[type_][active]
        except KeyError:
            return "#808080"

    ## ノードの背景色を表す文字列を返す。
    #
    # @param depth depth値（数値）
    # @param active アクティブなノードの時True
    #
    def node_bgcolor(self,depth,active):
        try:
            return self.__node_bgcolor[depth][active]
        except KeyError:
            return "#ffffff"

    ## エッジの線のstyleを表す文字列を返す。
    #
    # @param type_ エッジ種別(文字列)
    # @param active アクティブなノードの時True
    #
    def edge_style(self,type_,active):
        try:
            return self.__edge_style[type_][active]
        except KeyError:
            return "solid" if active else "dotted"

    ## エッジの線の色を表す文字列を返す。
    #
    # @param type_ エッジ種別(文字列)
    # @param active アクティブなノードの時True
    #
    def edge_color(self,type_,active):
        try:
            return self.__edge_color[type_][active]
        except KeyError:
            return "#404040"

#----

## DOTファイル出力処理メイン
#
def output_dot(digraph, fp, without_rank=False, stderr_=sys.stderr):
    df = DotForm(fp, stderr_)
    df.print("digraph {")
    df.indent()
    df.print("")
    df.print("layout=dot")
    df.print("")
    _output_default_graph_attr(df)
    df.print("")
    _output_default_node_attr(df)
    df.print("")
    _output_default_edge_attr(df)
    df.print("")
    _output_nodes(digraph, df)
    df.print("")
    _output_edges(digraph, df)
    if not without_rank:
        df.print("")
        _output_ranks(digraph, df)
    df.dedent()
    df.print("}\n")

## GRAPHのデフォルト属性の出力
#
#  @note 今のところ出力するものは無い
#
def _output_default_graph_attr(df):
    df.print("# default graph parameters if any")
    df.print("graph [")
    df.indent()
    #df.print("rankdir=LR")
    df.dedent()
    df.print("]")
#----

## NODEのデフォルト属性の出力
#
#  @note 今のところ出力するものは無い
#
def _output_default_node_attr(df):
    df.print("# default node parameters if any")
    df.print("node [")
    df.indent()
    df.dedent()
    df.print("]")
#----

## EDGEのデフォルト属性の出力
#
#  @note 今のところ出力するものは無い
#
def _output_default_edge_attr(df):
    df.print("# default edge parameters if any")
    df.print("edge [")
    df.indent()
    df.dedent()
    df.print("]")
#----

## 有向グラフのノードを出力する
#
def _output_nodes(digraph, df):
    df.print("# node definitions")
    for dn in sorted(digraph['nodes'].values(), key=lambda n: n['id']):
        df.print("N", dn['id'], " [")
        df.indent()
        _active = dn['active']
        _depth = dn['depth']
        _label = dn['label']
        _type = dn['type']
        _debug_mode = "<" in _label
        df.print("shape=", '"', df.node_shape(_type,_debug_mode), '"')
        df.print("style=", '"', df.node_style(_type,_active), '"')
        df.print("label=", '"', _label, '"')
        df.print("color=", '"', df.node_color(_type,_active), '"')
        df.print("bgcolor=", '"', df.node_bgcolor(_depth,_active), '"')
        df.dedent()
        df.print("]")
#----

## 有向グラフのエッジを出力する
#
def _output_edges(digraph, df):
    df.print("# edge definitions")
    for de in sorted(digraph['edges'].values(),
                     key=lambda e: str(e['tail'])+"-" + str(e['head'])+"-"+e['label']):
        _active = de['active']
        _type = de['type']
        df.print("N", de['tail'], "->", "N", de['head'], "[")
        df.indent()
        df.print("label=", '"', de['label'], '"')
        df.print("color=", '"', df.edge_color(_type,_active), '"' )
        if _type == "unify":
            df.print("dir=none;style=", df.edge_style(_type,_active))
        else:
            df.print("style=", df.edge_style(_type,_active))
        df.dedent()
        df.print("]")
#----

## ランクの情報を出力する。
#
def _output_ranks(digraph, df):
    def print_rank(same_depth):
        if len(same_depth) >= 2:
            rank_items = ""
            for s in sorted(same_depth,key=lambda x:x['id']):
                rank_items += " N"+str(s['id'])+";"
            df.print( "{ rank=same;", rank_items, " }")
        
    df.print("# ranks")
    last_depth = -1
    same_depth = []
    for dn in sorted(digraph['nodes'].values(),key=lambda x:x['depth']):
        if dn['depth'] == last_depth:
            same_depth.append(dn)
        else:
            print_rank(same_depth)
            last_depth = dn['depth']
            same_depth = [ dn ]
    print_rank(same_depth)
#

## 入力のjsonファイルを読み取って、読み取り結果の辞書を返す。
#
#  @note この関数は標準入力から読み取る場合を想定している。
#
def read(fp):
    digraph = json.load(fp)
    return digraph

## 入力のjsonファイルを読み取って、読み取り結果の辞書を返す。
#
#  @note この関数はファイルから読み取る場合に使う。
#
def read_from_file(filename):
    with open(filename, "r") as fp:
        digraph = read(fp)
    return digraph


def stub():
    """This is a stub for doxygen malfunction.
    Doxygen generates false callgraph from read_from_file() to todot() without this."""
    pass


## 処理本体
#
def todot(input_json_file,
          without_rank=False, output_to_file=False, stderr_=sys.stderr ):
    # 入力がファイルの場合、ファイル名をチェック
    if len(input_json_file) > 5 and input_json_file[-5:].lower() != ".json":
        print( "input has to be .json file", file=stderr_ )
        return False

    option_str = ""
    if without_rank:
        option_str += "-r"
    
    # 入力ファイル又は標準入力からグラフ情報を読み取る。
    # （入力元とコマンドラインオプションに応じて出力ファイル名を合成する）
    outfile = ""
    if input_json_file == "":
        digraph = read(sys.stdin)
        if output_to_file:
            outfile = "out" + option_str +".dot"
    else:
        digraph = read_from_file(input_json_file)
        if output_to_file:
            outfile = input_json_file[:-5] + option_str + ".dot"

    # 標準出力又は出力先ファイルへDOTイメージを出力する。
    if outfile == "":
        output_dot(digraph, fp=sys.stdout, without_rank=without_rank, stderr_=stderr_)
    else:
        with open(outfile, "w") as fp:
            output_dot(digraph, fp=fp, without_rank=without_rank, stderr_=stderr_)

    #pdb.set_trace()
    return True
#----

    
## コマンドラインから単体で起動されたときの処理
#
def main():
    # パーサーに引数の情報を登録
    parser = argparse.ArgumentParser()
    parser.add_argument("input_json_file", nargs='?', default="",
                        help="directed graph file output by david.visualize.\n"
                        +'output file\'s extention is automatically set to ".dot"\n'
                        +"stdin used if not specified (and output to stdout).\n")
    parser.add_argument("-f", help="output to file. (always stdout without this options)",
                        action="store_true")
    parser.add_argument("-r", help="don't output ranks to dot file.",
                        action="store_true")
    parser.add_argument("--version", action="version", version="%(prog)s " + VERSION_STRING)
    # コマンドライン引数をパース
    args = parser.parse_args()

    if not todot( args.input_json_file, args.r, args.f ):
        parser.print_usage()
        exit(-1)
#----


## モジュールの初期化処理（単体起動の時はmain()を実行する）
if __name__ == "__main__":
    main()
