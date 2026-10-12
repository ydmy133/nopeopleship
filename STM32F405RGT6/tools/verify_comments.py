"""核对补注释前后全部纳入检查的自研 C 文件；基准清单不自动重写。"""
import json
from pathlib import Path
from source_tokens import token_digest, verify_algorithms

root = Path(__file__).resolve().parents[1]
baseline = json.loads((root/'docs/validation/comment_baseline.json').read_text(encoding='utf-8'))
for relative, expected in baseline['token_sha256'].items():
    assert token_digest(root/relative) == expected, relative+' executable tokens changed'
verify_algorithms(root)
print(f"PASS: {len(baseline['token_sha256'])} C/header files match pre-comment tokens; 19 algorithms match frozen originals")
