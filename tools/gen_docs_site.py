#!/usr/bin/env python3
"""Render docs/**/*.md into a static GitHub Pages site (stdlib only).

Usage:
    python tools/gen_docs_site.py            # write site/
    python tools/gen_docs_site.py --check    # write site/ and fail on broken internal links

The generated site is deliberately kept out of git (site/ is gitignored);
.github/workflows/docs.yml regenerates it before uploading the Pages artifact.
"""
from __future__ import annotations

import argparse
import difflib
import html
import posixpath
import re
import shutil
import sys
from collections import defaultdict
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC = ROOT / "docs"
DEFAULT_OUT = ROOT / "site"
ASSET_DIR = Path(__file__).resolve().parent / "docs_site_assets"

DIR_TITLES = {
    "guide": "使用指南",
    "charts": "图族参考",
    "graph": "图可视化",
    "design": "设计决策",
    "reference": "速查手册",
    "gallery": "示例画廊",
    "images": "图片资源",
}
HOME_ORDER = ["index", "getting_started", "installation", "architecture", "gallery", "limits"]

TABLE_SEP = re.compile(r"^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?\s*$")
LIST_ITEM = re.compile(r"^(\s*)([-*+]|\d+\.)\s+(.*)$")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
FENCE = re.compile(r"^\s*(`{3,}|~{3,})\s*([^\s`]*)\s*$")
HTML_BLOCK = re.compile(r"^\s*<(div|table|pre|details|summary|p|ul|ol|section|figure|img|br|hr|iframe)\b", re.I)


def slugify(text: str) -> str:
    text = re.sub(r"^#+\s*", "", text.strip()).lower()
    text = re.sub(r"[`*_~\[\]()]", "", text)
    text = re.sub(r"[\s/]+", "-", text)
    text = re.sub(r"[^\w\-]", "", text, flags=re.UNICODE)
    text = re.sub(r"-{2,}", "-", text).strip("-")
    return text or "section"


def display_title(stem: str, text: str) -> str:
    m = HEADING.match(text.strip().splitlines()[0] if text.strip() else "")
    if m:
        return inline_plain(m.group(2))
    return stem.replace("_", " ").strip() or "Untitled"


def inline_plain(text: str) -> str:
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"\1", text)
    text = re.sub(r"\*([^*]+)\*", r"\1", text)
    return re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text).strip()


class Renderer:
    """Renders one Markdown page, recording anchors and links for validation."""

    def __init__(self, page: str, checker: "LinkChecker", aliases: set[str]):
        self.page = page
        self.checker = checker
        self.aliases = aliases
        self.ids: list[str] = []
        self.toc: list[tuple[int, str, str]] = []
        self.fragments: set[str] = set()

    def rewrite_url(self, url: str) -> str:
        url = url.strip()
        if url.startswith("<") and url.endswith(">"):
            url = url[1:-1]
        parts = urlsplit(url)
        if parts.scheme in ("http", "https", "mailto", "data") or url.startswith("#"):
            if parts.fragment:
                self.fragments.add(parts.fragment)
                self.checker.add(self.page, self.page, parts.fragment)
            return url
        path = parts.path
        if not path:
            return url
        if path.lower().endswith(".md"):
            path = path[:-3] + ".html"
        elif path.endswith("/"):
            path += "index.html"
        elif "." not in posixpath.basename(path):
            path += "/index.html"
        target = posixpath.normpath(posixpath.join(posixpath.dirname(self.page), path))
        self.checker.add(self.page, target, parts.fragment)
        return urlunsplit(("", "", path, parts.query, parts.fragment))

    def inline(self, text: str) -> str:
        stashed: list[str] = []

        def stash(fragment: str) -> str:
            token = f"\x01{len(stashed)}\x02"
            stashed.append(fragment)
            return token

        def code_sub(m: re.Match[str]) -> str:
            return stash('<code class="inline-code">' + html.escape(m.group(1)) + "</code>")

        def image_sub(m: re.Match[str]) -> str:
            src = html.escape(self.rewrite_url(m.group(2)), quote=True)
            alt = html.escape(m.group(1), quote=True)
            return stash(f'<img class="doc-image" src="{src}" alt="{alt}" loading="lazy">')

        def link_sub(m: re.Match[str]) -> str:
            label = self.inline(m.group(1))
            href = html.escape(self.rewrite_url(m.group(2)), quote=True)
            return stash(f'<a href="{href}">{label}</a>')

        text = re.sub(r"`([^`\n]+)`", code_sub, text)
        text = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)", image_sub, text)
        text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)", link_sub, text)
        text = html.escape(text, quote=False)
        text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<!\*)\*([^*\n]+)\*(?!\*)", r"<em>\1</em>", text)
        text = re.sub(r"~~([^~]+)~~", r"<del>\1</del>", text)
        for i, fragment in enumerate(stashed):
            text = text.replace(f"\x01{i}\x02", fragment)
        return text

    def _heading(self, level: int, text: str) -> str:
        anchor = slugify(text)
        seen = self.ids.count(anchor)
        if seen:
            anchor = f"{anchor}-{seen}"
        self.ids.append(anchor)
        alias_html = ""
        for alias in sorted(self.aliases):
            if alias == anchor or alias in self.ids:
                continue
            if alias == slugify(inline_plain(text)) or difflib.SequenceMatcher(
                None, alias, anchor
            ).ratio() > 0.92:
                alias_html += f'<span class="anchor-alias" id="{html.escape(alias, quote=True)}"></span>'
                self.ids.append(alias)
        if level in (2, 3):
            self.toc.append((level, anchor, inline_plain(text)))
        return f'<h{level} id="{html.escape(anchor, quote=True)}">{alias_html}{self.inline(text)}</h{level}>'

    def _table(self, lines: list[str], start: int) -> tuple[str, int]:
        def cells(line: str) -> list[str]:
            line = line.strip().strip("|")
            line = line.replace(r"\|", "\x03")
            return [c.strip().replace("\x03", "|") for c in line.split("|")]

        head = cells(lines[start])
        aligns: list[str] = []
        for spec in cells(lines[start + 1]):
            aligns.append("center" if spec.startswith(":") and spec.endswith(":") else
                           "right" if spec.endswith(":") else
                           "left" if spec.startswith(":") else "")
        out = ['<div class="table-scroll"><table>', "<thead><tr>"]
        for i, cell in enumerate(head):
            align = f' style="text-align:{aligns[i]}"' if i < len(aligns) and aligns[i] else ""
            out.append(f"<th{align}>{self.inline(cell)}</th>")
        out.append("</tr></thead><tbody>")
        i = start + 2
        while i < len(lines) and lines[i].strip().startswith("|"):
            row = cells(lines[i])
            out.append("<tr>")
            for c, cell in enumerate(row):
                align = f' style="text-align:{aligns[c]}"' if c < len(aligns) and aligns[c] else ""
                out.append(f"<td{align}>{self.inline(cell)}</td>")
            out.append("</tr>")
            i += 1
        out.append("</tbody></table></div>")
        return "".join(out), i

    def _list(self, lines: list[str], start: int) -> tuple[str, int]:
        out: list[str] = []
        stack: list[tuple[int, str]] = []
        i = start
        while i < len(lines):
            m = LIST_ITEM.match(lines[i])
            if not m:
                break
            indent = len(m.group(1).expandtabs(4))
            ordered = m.group(2).endswith(".")
            kind = "ol" if ordered else "ul"
            while stack and indent < stack[-1][0]:
                out.append(f"</{stack.pop()[1]}>")
            if not stack or indent > stack[-1][0] or kind != stack[-1][1]:
                if stack:
                    out.append(f"</{stack.pop()[1]}>")
                css = ' class="task-list"' if not ordered else ""
                out.append(f"<{kind}{css}>")
                stack.append((indent, kind))
            content = m.group(3)
            task = re.match(r"^\[([ xX])\]\s*(.*)$", content)
            if task:
                checked = " checked" if task.group(1).lower() == "x" else ""
                content = f'<input type="checkbox" disabled{checked}> {self.inline(task.group(2))}'
            else:
                content = self.inline(content)
            out.append(f"<li>{content}</li>")
            i += 1
        while stack:
            out.append(f"</{stack.pop()[1]}>")
        return "".join(out), i

    def render(self, text: str) -> str:
        lines = text.replace("\r\n", "\n").split("\n")
        out: list[str] = []
        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()
            if not stripped:
                i += 1
                continue
            fence = FENCE.match(line)
            if fence:
                marker, lang = fence.group(1)[0], fence.group(2)
                i += 1
                code: list[str] = []
                while i < len(lines) and not re.match(rf"^\s*{re.escape(marker)}{{3,}}\s*$", lines[i]):
                    code.append(lines[i])
                    i += 1
                i += 1
                label = html.escape(lang or "text", quote=True)
                out.append(
                    '<div class="code-block"><div class="code-head">'
                    f'<span class="code-lang">{label}</span>'
                    '<button class="copy-btn" type="button" aria-label="复制代码">复制</button>'
                    '</div><pre><code>'
                    + html.escape("\n".join(code))
                    + "</code></pre></div>"
                )
                continue
            if i + 1 < len(lines) and stripped.startswith("|") and TABLE_SEP.match(lines[i + 1]):
                block, i = self._table(lines, i)
                out.append(block)
                continue
            heading = HEADING.match(stripped)
            if heading:
                out.append(self._heading(len(heading.group(1)), heading.group(2)))
                i += 1
                continue
            if re.match(r"^\s*(?:---|\*\*\*|___)\s*$", line):
                out.append("<hr>")
                i += 1
                continue
            if HTML_BLOCK.match(line):
                out.append(line)
                i += 1
                continue
            if stripped.startswith(">"):
                quote: list[str] = []
                while i < len(lines) and lines[i].strip().startswith(">"):
                    quote.append(re.sub(r"^\s*>\s?", "", lines[i]))
                    i += 1
                out.append(f"<blockquote>{self.render(chr(10).join(quote))}</blockquote>")
                continue
            if LIST_ITEM.match(line):
                block, i = self._list(lines, i)
                out.append(block)
                continue
            para: list[str] = []
            while i < len(lines):
                s = lines[i].strip()
                if (not s or FENCE.match(lines[i]) or HEADING.match(s) or s.startswith(">")
                        or LIST_ITEM.match(lines[i]) or HTML_BLOCK.match(lines[i])
                        or re.match(r"^\s*(?:---|\*\*\*|___)\s*$", lines[i])
                        or (i + 1 < len(lines) and s.startswith("|") and TABLE_SEP.match(lines[i + 1]))):
                    break
                para.append(s)
                i += 1
            if para:
                out.append("<p>" + "<br>".join(self.inline(s) for s in para) + "</p>")
        return "\n".join(out)


class LinkChecker:
    def __init__(self) -> None:
        self.links: list[tuple[str, str, str]] = []
        self.pages: dict[str, set[str]] = {}
        self.files: set[str] = set()

    def add(self, source: str, target: str, fragment: str) -> None:
        self.links.append((source, target, fragment))

    def broken(self) -> list[str]:
        problems: list[str] = []
        for source, target, fragment in self.links:
            if target not in self.pages and target not in self.files:
                problems.append(f"{source}: missing page {target}")
            elif fragment and target in self.pages and fragment not in self.pages[target]:
                problems.append(f"{source}: missing anchor #{fragment} in {target}")
        return problems


def relative_prefix(page: str) -> str:
    depth = posixpath.dirname(page).count("/") + (1 if posixpath.dirname(page) else 0)
    return "../" * depth


def nav_html(pages: dict[str, str], current: str, prefix: str) -> str:
    groups: dict[str, list[str]] = defaultdict(list)
    for page in pages:
        folder = posixpath.dirname(page)
        groups[folder].append(page)

    def order(folder: str) -> tuple[int, str]:
        return (0, "") if not folder else (1, folder)

    parts = ['<nav class="side-nav" aria-label="文档导航">',
             '<label class="nav-search"><span>筛选文档</span><input id="nav-filter" type="search" placeholder="输入关键词…" autocomplete="off"></label>']
    for folder in sorted(groups, key=order):
        items = groups[folder]
        if not folder:
            items.sort(key=lambda p: (HOME_ORDER.index(posixpath.basename(p)[:-5])
                                      if posixpath.basename(p)[:-5] in HOME_ORDER else 99, p))
            label = "开始使用"
        else:
            items.sort()
            label = DIR_TITLES.get(folder, folder.replace("_", " ").title())
        parts.append(f'<details class="nav-group" open><summary>{html.escape(label)}<span class="nav-count">{len(items)}</span></summary>')
        parts.append('<ul>')
        for page in items:
            href = posixpath.relpath(page, posixpath.dirname(current) or ".")
            active = ' class="active" aria-current="page"' if page == current else ""
            parts.append(f'<li><a href="{html.escape(href, quote=True)}"{active} data-nav>{html.escape(pages[page])}</a></li>')
        parts.append("</ul></details>")
    parts.append("</nav>")
    return "".join(parts)


def page_html(title: str, body: str, toc: list[tuple[int, str, str]], page: str,
              pages: dict[str, str], ordered: list[str], base: str = "") -> str:
    prefix = relative_prefix(page)
    base_tag = f'<base href="{html.escape(base, quote=True)}">' if base else ""
    toc_html = ""
    mobile_toc_html = ""
    if toc:
        toc_items = "".join(
            f'<li class="toc-{level}"><a href="#{html.escape(anchor, quote=True)}">{html.escape(text)}</a></li>'
            for level, anchor, text in toc
        )
        toc_html = f'<aside class="toc"><div class="toc-title">本页目录</div><ul>{toc_items}</ul></aside>'
        mobile_toc_html = ('<details class="mobile-toc"><summary>本页目录 · 快速跳转</summary>'
                           f'<ul>{toc_items}</ul></details>')
    index = ordered.index(page) if page in ordered else -1
    pager = []
    if index > 0:
        prev = ordered[index - 1]
        pager.append(f'<a class="pager-link prev" href="{html.escape(posixpath.relpath(prev, posixpath.dirname(page) or "."), quote=True)}">← {html.escape(pages[prev])}</a>')
    if 0 <= index < len(ordered) - 1:
        nxt = ordered[index + 1]
        pager.append(f'<a class="pager-link next" href="{html.escape(posixpath.relpath(nxt, posixpath.dirname(page) or "."), quote=True)}">{html.escape(pages[nxt])} →</a>')
    pager_html = f'<nav class="pager">{"".join(pager)}</nav>' if pager else ""
    return f'''<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="termchart 文档：{html.escape(title, quote=True)}">
<title>{html.escape(title)} · termchart 文档</title>
{base_tag}
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%230d1117'/%3E%3Cpath d='M5 24 L12 13 L18 19 L27 7' stroke='%234ee5a6' stroke-width='3' fill='none'/%3E%3C/svg%3E">
<link rel="stylesheet" href="{prefix}assets/style.css">
</head>
<body>
<header class="topbar">
  <button id="nav-toggle" type="button" class="icon-btn nav-toggle" aria-label="打开文档导航" aria-expanded="false" aria-controls="sidebar">☰</button>
  <a class="brand" href="{prefix}index.html"><span class="brand-mark">▤</span> termchart <span class="brand-sub">文档</span></a>
  <div class="top-actions">
    <a href="https://github.com/Jackey7980/termchart" rel="noopener">GitHub ↗</a>
    <button id="theme-toggle" type="button" class="icon-btn" aria-label="切换主题">☾</button>
  </div>
</header>
<div class="nav-scrim" id="nav-scrim"></div>
<div class="layout">
  <div class="sidebar" id="sidebar">{nav_html(pages, page, prefix)}</div>
  <main class="content">
    {mobile_toc_html}
    <article class="markdown-body">{body}</article>
    {pager_html}
    <footer class="footer">由 <code>tools/gen_docs_site.py</code> 从 docs/ 生成 · <a href="https://github.com/Jackey7980/termchart">termchart</a></footer>
  </main>
  {toc_html}
</div>
<script src="{prefix}assets/site.js" defer></script>
</body>
</html>
'''


def collect_fragments(md_files: dict[str, str]) -> dict[str, set[str]]:
    """Fragments pointing at a page are used as anchor aliases during rendering."""
    result: dict[str, set[str]] = defaultdict(set)
    for source, text in md_files.items():
        for m in re.finditer(r"\]\(([^)]+)\)", text):
            parts = urlsplit(m.group(1).strip().strip("<>"))
            if not parts.fragment:
                continue
            if not parts.path or parts.path.endswith(".md"):
                target = posixpath.normpath(posixpath.join(
                    posixpath.dirname(source),
                    parts.path if parts.path else posixpath.basename(source),
                )).replace(".md", ".html")
                result[target].add(parts.fragment)
    return result


def build(src: Path, out: Path) -> list[str]:
    if not src.is_dir():
        raise SystemExit(f"docs source not found: {src}")
    md_files: dict[str, str] = {}
    for path in src.rglob("*.md"):
        rel = path.relative_to(src).as_posix()
        md_files[rel] = path.read_text(encoding="utf-8")

    checker = LinkChecker()
    aliases = collect_fragments(md_files)
    rendered: dict[str, str] = {}
    pages: dict[str, str] = {}
    toc_by_page: dict[str, list[tuple[int, str, str]]] = {}
    for rel, text in md_files.items():
        page = rel[:-3] + ".html"
        renderer = Renderer(page, checker, aliases.get(page, set()))
        body = renderer.render(text)
        rendered[page] = body
        pages[page] = display_title(posixpath.basename(rel)[:-3], text)
        toc_by_page[page] = renderer.toc
        checker.pages[page] = set(renderer.ids)

    ordered = sorted(pages, key=lambda p: (
        0 if posixpath.dirname(p) == "" else 1,
        posixpath.dirname(p),
        HOME_ORDER.index(posixpath.basename(p)[:-5]) if (posixpath.dirname(p) == ""
            and posixpath.basename(p)[:-5] in HOME_ORDER) else 99,
        p,
    ))

    if out.exists():
        shutil.rmtree(out)
    (out / "assets").mkdir(parents=True)
    for asset in ASSET_DIR.iterdir():
        if asset.is_file():
            shutil.copy2(asset, out / "assets" / asset.name)

    for page, body in rendered.items():
        target = out / page
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(page_html(pages[page], body, toc_by_page[page], page, pages, ordered), encoding="utf-8")

    # Directory landing pages (e.g. /guide/) and a branded 404 page.
    for folder in sorted({posixpath.dirname(p) for p in pages if posixpath.dirname(p)}):
        children = sorted(p for p in pages if posixpath.dirname(p) == folder)
        rows = "".join(
            f'<li><a href="{html.escape(posixpath.basename(p), quote=True)}">{html.escape(pages[p])}</a></li>'
            for p in children
        )
        body = (f"<h1>{html.escape(DIR_TITLES.get(folder, folder))}</h1>"
                f'<p class="dir-intro">共 {len(children)} 篇文档，按文件名排列。</p><ul class="dir-list">{rows}</ul>')
        target = out / folder / "index.html"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(page_html(DIR_TITLES.get(folder, folder), body, [], f"{folder}/index.html", pages, ordered), encoding="utf-8")
        checker.pages[f"{folder}/index.html"] = set()

    not_found = page_html("页面未找到",
        '<h1>404 · 页面未找到</h1><p>这个地址没有对应的文档。可以回到<a href="index.html">文档首页</a>，或在左侧导航中搜索。</p>',
        [], "404.html", pages, ordered, base="https://jackey7980.github.io/termchart/")
    (out / "404.html").write_text(not_found, encoding="utf-8")
    checker.pages["404.html"] = set()

    for path in src.rglob("*"):
        if path.is_file() and path.suffix.lower() != ".md" and not path.name.startswith("."):
            rel = path.relative_to(src)
            target = out / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            checker.files.add(rel.as_posix())

    sitemap = "".join(
        f"<url><loc>https://jackey7980.github.io/termchart/{p}</loc></url>" for p in sorted(pages)
    )
    (out / "sitemap.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'
        + sitemap + "</urlset>\n", encoding="utf-8")
    return checker.broken()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC, help="Markdown 源目录")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="站点输出目录")
    parser.add_argument("--check", action="store_true", help="校验内部链接，发现断链时返回非零")
    args = parser.parse_args()
    broken = build(args.src, args.out)
    print(f"generated {args.out} ({len(list(args.out.rglob('*.html')))} html pages)")
    if broken:
        print(f"{len(broken)} broken internal link(s):", file=sys.stderr)
        for problem in broken[:200]:
            print("  " + problem, file=sys.stderr)
        if args.check:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
