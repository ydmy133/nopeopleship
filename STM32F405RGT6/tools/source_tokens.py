"""保留 C 字符串、运算符及预处理行边界，仅忽略注释与普通空白。"""
import hashlib
import json
import re
from pathlib import Path

_LEX = re.compile(
    r'(?P<comment>/\*[\s\S]*?\*/|//[^\n]*)'
    r'|(?P<literal>(?:u8|[LuU])?"(?:\\[\s\S]|[^"\\])*"|(?:[LuU])?\'(?:\\[\s\S]|[^\'\\])*\')'
    r'|(?P<space>\s+)'
    r'|(?P<token>[A-Za-z_]\w*|(?:\d|\.\d)(?:[eEpP][+-]|[A-Za-z_0-9.])*'
    r'|>>=|<<=|\.\.\.|##|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[+*/%&|^!-]=|.)'
)


def c_tokens(source):
    # C 翻译阶段先折叠续行；不能把字符串中的 //、/* 当作注释。
    source = re.sub(r'\\\r?\n', '', source.replace('\r\n', '\n'))
    result, line_start, directive = [], True, False
    for match in _LEX.finditer(source):
        kind, value = match.lastgroup, match.group()
        if kind in ('comment', 'space'):
            if '\n' in value:
                if directive:
                    result.append('<preprocessor-end>')
                line_start, directive = True, False
            continue
        if line_start and value == '#':
            directive = True
        line_start = False
        result.append(value)
    if directive:
        result.append('<preprocessor-end>')
    return result


def token_digest(path):
    tokens = c_tokens(Path(path).read_text(encoding='utf-8-sig'))
    return hashlib.sha256(json.dumps(tokens, ensure_ascii=False).encode('utf-8')).hexdigest()


def verify_algorithms(root):
    """与冻结的旧算法逐 token 比较；允许本次中文注释改变文件字节。"""
    manifest = json.loads((root/'tests/fixtures/legacy/sha256.json').read_text())
    count = 0
    for old_path, digest in manifest.items():
        if not old_path.replace('\\', '/').startswith('User/boat/'):
            continue
        name = Path(old_path.replace('\\', '/')).name
        original = root/'tests/fixtures/legacy'/name
        current = root/'App/Control/Boat'/name
        assert hashlib.sha256(original.read_bytes()).hexdigest() == digest, original
        assert c_tokens(current.read_text(encoding='utf-8-sig')) == c_tokens(original.read_text(encoding='utf-8-sig')), current
        count += 1
    assert count == 19, 'missing original algorithm files'
    return count
