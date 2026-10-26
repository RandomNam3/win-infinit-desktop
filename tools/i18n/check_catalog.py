#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compares the texts of the apps with the German catalog po/de/win-infinite-desktop.po.

Finds every i18n(), i18nc(), i18np(), i18ncp(), kli18n() and kli18nc() call in src/ (C++
and QML) and every label and explanation of the settings schema, and reports

  missing   texts without a translation (exit code 1 with --strict)
  unused    catalog entries no text uses any more

Usage: tools/i18n/check_catalog.py [--strict] [--unused] [--root <repository>]
"""
import argparse
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

LITERAL = r'"((?:[^"\\]|\\.)*)"'
CALL = re.compile(r'\b(kli18nc?|i18nc?p?)\(\s*((?:' + LITERAL + r'\s*)+)(?:,\s*((?:' + LITERAL + r'\s*)+))?(?:,\s*((?:' + LITERAL + r'\s*)+))?', re.S)


def unquote(text):
    return bytes(text, 'utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')


def join_literals(group):
    if group is None:
        return None
    return ''.join(unquote(part) for part in re.findall(LITERAL, group))


def parse_po(path):
    """{(context, msgid): (msgstrs, has_plural)} of the non-fuzzy entries."""
    entries = {}
    current = {}
    key = None
    fuzzy = False

    def commit():
        nonlocal current, fuzzy
        if current.get('msgid'):
            strings = [value for name, value in sorted(current.items()) if name.startswith('msgstr')]
            if not fuzzy:
                entries[(current.get('msgctxt', ''), current['msgid'])] = (strings, 'msgid_plural' in current)
        current = {}
        fuzzy = False

    for raw in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith('#'):
            if line.startswith('#,') and 'fuzzy' in line:
                if key and key.startswith('msgstr'):
                    commit()
                    key = None
                fuzzy = True
            continue
        if line.startswith('"'):
            current[key] = current.get(key, '') + unquote(line[1:-1])
            continue
        name, _, value = line.partition(' ')
        if name in ('msgctxt', 'msgid') and key and key.startswith('msgstr'):
            commit()
        key = name
        current[key] = unquote(value.strip()[1:-1])
    commit()
    return entries


def source_texts(root):
    found = {}
    files = list(root.glob('src/**/*.cpp')) + list(root.glob('src/**/*.h')) + list(root.glob('src/**/*.qml'))
    for path in files:
        text = path.read_text(encoding='utf-8')
        for match in CALL.finditer(text):
            function = match.group(1)
            args = [join_literals(match.group(i)) for i in (2, 4, 6)]
            base = function.replace('kli18n', '').replace('i18n', '')
            if 'c' in base:
                context, message = args[0], args[1]
            else:
                context, message = '', args[0]
            if message is None:
                continue
            found.setdefault((context, message), str(path.relative_to(root)))
    for schema in root.glob('src/**/*.kcfg'):
        for element in ET.parse(schema).getroot().iter():
            if element.tag.endswith('label') or element.tag.endswith('whatsthis'):
                message = ' '.join((element.text or '').split())
                if message:
                    found.setdefault(('', message), str(schema.relative_to(root)))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--root', default=pathlib.Path(__file__).resolve().parents[2], type=pathlib.Path)
    parser.add_argument('--strict', action='store_true', help='exit with 1 if a text has no translation')
    parser.add_argument('--unused', action='store_true', help='also list catalog entries nothing uses')
    options = parser.parse_args()

    catalog = parse_po(options.root / 'po/de/win-infinite-desktop.po')
    texts = source_texts(options.root)
    missing = [(key, where) for key, where in sorted(texts.items(), key=lambda item: item[1]) if key not in catalog or not any(catalog[key][0])]
    for (context, message), where in missing:
        print(f'missing\t{where}\t{context!r}\t{message!r}')
    if options.unused:
        for context, message in sorted(set(catalog) - set(texts)):
            print(f'unused\t{context!r}\t{message!r}')
    print(f'{len(texts)} texts, {len(catalog)} catalog entries, {len(missing)} missing', file=sys.stderr)
    return 1 if options.strict and missing else 0


if __name__ == '__main__':
    sys.exit(main())
