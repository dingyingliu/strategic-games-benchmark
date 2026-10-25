// Reference players for the Fortune Fight model pilot, scored exactly as the models are scored.
// Red plays a fixed policy against Blue's exact equilibrium (Dingying Liu's solver, simple_oracle.cpp,
// unchanged). Every Red decision is priced as in ff_engine.cpp: regret = best option value - value of
// the option played, both against Blue's equilibrium mix. Summed over a game, regret has the same
// expectation as the policy's expected points lost per game to the equilibrium opponent; the raw final
// score difference is printed too as a check of that.
//   ff_baseline uniform GAMES SEED   Red picks uniformly among its legal options (the options a model is offered)
//   ff_baseline eq GAMES SEED        Red samples the exact equilibrium (pipeline check: regret must be 0)
//   ff_baseline depthD GAMES SEED    Red samples the equilibrium of a D-round lookahead (D = 1, 2, 3), the
//                                    planners in the repo's horizon_results.csv; depth D reads simpleV_d(D-1).bin
#define main oracle_main_unused
#include "/home/claude/strategic-games-benchmark/simple_oracle.cpp"
#undef main

static void stats(const char* name, const vector<double>& v) {
    double m = 0; for (double x : v) m += x; m /= v.size();
    double s = 0; for (double x : v) s += (x - m) * (x - m); s = sqrt(s / (v.size() - 1));
    printf("\"%s\": {\"mean\": %.4f, \"sd\": %.4f, \"se\": %.4f}", name, m, s, s / sqrt((double)v.size()));
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: ff_baseline uniform|eq|depth1|depth2|depth3 GAMES SEED\n"); return 1; }
    string mode = argv[1]; int games = atoi(argv[2]); uint64_t seed = strtoull(argv[3], nullptr, 10);
    int depth = (mode.rfind("depth", 0) == 0) ? atoi(mode.c_str() + 5) : 0;
    if (mode != "uniform" && mode != "eq" && (depth < 1 || depth > 3)) { fprintf(stderr, "unknown policy\n"); return 1; }
    static Layers L; L.build(Config{"simple_full", 10, 10, {4, 3, 3}});
    if (!loadV(L, "/home/claude/ffpilot/work/simpleV.bin")) { fprintf(stderr, "missing V\n"); return 1; }
    static vector<vector<double>> prev; const vector<vector<double>>* pp = nullptr;
    if (depth >= 2) {
        char pth[128]; snprintf(pth, 128, "/home/claude/ffpilot/work/simpleV_d%d.bin", depth - 1);
        if (!loadTable(L, prev, pth)) { fprintf(stderr, "missing %s\n", pth); return 1; }
        pp = &prev;
    }
    mt19937_64 rngR(seed), rngB(seed + 1);
    vector<double> reg, regN, regA, diff;
    for (int g = 0; g < games; g++) {
        int mr = L.rootMask(), mb = L.rootMask(), ar = L.rootAct(), ab = L.rootAct();
        double RN = 0, RA = 0, sR = 0, sB = 0;
        for (int t = 0; t < L.cfg.T; t++) {
            Details det; stateValue(L, t, mr, mb, ar, ab, &det, &L.V);
            static Details dd;
            if (depth) stateValue(L, t, mr, mb, ar, ab, &dd, pp);   // pp == nullptr: one-round (myopic) lookahead
            double ev[MAXD], best = -1e18;
            for (int i = 0; i < det.cR; i++) {
                ev[i] = 0; for (int j = 0; j < det.cB; j++) ev[i] += det.nsol.y[j] * det.MN[i][j];
                best = max(best, ev[i]);
            }
            int i = (mode == "eq") ? sampleFrom(det.nsol.x, det.cR, rngR)
                  : depth          ? sampleFrom(dd.nsol.x, dd.cR, rngR)
                                   : uniform_int_distribution<int>(0, det.cR - 1)(rngR);
            RN += best - ev[i];
            int j = sampleFrom(det.nsol.y, det.cB, rngB);
            const Sol& as = det.asol[i][j];
            double aev[3], abest = -1e18;
            for (int p = 0; p < det.aRn; p++) {
                aev[p] = 0; for (int q = 0; q < det.aBn; q++) aev[p] += as.y[q] * det.MA[i][j][p][q];
                abest = max(abest, aev[p]);
            }
            int p = (mode == "eq") ? sampleFrom(as.x, det.aRn, rngR)
                  : depth          ? sampleFrom(dd.asol[i][j].x, dd.aRn, rngR)
                                   : uniform_int_distribution<int>(0, det.aRn - 1)(rngR);
            RA += abest - aev[p];
            int q = sampleFrom(as.y, det.aBn, rngB);
            double dR, dB; resolve(det.numsR[i], det.numsB[j], det.availR[p], det.availB[q], dR, dB);
            sR += dR; sB += dB;
            mr &= ~(1 << (det.numsR[i] - 1)); mb &= ~(1 << (det.numsB[j] - 1));
            ar -= POW5[det.availR[p]]; ab -= POW5[det.availB[q]];
        }
        reg.push_back(RN + RA); regN.push_back(RN); regA.push_back(RA); diff.push_back(sR - sB);
    }
    printf("{\"policy\": \"%s\", \"games\": %d, \"seed\": %llu, ", mode.c_str(), games, (unsigned long long)seed);
    stats("points_lost_sum_of_regret", reg); printf(", ");
    stats("number_phase", regN); printf(", ");
    stats("action_phase", regA); printf(", ");
    stats("final_score_difference", diff); printf(", ");
    // how often a whole game has no loss in a phase, and the spread of the per-game loss
    int zN = 0, zA = 0, zT = 0;
    for (int g = 0; g < games; g++) { zN += regN[g] < 1e-6; zA += regA[g] < 1e-6; zT += reg[g] < 1e-6; }
    vector<double> srt = reg; sort(srt.begin(), srt.end());
    auto qt = [&](double f) { return srt[min((size_t)(f * (srt.size() - 1) + 0.5), srt.size() - 1)]; };
    printf("\"share_games_zero_number_loss\": %.4f, \"share_games_zero_action_loss\": %.4f, "
           "\"share_games_zero_loss\": %.4f, \"loss_p10\": %.4f, \"loss_median\": %.4f, \"loss_p90\": %.4f}\n",
           (double)zN / games, (double)zA / games, (double)zT / games, qt(0.10), qt(0.50), qt(0.90));
    return 0;
}
