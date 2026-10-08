"""Look up JLCPCB/LCSC parts: library type (Basic/Extended/Preferred), MPN, package, stock, price.

usage: python tools/jlc_lookup.py C15850 C8678 ...           exact LCSC codes
       python tools/jlc_lookup.py -s "SMAJ28A" [-n 10]        keyword search (Basic first)
"""
import json
import sys
import urllib.request

API = "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList"


def query(keyword: str, size: int = 10) -> list:
    body = json.dumps({"keyword": keyword, "currentPage": 1, "pageSize": size}).encode()
    req = urllib.request.Request(API, data=body, headers={"Content-Type": "application/json", "User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        data = json.load(r)
    return (data.get("data") or {}).get("componentPageInfo", {}).get("list") or []


def kind(c: dict) -> str:
    if c.get("componentLibraryType") == "base":
        return "BASIC"
    if c.get("preferredComponentFlag"):
        return "PREFERRED"
    return "extended"


def show(c: dict) -> str:
    price = (c.get("componentPrices") or [{}])[0].get("productPrice")
    return (f"{c.get('componentCode'):>9} {kind(c):9} stock={c.get('stockCount'):>8} ${price} "
            f"{c.get('componentModelEn')} | {c.get('componentSpecificationEn')} | {c.get('erpComponentName')} | {c.get('componentTypeEn')}")


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "-s":
        n = int(args[3]) if len(args) > 3 and args[2] == "-n" else 10
        res = query(args[1], n)
        res.sort(key=lambda c: (kind(c) != "BASIC", kind(c) != "PREFERRED", -(c.get("stockCount") or 0)))
        for c in res:
            print(show(c))
    else:
        for code in args:
            hit = [c for c in query(code, 5) if c.get("componentCode") == code]
            print(show(hit[0]) if hit else f"{code:>9} NOT FOUND")
