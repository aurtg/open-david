#! /usr/bin/python3
# -*- coding: utf-8 -*-

## @file visualize.py
#
#  @brief Davidの出力したハイパーノードを単位としたエッジ表現のjsonファ
#  イルから、ノードを単位としたエッジによる有向グラフの構造を作成して
#  jsonファイルに出力する。
#
#  @note utilモジュールの相対インポートが可能なときはdavidパッケージの
#        一部として使われている。そうでない（importがSystemError又は
#        ImportErrorになる）時はたぶん単独で使われている。
#

import argparse
import enum
import json
#import pdb # pdb.set_trace()を使う時は必要
import sys

try: 
    from . import util
except (ImportError, SystemError): 
    import util

## @name このスクリプトのバージョン文字列
##@{
VERSION_STRING = "1.1.0"
##@}


## デバッグモードを示す定数の定義
class DebugMode(enum.IntEnum):
    NONE = 0
    ## 動作トレース出力
    TRACE = 1
    ## 出力のノード／エッジに情報を付加
    INFO = 2
    ## テーブルダンプ
    DUMP = 4


## デバッグモードフラグの設定とモードの判定
class DebugFlag:
    ## デバッグモード変数（クラス変数）
    debug_mode = 0

    ## コマンドラインパラメータを元にデバッグフラグを登録する

    @classmethod
    def register(cls,clear=False,debug_trace=False,debug_dump=False,debug_info=False):
        if clear:
            cls.debug_mode = 0
        if debug_trace:
            cls.add(DebugMode.TRACE)
        if debug_dump:
            cls.add(DebugMode.DUMP)
        if debug_info:
            cls.add(DebugMode.INFO)

    ## debug_modeクラス変数へのフラグの追加
    #
    @classmethod
    def add(cls,mode):
        cls.debug_mode |= mode

    ## debug_modeクラス変数の特定のフラグがオンかどうかを返す
    #
    # @return 指定されたビットパターンでフラグが一つでもオンであればTrue。
    #
    @classmethod
    def get(cls, flag = ~0):
        return (cls.debug_mode & flag) != 0


## 実行中又は呼び出しスタックにある関数名を返す
#
# @param depth フレーム番号、0=現在実行中、1=呼び出しているフレーム、...
#
# @attention
#   sysのprotectedなメンバへの参照になるのでpylintに掛けると警告が出るが、この関数では抑止する。
#
# @note
#   この関数への呼び出し自体がスタックフレームを一つ進めるので、
#   ここでsys._getframe()に与えるdepth引数は直接sys._getframe()を
#   呼び出す時よりは一つ大きな値を指定する必要がある。
#
# @hidecallergraph
def function_name(depth=0):
    return sys._getframe(depth+1).f_code.co_name # pylint: disable=protected-access
#---

## ノードの表現。DavidNodeから、又は個別に属性を与えて生成可能
#
class Node:

    ## インスタンス生成時の初期化（メンバ変数の定義）
    #
    #  @param[in] id_    ノードの識別番号（整数）
    #  @param[in] label  ノードの名前（文字列）
    #  @param[in] type_  ノードのタイプ（文字列）
    #  @param[in] depth  depth値（数値）
    #  @param[in] active ノードがactiveかどうかを示す（真偽）
    #  @param[in] dn     DavidNodeからコピーする時はこの引数だけ指定すればよい。他の引数は無視される。
    #
    def __init__(self, id_=-1, label="", type_="", depth=-1, active=False, dn=None):
        ## ノードの識別番号（整数）
        self.id = id_
        ## ノードの名前（文字列）
        self.label = label
        ## ノードのタイプ（文字列）
        self.type = type_           
        ## ノードのdepth値（数値）
        self.depth = depth
        ## ノードがactiveであればTrue
        self.active = active
        if dn:
            self.id = dn.index()
            self.label = dn.atom()
            self.type = dn.type()
            self.depth = dn.depth()
            self.active = dn.active()
        if DebugFlag.get(DebugMode.INFO):
            self.label += "\n[{}:{}/{:G}/{}]".format(self.id,self.type,self.depth,
                                                     "active" if self.active else "not-active")

    ## ノードのラベルからデバッグ情報を取り除いたatom部分を返す。
    #
    def atom(self):
        try:
            return self.label[0:self.label.index("\n")]
        except ValueError:
            return self.label

    ## ノードのラベルのデバッグ情報を取り除いたatom部分を、atomsリストの情報で置き換える。
    #
    # @param atoms atoms[0]は述語を示す文字列、atoms[1]はパラメータ文字列のリスト
    #
    # @note
    #   atomsはタプル(immutable)かも知れないしリスト(mutable)かも知れない。
    #
    def replace_label(self, atoms):
        try:
            debug_info = self.label[self.label.index("\n"):]
        except ValueError:
            debug_info = ""
        if atoms[0] == "=":
            self.label = "(" + atoms[1][0] + " = " + atoms[1][1] + ")" + debug_info
        else:
            self.label = atoms[0] + "(" + ", ".join(atoms[1]) + ")" + debug_info

#----

## エッジの表現。
class Edge:

    ## インスタンス生成時の初期化（メンバ変数の定義）
    # @param[in] eid 入力データにおけるエッジ配列のインデックス
    # @param[in] tail 始点Nodeの識別番号（整数）
    # @param[in] head 終点Nodeの識別番号（整数）
    # @param[in] label エッジの名前（文字列）
    # @param[in] type_ エッジのタイプ（文字列）
    # @param[in] active エッジが有効かどうかを示す（真偽）
    # 
    def __init__(self, eid=-1, tail=-1, head=-1, rid=-1, label="", type_="", active=False):
        ## 始点Nodeの識別番号（id値、整数）
        ## @attention Node配列の索引番号ではない。
        self.tail = tail
        ## 始点Nodeの識別番号（id値、整数）
        ## @attention Node配列の索引番号ではない。
        self.head = head
        ## 使われているルールのID番号
        ## ルールと無関係の場合は -1 を与える
        self.rid = rid
        ## エッジの名前（文字列）
        self.label = label
        ## エッジのタイプ（文字列）
        self.type = type_
        ## エッジが有効なときTrue
        self.active = active
        ## 元データにおけるEdge-idx
        ## @attention 元データに無いエッジの場合は -1 を与える。
        self.eid = eid
#----


## 有向グラフ作成／管理用クラス
class DiGraph:

    ## インスタンス初期化時の処理（メンバ変数の定義）
    #
    #  @note dictで表現されるデータはjson.dump()の対象。それ以外は作業用
    #
    #  @param do_unificate ユニフィケーションを行なう時はTrue
    #  @param stderr_ エラーメッセージ出力先を変更する際に指定する
    #
    def __init__(self,do_unificate,stderr_=sys.stderr):
        ## jsonの出力に使う辞書データ。
        self._digraph = {}
        self._digraph['version'] = VERSION_STRING
        self._digraph['nodes'] = {}
        self._digraph['edges'] = {}
        self._digraph['info'] = {}
        self._digraph['rules'] = []

        ## @name 作業用変数。Nodeデータのリスト、Edgeデータのリスト、等価関係の辞書、他
        #@{
        ## Nodeの操作用の配列
        self._nodes = []
        ## Edgeの操作用の配列
        self._edges = []
        ## パラメータの等価関係を求めた辞書
        self._equal_params = {}
        ## 等価関係の前後関係を規定するための出現順の通し番号
        self._equal_seq = 0
        ## 等価なノードのidのリストのリスト
        self._equal_nodes = []
        ## ユニフィケーションを実行するかどうかのフラグ
        self._do_unificate = do_unificate
        ## エラーやトレース、ダンプの出力先
        self._stderr = stderr_
        #@}
    #----
        
    ## 一つのproof graphを元に一つのグラフ構造を生成する
    def construct(self,pg):
        self._create_nodes(pg) # 出力用のノードの配列を作る
        self._create_edges(pg) # 出力用のエッジの配列を作る
        self._create_rules(pg)
        self._unify_identical_nodes() # 同一論理式のノードを繋ぐunifyエッジを追加する
        if self._do_unificate:
            self._remove_nonactive_elements() # activeでない要素を削除する
            self._unify_equivalent_nodes()    # 等価関係に基づいてノードを整理・統合する
            self._remove_redundant_edges()    # 始点又は終点が欠けているエッジを削除する
        self._add_nodes() # 出力用の辞書にノードを追加する
        self._add_edges() # 出力用の辞書にエッジを追加する
    #----

    ## jsonファイルへの有向グラフデータの出力
    def output(self,fp=sys.stdout):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        json.dump(self._digraph, fp=fp, indent="\t", sort_keys=True)
        print(file=fp) # json.dump() doesn't output last line's new-line.
    #----

    ## proof-graph から実行時間などのメタ情報を抽出してJSONに追加する。
    def add_meta_info(self, name, value):
        self._digraph['info'][name] = value

    ## proof graphからノードの情報を抽出して出力用Nodeリストの原型を作成する
    def _create_nodes(self,pg):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        for dn in sorted(pg.nodes.values(),key=lambda n_: n_.index()):
            self._nodes.append(Node(dn=dn))
    #----

    ## proof graphからhypernode向けのエッジの情報を抽出してnode向けに再構成し、
    # Edgeリストの原型を作成する。
    # 処理に伴い、Nodeリストの一部が削除されたり、又は追加されることがある。
    def _create_edges(self,pg):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        for de in sorted(pg.edges.values(),key=lambda d_: d_.index()):
            self._decompose_hypernode_edge(de,pg)
    #----

    ## proof-graph に含まれるルール群を整形した上で self._digraph に追加する。
    def _create_rules(self, pg):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        for r in pg.rules.values():
            self._digraph['rules'].append({
                'rid' : r.rid(),
                'name' : r.name(),
                'formula' : r.string() })

    ## 同一の論理式を持つノードの間にunifyエッジを追加する
    #
    #  @details
    #    処理で使われるatoms変数は論理式の文字列をキーとし、
    #    同一論理式のノードのリストを値として持つ辞書。<br>
    #    ノードリストの要素が二つ以上あればunifyエッジを張る必要がある。<br>
    #    但し、等価なunifyエッジが既に存在する場合は張らない。
    #    （この判定のために、既存のエッジの始終点のノード番号のペアを要素とする
    #      existing_edges集合を作成する）
    #
    def _unify_identical_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        atoms = {}
        is_equality = lambda n: n.atom().startswith('(')
        
        for n in self._nodes:
            try:
                if n.atom() and not is_equality(n) and n.type != "junction":
                    atoms[n.atom()].append(n)
            except KeyError:
                atoms[n.atom()] = [n]

        existing_edges = { tuple(sorted((e.tail, e.head)))
                           for e in self._edges if e.type == "unify" }
        for nodes in sorted(atoms.values(), key=lambda n: n[0].id):
            if len(nodes) > 1:
                last_n = nodes[0]
                for n in nodes[1:]:
                    if tuple(sorted((last_n.id, n.id))) in existing_edges:
                        continue
                    if DebugFlag.get(DebugMode.TRACE):
                        print("TRACE: {}() appended UNIFY EDGE between "
                              "node[{}:{}] and node[{}:{}]."
                              .format(function_name(), last_n.id, last_n.atom(), n.id, n.atom()),
                              file=self._stderr)
                    self._edges.append(Edge(-1, last_n.id, n.id, -1,
                                            "", "unify", (last_n.active and n.active)))
                    existing_edges.add(tuple(sorted((last_n.id, n.id))))

                    last_n = n
    #----

    ## アクティブでないNode／Edgeを削除する
    def _remove_nonactive_elements(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}() -u".format(function_name()), file=self._stderr)

        new_nodes = [ n for n in self._nodes if n.active ]
        self._dump_nodes_diff(new_nodes)
        self._nodes = new_nodes

        new_edges = [ e for e in self._edges if e.active ]
        self._dump_edges_diff(new_edges)
        self._edges = new_edges
    #----

    ## 等価関係に基づいてノードを整理・統合する
    def _unify_equivalent_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}() -u".format(function_name()), file=self._stderr)

        self._list_equalities()
        #pdb.set_trace()
        self._consolidate_equal_parameters()
        #pdb.set_trace()
        self._merge_logically_equal_nodes()
    #----

    ## 等価関係にある変数の対、又は変数と定数の対を列挙した辞書を作成する。<br>
    #  キーは原則として変数であり、キーと等価関係にある変数又は定数を値として保持する。<br>
    #  等価関係は、定数との等価関係が最優先、変数間の等価関係は先に現れた方が優先される。
    def _list_equalities(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        self._equal_params = {}
        self._equal_seq = 0
        for n in self._nodes:
            if n.type == "junction":
                continue
            atoms = util.splitAtom(n.atom())
            if atoms[0] != '=':
                continue
            n.active = False # 後で削除する際のマークにする
            self._append_equal_params(atoms[1][0],atoms[1][1])
        self._normalize_equal_params()

        if DebugFlag.get(DebugMode.TRACE):
            for k, e in sorted(self._equal_params.items()):
                print("TRACE: {}() _equal_params[{}] = {}"
                      .format(function_name(), k, e), file=self._stderr)

        new_nodes = [ n for n in self._nodes if n.active ]
        self._dump_nodes_diff(new_nodes)
        self._nodes = new_nodes
    #----

    ## x=yの等価関係の内容に基づき、新規の関係を追加、又は既存の関係を更新する。<br>
    #
    #  @note _equal_paramsは 置換文字列をキーとし, 置換対象変数のリストを値とする辞書。<br>
    #        最終的には、変数名が対象リストにあれば置換対象の文字列（変数又は定数）
    #        に置き換える、という風に使う。<br>
    #        この関数ではテーブル構築の前段（個別の関係の登録と簡単な統合処理）を担当。
    #
    # @param x 等価関係の左辺
    # @param y 等価関係の右辺
    #
    # @details
    # - 変数と変数の等価関係は、初出の場合、キーには x、値（置換対象のリスト）
    #   には 出現順を示す通し番号と y を登録する。<br>
    #   登録される等価関係のx（y）が既に辞書のキーに存在する場合、
    #   y（x）を値（置換対象のリスト）に追加する。<br>
    #   （注:重複は許さない）
    # - 定数と変数の等価関係は、定数が初出の場合、定数をキー、変数を値として
    #   （出現順を示す通し番号と共に）登録する。<br>
    #   定数が既に辞書のキーとして存在する場合、変数を値のリストに追加する。<br>
    #   （注:重複は許さない）。
    # - 定数と定数の等価関係は、変数と変数の等価関係の処理に順ずる。
    #
    def _append_equal_params(self, x, y):
        x_is_var = not util.isConst(x)
        y_is_var = not util.isConst(y)
        if (x_is_var and y_is_var) or (not x_is_var and not y_is_var):
            if not self._equal_params or (not x in self._equal_params.keys() and not y in self._equal_params.keys()):
                self._equal_params[x] = [self._equal_seq, y]
                self._equal_seq += 1
            elif x in self._equal_params.keys():
                if not y in self._equal_params[x].values():
                    self._equal_params[x].append(y)
            elif not x in self._equal_params[y].values():
                self._equal_params[y].append(x)
        else:
            c, v = (y, x) if x_is_var else (x, y)
            if not c in self._equal_params.keys():
                self._equal_params[c] = [self._equal_seq]
                self._equal_seq += 1
            if not v in self._equal_params[c]:
                self._equal_params[c].append(v)
    #----

    ## 等価関係にある_equal_paramsテーブルの要素を統合して一つの代表値に収束させる。
    #  
    # @details
    # - _equal_paramsの要素xの値のリストに要素yのキー又はリストの値のいずれかが含まれる場合、
    #   又は、要素yの値のリストに要素xのキー又はリストの値のいずれかが含まれる場合、
    #   要素xと要素yの値のリストを統合する。
    # - 上記の処理を、統合が発生しなくなるまで繰り返す。
    # - 最後に、通し番号が負値になっている等価関係を削除して_equal_params辞書を完成する。
    #
    def _normalize_equal_params(self):
        while True:
            integrated = False
            for kx, vx in sorted(self._equal_params.items()):
                if vx[0] < 0:
                    continue
                for ky, vy in sorted(self._equal_params.items()):
                    if kx is ky or vy[0] < 0:
                        continue
                    if self._integrate_equal_params(kx, ky) \
                    or self._integrate_equal_params(ky, kx):
                        integrated = True
            if not integrated:
                break
        self._equal_params = { k : v for k, v in sorted(self._equal_params.items()) if v[0] >= 0 }
    #----

    ## _equal_paramsの要素x（キー=kx）の値のリストに要素y（キー=ky）
    #  のキー又はリスト中の変数／定数が含まれる場合、<br>
    #  要素xと要素yの値のリストを統合する。
    #  
    # @note
    # - 統合の際は、代表値が定数であるか、又は出現順を示す通し番号が小さい方に追加し、
    #   追加された方の通し番号は（後で辞書から削除するために）-1に変更する。
    #
    # @param kx 要素xのキー文字列
    # @param ky 要素yのキー文字列
    #
    # @return 統合された場合はTrue
    #
    def _integrate_equal_params(self, kx, ky):
        is_equal = (kx in self._equal_params[ky][1:]) or \
                   [vx for vx in self._equal_params[kx][1:] if vx in self._equal_params[ky][1:]]
        if not is_equal:
            return False

        kx_is_const, ky_is_const = util.isConst(kx), util.isConst(ky)
        km, kd = kx, ky
        if (not kx_is_const and ky_is_const) \
        or (kx_is_const == ky_is_const and self._equal_params[kx][0] > self._equal_params[ky][0]):
            km, kd = ky, kx

        for v in [kd] + self._equal_params[kd][1:]:
            if not v in self._equal_params[km][1:]:
                self._equal_params[km].append(v)
        self._equal_params[kd][0] = -1
        return True
    #----

    ## 等価関係にある全ての（ノードの論理式の）パラメータを代表値に置き換える。
    #
    #
    def _consolidate_equal_parameters(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        for n in self._nodes:
            if n.type == "junction":
                continue
            self._replace_atom_parameters(n)
    #----

    ## ノードのパラメータを_equal_params辞書に基づいて代表値に置換し、labelを書き換える。
    #
    # @return 置換を行なった時はTrue
    #
    def _replace_atom_parameters(self, n):
        replaced = False
        atoms = util.splitAtom(n.atom())
        if atoms[0] == "=":
            return False
        new_atoms = [ atoms[0], list(atoms[1]) ]
        for i, a in enumerate(atoms[1]):
            for v, e in sorted(self._equal_params.items()):
                if a in e[1:]:
                    new_atoms[1][i] = v
                    replaced = True
                    break
        if replaced :
            n.replace_label(new_atoms)
        return replaced
    #----
    
    ## 全く同じ論理式を持つノードをマージする。
    # @details
    #   複数のノード間で一番小さなidを持つものを見つけるために、_equal_nodes リストを用いる。<br>
    #   _equal_nodes は同一論理式のノードのidのリストを要素にもつリスト。<br>
    #   各リストは先頭に一番小さなノードのidが格納されるように構築する。
    #
    def _merge_logically_equal_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        self._equal_nodes = []
        self._list_equal_nodes()
        self._merge_equal_nodes()
        self._relink_edges_with_equal_nodes()
    #----
    
    ## 全く同じ論理式を持つノードを見つけて、idの小さい順に列挙する。
    def _list_equal_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        last_atom = ""
        self._equal_nodes = []
        same_nodes = []
        sorted_nodes = sorted(self._nodes, key=lambda n: n.atom()+"\n"+str(n.id))
        for n in sorted_nodes:
            if n.type == "junction":
                continue
            cur_atom = n.atom()
            if cur_atom != last_atom:
                if len(same_nodes) > 1:
                    self._equal_nodes.append(same_nodes)
                last_atom = cur_atom
                same_nodes = [n.id]
            else:
                same_nodes.append(n.id)
        if len(same_nodes) > 1:
            self._equal_nodes.append(sorted(same_nodes))
    #----
    
    ## 同じ論理式を持つノードを、idが一番小さいものを残して他は消去する。
    def _merge_equal_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        for n in self._nodes:
            for enlist in self._equal_nodes:
                if n.id in enlist[1:]:
                    n.active = False # 削除する際のマークにする
                    break

        new_nodes = [ n for n in self._nodes if n.active ]
        self._dump_nodes_diff(new_nodes)
        self._nodes = new_nodes
    #---

    ## 消去されるノードに対して張られているエッジの端点を残されるノードに付け替える。
    def _relink_edges_with_equal_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        for e in self._edges:
            for enlist in self._equal_nodes:
                if e.tail in enlist[1:]:
                    e.tail = enlist[0]
                    break
            for enlist in self._equal_nodes:
                if e.head in enlist[1:]:
                    e.head = enlist[0]
                    break
    #----

    ## 始点又は終点が欠けているエッジ及び始点と終点が同一のエッジを削除する
    def _remove_redundant_edges(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}() -u".format(function_name()), file=self._stderr)

        new_edges = [ e for e in self._edges if e.tail >= 0 and e.head >= 0 and e.tail != e.head ]
        self._dump_edges_diff(new_edges)
        self._edges = new_edges
    #----

    ## ノードの情報をjson.dump()用の辞書に設定する
    def _add_nodes(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        self._digraph['nodes'] = {}
        for i, n in enumerate(self._nodes):
            self._digraph['nodes'][i] = n.__dict__
    #----

    ## エッジの情報をjson.dump()用の辞書に設定する
    def _add_edges(self):
        if DebugFlag.get(DebugMode.TRACE):
            print("TRACE: {}()".format(function_name()), file=self._stderr)

        self._digraph['edges'] = {}
        for i, e in enumerate(self._edges):
            self._digraph['edges'][i] = e.__dict__
    #----

    ## ハイパーノードを構成要素とするエッジを、
    #  個別のノードを構成要素とするエッジに分解してEdge配列に追加する<br>
    #  中継ノードが必要なときはNode配列の末尾に追加する
    #
    #  - unifyエッジの場合、常に2個ある筈の始点ノードを無向エッジで繋ぐ。
    #    その際、-uオプションが指定されておらず、
    #    終点ノードのそれぞれに対する参照が他に無い場合は、
    #    当該の終点ノードは出力されないようにする。
    #    （-u指定時は終点に記載された情報が処理に必要で、
    #      ノードはそちらの処理で消去される）
    #  - 始点と終点にそれぞれノードが一個だけある時は、
    #    始点から終点への有向エッジをひとつ生成する。
    #  - 始点又は終点にノードが複数ある時は中継ノードを生成して、
    #    全ての始点から中継ノードへのエッジ及び、
    #    中継ノードから全ての終点への有向エッジを生成する。
    #  - （unifyエッジ以外で）始点又は終点いずれかにノードが無い場合はエッジは生成しない。
    # 
    # @param[in] de DavidEdgeのインスタンス
    # @param[in] pg de の参照先のハイパーノード／ルールを含む DavidProofGraph のインスタンス
    #
    def _decompose_hypernode_edge(self,de,pg):
        try:
            # ハイパーノードの終点側を構成するノードのリスト
            headnodes = pg.hypernodes[de.head()].referNodes()
        except KeyError:
            headnodes = [] # 終点が無い
        try:
            # ハイパーノードの始点側を構成するノードのリスト
            tailnodes = pg.hypernodes[de.referTail().index()].referNodes()
        except KeyError:
            tailnodes = [] # 始点が無い

        edgename = self._edge_name(de, headnodes)

        if de.type() == "unify":                          # unifyエッジ
            # 始点ノード間をつなぐエッジを生成
            tail_index = tailnodes[0].index() if len(tailnodes) > 0 else -1
            head_index = tailnodes[1].index() if len(tailnodes) > 1 else -1
            self._edges.append(
                Edge(de.index(), tail_index, head_index, -1, edgename, de.type(), de.active()))

            if not self._do_unificate:
                # 元のプルーフグラフでこのエッジ以外では参照されていない終点ノードを、
                # 出力用のノードデータから削除
                removing_node_ids = set()
                for n in headnodes:
                    related_edges = [e for e in sorted(pg.edges.values(), key=lambda e_: e_.index())
                                     if (not e is de) and 
                                     ((e.referHead() and (n in e.referHead().referNodes())) or
                                      n in e.referTail().referNodes()) ]
                    if not related_edges:
                        removing_node_ids.add(n.index())
                new_nodes = [n for n in self._nodes if not n.id in removing_node_ids]
                self._dump_nodes_diff(new_nodes)
                self._nodes = new_nodes

        # 一つの始点ノードに一つの終点ノード
        elif len(tailnodes) == 1 and len(headnodes) == 1:
            self._edges.append(Edge(de.index(), tailnodes[0].index(), headnodes[0].index(), de.rule(),
                                    edgename, de.type(), de.active()))
        # 始点又は終点に複数のノード
        elif len(tailnodes) > 1 or len(headnodes) > 1:
            depth = (sum([n.depth() for n in tailnodes]+[n.depth() for n in headnodes])
                     / (len(tailnodes) + len(headnodes)))
            # 中継ノードのIDは既存のidの最大値+1（注:要素数+1ではダメ）
            relay_node = Node(max((n.id for n in self._nodes))+1,
                              edgename, "junction", depth, de.active())
            self._nodes.append(relay_node)
            for n in tailnodes:
                self._edges.append(Edge(de.index(), n.index(), relay_node.id, de.rule(),
                                        edgename, de.type(), de.active()))
            for n in headnodes:
                self._edges.append(Edge(de.index(), relay_node.id, n.index(), de.rule(),
                                        edgename, de.type(), de.active()))
    #----

    ## エッジの名前を合成して返す
    #
    # @param[in] de 名前を付けるDavidEdge
    # @param[in] headnodes 終点ノードのDavidNodeのリスト
    #
    # @return de.typeがhypothesize又はimplicateの時はルール名、<br>
    #         unifyの時は終点ノードのatomを^で連結。<br>
    #         名前が付けられない時は空文字列
    #
    def _edge_name(self, de, headnodes):
        edgename = ""
        rulestr = ""
        if de.type() == "hypothesize" or de.type() == "implicate":
            if de.referRule():
                edgename = de.referRule().name()
                rulestr = "/rid={}".format(de.referRule().rid())
        elif de.type() == "unify":
            edgename = " ^ ".join([n.atom() for n in headnodes])

        if DebugFlag.get(DebugMode.INFO):
            edgename += "\n<{}:{}/{}{}>".format(de.index(),de.type(),
                                                "active" if de.active() else "not-active",
                                                rulestr)
        return edgename
    #----

    ## TRACEオンであれば呼び出し元関数名と共にノードの差分をダンプ出力する
    #
    def _dump_nodes_diff(self,new_nodes,title="removed nodes"):
        if DebugFlag.get(DebugMode.TRACE):
            nodes_diff = set([n for n in self._nodes]) - set([n for n in new_nodes])
            if nodes_diff:
                print("TRACE: {}() {} {}"
                      .format(function_name(1), title,
                              [ (n.id, n.atom()) for n in sorted(nodes_diff, key=lambda n_:n_.id)]),
                      file=self._stderr)
    #----
    
    ## TRACEオンであれば呼び出し元関数名と共にエッジの差分をダンプ出力する
    #
    def _dump_edges_diff(self,new_edges,title="removed edges"):
        if DebugFlag.get(DebugMode.TRACE):
            edges_diff = set([e for e in self._edges]) - set([e for e in new_edges])
            if edges_diff:
                print("TRACE: {}() {} {}"
                      .format(function_name(1), title, 
                              [(e.label, str(e.tail)+"->"+str(e.head)) 
                               for e in sorted(edges_diff, 
                                               key=lambda e_:
                                               e_.label+str(e_.tail)+"->"+str(e_.head))]),
                      file=self._stderr)
    #----


def stub():
    """This is a stub for doxygen malfunction.
    Doxygen generates false callgraph from DiGraph._dump_edges_diff() to visualize() without this."""
    pass


## visualize処理本体
#
#  フラグ類の処理後、入力データをDavidObjectのインスタンスに読み込み（DavidObject.read() or .load()）、<br>
#  DavidObjectのresultsに含まれるproof graphごとにそれぞれ有向グラフ構造を生成（DiGraph.construct()）して
#  出力（Digraph.output()）する。
#
# @param input_json_file 処理対象のファイル名（空の時は標準入力）
# @param unification     ユニフィケーション（等価関係の代入）処理を実行する時はTrue
# @param out_to_stdout   結果を標準出力へ出力する時はTrue
# @param debug_trace     デバッグ用トレースを実行する時はTrue
# @param debug_dump      デバッグ用テーブルダンプを実行する時はTrue
# @param debug_info      ノード／エッジのラベルにデバッグ用情報を付加する時はTrue
# @param stderr_         エラーメッセージ出力先を変更する際に指定する
#
# @return 問題なく処理できればTrue、エラーがあればFalse
#
def visualize(input_json_file, 
              unification=False, out_to_stdout=False, 
              debug_trace=False, debug_dump=False, debug_info=False,
              stderr_=sys.stderr):

    # デバッグフラグはクラス変数をモジュール全域で参照する。
    DebugFlag.register(clear=True, debug_trace=debug_trace, 
                       debug_dump=debug_dump, debug_info=debug_info)
    
    # DavidObjectのインスタンスを生成
    obj = util.DavidObject() 

    fn_base = "out" # デフォルトの出力先ファイル名は out.graph.N.json (N=1,2..)
    if input_json_file == "":
        obj.read(sys.stdin) # ファイルが指定されていない時は標準入力から読み込む
    else:
        obj.load(input_json_file)
        fn_base = input_json_file[:-5] # ファイルが指定されている時はファイルに出力

    if out_to_stdout:
        fn_base = ""

    if DebugFlag.get(DebugMode.DUMP):
        obj.dump("DavidObject proof graph (after load).",file=stderr_)

    option_str = ""
    if DebugFlag.debug_mode:
        option_str += "-u" if unification else ""
        option_str += "-di" if DebugFlag.get(DebugMode.INFO) else ""
    
    # DavidObjectのresultsに含まれるproof graphごとにそれぞれ有向グラフ構造を生成して出力する。
    graph_no = 0
    for r in sorted(obj.results.values(), key=lambda r_: r_.index()):
        digraph = DiGraph(unification,stderr_=stderr_)

        digraph.add_meta_info('version', obj.version())
        digraph.add_meta_info('executed', obj.time_executed())
        digraph.add_meta_info('elapsed', r.time_elapsed())
        digraph.add_meta_info('name', r.name())
        
        for pg_index, pg in enumerate(r.solutions):
            graph_no += 1 # グラフ番号は生成順の連番

            if DebugFlag.get(DebugMode.TRACE):
                print("TRACE: processing result[{}]:{}, proof_graph[{}], graph_no={}"
                      .format(r.index(), r.name(), pg_index, graph_no), file=stderr_)
        
            digraph.construct(pg)
            if fn_base == "":
                digraph.output(fp=sys.stdout)
            else:
                filename = "{}.graph.{}{}.json".format(fn_base,graph_no,option_str)
                with open(filename,"w") as fp:
                    digraph.output(fp=fp)

            if DebugFlag.get(DebugMode.TRACE):
                print("TRACE:", file=stderr_)

    #pdb.set_trace() # データの中身を見たいときはここでデバッガに制御を渡す。
    return True
#----    


## コマンドラインから単体で起動されたときの処理
#
#  argparseモジュールを使ってコマンドラインパラメータを解析し、visualize()を呼び出す。
#
def main():
    # パーサーに引数の情報を登録
    parser = argparse.ArgumentParser()
    parser.add_argument("input_json_file", nargs='?', default="",
                        help="David output file to be visualized. (stdin if not specified)" )
    parser.add_argument("-o", help="Output to stdout (to ...graph.N.json {n:1..} if not specified)",
                        action="store_true")
    parser.add_argument("-u", help="Unify equal nodes and/or edges.", action="store_true")
    parser.add_argument("-dt", "--debug_trace", help="with debug trace.", action="store_true")
    parser.add_argument("-dd", "--debug_dump", help="with debug dump.", action="store_true")
    parser.add_argument("-di", "--debug_info", help="put debug info to nodes and edges.", 
                        action="store_true")
    parser.add_argument("--version", action="version", version="%(prog)s " + VERSION_STRING)
    # コマンドライン引数をパース
    args = parser.parse_args()

    return visualize( args.input_json_file, args.u, args.o, 
                      args.debug_trace, args.debug_dump, args.debug_info )
#----


## モジュールの初期化処理（単体起動の時はmain()を実行する）
if __name__ == "__main__":
    main()
