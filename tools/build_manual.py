"""Build docs/MANUAL.html from docs/MANUAL.md (the one master): the companion's colours, and every picture embedded in
the file, so the manual is a single page that opens in any browser, offline, from the download.

    python -I tools/build_manual.py        (needs the `markdown` package)
"""
import base64, os, re
import markdown

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DOCS = os.path.join(ROOT, "docs")

CSS = """
:root { --bg: #0f1419; --panel: #1a1f26; --alt: #242b33; --line: #2e3440; --olive: #4a5a42; --cream: #e5e9f0;
        --ink: #d8dee9; --mute: #8b93a1; --blue: #5b8def; }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--ink); font: 16px/1.6 'Segoe UI', system-ui, sans-serif; }
main { max-width: 900px; margin: 0 auto; padding: 24px 20px 64px; }
h1 { color: var(--cream); font-size: 28px; margin: 8px 0 16px; display: flex; align-items: center; gap: 12px; }
h1 img { width: 40px; height: 40px; }
h2 { color: var(--cream); font-size: 20px; margin: 36px 0 10px; padding-bottom: 6px; border-bottom: 1px solid var(--line); }
a { color: var(--blue); }
code { background: var(--alt); padding: 1px 6px; border-radius: 4px; font-size: 14px; }
table { border-collapse: collapse; width: 100%; margin: 12px 0; background: var(--panel); }
th, td { border: 1px solid var(--line); padding: 8px 10px; text-align: left; vertical-align: top; }
th { background: var(--alt); color: var(--cream); }
img.shot { display: block; max-width: 100%; margin: 16px auto; border: 1px solid var(--line); border-radius: 6px; }
li { margin: 4px 0; }
strong { color: var(--cream); }
"""


def embed(match):
    alt, src = match.group(1), match.group(2)
    path = os.path.join(DOCS, src)
    data = base64.b64encode(open(path, "rb").read()).decode()
    return f'<img class="shot" alt="{alt}" src="data:image/png;base64,{data}">'


def main():
    md = open(os.path.join(DOCS, "MANUAL.md"), encoding="utf-8").read()
    html = markdown.markdown(md, extensions=["tables"])
    html = re.sub(r'<img alt="([^"]*)" src="([^"]+)"\s*/?>', embed, html)
    icon = base64.b64encode(open(os.path.join(ROOT, "assets", "app_icon.png"), "rb").read()).decode()
    html = html.replace("<h1>", f'<h1><img alt="" src="data:image/png;base64,{icon}">', 1)
    title = re.search(r"^# (.+)$", md, re.M).group(1)
    page = (f'<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">\n<title>{title}</title>\n'
            f'<style>{CSS}</style>\n</head>\n<body>\n<main>\n{html}\n</main>\n</body>\n</html>\n')
    out = os.path.join(DOCS, "MANUAL.html")
    open(out, "w", encoding="utf-8", newline="\n").write(page)
    print(f"{out}: {len(page) // 1024} KB")


if __name__ == "__main__":
    main()
