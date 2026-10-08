"""Search JLCPCB parts and keep only the default library (Basic, Preferred).

usage: python tools/jlc_find_default.py "keyword" [pages]
"""
import sys

from jlc_lookup import API, kind  # noqa: F401
import json
import urllib.request


def page(keyword: str, n: int, extra: dict) -> list:
    body = dict({"keyword": keyword, "currentPage": n, "pageSize": 100}, **extra)
    req = urllib.request.Request(API, data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json", "User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        d = json.load(r)
    return ((d.get("data") or {}).get("componentPageInfo") or {}).get("list") or []


if __name__ == "__main__":
    kw = sys.argv[1]
    pages = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    seen = {}
    for extra in ({"preferredComponentFlag": True}, {}):
        for n in range(1, (2 if extra else pages) + 1):
            for c in page(kw, n, extra):
                if kind(c) in ("BASIC", "PREFERRED"):
                    seen[c["componentCode"]] = c
    for c in sorted(seen.values(), key=lambda c: (kind(c) != "BASIC", -(c.get("stockCount") or 0))):
        price = (c.get("componentPrices") or [{}])[0].get("productPrice")
        print(f"{c['componentCode']:>9} {kind(c):9} stock={c.get('stockCount'):>8} ${price} {c.get('componentModelEn')} | "
              f"{c.get('componentSpecificationEn')} | {c.get('erpComponentName')}")
