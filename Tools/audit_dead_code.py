"""找出 Baka！项目里"没有任何引用"的 C++ 符号（函数 / 成员），并同时检查蓝图资产是否引用。

用法:
    python audit_dead_code.py            # 全量扫描
    python audit_dead_code.py --assets   # 额外扫描 Content 下的 .uasset/.umap（较慢，约 1 分钟）

判定口径:
    · 源码引用次数只算"声明/定义"以外的地方；引用数 <= 阈值即视为可疑。
    · 蓝图可调用的符号（UFUNCTION/UPROPERTY）必须再过一遍资产扫描，
      因为蓝图引用不会出现在 C++ 源码里。
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from collections import defaultdict

SRC = r"F:\AI_WorkSpeace\baka\Source\ai"
CONTENT = r"F:\AI_WorkSpeace\baka\Content"

# 声明行：返回类型 + 名字 + 参数列表 + ';'
DECL_RE = re.compile(
    r"""^\s*
        (?:template\s*<[^>]*>\s*)?
        (?:virtual\s+|static\s+|explicit\s+|inline\s+|FORCEINLINE\s+|friend\s+)*
        (?:const\s+)?
        [A-Za-z_][\w:<>,\s\*&]*?          # 返回类型
        \b(\w+)\s*                        # 名字
        \(                                # 参数开始
    """,
    re.VERBOSE,
)

KEYWORDS = {
    "if", "for", "while", "switch", "return", "else", "sizeof", "new", "delete",
    "UPROPERTY", "UFUNCTION", "GENERATED_BODY", "GENERATED_UCLASS_BODY", "public",
    "private", "protected", "class", "struct", "enum", "namespace", "using",
    "typedef", "static_assert", "check", "ensure", "cast", "DECLARE", "template",
}

# 引擎/框架里那些"由引擎调用"的名字：即使源码里没引用也不能删
ENGINE_CALLED = re.compile(
    r"^(BeginPlay|EndPlay|Tick|PostInitProperties|PostInitializeComponents|"
    r"GetLifetimeReplicatedProps|ReplicateSubobjects|PossessedBy|UnPossessed|"
    r"SetupPlayerInputComponent|BeginDestroy|Serialize|OnConstruction|"
    r"PostLoad|PostNetInit|TakeDamage|OnRep_\w+|Receive\w*|"
    r"NativeInitializeAbility|OnAvatarSet|ActivateAbility|EndAbility|"
    r"ApplyGameplayEffect|Handle\w+|Process\w*Death|DeactivateToPool|"
    r"ActivatePooled\w*|ResetForPool|Initialize\w*|Configure\w*)$"
)


def read_sources():
    files = []
    for root, _dirs, names in os.walk(SRC):
        for n in names:
            if n.endswith((".h", ".cpp")):
                files.append(os.path.join(root, n))
    texts = {}
    for f in files:
        with open(f, "r", encoding="utf-8", errors="replace") as fh:
            texts[f] = fh.read()
    return files, texts


def collect_asset_names(asset_dir: str) -> dict[str, list[str]]:
    """返回 {符号名: [引用它的资产文件名]}；按需查找以省时间。"""
    assets = []
    for root, _dirs, names in os.walk(asset_dir):
        for n in names:
            if n.endswith((".uasset", ".umap")):
                assets.append(os.path.join(root, n))
    return assets


def asset_hit_map(assets, symbols) -> dict[str, list[str]]:
    """一趟扫完所有资产，返回 {符号: [资产名]}。

    为什么不做成"每个符号扫一遍"：Content 有 1.4 GB，符号有几十个，
    逐个扫等于把磁盘读几十遍（分钟级）。一趟读入、逐个符号 Contains 是秒级的。
    """
    wanted = [(s, s.encode("ascii", "ignore")) for s in symbols]
    hits: dict[str, list[str]] = {s: [] for s in symbols}
    for path in assets:
        try:
            with open(path, "rb") as fh:
                data = fh.read()
        except OSError:
            continue
        base = os.path.basename(path)
        for name, raw in wanted:
            if raw and raw in data:
                hits[name].append(base)
    return hits


def collect_members(texts):
    """UPROPERTY 成员变量：只看头文件里 UPROPERTY 宏后面几行内的声明。"""
    member_re = re.compile(
        r"^\s*(?:mutable\s+)?(?:TObjectPtr<[^>]+>|TArray<[^>]+>|TMap<[^>]+>|TSubclassOf<[^>]+>|"
        r"F\w+|U\w+|A\w+|E\w+|[A-Za-z_]\w*)\s*[*&]?\s*(\w+)\s*(?:=[^;]*)?;"
    )
    out = []
    for path, text in texts.items():
        if not path.endswith(".h"):
            continue
        lines = text.splitlines()
        for i, line in enumerate(lines):
            if "UPROPERTY" not in line:
                continue
            for j in range(i + 1, min(i + 8, len(lines))):
                cand = lines[j]
                if not cand.strip() or cand.strip().startswith("//"):
                    continue
                m = member_re.match(cand)
                if m:
                    out.append((m.group(1), os.path.basename(path), j + 1, cand.strip()))
                    break
                if "(" in cand and ")" in cand:
                    break
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", action="store_true", help="扫描 Content 资产里的引用")
    ap.add_argument("--max-refs", type=int, default=1, help="引用次数 <= 该值即报告")
    ap.add_argument("--members", action="store_true", help="同时检查 UPROPERTY 成员变量")
    args = ap.parse_args()

    files, texts = read_sources()
    joined = "\n".join(texts.values())

    # 统计每个标识符的全局出现次数
    counts: dict[str, int] = defaultdict(int)
    for token in re.finditer(r"\b[A-Za-z_]\w*\b", joined):
        counts[token.group(0)] += 1

    decls = []  # (symbol, file, line)
    for path, text in texts.items():
        if not path.endswith(".h"):
            continue
        for i, line in enumerate(text.splitlines(), start=1):
            if "(" not in line or line.strip().startswith("//"):
                continue
            m = DECL_RE.match(line)
            if not m:
                continue
            name = m.group(1)
            if name in KEYWORDS or ENGINE_CALLED.match(name):
                continue
            # 只看头文件里的"声明"：以 ; 结尾或下一行是 { 的都不算，避免把调用当声明
            if ";" not in line and not line.rstrip().endswith(")"):
                continue
            decls.append((name, os.path.basename(path), i, line.strip()))

    suspicious = []
    seen = set()
    for name, fname, line_no, line in decls:
        if (name, fname) in seen:
            continue
        seen.add((name, fname))
        if counts[name] <= args.max_refs:
            suspicious.append((name, fname, line_no, line))

    if args.members:
        for name, fname, line_no, line in collect_members(texts):
            if (name, fname) in seen:
                continue
            seen.add((name, fname))
            # 成员变量：变量名出现次数 <= 1 说明只有声明
            if counts[name] <= 1:
                suspicious.append((name, fname, line_no, line))

    print(f"头文件声明总数（去重）= {len(seen)}")
    print(f"源码引用 <= {args.max_refs} 的可疑符号 = {len(suspicious)}")
    print()

    if args.assets:
        assets = collect_asset_names(CONTENT)
        print(f"Content 资产数 = {len(assets)}，开始扫描蓝图引用……")
        hit_map = asset_hit_map(assets, [name for name, _f, _l, _line in suspicious])
        rows = []
        for name, fname, line_no, line in suspicious:
            hits = hit_map.get(name, [])
            rows.append((name, fname, line_no, hits))
            mark = "蓝图在用" if hits else "无人引用"
            print(f"  [{mark}] {name}  ({fname}:{line_no})  {hits[:3] if hits else ''}")
        dead = [r for r in rows if not r[3]]
        print()
        print(f"=== 源码 + 蓝图都无人引用：{len(dead)} 个 ===")
        for name, fname, line_no, _ in dead:
            print(f"  {name}  ({fname}:{line_no})")
    else:
        for name, fname, line_no, line in suspicious:
            print(f"  {name}  ({fname}:{line_no})   {line[:100]}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
