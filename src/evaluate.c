/*
 * evaluate — does the recommender actually predict real friendships?
 *
 * Link-prediction protocol on a real network:
 *   1. pick test users with at least MIN_DEG friends (seeded RNG)
 *   2. hide HIDE_FRAC of each test user's friendships (remove the edges)
 *   3. ask each method for its top-K suggestions
 *   4. a suggestion is a HIT if it is one of the hidden friends
 * Metrics: precision@K, recall@K, hit-rate@K (>=1 hidden friend in top K),
 * plus average milliseconds per recommendation.
 *
 *   ./evaluate data/facebook_pages_edges.csv.gz [users] [k]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "graph.h"

#define MIN_DEG 10
#define HIDE_FRAC 0.2

static unsigned long long rng = 42;
static unsigned rnd(void) { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return (unsigned)(rng >> 33); }

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "data/facebook_pages_edges.csv.gz";
    int n_users = argc > 2 ? atoi(argv[2]) : 1000, K = argc > 3 ? atoi(argv[3]) : 10;
    Graph *g = graph_new();
    clock_t t0 = clock();
    if (graph_load_edges(g, path) < 0) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    int largest, comps = graph_components(g, &largest);
    fprintf(stderr, "loaded %d users, %ld friendships in %.2fs (%d components, largest %d)\n",
            g->n, g->m, (double)(clock() - t0) / CLOCKS_PER_SEC, comps, largest);

    /* choose test users */
    int *cand = malloc(sizeof(int) * (size_t)g->n), nc = 0;
    for (int i = 0; i < g->n; i++) if (g->adj[i].deg >= MIN_DEG) cand[nc++] = i;
    for (int i = nc - 1; i > 0; i--) { int j = (int)(rnd() % (unsigned)(i + 1)); int t = cand[i]; cand[i] = cand[j]; cand[j] = t; }
    if (n_users > nc) n_users = nc;

    const RecMethod methods[] = { REC_POPULAR, REC_COMMON, REC_JACCARD, REC_ADAMIC_ADAR };
    const int NM = 4;
    double prec[4] = {0}, rec[4] = {0}, hit[4] = {0}, ms[4] = {0};
    Rec *out = malloc(sizeof(Rec) * (size_t)K);
    int *hidden = malloc(sizeof(int) * (size_t)g->n);

    for (int t = 0; t < n_users; t++) {
        int u = cand[t];
        int deg = g->adj[u].deg, nh = (int)(deg * HIDE_FRAC);
        if (nh < 1) nh = 1;
        /* hide nh random friendships of u */
        int *fr = malloc(sizeof(int) * (size_t)deg);
        memcpy(fr, g->adj[u].nbr, sizeof(int) * (size_t)deg);
        for (int i = deg - 1; i > 0; i--) { int j = (int)(rnd() % (unsigned)(i + 1)); int x = fr[i]; fr[i] = fr[j]; fr[j] = x; }
        for (int i = 0; i < nh; i++) { hidden[i] = fr[i]; graph_remove_edge(g, u, fr[i]); }
        qsort(hidden, (size_t)nh, sizeof(int), cmp_int);

        for (int m = 0; m < NM; m++) {
            clock_t c = clock();
            int k = graph_recommend(g, u, methods[m], K, out);
            ms[m] += 1000.0 * (double)(clock() - c) / CLOCKS_PER_SEC;
            int hits = 0;
            for (int i = 0; i < k; i++)
                if (bsearch(&out[i].idx, hidden, (size_t)nh, sizeof(int), cmp_int)) hits++;
            prec[m] += (double)hits / K;
            rec[m] += (double)hits / nh;
            hit[m] += hits > 0;
        }
        for (int i = 0; i < nh; i++) graph_add_edge(g, u, hidden[i]);   /* restore */
        free(fr);
    }

    printf("{\n  \"dataset\": \"%s\",\n  \"users\": %d, \"friendships\": %ld,\n", path, g->n, g->m);
    printf("  \"test_users\": %d, \"k\": %d, \"hidden_fraction\": %.2f, \"min_degree\": %d,\n", n_users, K, HIDE_FRAC, MIN_DEG);
    printf("  \"results\": [\n");
    for (int m = 0; m < NM; m++)
        printf("    {\"method\": \"%s\", \"precision_at_k\": %.4f, \"recall_at_k\": %.4f, \"hit_rate_at_k\": %.4f, \"ms_per_query\": %.3f}%s\n",
               rec_method_name(methods[m]), prec[m] / n_users, rec[m] / n_users, hit[m] / n_users, ms[m] / n_users,
               m + 1 < NM ? "," : "");
    printf("  ]\n}\n");
    free(cand); free(out); free(hidden); graph_free(g);
    return 0;
}
