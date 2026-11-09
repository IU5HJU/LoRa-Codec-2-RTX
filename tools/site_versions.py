#!/usr/bin/env python3
"""Elenco delle versioni installabili dalla pagina web (versions.json).

La pagina di installazione mostra un menu a tendina con la versione del firmware da installare: le versioni rilasciate
(stabili, candidate al rilascio, beta) e, se c'e', quella di sviluppo (ultima di main). Questo programma costruisce
l'elenco; lo usa tools/make_site.sh. Le fasi si leggono dal nome del tag come in docs/versioni.md:

    v0.2.0          stabile
    v0.2.0-rc.1     candidata al rilascio
    v0.1.0-beta.1   beta
    main-GG/MM/AAAA sviluppo (non e' un tag)

Ordine nel menu: stabili dalla piu' recente, poi rc, poi beta, poi lo sviluppo. La versione proposta e' la prima versione
rilasciata (stabile se c'e', altrimenti la rc o la beta piu' recente); senza versioni rilasciate, lo sviluppo.

    site_versions.py --current-id ID --current-version V --current-commit C [--releases-dir DIR] [--index FILE] [--out FILE]
"""
import argparse
import json
import os
import re
import sys

PHASE_ORDER = {"stable": 0, "rc": 1, "beta": 2}
TAG_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-(rc|beta)\.(\d+))?$")


def parse_tag(tag):
    """(maggiore, minore, correzione, fase, numero di fase) oppure None se non e' un tag di versione."""
    m = TAG_RE.match(tag or "")
    if not m:
        return None
    major, minor, patch, phase, num = m.groups()
    return int(major), int(minor), int(patch), phase or "stable", int(num or 0)


def kind_of(version):
    """'stable', 'rc', 'beta', 'dev' (main-GG/MM/AAAA) oppure None."""
    if (version or "").startswith("main-") or version == "dev":
        return "dev"
    p = parse_tag(version)
    return p[3] if p else None


def _sort_key(entry):
    """Stabili, poi rc, poi beta; dentro ogni fase dalla piu' recente. Lo sviluppo e' sempre ultimo."""
    if entry["kind"] == "dev":
        return (1, 0, 0, 0, 0, 0)
    major, minor, patch, phase, num = parse_tag(entry["version"])
    return (0, PHASE_ORDER[phase], -major, -minor, -patch, -num)


def build(current_id, current_version, current_commit, releases):
    """releases: elenco di dict con 'tag' e, se noto, 'date' (AAAA-MM-GG). Il build corrente non si ripete tra le release."""
    entries = []
    kind = kind_of(current_version)
    if kind:
        entries.append({"id": current_id, "version": current_version, "kind": kind, "commit": current_commit, "date": None})
    for r in releases:
        tag = r.get("tag")
        if not tag or tag == current_id or parse_tag(tag) is None:
            continue
        entries.append({"id": tag, "version": tag, "kind": kind_of(tag), "commit": None, "date": r.get("date")})
    entries.sort(key=_sort_key)
    released = [e for e in entries if e["kind"] != "dev"]
    default = (released[0] if released else (entries[0] if entries else {"id": None}))["id"]
    return {"default": default, "versions": entries}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--current-id", required=True, help="cartella del build corrente: 'dev' oppure il tag")
    ap.add_argument("--current-version", required=True, help="versione scritta nel build corrente (tag o main-GG/MM/AAAA)")
    ap.add_argument("--current-commit", required=True)
    ap.add_argument("--releases-dir", help="cartella con una sottocartella per ogni tag scaricato (solo quelle con i binari)")
    ap.add_argument("--index", help="releases/index.json di 'gh release list --json tagName,publishedAt'")
    ap.add_argument("--out", help="file di uscita (predefinito: standard output)")
    a = ap.parse_args()

    dates = {}
    if a.index and os.path.exists(a.index):
        for r in json.load(open(a.index, encoding="utf-8")):
            dates[r.get("tagName")] = (r.get("publishedAt") or "")[:10] or None
    releases = []
    if a.releases_dir and os.path.isdir(a.releases_dir):
        for tag in sorted(os.listdir(a.releases_dir)):
            d = os.path.join(a.releases_dir, tag)
            if os.path.isdir(d) and any(f.endswith("-web_factory.bin") for f in os.listdir(d)):
                releases.append({"tag": tag, "date": dates.get(tag)})
    out = build(a.current_id, a.current_version, a.current_commit, releases)
    text = json.dumps(out, indent=2, ensure_ascii=False) + "\n"
    if a.out:
        open(a.out, "w", encoding="utf-8").write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
