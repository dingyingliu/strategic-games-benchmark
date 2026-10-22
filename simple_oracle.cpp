// simple_oracle.cpp
// Exact full-horizon equilibrium oracle for the SIMPLE Fortune Fight variant:
//   - numbers 1..N, one copy each, T = N rounds (fully locked)
//   - actions: Hold x4, Steal x3, Trap x3 (locked: sum of caps = T)
//   - each round: simultaneous number commit -> reveal -> simultaneous action
//     commit -> reveal -> score
// Resolution (Red action vs Blue action):
//   H/H: each scores own | H/S: stealer scores holder's number | H/T: holder
//   scores own, trapper 0 | S/S: each scores the opponent's number | S/T:
//   trapper scores both | T/T: both 0
// Objective: expected final score difference (zero-sum).
//
// Subcommands:
//   test  - LP unit battery
//   mini  - full battery on a 6-card validation instance (incl. all BR passes)
//   solve - solve the full 10-round game, validate, save value tables
//   sims  - equilibrium self-play stats + myopic comparison (loads tables)
//   brU / brM / brE - exact best response vs uniform / myopic / true-eq policy
//
// Build: g++ -O3 -march=native -o simple_oracle simple_oracle.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_set>
using namespace std;

static long long LP_COUNT = 0;
static long long BR_VIOL_COUNT = 0;
static double    BR_VIOL_MAX   = 0.0;

// ---------------------------------------------------------------- LP solver
static const int MAXD = 10;

struct Sol {
    int m = 0, n = 0;
    double v = 0;
    double x[MAXD] = {0};
    double y[MAXD] = {0};
};

static void solveGameCore(int m, int n, const double A[MAXD][MAXD], Sol& s, double& violOut);

static void solveGame(int m, int n, const double A[MAXD][MAXD], Sol& s) {
    LP_COUNT++;
    double viol;
    solveGameCore(m, n, A, s, viol);
    if (viol > 1e-6 && m > 1 && n > 1) {
        // degenerate solve: retry with tiny deterministic jitter to break ties
        double B[MAXD][MAXD];
        uint64_t h = 88172645463325252ULL;
        for (int attempt = 0; attempt < 3 && viol > 1e-6; attempt++) {
            for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) {
                h ^= h << 13; h ^= h >> 7; h ^= h << 17;
                B[i][j] = A[i][j] + 1e-9 * (double)(h % 1000);
            }
            Sol s2; double v2;
            solveGameCore(m, n, B, s2, v2);
            // re-verify the jittered solution against the ORIGINAL matrix
            double worstRow = 1e300, bestCol = -1e300;
            for (int j = 0; j < n; j++) {
                double e = 0; for (int i = 0; i < m; i++) e += s2.x[i] * A[i][j];
                worstRow = min(worstRow, e);
            }
            for (int i = 0; i < m; i++) {
                double e = 0; for (int j = 0; j < n; j++) e += A[i][j] * s2.y[j];
                bestCol = max(bestCol, e);
            }
            double v2orig = max(s2.v - worstRow, bestCol - s2.v);
            if (v2orig < viol) { s = s2; viol = v2orig; }
        }
    }
    if (viol > 1e-6) { BR_VIOL_COUNT++; if (viol > BR_VIOL_MAX) BR_VIOL_MAX = viol; }
}

static void solveGameCore(int m, int n, const double A[MAXD][MAXD], Sol& s, double& violOut) {
    s.m = m; s.n = n;
    violOut = 0;
    for (int i = 0; i < MAXD; i++) { s.x[i] = 0; s.y[i] = 0; }
    if (m == 1 && n == 1) { s.v = A[0][0]; s.x[0] = 1; s.y[0] = 1; return; }
    if (m == 1) {
        int jb = 0; for (int j = 1; j < n; j++) if (A[0][j] < A[0][jb]) jb = j;
        s.v = A[0][jb]; s.x[0] = 1; s.y[jb] = 1; return;
    }
    if (n == 1) {
        int ib = 0; for (int i = 1; i < m; i++) if (A[i][0] > A[ib][0]) ib = i;
        s.v = A[ib][0]; s.x[ib] = 1; s.y[0] = 1; return;
    }
    double mn = A[0][0];
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) mn = min(mn, A[i][j]);
    const double shift = 1.0 - mn;
    const int cols = n + m;
    double T[MAXD][2 * MAXD + 1];
    double Z[2 * MAXD + 2];
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) T[i][j] = A[i][j] + shift;
        for (int j = n; j < cols; j++) T[i][j] = (j - n == i) ? 1.0 : 0.0;
        T[i][cols] = 1.0;
    }
    for (int j = 0; j <= cols; j++) Z[j] = 0.0;
    for (int j = 0; j < n; j++) Z[j] = -1.0;
    int basis[MAXD]; for (int i = 0; i < m; i++) basis[i] = n + i;
    const double EPS = 1e-11;
    for (int it = 0; it < 5000; it++) {
        int pj = -1;
        for (int j = 0; j < cols; j++) if (Z[j] < -EPS) { pj = j; break; }
        if (pj < 0) break;
        int pr = -1; double best = 1e300; int bb = 1 << 30;
        for (int i = 0; i < m; i++) if (T[i][pj] > EPS) {
            double rat = T[i][cols] / T[i][pj];
            if (rat < best - 1e-12 || (rat < best + 1e-12 && basis[i] < bb)) {
                best = rat; pr = i; bb = basis[i];
            }
        }
        if (pr < 0) break;
        double p = T[pr][pj];
        for (int j = 0; j <= cols; j++) T[pr][j] /= p;
        for (int i = 0; i < m; i++) if (i != pr) {
            double f = T[i][pj];
            if (f != 0) for (int j = 0; j <= cols; j++) T[i][j] -= f * T[pr][j];
        }
        { double f = Z[pj]; if (f != 0) for (int j = 0; j <= cols; j++) Z[j] -= f * T[pr][j]; }
        basis[pr] = pj;
    }
    double sumy = Z[cols];
    double vp = 1.0 / max(sumy, 1e-300);
    s.v = vp - shift;
    for (int i = 0; i < m; i++) if (basis[i] < n) s.y[basis[i]] = T[i][cols] * vp;
    for (int i = 0; i < m; i++) s.x[i] = Z[n + i] * vp;
    double sx = 0, sy = 0;
    for (int i = 0; i < m; i++) { if (s.x[i] < 0) s.x[i] = 0; sx += s.x[i]; }
    for (int j = 0; j < n; j++) { if (s.y[j] < 0) s.y[j] = 0; sy += s.y[j]; }
    if (sx > 0) for (int i = 0; i < m; i++) s.x[i] /= sx; else s.x[0] = 1;
    if (sy > 0) for (int j = 0; j < n; j++) s.y[j] /= sy; else s.y[0] = 1;
    double worstRow = 1e300, bestCol = -1e300;
    for (int j = 0; j < n; j++) {
        double e = 0; for (int i = 0; i < m; i++) e += s.x[i] * A[i][j];
        worstRow = min(worstRow, e);
    }
    for (int i = 0; i < m; i++) {
        double e = 0; for (int j = 0; j < n; j++) e += A[i][j] * s.y[j];
        bestCol = max(bestCol, e);
    }
    violOut = max(s.v - worstRow, bestCol - s.v);
}

// -------------------------------------------------------------------- rules
enum { H = 0, S = 1, TP = 2 };
static const int POW5[3] = {1, 5, 25};

// returns (scoreR, scoreB) for revealed values r,b and actions aR,aB
static inline void resolve(int r, int b, int aR, int aB, double& sR, double& sB) {
    sR = 0; sB = 0;
    if      (aR == H  && aB == H ) { sR = r; sB = b; }
    else if (aR == H  && aB == S ) { sB = r; }
    else if (aR == H  && aB == TP) { sR = r; }
    else if (aR == S  && aB == H ) { sR = b; }
    else if (aR == S  && aB == S ) { sR = b; sB = r; }
    else if (aR == S  && aB == TP) { sB = r + b; }
    else if (aR == TP && aB == H ) { sB = b; }
    else if (aR == TP && aB == S ) { sR = r + b; }
    else { /* T/T: nothing */ }
}

struct Config { string name; int N, T; int caps[3]; };

// ------------------------------------------------------------------- layers
struct Layers {
    Config cfg;
    int totAct;
    vector<vector<int>> masks, maskRank, acts, actRank;
    vector<size_t> nMask, nAct, layerSize;
    vector<vector<double>> V;

    void build(const Config& c) {
        cfg = c;
        totAct = c.caps[0] + c.caps[1] + c.caps[2];
        int T = c.T, N = c.N;
        masks.assign(T, {}); maskRank.assign(T, vector<int>(1 << N, -1));
        acts.assign(T, {});  actRank.assign(T, vector<int>(125, -1));
        nMask.assign(T, 0); nAct.assign(T, 0); layerSize.assign(T, 0);
        for (int k = 0; k < T; k++) {
            for (int m = 0; m < (1 << N); m++)
                if (__builtin_popcount(m) == N - k) {
                    maskRank[k][m] = (int)masks[k].size(); masks[k].push_back(m);
                }
            for (int id = 0; id < 125; id++) {
                int cnt[3], sum = 0; bool ok = true;
                for (int a = 0; a < 3; a++) {
                    cnt[a] = (id / POW5[a]) % 5;
                    if (cnt[a] > c.caps[a]) ok = false;
                    sum += cnt[a];
                }
                if (ok && sum == totAct - k) {
                    actRank[k][id] = (int)acts[k].size(); acts[k].push_back(id);
                }
            }
            nMask[k] = masks[k].size(); nAct[k] = acts[k].size();
            layerSize[k] = nMask[k] * nMask[k] * nAct[k] * nAct[k];
        }
        V.assign(T, {});
    }
    inline size_t idx(int k, int mr, int mb, int ar, int ab) const {
        return (((size_t)maskRank[k][mr] * nMask[k] + maskRank[k][mb]) * nAct[k]
                + actRank[k][ar]) * nAct[k] + actRank[k][ab];
    }
    inline void decode(int k, size_t si, int& mr, int& mb, int& ar, int& ab) const {
        ab = acts[k][si % nAct[k]]; si /= nAct[k];
        ar = acts[k][si % nAct[k]]; si /= nAct[k];
        mb = masks[k][si % nMask[k]]; si /= nMask[k];
        mr = masks[k][(int)si];
    }
    int rootMask() const { return (1 << cfg.N) - 1; }
    int rootAct()  const { return cfg.caps[0] + cfg.caps[1] * 5 + cfg.caps[2] * 25; }
};

struct Details {
    int cR = 0, cB = 0, aRn = 0, aBn = 0;
    int numsR[MAXD], numsB[MAXD], availR[3], availB[3];
    double MN[MAXD][MAXD];
    Sol nsol;
    double MA[MAXD][MAXD][3][3];
    Sol asol[MAXD][MAXD];
};

static double stateValue(const Layers& L, int k, int mr, int mb, int ar, int ab,
                         Details* det, const vector<vector<double>>* contT) {
    const Config& cfg = L.cfg;
    int numsR[MAXD], cR = 0, numsB[MAXD], cB = 0;
    for (int v = 0; v < cfg.N; v++) {
        if ((mr >> v) & 1) numsR[cR++] = v + 1;
        if ((mb >> v) & 1) numsB[cB++] = v + 1;
    }
    int availR[3], aRn = 0, availB[3], aBn = 0;
    for (int a = 0; a < 3; a++) {
        if ((ar / POW5[a]) % 5 > 0) availR[aRn++] = a;
        if ((ab / POW5[a]) % 5 > 0) availB[aBn++] = a;
    }
    double MN[MAXD][MAXD];
    for (int i = 0; i < cR; i++) {
        for (int j = 0; j < cB; j++) {
            int r = numsR[i], b = numsB[j];
            double MA[MAXD][MAXD];
            for (int p = 0; p < aRn; p++) {
                for (int q = 0; q < aBn; q++) {
                    double sR, sB;
                    resolve(r, b, availR[p], availB[q], sR, sB);
                    double cv = 0;
                    if (contT && k + 1 < cfg.T) {
                        int mr2 = mr & ~(1 << (r - 1)), mb2 = mb & ~(1 << (b - 1));
                        int ar2 = ar - POW5[availR[p]], ab2 = ab - POW5[availB[q]];
                        cv = (*contT)[k + 1][L.idx(k + 1, mr2, mb2, ar2, ab2)];
                    }
                    MA[p][q] = (sR - sB) + cv;
                }
            }
            Sol as; solveGame(aRn, aBn, MA, as);
            MN[i][j] = as.v;
            if (det) {
                for (int p = 0; p < aRn; p++) for (int q = 0; q < aBn; q++)
                    det->MA[i][j][p][q] = MA[p][q];
                det->asol[i][j] = as;
            }
        }
    }
    Sol ns; solveGame(cR, cB, MN, ns);
    if (det) {
        det->cR = cR; det->cB = cB; det->aRn = aRn; det->aBn = aBn;
        memcpy(det->numsR, numsR, sizeof(numsR)); memcpy(det->numsB, numsB, sizeof(numsB));
        memcpy(det->availR, availR, sizeof(availR)); memcpy(det->availB, availB, sizeof(availB));
        for (int i = 0; i < cR; i++) for (int j = 0; j < cB; j++) det->MN[i][j] = MN[i][j];
        det->nsol = ns;
    }
    return ns.v;
}

static void solveAll(Layers& L) {
    for (int k = L.cfg.T - 1; k >= 0; k--) {
        L.V[k].assign(L.layerSize[k], 0.0);
        for (size_t si = 0; si < L.layerSize[k]; si++) {
            int mr, mb, ar, ab;
            L.decode(k, si, mr, mb, ar, ab);
            L.V[k][si] = stateValue(L, k, mr, mb, ar, ab, nullptr, &L.V);
        }
        fprintf(stderr, "  layer k=%d done (%zu states)\n", k, L.layerSize[k]);
    }
}

static void saveV(const Layers& L, const char* path) {
    FILE* f = fopen(path, "wb");
    int hdr[3] = {L.cfg.N, L.cfg.T, L.totAct};
    fwrite(hdr, sizeof(int), 3, f);
    for (int k = 0; k < L.cfg.T; k++)
        fwrite(L.V[k].data(), sizeof(double), L.layerSize[k], f);
    fclose(f);
}
static bool loadV(Layers& L, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    int hdr[3];
    if (fread(hdr, sizeof(int), 3, f) != 3 || hdr[0] != L.cfg.N || hdr[1] != L.cfg.T) { fclose(f); return false; }
    for (int k = 0; k < L.cfg.T; k++) {
        L.V[k].assign(L.layerSize[k], 0.0);
        if (fread(L.V[k].data(), sizeof(double), L.layerSize[k], f) != L.layerSize[k]) { fclose(f); return false; }
    }
    fclose(f);
    return true;
}

static void validate(const Layers& L, double& rootV, double& maxAntisym) {
    rootV = L.V[0][L.idx(0, L.rootMask(), L.rootMask(), L.rootAct(), L.rootAct())];
    maxAntisym = 0;
    mt19937_64 rng(12345);
    for (int k = 0; k < L.cfg.T; k++) {
        for (int t = 0; t < 400; t++) {
            size_t si = rng() % L.layerSize[k];
            int mr, mb, ar, ab;
            L.decode(k, si, mr, mb, ar, ab);
            maxAntisym = max(maxAntisym,
                fabs(L.V[k][si] + L.V[k][L.idx(k, mb, mr, ab, ar)]));
        }
    }
}

// policy: 0 uniform, 1 myopic stage equilibrium, 2 true equilibrium
static double bestResponse(const Layers& L, int policy) {
    const Config& cfg = L.cfg;
    vector<vector<double>> U(cfg.T);
    for (int k = cfg.T - 1; k >= 0; k--) {
        U[k].assign(L.layerSize[k], 0.0);
        for (size_t si = 0; si < L.layerSize[k]; si++) {
            int mr, mb, ar, ab;
            L.decode(k, si, mr, mb, ar, ab);
            Details det;
            if (policy != 0)
                stateValue(L, k, mr, mb, ar, ab, &det, policy == 2 ? &L.V : nullptr);
            else {
                det.cR = det.cB = det.aRn = det.aBn = 0;
                for (int v = 0; v < cfg.N; v++) {
                    if ((mr >> v) & 1) det.numsR[det.cR++] = v + 1;
                    if ((mb >> v) & 1) det.numsB[det.cB++] = v + 1;
                }
                for (int a = 0; a < 3; a++) {
                    if ((ar / POW5[a]) % 5 > 0) det.availR[det.aRn++] = a;
                    if ((ab / POW5[a]) % 5 > 0) det.availB[det.aBn++] = a;
                }
            }
            double best = -1e300;
            for (int i = 0; i < det.cR; i++) {
                double acc = 0;
                for (int j = 0; j < det.cB; j++) {
                    double yb = (policy == 0) ? 1.0 / det.cB : det.nsol.y[j];
                    if (yb < 1e-15) continue;
                    int r = det.numsR[i], b = det.numsB[j];
                    double binner = -1e300;
                    for (int p = 0; p < det.aRn; p++) {
                        double e = 0;
                        for (int q = 0; q < det.aBn; q++) {
                            double yq = (policy == 0) ? 1.0 / det.aBn : det.asol[i][j].y[q];
                            if (yq < 1e-15) continue;
                            double sR, sB;
                            resolve(r, b, det.availR[p], det.availB[q], sR, sB);
                            double cont = 0;
                            if (k + 1 < cfg.T) {
                                int mr2 = mr & ~(1 << (r - 1)), mb2 = mb & ~(1 << (b - 1));
                                int ar2 = ar - POW5[det.availR[p]], ab2 = ab - POW5[det.availB[q]];
                                cont = U[k + 1][L.idx(k + 1, mr2, mb2, ar2, ab2)];
                            }
                            e += yq * ((sR - sB) + cont);
                        }
                        binner = max(binner, e);
                    }
                    acc += yb * binner;
                }
                best = max(best, acc);
            }
            U[k][si] = best;
        }
        fprintf(stderr, "  BR(policy=%d) layer k=%d done\n", policy, k);
    }
    return U[0][L.idx(0, L.rootMask(), L.rootMask(), L.rootAct(), L.rootAct())];
}

// --------------------------------------------------------------------- sims
struct Stats {
    long long games = 0;
    long long actUse[3] = {0};
    long long ties = 0, decided = 0, comebacks = 0;
    vector<double> numGaps, actGaps;
    vector<int> numSupport, actSupport;
    double rGapNum[12] = {0}, rGapAct[12] = {0};
    double rSupNum[12] = {0}, rSupAct[12] = {0};
    long long rForcedNum[12] = {0}, rForcedAct[12] = {0}, rCnt[12] = {0};
    long long cmpNum = 0, cmpNumFlip = 0, cmpAct = 0, cmpActFlip = 0;
    double cmpNumAbs = 0, cmpActAbs = 0;
    long long cmpNumOpts = 0, cmpActOpts = 0;
};

static int sampleFrom(const double* p, int n, mt19937_64& rng) {
    double u = (double)(rng() >> 11) / 9007199254740992.0, acc = 0;
    for (int i = 0; i < n; i++) { acc += p[i]; if (u <= acc) return i; }
    return n - 1;
}
static void gapSupport(const double* ev, int n, const double* mix,
                       double& gap, int& support) {
    double b1 = -1e300, b2 = -1e300;
    for (int i = 0; i < n; i++) {
        if (ev[i] > b1) { b2 = b1; b1 = ev[i]; }
        else if (ev[i] > b2) b2 = ev[i];
    }
    gap = (n >= 2) ? (b1 - b2) : 0.0;
    support = 0;
    for (int i = 0; i < n; i++) if (mix[i] > 1e-4) support++;
}

static void runSims(const Layers& L, int games, int cmpGames, Stats& st, uint64_t seed) {
    const Config& cfg = L.cfg;
    mt19937_64 rng(seed);
    int halfRound = cfg.T / 2;
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        double sR = 0, sB = 0, halfDiff = 0;
        for (int t = 0; t < cfg.T; t++) {
            Details det;
            stateValue(L, t, mr, mb, ar, ab, &det, &L.V);
            double evR[MAXD], evB[MAXD];
            for (int i = 0; i < det.cR; i++) {
                double e = 0; for (int j = 0; j < det.cB; j++) e += det.nsol.y[j] * det.MN[i][j];
                evR[i] = e;
            }
            for (int j = 0; j < det.cB; j++) {
                double e = 0; for (int i = 0; i < det.cR; i++) e += det.nsol.x[i] * det.MN[i][j];
                evB[j] = -e;
            }
            double gap; int sup;
            gapSupport(evR, det.cR, det.nsol.x, gap, sup);
            st.numGaps.push_back(gap); st.numSupport.push_back(sup);
            st.rGapNum[t] += gap; st.rSupNum[t] += sup;
            if (det.cR == 1) st.rForcedNum[t]++;
            gapSupport(evB, det.cB, det.nsol.y, gap, sup);
            st.numGaps.push_back(gap); st.numSupport.push_back(sup);
            st.rGapNum[t] += gap; st.rSupNum[t] += sup;
            if (det.cB == 1) st.rForcedNum[t]++;
            st.rCnt[t] += 2;

            int i = sampleFrom(det.nsol.x, det.cR, rng);
            int j = sampleFrom(det.nsol.y, det.cB, rng);

            bool doCmp = (g < cmpGames);
            Details dm;
            if (doCmp) {
                stateValue(L, t, mr, mb, ar, ab, &dm, nullptr);
                double evRm[MAXD];
                for (int a = 0; a < dm.cR; a++) {
                    double e = 0; for (int c = 0; c < dm.cB; c++) e += dm.nsol.y[c] * dm.MN[a][c];
                    evRm[a] = e;
                }
                double bt = -1e300, bm = -1e300; int am = 0;
                for (int a = 0; a < det.cR; a++) bt = max(bt, evR[a]);
                for (int a = 0; a < dm.cR; a++) if (evRm[a] > bm) { bm = evRm[a]; am = a; }
                st.cmpNum++;
                if (fabs(evR[am] - bt) > 1e-6) st.cmpNumFlip++;
                for (int a = 0; a < det.cR; a++) {
                    st.cmpNumAbs += fabs((bt - evR[a]) - (bm - evRm[a]));
                    st.cmpNumOpts++;
                }
            }

            const Sol& as = det.asol[i][j];
            double aevR[3], aevB[3];
            for (int p = 0; p < det.aRn; p++) {
                double e = 0; for (int q = 0; q < det.aBn; q++) e += as.y[q] * det.MA[i][j][p][q];
                aevR[p] = e;
            }
            for (int q = 0; q < det.aBn; q++) {
                double e = 0; for (int p = 0; p < det.aRn; p++) e += as.x[p] * det.MA[i][j][p][q];
                aevB[q] = -e;
            }
            gapSupport(aevR, det.aRn, as.x, gap, sup);
            st.actGaps.push_back(gap); st.actSupport.push_back(sup);
            st.rGapAct[t] += gap; st.rSupAct[t] += sup;
            if (det.aRn == 1) st.rForcedAct[t]++;
            gapSupport(aevB, det.aBn, as.y, gap, sup);
            st.actGaps.push_back(gap); st.actSupport.push_back(sup);
            st.rGapAct[t] += gap; st.rSupAct[t] += sup;
            if (det.aBn == 1) st.rForcedAct[t]++;

            if (doCmp) {
                const Sol& asm_ = dm.asol[i][j];
                double aevRm[3];
                for (int p = 0; p < dm.aRn; p++) {
                    double e = 0; for (int q = 0; q < dm.aBn; q++) e += asm_.y[q] * dm.MA[i][j][p][q];
                    aevRm[p] = e;
                }
                double bt = -1e300, bm = -1e300; int am = 0;
                for (int p = 0; p < det.aRn; p++) bt = max(bt, aevR[p]);
                for (int p = 0; p < dm.aRn; p++) if (aevRm[p] > bm) { bm = aevRm[p]; am = p; }
                st.cmpAct++;
                if (fabs(aevR[am] - bt) > 1e-6) st.cmpActFlip++;
                for (int p = 0; p < det.aRn; p++) {
                    st.cmpActAbs += fabs((bt - aevR[p]) - (bm - aevRm[p]));
                    st.cmpActOpts++;
                }
            }

            int p = sampleFrom(as.x, det.aRn, rng);
            int q = sampleFrom(as.y, det.aBn, rng);
            int r = det.numsR[i], b = det.numsB[j];
            int aRc = det.availR[p], aBc = det.availB[q];
            double dsR, dsB;
            resolve(r, b, aRc, aBc, dsR, dsB);
            st.actUse[aRc]++; st.actUse[aBc]++;
            sR += dsR; sB += dsB;
            if (t + 1 == halfRound) halfDiff = sR - sB;

            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[aRc]; ab -= POW5[aBc];
        }
        st.games++;
        double diff = sR - sB;
        if (fabs(diff) < 1e-9) st.ties++;
        else {
            st.decided++;
            if ((diff > 0 && halfDiff < -1e-9) || (diff < 0 && halfDiff > 1e-9)) st.comebacks++;
        }
    }
}

static double myopicVsEq(const Layers& L, int games, uint64_t seed) {
    const Config& cfg = L.cfg;
    mt19937_64 rng(seed);
    double total = 0;
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        double sR = 0, sB = 0;
        for (int t = 0; t < cfg.T; t++) {
            Details dt, dm;
            stateValue(L, t, mr, mb, ar, ab, &dt, &L.V);
            stateValue(L, t, mr, mb, ar, ab, &dm, nullptr);
            int i = sampleFrom(dm.nsol.x, dm.cR, rng);
            int j = sampleFrom(dt.nsol.y, dt.cB, rng);
            int p = sampleFrom(dm.asol[i][j].x, dm.aRn, rng);
            int q = sampleFrom(dt.asol[i][j].y, dt.aBn, rng);
            int r = dt.numsR[i], b = dt.numsB[j];
            double dsR, dsB;
            resolve(r, b, dt.availR[p], dt.availB[q], dsR, dsB);
            sR += dsR; sB += dsB;
            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[dt.availR[p]]; ab -= POW5[dt.availB[q]];
        }
        total += sR - sB;
    }
    return total / games;
}

// full-hand stage dominance sweep over all (r,b) value pairs
static void dominanceSweep(const Config& cfg) {
    int pairsChecked = 0, domCount = 0;
    for (int r = 1; r <= cfg.N; r++)
        for (int b = 1; b <= cfg.N; b++) {
            pairsChecked++;
            double row[3][3];
            for (int aR = 0; aR < 3; aR++)
                for (int aB = 0; aB < 3; aB++) {
                    double sR, sB;
                    resolve(r, b, aR, aB, sR, sB);
                    row[aR][aB] = sR - sB;
                }
            for (int a1 = 0; a1 < 3; a1++)
                for (int a2 = 0; a2 < 3; a2++) {
                    if (a1 == a2) continue;
                    bool dom = true;
                    for (int aB = 0; aB < 3; aB++)
                        if (row[a1][aB] < row[a2][aB] - 1e-12) dom = false;
                    if (dom) domCount++;
                }
        }
    printf("\"dominance\": {\"rb_pairs\": %d, \"weakly_dominated_action_pairs\": %d},\n",
           pairsChecked, domCount);
}

static void pct(vector<double>& v, double& mean, double& med, double& p90,
                double& fracFlat, double& fracDecisive) {
    if (v.empty()) { mean = med = p90 = fracFlat = fracDecisive = 0; return; }
    sort(v.begin(), v.end());
    double s = 0; for (double x : v) s += x;
    mean = s / v.size();
    med = v[v.size() / 2];
    p90 = v[(size_t)(0.9 * (v.size() - 1))];
    long long fl = 0, dc = 0;
    for (double x : v) { if (x < 0.05) fl++; if (x > 0.5) dc++; }
    fracFlat = (double)fl / v.size(); fracDecisive = (double)dc / v.size();
}

static void printSims(const Layers& L, int games, int cmpGames) {
    Stats st;
    runSims(L, games, cmpGames, st, 20260810ULL);
    double G = (double)st.games;
    printf("\"sim_games\": %lld,\n", st.games);
    printf("\"action_use_per_game\": {\"Hold\": %.3f, \"Steal\": %.3f, \"Trap\": %.3f},\n",
           st.actUse[0] / G, st.actUse[1] / G, st.actUse[2] / G);
    printf("\"tie_rate\": %.8f, \"comeback_rate\": %.8f,\n",
           st.ties / G, st.comebacks / max((double)st.decided, 1.0));
    double m, md, p90, fl, dc;
    pct(st.numGaps, m, md, p90, fl, dc);
    printf("\"num_gap\": {\"mean\": %.3f, \"median\": %.3f, \"p90\": %.3f, "
           "\"frac_flat\": %.3f, \"frac_decisive\": %.3f},\n", m, md, p90, fl, dc);
    pct(st.actGaps, m, md, p90, fl, dc);
    printf("\"act_gap\": {\"mean\": %.3f, \"median\": %.3f, \"p90\": %.3f, "
           "\"frac_flat\": %.3f, \"frac_decisive\": %.3f},\n", m, md, p90, fl, dc);
    double sn = 0, sa = 0;
    for (int x : st.numSupport) sn += x;
    for (int x : st.actSupport) sa += x;
    printf("\"num_support_mean\": %.3f, \"act_support_mean\": %.3f,\n",
           sn / max((double)st.numSupport.size(), 1.0),
           sa / max((double)st.actSupport.size(), 1.0));
    printf("\"round_num_gap_mean\": [");
    for (int t = 0; t < L.cfg.T; t++) printf("%s%.3f", t ? "," : "", st.rGapNum[t] / max((double)st.rCnt[t], 1.0));
    printf("],\n\"round_act_gap_mean\": [");
    for (int t = 0; t < L.cfg.T; t++) printf("%s%.3f", t ? "," : "", st.rGapAct[t] / max((double)st.rCnt[t], 1.0));
    printf("],\n\"round_num_support_mean\": [");
    for (int t = 0; t < L.cfg.T; t++) printf("%s%.3f", t ? "," : "", st.rSupNum[t] / max((double)st.rCnt[t], 1.0));
    printf("],\n\"round_act_support_mean\": [");
    for (int t = 0; t < L.cfg.T; t++) printf("%s%.3f", t ? "," : "", st.rSupAct[t] / max((double)st.rCnt[t], 1.0));
    printf("],\n\"round_act_forced_frac\": [");
    for (int t = 0; t < L.cfg.T; t++) printf("%s%.3f", t ? "," : "", st.rForcedAct[t] / max((double)st.rCnt[t], 1.0));
    printf("],\n");
    if (st.cmpNum) {
        printf("\"myopic_flip_num\": %.8f, \"myopic_flip_act\": %.8f, "
               "\"myopic_regret_abs_err_num\": %.8f, \"myopic_regret_abs_err_act\": %.8f,\n",
               (double)st.cmpNumFlip / st.cmpNum, (double)st.cmpActFlip / st.cmpAct,
               st.cmpNumAbs / max((double)st.cmpNumOpts, 1.0),
               st.cmpActAbs / max((double)st.cmpActOpts, 1.0));
    }
    double h2h = myopicVsEq(L, 4000, 777ULL);
    printf("\"myopic_vs_eq_meandiff\": %.8f,\n", h2h);
}

static void printOpening(const Layers& L) {
    Details det;
    stateValue(L, 0, L.rootMask(), L.rootMask(), L.rootAct(), L.rootAct(), &det, &L.V);
    printf("\"opening_number_mix\": [");
    for (int i = 0; i < det.cR; i++) printf("%s%.8f", i ? "," : "", det.nsol.x[i]);
    printf("],\n");
    int iHi = det.cR - 1, jLo = 0;   // r = N vs b = 1
    printf("\"opening_actmix_rN_vs_b1\": {\"value\": %.8f, \"red\": [%.8f,%.8f,%.8f], \"blue\": [%.8f,%.8f,%.8f]},\n",
           det.asol[iHi][jLo].v,
           det.asol[iHi][jLo].x[0], det.asol[iHi][jLo].x[1], det.asol[iHi][jLo].x[2],
           det.asol[iHi][jLo].y[0], det.asol[iHi][jLo].y[1], det.asol[iHi][jLo].y[2]);
}

// ---------------------------------------------------- long-horizon metrics
static void saveTable(const Layers& L, const vector<vector<double>>& V, const char* path) {
    FILE* f = fopen(path, "wb");
    int hdr[3] = {L.cfg.N, L.cfg.T, L.totAct};
    fwrite(hdr, sizeof(int), 3, f);
    for (int k = 0; k < L.cfg.T; k++)
        fwrite(V[k].data(), sizeof(double), L.layerSize[k], f);
    fclose(f);
}
static bool loadTable(const Layers& L, vector<vector<double>>& V, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    int hdr[3];
    if (fread(hdr, sizeof(int), 3, f) != 3 || hdr[0] != L.cfg.N || hdr[1] != L.cfg.T) { fclose(f); return false; }
    V.assign(L.cfg.T, {});
    for (int k = 0; k < L.cfg.T; k++) {
        V[k].assign(L.layerSize[k], 0.0);
        if (fread(V[k].data(), sizeof(double), L.layerSize[k], f) != L.layerSize[k]) { fclose(f); return false; }
    }
    fclose(f);
    return true;
}

// compute the depth-d value table: V_d(k) solved with continuation V_{d-1}(k+1)
static void computeDepthTable(const Layers& L, const vector<vector<double>>* prev,
                              vector<vector<double>>& out) {
    out.assign(L.cfg.T, {});
    for (int k = L.cfg.T - 1; k >= 0; k--) {
        out[k].assign(L.layerSize[k], 0.0);
        for (size_t si = 0; si < L.layerSize[k]; si++) {
            int mr, mb, ar, ab;
            L.decode(k, si, mr, mb, ar, ab);
            out[k][si] = stateValue(L, k, mr, mb, ar, ab, nullptr, prev);
        }
        fprintf(stderr, "  depth table layer k=%d done\n", k);
    }
}

// full-hand stage-structure sweep: pure saddle points and equilibrium support
static void stagePass(const Config& cfg) {
    int saddle = 0, sup3 = 0, sup2 = 0, sup1 = 0;
    for (int r = 1; r <= cfg.N; r++)
        for (int b = 1; b <= cfg.N; b++) {
            double A[MAXD][MAXD];
            for (int aR = 0; aR < 3; aR++)
                for (int aB = 0; aB < 3; aB++) {
                    double sR, sB; resolve(r, b, aR, aB, sR, sB);
                    A[aR][aB] = sR - sB;
                }
            double maxmin = -1e300, minmax = 1e300;
            for (int i = 0; i < 3; i++) {
                double mn = 1e300;
                for (int j = 0; j < 3; j++) mn = min(mn, A[i][j]);
                maxmin = max(maxmin, mn);
            }
            for (int j = 0; j < 3; j++) {
                double mx = -1e300;
                for (int i = 0; i < 3; i++) mx = max(mx, A[i][j]);
                minmax = min(minmax, mx);
            }
            if (fabs(maxmin - minmax) < 1e-9) saddle++;
            Sol s; solveGame(3, 3, A, s);
            int supx = 0, supy = 0;
            for (int i = 0; i < 3; i++) { if (s.x[i] > 1e-6) supx++; if (s.y[i] > 1e-6) supy++; }
            int sup = min(supx, supy);
            if (supx == 3 && supy == 3) sup3++;
            else if (sup >= 2) sup2++;
            else sup1++;
        }
    printf("{\"stage_rb_pairs\": %d, \"pure_saddle_states\": %d, "
           "\"full_support_states\": %d, \"support2_states\": %d, \"support1_states\": %d}\n",
           cfg.N * cfg.N, saddle, sup3, sup2, sup1);
}

// horizon-regret pass: how does a depth-d recommendation price under the full oracle?
static void hrPass(const Layers& L, int d, const vector<vector<double>>* prev,
                   int games, uint64_t seed) {
    mt19937_64 rng(seed);
    long long nNum = 0, nAct = 0, flipNum = 0, flipAct = 0;
    double pickRegNum = 0, pickRegAct = 0, mixRegNum = 0, mixRegAct = 0;
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        for (int t = 0; t < L.cfg.T; t++) {
            Details df, dd;
            stateValue(L, t, mr, mb, ar, ab, &df, &L.V);
            stateValue(L, t, mr, mb, ar, ab, &dd, prev);
            // number phase, Red side
            double evF[MAXD];
            for (int i = 0; i < df.cR; i++) {
                double e = 0; for (int j = 0; j < df.cB; j++) e += df.nsol.y[j] * df.MN[i][j];
                evF[i] = e;
            }
            double Vst = df.nsol.v;
            if (df.cR >= 2) {
                int pick = 0; for (int i = 1; i < dd.cR; i++) if (dd.nsol.x[i] > dd.nsol.x[pick]) pick = i;
                nNum++;
                if (evF[pick] < Vst - 1e-6) flipNum++;
                pickRegNum += Vst - evF[pick];
                double mev = 0; for (int i = 0; i < dd.cR; i++) mev += dd.nsol.x[i] * evF[i];
                mixRegNum += Vst - mev;
            }
            // number phase, Blue side (columns)
            if (df.cB >= 2) {
                double evFB[MAXD];
                for (int j = 0; j < df.cB; j++) {
                    double e = 0; for (int i = 0; i < df.cR; i++) e += df.nsol.x[i] * df.MN[i][j];
                    evFB[j] = -e;
                }
                int pick = 0; for (int j = 1; j < dd.cB; j++) if (dd.nsol.y[j] > dd.nsol.y[pick]) pick = j;
                nNum++;
                if (evFB[pick] < -Vst - 1e-6) flipNum++;
                pickRegNum += (-Vst) - evFB[pick];
                double mev = 0; for (int j = 0; j < dd.cB; j++) mev += dd.nsol.y[j] * evFB[j];
                mixRegNum += (-Vst) - mev;
            }
            int i = sampleFrom(df.nsol.x, df.cR, rng);
            int j = sampleFrom(df.nsol.y, df.cB, rng);
            // action phase
            const Sol& asF = df.asol[i][j];
            const Sol& asD = dd.asol[i][j];
            double aevF[3];
            for (int p = 0; p < df.aRn; p++) {
                double e = 0; for (int q = 0; q < df.aBn; q++) e += asF.y[q] * df.MA[i][j][p][q];
                aevF[p] = e;
            }
            double Va = asF.v;
            if (df.aRn >= 2) {
                int pick = 0; for (int p = 1; p < dd.aRn; p++) if (asD.x[p] > asD.x[pick]) pick = p;
                nAct++;
                if (aevF[pick] < Va - 1e-6) flipAct++;
                pickRegAct += Va - aevF[pick];
                double mev = 0; for (int p = 0; p < dd.aRn; p++) mev += asD.x[p] * aevF[p];
                mixRegAct += Va - mev;
            }
            if (df.aBn >= 2) {
                double aevFB[3];
                for (int q = 0; q < df.aBn; q++) {
                    double e = 0; for (int p = 0; p < df.aRn; p++) e += asF.x[p] * df.MA[i][j][p][q];
                    aevFB[q] = -e;
                }
                int pick = 0; for (int q = 1; q < dd.aBn; q++) if (asD.y[q] > asD.y[pick]) pick = q;
                nAct++;
                if (aevFB[pick] < -Va - 1e-6) flipAct++;
                pickRegAct += (-Va) - aevFB[pick];
                double mev = 0; for (int q = 0; q < dd.aBn; q++) mev += asD.y[q] * aevFB[q];
                mixRegAct += (-Va) - mev;
            }
            int p = sampleFrom(asF.x, df.aRn, rng);
            int q = sampleFrom(asF.y, df.aBn, rng);
            int r = df.numsR[i], b = df.numsB[j];
            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[df.availR[p]]; ab -= POW5[df.availB[q]];
        }
    }
    // head-to-head: depth-d policy (Red) vs full equilibrium (Blue)
    mt19937_64 rng2(seed + 1);
    double total = 0;
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        double sR = 0, sB = 0;
        for (int t = 0; t < L.cfg.T; t++) {
            Details dt, dd;
            stateValue(L, t, mr, mb, ar, ab, &dt, &L.V);
            stateValue(L, t, mr, mb, ar, ab, &dd, prev);
            int i = sampleFrom(dd.nsol.x, dd.cR, rng2);
            int j = sampleFrom(dt.nsol.y, dt.cB, rng2);
            int p = sampleFrom(dd.asol[i][j].x, dd.aRn, rng2);
            int q = sampleFrom(dt.asol[i][j].y, dt.aBn, rng2);
            int r = dt.numsR[i], b = dt.numsB[j];
            double dsR, dsB;
            resolve(r, b, dt.availR[p], dt.availB[q], dsR, dsB);
            sR += dsR; sB += dsB;
            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[dt.availR[p]]; ab -= POW5[dt.availB[q]];
        }
        total += sR - sB;
    }
    printf("{\"depth\": %d, \"n_num\": %lld, \"n_act\": %lld,\n", d, nNum, nAct);
    printf("\"flip_num\": %.8f, \"flip_act\": %.8f,\n",
           (double)flipNum / max(nNum, 1LL), (double)flipAct / max(nAct, 1LL));
    printf("\"pick_regret_num\": %.8f, \"pick_regret_act\": %.8f,\n",
           pickRegNum / max(nNum, 1LL), pickRegAct / max(nAct, 1LL));
    printf("\"mix_regret_num\": %.8f, \"mix_regret_act\": %.8f,\n",
           mixRegNum / max(nNum, 1LL), mixRegAct / max(nAct, 1LL));
    printf("\"h2h_vs_full_eq\": %.8f}\n", total / games);
}

// strategic sacrifice, shadow prices, flexibility (all against the full oracle)
static void depthPass(const Layers& L, int games, uint64_t seed) {
    const Config& cfg = L.cfg;
    mt19937_64 rng(seed);
    long long sacN[12] = {0}, sacDen[12] = {0};
    double sacImm = 0, sacGain = 0; long long sacTot = 0, denTot = 0;
    double shadow[3][3][12]; long long shadowN[3][3][12];
    memset(shadow, 0, sizeof(shadow)); memset(shadowN, 0, sizeof(shadowN));
    double flexEq[12] = {0}; long long flexCnt[12] = {0};
    long long monoEq8 = 0, monoEq9 = 0, monoCnt = 0;
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        for (int t = 0; t < cfg.T; t++) {
            Details df;
            stateValue(L, t, mr, mb, ar, ab, &df, &L.V);
            // flexibility (Red side)
            flexEq[t] += df.aRn; flexCnt[t]++;
            if (t == 7 && df.aRn == 1) monoEq8++;
            if (t == 8 && df.aRn == 1) monoEq9++;
            if (t == 7) monoCnt++;
            // shadow prices: exchange one remaining action card (Red side)
            size_t base = L.idx(t, mr, mb, ar, ab);
            for (int l = 0; l < 3; l++) {
                int cl = (ar / POW5[l]) % 5;
                if (cl < 1) continue;
                for (int gA = 0; gA < 3; gA++) {
                    if (gA == l) continue;
                    int cg = (ar / POW5[gA]) % 5;
                    if (cg + 1 > cfg.caps[gA]) continue;
                    int ar2 = ar - POW5[l] + POW5[gA];
                    double lam = L.V[t][L.idx(t, mr, mb, ar2, ab)] - L.V[t][base];
                    shadow[gA][l][t] += lam; shadowN[gA][l][t]++;
                }
            }
            int i = sampleFrom(df.nsol.x, df.cR, rng);
            int j = sampleFrom(df.nsol.y, df.cB, rng);
            // strategic sacrifice at the action phase (Red and Blue sides)
            int r = df.numsR[i], b = df.numsB[j];
            for (int side = 0; side < 2; side++) {
                int an = side == 0 ? df.aRn : df.aBn;
                if (an < 2) continue;
                double u[3], Q[3];
                for (int p = 0; p < an; p++) {
                    double eu = 0, eq = 0;
                    int on = side == 0 ? df.aBn : df.aRn;
                    for (int q = 0; q < on; q++) {
                        double w = side == 0 ? df.asol[i][j].y[q] : df.asol[i][j].x[q];
                        double sR, sB;
                        if (side == 0) resolve(r, b, df.availR[p], df.availB[q], sR, sB);
                        else           resolve(r, b, df.availR[q], df.availB[p], sR, sB);
                        double imm = side == 0 ? (sR - sB) : (sB - sR);
                        double full = side == 0 ? df.MA[i][j][p][q] : -df.MA[i][j][q][p];
                        eu += w * imm; eq += w * full;
                    }
                    u[p] = eu; Q[p] = eq;
                }
                double maxU = -1e300, maxQ = -1e300;
                for (int p = 0; p < an; p++) { maxU = max(maxU, u[p]); maxQ = max(maxQ, Q[p]); }
                bool inter = false;
                for (int p = 0; p < an; p++)
                    if (Q[p] > maxQ - 1e-6 && u[p] > maxU - 1e-6) inter = true;
                denTot++; sacDen[t]++;
                if (!inter) {
                    sacTot++; sacN[t]++;
                    double uBestQ = -1e300, qBestU = -1e300;
                    for (int p = 0; p < an; p++) {
                        if (Q[p] > maxQ - 1e-6) uBestQ = max(uBestQ, u[p]);
                        if (u[p] > maxU - 1e-6) qBestU = max(qBestU, Q[p]);
                    }
                    sacImm += maxU - uBestQ;
                    sacGain += maxQ - qBestU;
                }
            }
            int p = sampleFrom(df.asol[i][j].x, df.aRn, rng);
            int q = sampleFrom(df.asol[i][j].y, df.aBn, rng);
            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[df.availR[p]]; ab -= POW5[df.availB[q]];
        }
    }
    // both-myopic flexibility
    double flexMy[12] = {0}; long long flexMyCnt[12] = {0};
    long long monoMy8 = 0, monoMy9 = 0, monoMyCnt = 0;
    mt19937_64 rng2(seed + 7);
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        for (int t = 0; t < cfg.T; t++) {
            Details dm;
            stateValue(L, t, mr, mb, ar, ab, &dm, nullptr);
            flexMy[t] += dm.aRn; flexMyCnt[t]++;
            if (t == 7 && dm.aRn == 1) monoMy8++;
            if (t == 8 && dm.aRn == 1) monoMy9++;
            if (t == 7) monoMyCnt++;
            int i = sampleFrom(dm.nsol.x, dm.cR, rng2);
            int j = sampleFrom(dm.nsol.y, dm.cB, rng2);
            int p = sampleFrom(dm.asol[i][j].x, dm.aRn, rng2);
            int q = sampleFrom(dm.asol[i][j].y, dm.aBn, rng2);
            int r = dm.numsR[i], b = dm.numsB[j];
            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[dm.availR[p]]; ab -= POW5[dm.availB[q]];
        }
    }
    printf("{\"ssr_overall\": %.8f,\n", (double)sacTot / max(denTot, 1LL));
    printf("\"ssr_by_round\": [");
    for (int t = 0; t < cfg.T; t++) printf("%s%.8f", t ? "," : "", (double)sacN[t] / max(sacDen[t], 1LL));
    printf("],\n");
    printf("\"sacrifice_mean_immediate_cost\": %.8f, \"sacrifice_mean_final_gain\": %.8f,\n",
           sacImm / max(sacTot, 1LL), sacGain / max(sacTot, 1LL));
    printf("\"flex_eq_by_round\": [");
    for (int t = 0; t < cfg.T; t++) printf("%s%.3f", t ? "," : "", flexEq[t] / max(flexCnt[t], 1LL));
    printf("],\n\"flex_myopic_by_round\": [");
    for (int t = 0; t < cfg.T; t++) printf("%s%.3f", t ? "," : "", flexMy[t] / max(flexMyCnt[t], 1LL));
    printf("],\n");
    printf("\"mono_type_round8\": {\"eq\": %.8f, \"myopic\": %.8f, \"random_exact\": %.8f},\n",
           (double)monoEq8 / max(monoCnt, 1LL), (double)monoMy8 / max(monoMyCnt, 1LL), 6.0 / 120.0);
    printf("\"mono_type_round9\": {\"eq\": %.8f, \"myopic\": %.8f, \"random_exact\": %.8f},\n",
           (double)monoEq9 / max(monoCnt, 1LL), (double)monoMy9 / max(monoMyCnt, 1LL), 12.0 / 45.0);
    printf("\"shadow_gain_lose_by_round\": {");
    const char* AN[3] = {"H", "S", "T"};
    bool first = true;
    for (int gA = 0; gA < 3; gA++)
        for (int l = 0; l < 3; l++) {
            if (gA == l) continue;
            printf("%s\"%s_for_%s\": [", first ? "" : ", ", AN[gA], AN[l]);
            first = false;
            for (int t = 0; t < cfg.T; t++)
                printf("%s%.3f", t ? "," : "", shadowN[gA][l][t] ? shadow[gA][l][t] / shadowN[gA][l][t] : 0.0);
            printf("]");
        }
    printf("}}\n");
}

// exact best response against the depth-d policy
static double bestResponsePol(const Layers& L, const vector<vector<double>>* polCont) {
    const Config& cfg = L.cfg;
    vector<vector<double>> U(cfg.T);
    for (int k = cfg.T - 1; k >= 0; k--) {
        U[k].assign(L.layerSize[k], 0.0);
        for (size_t si = 0; si < L.layerSize[k]; si++) {
            int mr, mb, ar, ab;
            L.decode(k, si, mr, mb, ar, ab);
            Details det;
            stateValue(L, k, mr, mb, ar, ab, &det, polCont);
            double best = -1e300;
            for (int i = 0; i < det.cR; i++) {
                double acc = 0;
                for (int j = 0; j < det.cB; j++) {
                    double yb = det.nsol.y[j];
                    if (yb < 1e-15) continue;
                    int r = det.numsR[i], b = det.numsB[j];
                    double binner = -1e300;
                    for (int p = 0; p < det.aRn; p++) {
                        double e = 0;
                        for (int q = 0; q < det.aBn; q++) {
                            double yq = det.asol[i][j].y[q];
                            if (yq < 1e-15) continue;
                            double sR, sB;
                            resolve(r, b, det.availR[p], det.availB[q], sR, sB);
                            double cont = 0;
                            if (k + 1 < cfg.T) {
                                int mr2 = mr & ~(1 << (r - 1)), mb2 = mb & ~(1 << (b - 1));
                                int ar2 = ar - POW5[det.availR[p]], ab2 = ab - POW5[det.availB[q]];
                                cont = U[k + 1][L.idx(k + 1, mr2, mb2, ar2, ab2)];
                            }
                            e += yq * ((sR - sB) + cont);
                        }
                        binner = max(binner, e);
                    }
                    acc += yb * binner;
                }
                best = max(best, acc);
            }
            U[k][si] = best;
        }
        fprintf(stderr, "  BRpol layer k=%d done\n", k);
    }
    return U[0][L.idx(0, L.rootMask(), L.rootMask(), L.rootAct(), L.rootAct())];
}

// ------------------------------------------- dataset artifact generation
static void writeMask(FILE* f, int mask, int N) {
    bool first = true;
    for (int v = 0; v < N; v++) if ((mask >> v) & 1) { fprintf(f, "%s%d", first ? "" : "|", v + 1); first = false; }
}
static void batteryPass(const Layers& L, const vector<vector<double>>* V1,
                        int games, int maxRows, const char* path, uint64_t seed) {
    const Config& cfg = L.cfg;
    FILE* f = fopen(path, "w");
    fprintf(f, "item_id,round,phase,red_numbers,blue_numbers,red_H,red_S,red_T,blue_H,blue_S,blue_T,"
               "revealed_red,revealed_blue,options,option_evs,eq_mix,eq_value,gap,band,support,"
               "depth1_sufficient,depth2_sufficient\n");
    mt19937_64 rng(seed);
    unordered_set<unsigned long long> seen;
    int rows = 0;
    const char* AN[3] = {"H", "S", "T"};
    auto band = [](double g) { return g < 0.05 ? "flat" : (g > 0.5 ? "decisive" : "intermediate"); };
    for (int g = 0; g < games && rows < maxRows; g++) {
        int mr = L.rootMask(), mb = L.rootMask();
        int ar = L.rootAct(), ab = L.rootAct();
        for (int t = 0; t < cfg.T && rows < maxRows; t++) {
            size_t si = L.idx(t, mr, mb, ar, ab);
            Details df, d1, d2;
            stateValue(L, t, mr, mb, ar, ab, &df, &L.V);
            stateValue(L, t, mr, mb, ar, ab, &d1, nullptr);
            if (V1) stateValue(L, t, mr, mb, ar, ab, &d2, V1);
            // number-phase item (Red seat)
            unsigned long long key = (((unsigned long long)t * 16777216ULL + si) * 2ULL) * 256ULL;
            if (df.cR >= 2 && seen.insert(key).second && rows < maxRows) {
                double evF[MAXD], best = -1e300;
                for (int i = 0; i < df.cR; i++) {
                    double e = 0; for (int j = 0; j < df.cB; j++) e += df.nsol.y[j] * df.MN[i][j];
                    evF[i] = e; best = max(best, e);
                }
                double gap; int sup;
                gapSupport(evF, df.cR, df.nsol.x, gap, sup);
                int p1 = 0; for (int i = 1; i < d1.cR; i++) if (d1.nsol.x[i] > d1.nsol.x[p1]) p1 = i;
                int p2 = 0; if (V1) for (int i = 1; i < d2.cR; i++) if (d2.nsol.x[i] > d2.nsol.x[p2]) p2 = i;
                fprintf(f, "FF-%04d,%d,number,", rows + 1, t + 1);
                writeMask(f, mr, cfg.N); fprintf(f, ",");
                writeMask(f, mb, cfg.N); fprintf(f, ",");
                fprintf(f, "%d,%d,%d,%d,%d,%d,,,", (ar) % 5, (ar / 5) % 5, (ar / 25) % 5,
                        (ab) % 5, (ab / 5) % 5, (ab / 25) % 5);
                for (int i = 0; i < df.cR; i++) fprintf(f, "%s%d", i ? "|" : "", df.numsR[i]);
                fprintf(f, ",");
                for (int i = 0; i < df.cR; i++) fprintf(f, "%s%.8f", i ? "|" : "", evF[i]);
                fprintf(f, ",");
                for (int i = 0; i < df.cR; i++) fprintf(f, "%s%.8f", i ? "|" : "", df.nsol.x[i]);
                fprintf(f, ",%.8f,%.8f,%s,%d,%d,%d\n", df.nsol.v, gap, band(gap), sup,
                        evF[p1] >= best - 1e-6 ? 1 : 0, V1 ? (evF[p2] >= best - 1e-6 ? 1 : 0) : -1);
                rows++;
            }
            int i = sampleFrom(df.nsol.x, df.cR, rng);
            int j = sampleFrom(df.nsol.y, df.cB, rng);
            // action-phase item (Red seat)
            unsigned long long key2 = (((unsigned long long)t * 16777216ULL + si) * 2ULL + 1ULL) * 256ULL
                                      + (unsigned long long)(i * 16 + j);
            if (df.aRn >= 2 && seen.insert(key2).second && rows < maxRows) {
                const Sol& as = df.asol[i][j];
                double aevF[3], best = -1e300;
                for (int p = 0; p < df.aRn; p++) {
                    double e = 0; for (int q = 0; q < df.aBn; q++) e += as.y[q] * df.MA[i][j][p][q];
                    aevF[p] = e; best = max(best, e);
                }
                double gap; int sup;
                gapSupport(aevF, df.aRn, as.x, gap, sup);
                int p1 = 0; for (int p = 1; p < d1.aRn; p++) if (d1.asol[i][j].x[p] > d1.asol[i][j].x[p1]) p1 = p;
                int p2 = 0; if (V1) for (int p = 1; p < d2.aRn; p++) if (d2.asol[i][j].x[p] > d2.asol[i][j].x[p2]) p2 = p;
                fprintf(f, "FF-%04d,%d,action,", rows + 1, t + 1);
                writeMask(f, mr, cfg.N); fprintf(f, ",");
                writeMask(f, mb, cfg.N); fprintf(f, ",");
                fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d,", (ar) % 5, (ar / 5) % 5, (ar / 25) % 5,
                        (ab) % 5, (ab / 5) % 5, (ab / 25) % 5, df.numsR[i], df.numsB[j]);
                for (int p = 0; p < df.aRn; p++) fprintf(f, "%s%s", p ? "|" : "", AN[df.availR[p]]);
                fprintf(f, ",");
                for (int p = 0; p < df.aRn; p++) fprintf(f, "%s%.8f", p ? "|" : "", aevF[p]);
                fprintf(f, ",");
                for (int p = 0; p < df.aRn; p++) fprintf(f, "%s%.8f", p ? "|" : "", as.x[p]);
                fprintf(f, ",%.8f,%.8f,%s,%d,%d,%d\n", as.v, gap, band(gap), sup,
                        aevF[p1] >= best - 1e-6 ? 1 : 0, V1 ? (aevF[p2] >= best - 1e-6 ? 1 : 0) : -1);
                rows++;
            }
            int p = sampleFrom(df.asol[i][j].x, df.aRn, rng);
            int q = sampleFrom(df.asol[i][j].y, df.aBn, rng);
            int r = df.numsR[i], b = df.numsB[j];
            mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
            ar -= POW5[df.availR[p]]; ab -= POW5[df.availB[q]];
        }
    }
    fclose(f);
    fprintf(stderr, "battery: %d rows\n", rows);
}

static void episodePass(const Layers& L, const char* path, uint64_t seed) {
    const Config& cfg = L.cfg;
    FILE* f = fopen(path, "w");
    mt19937_64 rng(seed);
    int mr = L.rootMask(), mb = L.rootMask();
    int ar = L.rootAct(), ab = L.rootAct();
    double sR = 0, sB = 0;
    const char* AN[3] = {"Hold", "Steal", "Trap"};
    fprintf(f, "{\n\"game\": \"Fortune Fight (simple, canonical)\",\n\"seed\": %llu,\n"
               "\"players\": [\"Red\", \"Blue\"], \"note\": \"Both players sample from the exact "
               "full-horizon equilibrium. continuation_value is the expected score difference "
               "(Red minus Blue) of the REMAINING game from the start of the round, excluding "
               "points already scored. subgame_value is the same quantity evaluated after both "
               "numbers are revealed, including the current round's scoring. Exact oracle outputs, "
               "printed to 8 decimal places.\",\n\"rounds\": [\n", (unsigned long long)seed);
    for (int t = 0; t < cfg.T; t++) {
        Details df;
        stateValue(L, t, mr, mb, ar, ab, &df, &L.V);
        int i = sampleFrom(df.nsol.x, df.cR, rng);
        int j = sampleFrom(df.nsol.y, df.cB, rng);
        const Sol& as = df.asol[i][j];
        int p = sampleFrom(as.x, df.aRn, rng);
        int q = sampleFrom(as.y, df.aBn, rng);
        int r = df.numsR[i], b = df.numsB[j];
        double dsR, dsB;
        resolve(r, b, df.availR[p], df.availB[q], dsR, dsB);
        fprintf(f, "  {\"round\": %d, \"continuation_value\": %.8f,\n   \"red_numbers\": [", t + 1, df.nsol.v);
        for (int v = 0; v < df.cR; v++) fprintf(f, "%s%d", v ? "," : "", df.numsR[v]);
        fprintf(f, "], \"blue_numbers\": [");
        for (int v = 0; v < df.cB; v++) fprintf(f, "%s%d", v ? "," : "", df.numsB[v]);
        fprintf(f, "],\n   \"red_actions\": {\"H\": %d, \"S\": %d, \"T\": %d}, "
                   "\"blue_actions\": {\"H\": %d, \"S\": %d, \"T\": %d},\n",
                (ar) % 5, (ar / 5) % 5, (ar / 25) % 5, (ab) % 5, (ab / 5) % 5, (ab / 25) % 5);
        fprintf(f, "   \"number_phase\": {\"red_mix\": [");
        for (int v = 0; v < df.cR; v++) fprintf(f, "%s%.8f", v ? "," : "", df.nsol.x[v]);
        fprintf(f, "], \"blue_mix\": [");
        for (int v = 0; v < df.cB; v++) fprintf(f, "%s%.8f", v ? "," : "", df.nsol.y[v]);
        fprintf(f, "], \"red_played\": %d, \"blue_played\": %d},\n", r, b);
        fprintf(f, "   \"action_phase\": {\"subgame_value\": %.8f, \"red_mix\": {", as.v);
        for (int v = 0; v < df.aRn; v++) fprintf(f, "%s\"%s\": %.8f", v ? ", " : "", AN[df.availR[v]], as.x[v]);
        fprintf(f, "}, \"blue_mix\": {");
        for (int v = 0; v < df.aBn; v++) fprintf(f, "%s\"%s\": %.8f", v ? ", " : "", AN[df.availB[v]], as.y[v]);
        fprintf(f, "}, \"red_played\": \"%s\", \"blue_played\": \"%s\"},\n", AN[df.availR[p]], AN[df.availB[q]]);
        sR += dsR; sB += dsB;
        fprintf(f, "   \"round_scores\": {\"red\": %.0f, \"blue\": %.0f}, "
                   "\"cumulative\": {\"red\": %.0f, \"blue\": %.0f}}%s\n",
                dsR, dsB, sR, sB, t + 1 < cfg.T ? "," : "");
        mr &= ~(1 << (r - 1)); mb &= ~(1 << (b - 1));
        ar -= POW5[df.availR[p]]; ab -= POW5[df.availB[q]];
    }
    fprintf(f, "],\n\"final\": {\"red\": %.0f, \"blue\": %.0f, \"difference\": %.0f}\n}\n", sR, sB, sR - sB);
    fclose(f);
}

// --------------------------------------------------------------------- main
static void expectNear(double a, double b, double tol, const char* msg) {
    if (fabs(a - b) > tol) { fprintf(stderr, "TEST FAIL %s: %f vs %f\n", msg, a, b); exit(1); }
}
static void runTests() {
    { double A[MAXD][MAXD] = {}; A[0][0] = 1; A[0][1] = -1; A[1][0] = -1; A[1][1] = 1;
      Sol s; solveGame(2, 2, A, s);
      expectNear(s.v, 0, 1e-9, "MP value"); expectNear(s.x[0], 0.5, 1e-9, "MP x"); }
    { double A[MAXD][MAXD] = {}; A[0][1] = -1; A[0][2] = 1; A[1][0] = 1; A[1][2] = -1; A[2][0] = -1; A[2][1] = 1;
      Sol s; solveGame(3, 3, A, s);
      expectNear(s.v, 0, 1e-9, "RPS value");
      for (int i = 0; i < 3; i++) expectNear(s.x[i], 1.0 / 3, 1e-9, "RPS mix"); }
    { double A[MAXD][MAXD] = {}; A[0][0] = 1; A[0][1] = 2; A[1][0] = 3; A[1][1] = 4;
      Sol s; solveGame(2, 2, A, s);
      expectNear(s.v, 3, 1e-9, "saddle"); }
    { mt19937_64 rng(42);
      for (int t = 0; t < 20000; t++) {
          int m = 2 + rng() % 9, n = 2 + rng() % 9;
          double A[MAXD][MAXD];
          for (int i = 0; i < m; i++) for (int j = 0; j < n; j++)
              A[i][j] = ((double)(rng() % 2001) - 1000) / 100.0;
          Sol s; solveGame(m, n, A, s);
      }
      if (BR_VIOL_COUNT > 0) { fprintf(stderr, "TEST FAIL random BR: %lld\n", BR_VIOL_COUNT); exit(1); } }
    fprintf(stderr, "All LP tests passed (%lld solves, 0 BR violations).\n", LP_COUNT);
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s test|mini|solve|sims|brU|brM|brE|stage|vd <d>|hr <d>|depth|brD <d>|battery|episode\n", argv[0]); return 1; }
    string cmd = argv[1];

    Config mini; mini.name = "simple_mini"; mini.N = 6; mini.T = 6;
    mini.caps[0] = 2; mini.caps[1] = 2; mini.caps[2] = 2;

    Config full; full.name = "simple_full"; full.N = 10; full.T = 10;
    full.caps[0] = 4; full.caps[1] = 3; full.caps[2] = 3;

    const char* VPATH = "./simpleV.bin";

    if (cmd == "test") { runTests(); return 0; }

    if (cmd == "mini") {
        Layers L; L.build(mini);
        size_t tot = 0; for (auto s : L.layerSize) tot += s;
        LP_COUNT = 0; BR_VIOL_COUNT = 0; BR_VIOL_MAX = 0;
        auto t0 = chrono::steady_clock::now();
        solveAll(L);
        double sec = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        double rootV, maxA; validate(L, rootV, maxA);
        printf("{\n\"config\": \"%s\", \"N\": %d, \"T\": %d, \"caps\": [%d,%d,%d],\n",
               mini.name.c_str(), mini.N, mini.T, mini.caps[0], mini.caps[1], mini.caps[2]);
        printf("\"states\": %zu, \"solve_seconds\": %.2f, \"solve_LPs\": %lld,\n", tot, sec, LP_COUNT);
        printf("\"root_value\": %.3e, \"max_antisymmetry_violation\": %.3e, \"LP_BR_violations\": %lld,\n",
               rootV, maxA, BR_VIOL_COUNT);
        dominanceSweep(mini);
        printOpening(L);
        printSims(L, 20000, 3000);
        double exU = bestResponse(L, 0), exM = bestResponse(L, 1), exE = bestResponse(L, 2);
        printf("\"exploitability_uniform\": %.8f, \"exploitability_myopic\": %.8f, "
               "\"exploitability_true_eq\": %.3e,\n\"done\": true\n}\n", exU, exM, exE);
        return 0;
    }

    Layers L; L.build(full);
    size_t tot = 0; for (auto s : L.layerSize) tot += s;

    if (cmd == "stage") { stagePass(full); return 0; }

    if (cmd == "vd") {
        int d = atoi(argv[2]);
        vector<vector<double>> prev, out;
        const vector<vector<double>>* pp = nullptr;
        char pth[128];
        if (d > 1) {
            snprintf(pth, 128, "./simpleV_d%d.bin", d - 1);
            if (!loadTable(L, prev, pth)) { fprintf(stderr, "missing %s\n", pth); return 1; }
            pp = &prev;
        }
        LP_COUNT = 0; BR_VIOL_COUNT = 0;
        auto t0 = chrono::steady_clock::now();
        computeDepthTable(L, pp, out);
        double sec = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        snprintf(pth, 128, "./simpleV_d%d.bin", d);
        saveTable(L, out, pth);
        printf("{\"depth_table\": %d, \"seconds\": %.1f, \"LPs\": %lld, \"BR_violations\": %lld}\n",
               d, sec, LP_COUNT, BR_VIOL_COUNT);
        return 0;
    }

    if (cmd == "brD") {
        int d = atoi(argv[2]);
        vector<vector<double>> prev;
        const vector<vector<double>>* pp = nullptr;
        if (d > 1) {
            char pth[128]; snprintf(pth, 128, "./simpleV_d%d.bin", d - 1);
            if (!loadTable(L, prev, pth)) { fprintf(stderr, "missing %s\n", pth); return 1; }
            pp = &prev;
        }
        double e = bestResponsePol(L, pp);
        printf("{\"exploitability_depth%d\": %.8f}\n", d, e);
        return 0;
    }

    if (cmd == "solve") {
        LP_COUNT = 0; BR_VIOL_COUNT = 0; BR_VIOL_MAX = 0;
        auto t0 = chrono::steady_clock::now();
        solveAll(L);
        double sec = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        saveV(L, VPATH);
        double rootV, maxA; validate(L, rootV, maxA);
        printf("{\n\"config\": \"%s\", \"N\": %d, \"T\": %d, \"caps\": [%d,%d,%d],\n",
               full.name.c_str(), full.N, full.T, full.caps[0], full.caps[1], full.caps[2]);
        printf("\"states\": %zu, \"solve_seconds\": %.2f, \"solve_LPs\": %lld, \"LPs_per_sec\": %.0f,\n",
               tot, sec, LP_COUNT, LP_COUNT / max(sec, 1e-9));
        printf("\"root_value\": %.3e, \"max_antisymmetry_violation\": %.3e, "
               "\"LP_BR_violations\": %lld, \"LP_BR_viol_max\": %.3e,\n",
               rootV, maxA, BR_VIOL_COUNT, BR_VIOL_MAX);
        dominanceSweep(full);
        printOpening(L);
        printf("\"done\": true\n}\n");
        return 0;
    }

    if (!loadV(L, VPATH)) { fprintf(stderr, "run solve first (missing %s)\n", VPATH); return 1; }

    if (cmd == "battery") {
        vector<vector<double>> V1;
        const vector<vector<double>>* pp = nullptr;
        if (loadTable(L, V1, "./simpleV_d1.bin")) pp = &V1;
        batteryPass(L, pp, 3000, 500, "./sample_battery.csv", 20260812ULL);
    } else if (cmd == "episode") {
        episodePass(L, "./example_episode.json", 42ULL);
        printf("episode written\n");
    } else if (cmd == "hr") {
        int d = atoi(argv[2]);
        vector<vector<double>> prev;
        const vector<vector<double>>* pp = nullptr;
        if (d > 1) {
            char pth[128]; snprintf(pth, 128, "./simpleV_d%d.bin", d - 1);
            if (!loadTable(L, prev, pth)) { fprintf(stderr, "missing %s\n", pth); return 1; }
            pp = &prev;
        }
        hrPass(L, d, pp, 3000, 20260810ULL);
    } else if (cmd == "depth") {
        depthPass(L, 4000, 20260810ULL);
    } else if (cmd == "sims") {
        printf("{\n\"config\": \"simple_full_sims\",\n");
        printSims(L, 20000, 3000);
        printf("\"done\": true\n}\n");
    } else if (cmd == "brU") {
        double e = bestResponse(L, 0);
        printf("{\"exploitability_uniform\": %.8f}\n", e);
    } else if (cmd == "brM") {
        double e = bestResponse(L, 1);
        printf("{\"exploitability_myopic\": %.8f}\n", e);
    } else if (cmd == "brE") {
        double e = bestResponse(L, 2);
        printf("{\"exploitability_true_eq\": %.3e}\n", e);
    } else { fprintf(stderr, "unknown command\n"); return 1; }
    return 0;
}
