"""Tiny s-expression parser for KiCad files."""
import re

_tok = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


def parse(text):
    stack = [[]]
    for m in _tok.finditer(text):
        o, c, s, a = m.groups()
        if o:
            stack.append([])
        elif c:
            x = stack.pop()
            stack[-1].append(x)
        elif s is not None:
            stack[-1].append(('s', s))
        elif a is not None:
            stack[-1].append(a)
    return stack[0][0]


def val(x):
    return x[1] if isinstance(x, tuple) else x


def find(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]
