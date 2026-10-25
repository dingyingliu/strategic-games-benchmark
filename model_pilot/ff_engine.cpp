// Fortune Fight game program for the model pilot.
// Uses Dingying Liu's exact solver (simple_oracle.cpp, unchanged) for the opponent and for scoring.
// The player is Red. Blue samples every decision from the exact full-horizon equilibrium.
// Every Red decision is logged at once (with the exact value of each option and the regret of the choice)
// to a private log the player never sees. The player only sees public game information.
#define main oracle_main_unused
#include "/home/claude/strategic-games-benchmark/simple_oracle.cpp"
#undef main
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const char* VFILE = "/home/claude/ffpilot/work/simpleV.bin";
static const char* PRIV  = "/home/claude/ffpilot/private";
static const char* ANAME[3] = {"Hold", "Steal", "Trap"};

struct G {
    int k = 0, phase = 0;              // phase 0 = number, 1 = action
    int mr = 0, mb = 0, ar = 0, ab = 0;
    int revR = 0, revB = 0;
    int scoreR = 0, scoreB = 0;
    unsigned long long seed = 0;
    int dec = 0, over = 0;
    int lastR = 0, lastB = 0, lastAR = -1, lastAB = -1, lastPR = 0, lastPB = 0;
};

static string dirOf(const string& id) { return string(PRIV) + "/" + id; }

static bool loadG(const string& id, G& g) {
    FILE* f = fopen((dirOf(id) + "/state.txt").c_str(), "r");
    if (!f) return false;
    int n = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %llu %d %d %d %d %d %d %d %d",
                   &g.k, &g.phase, &g.mr, &g.mb, &g.ar, &g.ab, &g.revR, &g.revB, &g.scoreR, &g.scoreB,
                   &g.seed, &g.dec, &g.over, &g.lastR, &g.lastB, &g.lastAR, &g.lastAB, &g.lastPR, &g.lastPB);
    fclose(f);
    return n == 19;
}
static void saveG(const string& id, const G& g) {
    string tmp = dirOf(id) + "/state.tmp", fin = dirOf(id) + "/state.txt";
    FILE* f = fopen(tmp.c_str(), "w");
    fprintf(f, "%d %d %d %d %d %d %d %d %d %d %llu %d %d %d %d %d %d %d %d\n",
            g.k, g.phase, g.mr, g.mb, g.ar, g.ab, g.revR, g.revB, g.scoreR, g.scoreB,
            g.seed, g.dec, g.over, g.lastR, g.lastB, g.lastAR, g.lastAB, g.lastPR, g.lastPB);
    fflush(f); fsync(fileno(f)); fclose(f);
    rename(tmp.c_str(), fin.c_str());
}
static void logLine(const string& id, const string& line) {
    FILE* f = fopen((dirOf(id) + "/log.jsonl").c_str(), "a");
    fprintf(f, "%s\n", line.c_str());
    fflush(f); fsync(fileno(f)); fclose(f);
}
static string jarr(const double* v, int n) {
    string s = "["; char b[64];
    for (int i = 0; i < n; i++) { snprintf(b, 64, "%s%.8f", i ? "," : "", v[i]); s += b; }
    return s + "]";
}
static string jiarr(const int* v, int n) {
    string s = "["; char b[32];
    for (int i = 0; i < n; i++) { snprintf(b, 32, "%s%d", i ? "," : "", v[i]); s += b; }
    return s + "]";
}
static void numsOf(int m, int* out, int& c) { c = 0; for (int v = 0; v < 10; v++) if ((m >> v) & 1) out[c++] = v + 1; }
static int cnt(int a, int t) { return (a / POW5[t]) % 5; }

static void printState(const G& g) {
    int nr[10], cr, nb[10], cb; numsOf(g.mr, nr, cr); numsOf(g.mb, nb, cb);
    if (g.k > 0 && g.lastAR >= 0)
        printf("Last round: you played %d and %s, Blue played %d and %s. You scored %d, Blue scored %d.\n",
               g.lastR, ANAME[g.lastAR], g.lastB, ANAME[g.lastAB], g.lastPR, g.lastPB);
    printf("Score: you (Red) %d, Blue %d.\n", g.scoreR, g.scoreB);
    if (g.over) { printf("GAME OVER. Final score: you (Red) %d, Blue %d.\n", g.scoreR, g.scoreB); return; }
    printf("Round %d of 10. %s phase.\n", g.k + 1, g.phase == 0 ? "Number" : "Action");
    printf("Your number cards left: "); for (int i = 0; i < cr; i++) printf("%d ", nr[i]); printf("\n");
    printf("Blue's number cards left: "); for (int i = 0; i < cb; i++) printf("%d ", nb[i]); printf("\n");
    printf("Your action cards left: Hold %d, Steal %d, Trap %d\n", cnt(g.ar, 0), cnt(g.ar, 1), cnt(g.ar, 2));
    printf("Blue's action cards left: Hold %d, Steal %d, Trap %d\n", cnt(g.ab, 0), cnt(g.ab, 1), cnt(g.ab, 2));
    if (g.phase == 1) printf("Revealed this round: your number %d, Blue's number %d.\n", g.revR, g.revB);
    printf(g.phase == 0 ? "Choose one of your number cards.\n" : "Choose Hold, Steal or Trap (from the action cards you have left).\n");
}

// One Red decision at the current state. choice: number (phase 0) or action index (phase 1).
// Returns false if the choice is not legal. forced marks automatic moves.
static bool step(const Layers& L, const string& id, const string& model, G& g, int choice, bool forced, string& note) {
    Details det;
    stateValue(L, g.k, g.mr, g.mb, g.ar, g.ab, &det, &L.V);
    mt19937_64 rng(g.seed * 1000003ULL + (unsigned long long)g.dec * 7919ULL + 17ULL);
    char buf[4096];
    if (g.phase == 0) {
        int i = -1; for (int t = 0; t < det.cR; t++) if (det.numsR[t] == choice) i = t;
        if (i < 0) return false;
        double ev[MAXD], best = -1e18;
        for (int t = 0; t < det.cR; t++) {
            ev[t] = 0; for (int j = 0; j < det.cB; j++) ev[t] += det.nsol.y[j] * det.MN[t][j];
            best = max(best, ev[t]);
        }
        int j = sampleFrom(det.nsol.y, det.cB, rng);
        snprintf(buf, sizeof buf,
            "{\"game\":\"%s\",\"model\":\"%s\",\"decision\":%d,\"round\":%d,\"phase\":\"number\",\"forced\":%s,"
            "\"options\":%s,\"option_evs\":%s,\"red_eq_mix\":%s,\"blue_mix\":%s,\"eq_value\":%.8f,"
            "\"choice\":%d,\"regret\":%.8f,\"blue_choice\":%d,\"score_before\":[%d,%d]}",
            id.c_str(), model.c_str(), g.dec, g.k + 1, forced ? "true" : "false",
            jiarr(det.numsR, det.cR).c_str(), jarr(ev, det.cR).c_str(), jarr(det.nsol.x, det.cR).c_str(),
            jarr(det.nsol.y, det.cB).c_str(), det.nsol.v, choice, best - ev[i], det.numsB[j], g.scoreR, g.scoreB);
        logLine(id, buf);
        g.revR = choice; g.revB = det.numsB[j]; g.phase = 1; g.dec++;
        snprintf(buf, sizeof buf, "You played %d. Blue played %d.\n", g.revR, g.revB); note += buf;
        return true;
    }
    int i = -1, j = -1;
    for (int t = 0; t < det.cR; t++) if (det.numsR[t] == g.revR) i = t;
    for (int t = 0; t < det.cB; t++) if (det.numsB[t] == g.revB) j = t;
    int p = -1; for (int t = 0; t < det.aRn; t++) if (det.availR[t] == choice) p = t;
    if (p < 0) return false;
    const Sol& as = det.asol[i][j];
    double ev[3], best = -1e18;
    for (int t = 0; t < det.aRn; t++) {
        ev[t] = 0; for (int q = 0; q < det.aBn; q++) ev[t] += as.y[q] * det.MA[i][j][t][q];
        best = max(best, ev[t]);
    }
    int q = sampleFrom(as.y, det.aBn, rng);
    double sR, sB; resolve(g.revR, g.revB, det.availR[p], det.availB[q], sR, sB);
    int optR[3]; for (int t = 0; t < det.aRn; t++) optR[t] = det.availR[t];
    string opts = "[";
    for (int t = 0; t < det.aRn; t++) { opts += (t ? ",\"" : "\""); opts += ANAME[optR[t]]; opts += "\""; }
    opts += "]";
    snprintf(buf, sizeof buf,
        "{\"game\":\"%s\",\"model\":\"%s\",\"decision\":%d,\"round\":%d,\"phase\":\"action\",\"forced\":%s,"
        "\"revealed\":[%d,%d],\"options\":%s,\"option_evs\":%s,\"red_eq_mix\":%s,\"blue_mix\":%s,\"eq_value\":%.8f,"
        "\"choice\":\"%s\",\"regret\":%.8f,\"blue_choice\":\"%s\",\"points\":[%d,%d],\"score_before\":[%d,%d]}",
        id.c_str(), model.c_str(), g.dec, g.k + 1, forced ? "true" : "false", g.revR, g.revB,
        opts.c_str(), jarr(ev, det.aRn).c_str(), jarr(as.x, det.aRn).c_str(), jarr(as.y, det.aBn).c_str(), as.v,
        ANAME[choice], best - ev[p], ANAME[det.availB[q]], (int)sR, (int)sB, g.scoreR, g.scoreB);
    logLine(id, buf);
    g.lastR = g.revR; g.lastB = g.revB; g.lastAR = choice; g.lastAB = det.availB[q];
    g.lastPR = (int)sR; g.lastPB = (int)sB; g.scoreR += (int)sR; g.scoreB += (int)sB;
    g.mr &= ~(1 << (g.revR - 1)); g.mb &= ~(1 << (g.revB - 1));
    g.ar -= POW5[choice]; g.ab -= POW5[det.availB[q]];
    snprintf(buf, sizeof buf, "You played %s. Blue played %s. You scored %d, Blue scored %d.\n",
             ANAME[choice], ANAME[det.availB[q]], (int)sR, (int)sB); note += buf;
    g.k++; g.phase = 0; g.revR = g.revB = 0; g.dec++;
    if (g.k >= L.cfg.T) g.over = 1;
    return true;
}

// Play every decision that has only one legal option, so the player is only asked real choices.
static void autoForced(const Layers& L, const string& id, const string& model, G& g, string& note) {
    while (!g.over) {
        int nr[10], cr; numsOf(g.mr, nr, cr);
        int legal[3], nl = 0; for (int t = 0; t < 3; t++) if (cnt(g.ar, t) > 0) legal[nl++] = t;
        if (g.phase == 0 && cr == 1) { note += "Only one number card left, played automatically. ";
                                       step(L, id, model, g, nr[0], true, note); continue; }
        if (g.phase == 1 && nl == 1) { note += "Only one kind of action card left, played automatically. ";
                                       step(L, id, model, g, legal[0], true, note); continue; }
        break;
    }
}

static int parseAction(string s) {
    for (auto& c : s) c = tolower(c);
    if (s == "h" || s == "hold") return H;
    if (s == "s" || s == "steal") return S;
    if (s == "t" || s == "trap") return TP;
    return -1;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: play new|state|move GAME [move]\n"); return 1; }
    string cmd = argv[1], id = argv[2];
    G g;
    if (cmd == "new") {   // new GAME SEED MODEL  (setup only, run by the organizer)
        if (argc < 5) { fprintf(stderr, "usage: new GAME SEED MODEL\n"); return 1; }
        mkdir(PRIV, 0700); mkdir(dirOf(id).c_str(), 0700);
        g.mr = g.mb = (1 << 10) - 1; g.ar = g.ab = 4 + 3 * 5 + 3 * 25;
        g.seed = strtoull(argv[3], nullptr, 10);
        saveG(id, g);
        FILE* f = fopen((dirOf(id) + "/model.txt").c_str(), "w"); fprintf(f, "%s\n", argv[4]); fclose(f);
        printState(g); return 0;
    }
    if (!loadG(id, g)) { printf("No game called %s.\n", id.c_str()); return 1; }
    if (cmd == "state") { printState(g); return 0; }
#ifdef TESTPEEK
    if (cmd == "peek") {   // test build only: print the current decision's options and the equilibrium mix
        static Layers L; L.build(Config{"simple_full", 10, 10, {4, 3, 3}});
        if (!loadV(L, VFILE)) return 1;
        Details det; stateValue(L, g.k, g.mr, g.mb, g.ar, g.ab, &det, &L.V);
        if (g.phase == 0) { for (int t = 0; t < det.cR; t++) printf("%d %.10f\n", det.numsR[t], det.nsol.x[t]); }
        else {
            int i = -1, j = -1;
            for (int t = 0; t < det.cR; t++) if (det.numsR[t] == g.revR) i = t;
            for (int t = 0; t < det.cB; t++) if (det.numsB[t] == g.revB) j = t;
            for (int t = 0; t < det.aRn; t++) printf("%s %.10f\n", ANAME[det.availR[t]], det.asol[i][j].x[t]);
        }
        return 0;
    }
#endif
    if (cmd != "move" || argc < 4) { printf("Use: play move %s <your choice>\n", id.c_str()); return 1; }
    if (g.over) { printState(g); return 0; }
    string model = "unknown";
    { FILE* f = fopen((dirOf(id) + "/model.txt").c_str(), "r"); char b[128];
      if (f && fscanf(f, "%127s", b) == 1) model = b; if (f) fclose(f); }
    int choice = (g.phase == 0) ? atoi(argv[3]) : parseAction(argv[3]);
    static Layers L; L.build(Config{"simple_full", 10, 10, {4, 3, 3}});
    if (!loadV(L, VFILE)) { printf("Game data missing.\n"); return 1; }
    string note;
    if (!step(L, id, model, g, choice, false, note)) {
        printf("That is not a legal choice right now.\n"); printState(g); return 1;
    }
    autoForced(L, id, model, g, note);
    saveG(id, g);
    printf("%s", note.c_str());
    printState(g);
    return 0;
}
