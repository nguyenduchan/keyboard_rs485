// Negotiated-congestion (PathFinder) grid router: A* maze search on 2 copper layers,
// 45-degree moves, vias, per-net width classes, fixed escape stubs on IC pins.
// build: g++ -O2 -std=c++14 -o pathfinder.exe pathfinder.cpp
// usage: pathfinder problem.txt result.txt [iterations]
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
typedef int8_t i8;
static const i8 HARD = 100, CONF = 127;

// ------------------------------------------------------------------ problem / rules
double BW = 120, BH = 120, CLR = 0.2, EDGE = 0.5, VIAD = 0.6, VIADRILL = 0.3, RES = 0.05;
vector<double> CW;
int K;  // number of track classes; kind K = via
struct Net { string name; int cls; };
vector<Net> nets;

struct Elem { int type; double a, b, c, d, w; int lay; };  // 0 rect 1 circle 2 seg 3 via

struct Term {
    int net = 0, lay = 0, single = 0;
    bool stub = false, circ = false, ok = false, hasExact = false;
    double px = 0, py = 0, hx = 0, hy = 0, dx = 0, dy = 0, edge = 0, lmin = 0, lmax = 0, ex = 0, ey = 0;
    string key;
    vector<int> cells;
};
vector<Term> terms;
vector<vector<int>> netTerms;

int W, H;
size_t LS;
inline int idx(int l, int x, int y) { return (int)(l * LS + (size_t)y * W + x); }
inline int kindOf(int k, int l) { return k * 2 + l; }

vector<i8> fixm[8];       // [kind] static obstacles, owner net
vector<uint8_t> occ[8];   // [kind] number of routed nets whose copper blocks a centre here
vector<uint8_t> mark[8];
vector<uint8_t> noVia;
vector<i8> halo[2];
vector<float> hist;       // [layer*LS + cell]

static inline void put(i8& c, i8 v) {
    if (v == HARD) { c = HARD; return; }
    if (c == 0) c = v;
    else if (c == v || c == HARD) {}
    else c = CONF;
}
static inline double rectDist(double px, double py, double cx, double cy, double hx, double hy) {
    double dx = max(fabs(px - cx) - hx, 0.0), dy = max(fabs(py - cy) - hy, 0.0);
    return sqrt(dx * dx + dy * dy);
}
static inline double segDist(double px, double py, double x1, double y1, double x2, double y2) {
    double vx = x2 - x1, vy = y2 - y1, wx = px - x1, wy = py - y1;
    double l2 = vx * vx + vy * vy, t = l2 > 0 ? (wx * vx + wy * vy) / l2 : 0;
    t = max(0.0, min(1.0, t));
    double qx = x1 + t * vx - px, qy = y1 + t * vy - py;
    return sqrt(qx * qx + qy * qy);
}

// call f(kind, offset) for every cell where a track/via centre of that kind would violate clearance to e
template <class F>
void forStamp(const Elem& e, F f) {
    for (int l = 0; l < 2; l++) {
        if (!(e.lay & (1 << l))) continue;
        for (int k = 0; k <= K; k++) {
            double extra = CLR + (k < K ? CW[k] / 2 : VIAD / 2);
            double r, bx0, by0, bx1, by1;
            if (e.type == 0) { r = extra; bx0 = e.a - e.c; by0 = e.b - e.d; bx1 = e.a + e.c; by1 = e.b + e.d; }
            else if (e.type == 1 || e.type == 3) { r = extra + (e.type == 1 ? e.c : VIAD / 2); bx0 = bx1 = e.a; by0 = by1 = e.b; }
            else { r = extra + e.w / 2; bx0 = min(e.a, e.c); bx1 = max(e.a, e.c); by0 = min(e.b, e.d); by1 = max(e.b, e.d); }
            int x0 = max(0, (int)floor((bx0 - r) / RES)), x1 = min(W - 1, (int)ceil((bx1 + r) / RES));
            int y0 = max(0, (int)floor((by0 - r) / RES)), y1 = min(H - 1, (int)ceil((by1 + r) / RES));
            int kd = kindOf(k, l);
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    double X = x * RES, Y = y * RES, d;
                    if (e.type == 0) d = rectDist(X, Y, e.a, e.b, e.c, e.d);
                    else if (e.type == 2) d = segDist(X, Y, e.a, e.b, e.c, e.d);
                    else d = hypot(X - e.a, Y - e.b);
                    if (d <= r + 1e-9) f(kd, (size_t)y * W + x);
                }
        }
    }
}
void stampFixed(const Elem& e, i8 val) {
    forStamp(e, [&](int kd, size_t o) { put(fixm[kd][o], val); });
}

// ------------------------------------------------------------------ routed nets
struct Seg { int net, layer; double x1, y1, x2, y2, w; };
struct Via { double x, y; };
struct NetRoute {
    vector<Seg> segs;
    vector<Via> vias;
    vector<pair<int, int>> pathCells;   // (cell incl. layer, class)
    vector<int> viaCells;               // plane offset
    vector<int> stamp[8];               // cells (offset) per kind
    bool complete = false;
};
vector<NetRoute> R;
vector<Seg> stubSegs;

void unstampNet(int n) {
    for (int kd = 0; kd < 2 * (K + 1); kd++) {
        for (int o : R[n].stamp[kd]) occ[kd][o]--;
        R[n].stamp[kd].clear();
    }
}
void stampNet(int n) {
    auto add = [&](int kd, size_t o) {
        if (!mark[kd][o]) { mark[kd][o] = 1; R[n].stamp[kd].push_back((int)o); }
    };
    for (auto& s : R[n].segs) forStamp(Elem{2, s.x1, s.y1, s.x2, s.y2, s.w, 1 << s.layer}, add);
    for (auto& v : R[n].vias) forStamp(Elem{3, v.x, v.y, 0, 0, 0, 3}, add);
    for (int kd = 0; kd < 2 * (K + 1); kd++)
        for (int o : R[n].stamp[kd]) { mark[kd][o] = 0; if (occ[kd][o] < 255) occ[kd][o]++; }
}

// ------------------------------------------------------------------ search
vector<float> G;
vector<int> PAR;
vector<uint32_t> GEN, CLO;
vector<i8> PDIR;
uint32_t curGen = 0;
const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
const int DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
const double BEND[5] = {0, 3, 8, 20, 40};
double VIA_COST = 35, HALO_COST = 0.6, HW = 1.3, PRES = 0.5;
long MAXEXP = 25000000;

struct BBox { int x0, y0, x1, y1; };

static inline bool fixFree(int kd, size_t o, int net) { i8 f = fixm[kd][o]; return f == 0 || f == net; }

// congestion multiplier for a centre at (kind, offset) of layer l
static inline double cong(int kd, int l, size_t o) {
    return (1.0 + hist[l * LS + o]) * (1.0 + PRES * occ[kd][o]);
}

bool search(int net, int k, bool hard, const vector<int>& sources, const vector<int>& targetTerm,
            const vector<BBox>& tb, const vector<uint8_t>& isSrc, vector<int>& path, int& hitTerm) {
    curGen++;
    typedef pair<float, int> PQ;
    priority_queue<PQ, vector<PQ>, greater<PQ>> pq;
    auto heur = [&](int ix, int iy) {
        double best = 1e18;
        for (auto& b : tb) {
            double dx = max(max(b.x0 - ix, ix - b.x1), 0), dy = max(max(b.y0 - iy, iy - b.y1), 0);
            best = min(best, max(dx, dy) + 0.4142 * min(dx, dy));
        }
        return best * HW;
    };
    for (int s : sources) {
        size_t rem = s % LS;
        G[s] = 0; PAR[s] = -1; GEN[s] = curGen; PDIR[s] = -1;
        pq.push({(float)heur((int)(rem % W), (int)(rem / W)), s});
    }
    long exps = 0;
    int found = -1;
    int kd0[2] = {kindOf(k, 0), kindOf(k, 1)}, kv[2] = {kindOf(K, 0), kindOf(K, 1)};
    while (!pq.empty()) {
        int cur = pq.top().second; pq.pop();
        if (CLO[cur] == curGen) continue;
        CLO[cur] = curGen;
        if (targetTerm[cur] >= 0 && !isSrc[cur]) { found = cur; break; }
        if (++exps > MAXEXP) break;
        int l = cur / LS; size_t rem = cur % LS; int iy = rem / W, ix = rem % W;
        float g = G[cur];
        int pd = PDIR[cur], kd = kd0[l];
        for (int d = 0; d < 8; d++) {
            int nx = ix + DX[d], ny = iy + DY[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            size_t o = (size_t)ny * W + nx;
            int ni = (int)(l * LS + o);
            if (CLO[ni] == curGen) continue;
            bool forced = targetTerm[ni] >= 0 || isSrc[ni];
            if (!forced) {
                if (!fixFree(kd, o, net)) continue;
                if (hard && occ[kd][o]) continue;
                if (d & 1) {
                    size_t oa = (size_t)iy * W + nx, ob = (size_t)ny * W + ix;
                    if (!fixFree(kd, oa, net) || !fixFree(kd, ob, net)) continue;
                }
            }
            double step = (d & 1) ? 1.4142 : 1.0;
            if (!(d & 1) && ((l == 0) != (d == 0 || d == 4))) step += 0.6;
            step *= forced ? 1.0 : cong(kd, l, o);
            if (pd >= 0) { int t = abs(d - pd); if (t > 4) t = 8 - t; step += BEND[t]; }
            { i8 hv = halo[l][o]; if (hv != 0 && hv != net) step += HALO_COST; }
            float ng = g + (float)step;
            if (GEN[ni] != curGen || ng < G[ni]) {
                GEN[ni] = curGen; G[ni] = ng; PAR[ni] = cur; PDIR[ni] = (i8)d;
                pq.push({(float)(ng + heur(nx, ny)), ni});
            }
        }
        if (!noVia[rem]) {
            int ol = 1 - l, ni = (int)(ol * LS + rem);
            bool forced = targetTerm[ni] >= 0 || isSrc[ni];
            if (CLO[ni] != curGen &&
                (forced || (fixFree(kv[0], rem, net) && fixFree(kv[1], rem, net) && fixFree(kd0[ol], rem, net) &&
                            (!hard || (!occ[kv[0]][rem] && !occ[kv[1]][rem] && !occ[kd0[ol]][rem]))))) {
                double step = VIA_COST * (1.0 + PRES * (occ[kv[0]][rem] + occ[kv[1]][rem])) * (1.0 + hist[rem] + hist[LS + rem]);
                float ng = g + (float)step;
                if (GEN[ni] != curGen || ng < G[ni]) {
                    GEN[ni] = curGen; G[ni] = ng; PAR[ni] = cur; PDIR[ni] = -1;
                    pq.push({(float)(ng + heur(ix, iy)), ni});
                }
            }
        }
    }
    if (found < 0) return false;
    hitTerm = targetTerm[found];
    path.clear();
    for (int c = found; c >= 0; c = PAR[c]) path.push_back(c);
    reverse(path.begin(), path.end());
    return true;
}

void commitPath(int n, int k, const vector<int>& path) {
    double w = CW[k];
    struct P { double x, y; int l; int c; };
    vector<P> pts;
    for (int c : path) { size_t rem = c % LS; pts.push_back({(rem % W) * RES, (rem / W) * RES, (int)(c / LS), c}); }
    for (size_t i = 0; i < pts.size(); i++) {
        R[n].pathCells.push_back({pts[i].c, k});
        if (i > 0 && pts[i].l != pts[i - 1].l) {
            R[n].vias.push_back({pts[i].x, pts[i].y});
            R[n].viaCells.push_back((int)(pts[i].c % LS));
        }
    }
    size_t i = 0;
    while (i + 1 < pts.size()) {
        if (pts[i + 1].l != pts[i].l) { i++; continue; }
        size_t j = i + 1;
        double dx = pts[j].x - pts[i].x, dy = pts[j].y - pts[i].y;
        while (j + 1 < pts.size() && pts[j + 1].l == pts[j].l && fabs(pts[j + 1].x - pts[j].x - dx) < 1e-9 &&
               fabs(pts[j + 1].y - pts[j].y - dy) < 1e-9)
            j++;
        R[n].segs.push_back({n, pts[i].l, pts[i].x, pts[i].y, pts[j].x, pts[j].y, w});
        i = j;
    }
}

void exactLink(int n, const Term& t, int cell, double w) {
    if (!t.hasExact) return;
    size_t rem = cell % LS;
    double cx = (rem % W) * RES, cy = (rem / W) * RES;
    double d = hypot(cx - t.ex, cy - t.ey);
    if (d > 1e-6 && d < 0.2) R[n].segs.push_back({n, (int)(cell / LS), cx, cy, t.ex, t.ey, w});
}

vector<string> unrouted;

// route net n from scratch (own stamps must already be removed)
bool routeNet(int n, bool hard) {
    NetRoute& r = R[n];
    r.segs.clear(); r.vias.clear(); r.pathCells.clear(); r.viaCells.clear();
    vector<int>& tl = netTerms[n];
    static vector<uint8_t> isSrc;
    static vector<int> targetTerm;
    if (isSrc.empty()) { isSrc.assign(2 * LS, 0); targetTerm.assign(2 * LS, -1); }
    vector<int> tree;
    vector<char> done(tl.size(), 0);
    int start = -1;
    for (size_t i = 0; i < tl.size(); i++) if (terms[tl[i]].ok) { start = (int)i; break; }
    bool ok = true;
    if (start < 0) return false;
    auto addTree = [&](int c) { if (!isSrc[c]) { isSrc[c] = 1; tree.push_back(c); } };
    for (int c : terms[tl[start]].cells) addTree(c);
    done[start] = 1;
    exactLink(n, terms[tl[start]], terms[tl[start]].cells[0], CW[0]);
    int cls0 = nets[n].cls;
    for (;;) {
        vector<BBox> tb;
        vector<int> tcells;
        for (size_t i = 0; i < tl.size(); i++) {
            if (done[i]) continue;
            Term& t = terms[tl[i]];
            if (!t.ok) { done[i] = 1; ok = false; unrouted.push_back(nets[n].name + " " + t.key + " (no access)"); continue; }
            BBox b{1 << 30, 1 << 30, -1, -1};
            for (int c : t.cells) {
                size_t rem = c % LS; int x = rem % W, y = rem / W;
                b.x0 = min(b.x0, x); b.x1 = max(b.x1, x); b.y0 = min(b.y0, y); b.y1 = max(b.y1, y);
                targetTerm[c] = (int)i; tcells.push_back(c);
            }
            tb.push_back(b);
        }
        if (tb.empty()) break;
        vector<int> path; int hit = -1, kUsed = -1;
        vector<int> ks = {cls0};
        if (cls0 > 0) ks.push_back(0);
        for (int k : ks) if (search(n, k, hard, tree, targetTerm, tb, isSrc, path, hit)) { kUsed = k; break; }
        for (int c : tcells) targetTerm[c] = -1;
        if (kUsed < 0) {
            for (size_t i = 0; i < tl.size(); i++)
                if (!done[i]) unrouted.push_back(nets[n].name + " " + terms[tl[i]].key);
            ok = false;
            break;
        }
        commitPath(n, kUsed, path);
        for (int c : path) addTree(c);
        for (int c : terms[tl[hit]].cells) addTree(c);
        exactLink(n, terms[tl[hit]], path.back(), CW[kUsed]);
        done[hit] = 1;
    }
    for (int c : tree) isSrc[c] = 0;
    r.complete = ok;
    return ok;
}

// number of path cells of net n lying in another net's keep-out
int conflicts(int n, bool addHist, float hf, int thr = 1) {  // thr: own copper counted in occ (1 if stamped)
    int cnt = 0;
    for (auto& pc : R[n].pathCells) {
        int l = pc.first / LS; size_t o = pc.first % LS;
        if (occ[kindOf(pc.second, l)][o] > thr) {
            cnt++;
            if (addHist) hist[pc.first] += hf;
        }
    }
    for (int o : R[n].viaCells)
        for (int l = 0; l < 2; l++)
            if (occ[kindOf(K, l)][o] > thr) { cnt++; if (addHist) hist[l * LS + o] += hf; }
    return cnt;
}

// ------------------------------------------------------------------ terminals / stubs
bool cellFree0(int net, int l, int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H) return false;
    return fixFree(kindOf(0, l), (size_t)y * W + x, net);
}

void setupTerminals() {
    for (auto& t : terms) {
        int net = t.net;
        if (t.stub) {
            int l = (t.lay & 1) ? 0 : 1;
            double ws = (t.lmax >= 1.0) ? 0.2 : 0.25;
            bool done = false;
            for (int pass = 0; pass < 3 && !done; pass++) {
                double ddx = t.dx, ddy = t.dy, edge = t.edge;
                if (pass == 1) { ddx = t.dy; ddy = t.dx; edge = (t.dx != 0) ? t.hy : t.hx; }
                if (pass == 2) { ddx = -t.dy; ddy = -t.dx; edge = (t.dx != 0) ? t.hy : t.hx; }
                for (double L = t.lmax; L >= t.lmin - 1e-9 && !done; L -= 0.1) {
                    double ex = t.px + ddx * (edge + L), ey = t.py + ddy * (edge + L);
                    int gx = (int)lround(ex / RES), gy = (int)lround(ey / RES);
                    double Gx = gx * RES, Gy = gy * RES, len = hypot(Gx - t.px, Gy - t.py);
                    bool good = cellFree0(net, l, gx, gy);
                    for (double s = edge + 0.1; s <= len + 1e-9 && good; s += 0.05) {
                        double u = min(s, len) / len;
                        if (!cellFree0(net, l, (int)lround((t.px + (Gx - t.px) * u) / RES), (int)lround((t.py + (Gy - t.py) * u) / RES)))
                            good = false;
                    }
                    if (!good) continue;
                    Seg s{net, l, t.px, t.py, Gx, Gy, ws};
                    stubSegs.push_back(s);
                    stampFixed(Elem{2, s.x1, s.y1, s.x2, s.y2, ws, 1 << l}, (i8)net);
                    t.cells.push_back(idx(l, gx, gy));
                    t.ok = done = true;
                }
            }
            if (done) continue;
            t.stub = false; t.single = 1;
        }
        for (int l = 0; l < 2; l++) {
            if (!(t.lay & (1 << l))) continue;
            if (t.single) {
                int cx0 = (int)lround(t.px / RES), cy0 = (int)lround(t.py / RES), best = -1;
                double bd = 1e18;
                for (int dy = -12; dy <= 12; dy++)
                    for (int dx = -12; dx <= 12; dx++) {
                        int cx = cx0 + dx, cy = cy0 + dy;
                        double x = cx * RES, y = cy * RES;
                        bool inn = t.circ ? hypot(x - t.px, y - t.py) <= t.hx - 0.1
                                          : (fabs(x - t.px) <= t.hx - 0.1 && fabs(y - t.py) <= t.hy - 0.1);
                        if (!inn || !cellFree0(net, l, cx, cy)) continue;
                        double d = hypot(x - t.px, y - t.py);
                        if (d < bd) { bd = d; best = idx(l, cx, cy); }
                    }
                if (best >= 0) { t.cells.push_back(best); t.ok = true; }
                t.ex = t.px; t.ey = t.py; t.hasExact = true;
            } else {
                int x0 = max(0, (int)floor((t.px - t.hx) / RES)), x1 = min(W - 1, (int)ceil((t.px + t.hx) / RES));
                int y0 = max(0, (int)floor((t.py - t.hy) / RES)), y1 = min(H - 1, (int)ceil((t.py + t.hy) / RES));
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++) {
                        double X = x * RES, Y = y * RES;
                        bool inn = t.circ ? hypot(X - t.px, Y - t.py) <= t.hx - 0.15
                                          : (fabs(X - t.px) <= t.hx - 0.15 && fabs(Y - t.py) <= t.hy - 0.15);
                        if (inn && cellFree0(net, l, x, y)) t.cells.push_back(idx(l, x, y));
                    }
                if (!t.cells.empty()) t.ok = true;
            }
        }
    }
}

void writeResult(const char* path) {
    ofstream o(path);
    o.setf(ios::fixed);
    o.precision(4);
    for (auto& s : stubSegs) o << "SEG " << s.net << " " << s.layer << " " << s.x1 << " " << s.y1 << " " << s.x2 << " " << s.y2 << " " << s.w << endl;
    for (size_t n = 1; n < nets.size(); n++) {
        for (auto& s : R[n].segs) o << "SEG " << s.net << " " << s.layer << " " << s.x1 << " " << s.y1 << " " << s.x2 << " " << s.y2 << " " << s.w << endl;
        for (auto& v : R[n].vias) o << "VIA " << n << " " << v.x << " " << v.y << endl;
    }
    set<string> u(unrouted.begin(), unrouted.end());
    for (auto& s : u) o << "UNROUTED " << s << endl;
}

double secs(chrono::steady_clock::time_point t) { return chrono::duration<double>(chrono::steady_clock::now() - t).count(); }

int main(int argc, char** argv) {
    if (argc < 3) { cerr << "usage: pathfinder problem result [iters]" << endl; return 1; }
    int ITERS = argc > 3 ? atoi(argv[3]) : 40;
    ifstream in(argv[1]);
    string line;
    nets.push_back({"", 0});
    vector<pair<string, string>> later;
    while (getline(in, line)) {
        istringstream ss(line);
        string kind; ss >> kind;
        if (kind == "BOARD") ss >> BW >> BH;
        else if (kind == "RULE") ss >> CLR >> EDGE >> VIAD >> VIADRILL;
        else if (kind == "CLASS") { double w; while (ss >> w) CW.push_back(w); }
        else if (kind == "NET") { int id, c; string nm; ss >> id >> c >> nm; nets.push_back({nm, c}); }
        else if (!kind.empty()) later.push_back({kind, line.substr(kind.size() + 1)});
    }
    K = (int)CW.size();
    if (nets.size() > 100 || K > 3) { cerr << "limits exceeded" << endl; return 1; }
    W = (int)round(BW / RES) + 1; H = (int)round(BH / RES) + 1; LS = (size_t)W * H;
    for (int kd = 0; kd < 2 * (K + 1); kd++) { fixm[kd].assign(LS, 0); occ[kd].assign(LS, 0); mark[kd].assign(LS, 0); }
    noVia.assign(LS, 0); halo[0].assign(LS, 0); halo[1].assign(LS, 0); hist.assign(2 * LS, 0.f);
    G.assign(2 * LS, 0); PAR.assign(2 * LS, -1); GEN.assign(2 * LS, 0); CLO.assign(2 * LS, 0); PDIR.assign(2 * LS, -1);
    R.resize(nets.size()); netTerms.resize(nets.size());
    cerr << "grid " << W << "x" << H << ", nets " << nets.size() - 1 << endl;

    for (int k = 0; k <= K; k++) {
        double m = EDGE + (k < K ? CW[k] / 2 : VIAD / 2);
        for (int l = 0; l < 2; l++)
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    double X = x * RES, Y = y * RES;
                    if (X < m || Y < m || X > BW - m || Y > BH - m) fixm[kindOf(k, l)][(size_t)y * W + x] = HARD;
                }
    }
    for (auto& tl : later) {
        istringstream ss(tl.second);
        if (tl.first == "OBS") {
            int net, lay; string sh; ss >> net >> lay >> sh;
            Elem e{};
            e.lay = lay;
            if (sh == "R") { e.type = 0; ss >> e.a >> e.b >> e.c >> e.d; }
            else { e.type = 1; ss >> e.a >> e.b >> e.c; e.d = e.c; }
            i8 v = (i8)(net < 0 ? HARD : net);
            stampFixed(e, v);
            double ext = VIAD / 2 + 0.15, hr = 0.9;
            int x0 = max(0, (int)floor((e.a - e.c - hr) / RES)), x1 = min(W - 1, (int)ceil((e.a + e.c + hr) / RES));
            int y0 = max(0, (int)floor((e.b - e.d - hr) / RES)), y1 = min(H - 1, (int)ceil((e.b + e.d + hr) / RES));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    double X = x * RES, Y = y * RES;
                    double d = e.type == 0 ? rectDist(X, Y, e.a, e.b, e.c, e.d) : max(0.0, hypot(X - e.a, Y - e.b) - e.c);
                    size_t o = (size_t)y * W + x;
                    if (d <= ext) noVia[o] = 1;
                    if (d <= hr)
                        for (int l = 0; l < 2; l++) if (lay & (1 << l)) put(halo[l][o], v);
                }
        } else if (tl.first == "TSTUB") {
            Term t; t.stub = true;
            ss >> t.net >> t.lay >> t.px >> t.py >> t.dx >> t.dy >> t.hx >> t.hy >> t.lmin >> t.lmax >> t.key;
            t.edge = (t.dx != 0) ? t.hx : t.hy;
            terms.push_back(t);
        } else if (tl.first == "TREG") {
            Term t; string sh;
            ss >> t.net >> t.lay >> t.single >> sh;
            if (sh == "R") { ss >> t.px >> t.py >> t.hx >> t.hy; }
            else { t.circ = true; ss >> t.px >> t.py >> t.hx; t.hy = t.hx; }
            ss >> t.key;
            terms.push_back(t);
        }
    }
    setupTerminals();
    for (size_t i = 0; i < terms.size(); i++) netTerms[terms[i].net].push_back((int)i);
    for (auto& t : terms) if (!t.ok) cerr << "terminal without access: " << t.key << " (" << nets[t.net].name << ")" << endl;

    vector<pair<double, int>> order;
    for (size_t n = 1; n < nets.size(); n++) {
        if (netTerms[n].size() < 2) continue;
        double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
        for (int ti : netTerms[n]) {
            x0 = min(x0, terms[ti].px); x1 = max(x1, terms[ti].px); y0 = min(y0, terms[ti].py); y1 = max(y1, terms[ti].py);
        }
        order.push_back({(x1 - x0) + (y1 - y0), (int)n});
    }
    sort(order.begin(), order.end());

    auto T0 = chrono::steady_clock::now();
    vector<int> todo;
    for (auto& o : order) todo.push_back(o.second);
    float HF = 1.0f;
    for (int it = 1; it <= ITERS; it++) {
        auto ti = chrono::steady_clock::now();
        unrouted.clear();
        for (int n : todo) {
            unstampNet(n);
            routeNet(n, false);
            stampNet(n);
        }
        // collect remaining unrouted for nets not rerouted this pass
        int total = 0;
        vector<int> bad;
        for (auto& o : order) {
            int n = o.second;
            int c = conflicts(n, true, HF);
            if (c) { bad.push_back(n); total += c; }
        }
        cerr << "iter " << it << ": routed " << todo.size() << " nets, conflicting nets " << bad.size()
             << " (cells " << total << "), pres " << PRES << ", " << secs(ti) << "s" << endl;
        for (int n : bad) cerr << " " << nets[n].name;
        cerr << endl;
        writeResult(argv[2]);
        if (bad.empty()) break;
        todo = bad;
        PRES *= 1.6;
    }
    // final legalisation: nets still in conflict are rerouted without any sharing (or dropped)
    unrouted.clear();
    for (auto& o : order) {
        int n = o.second;
        if (!R[n].complete) unrouted.push_back(nets[n].name + " (incomplete)");
        if (!conflicts(n, false, 0)) continue;
        unstampNet(n);
        bool ok = routeNet(n, true);
        if (!ok || conflicts(n, false, 0, 0)) {
            R[n].segs.clear(); R[n].vias.clear(); R[n].pathCells.clear(); R[n].viaCells.clear();
            unrouted.push_back(nets[n].name + " (dropped: congestion)");
            cerr << "dropped " << nets[n].name << endl;
        } else {
            stampNet(n);
            cerr << "legalised " << nets[n].name << endl;
        }
    }
    writeResult(argv[2]);
    cerr << "done in " << secs(T0) << "s" << endl;
    return 0;
}
