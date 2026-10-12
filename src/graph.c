#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ utils */
static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}
static void *xrealloc(void *p, size_t n) {
    p = realloc(p, n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}
static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = xmalloc(n);
    memcpy(d, s, n);
    return d;
}

/* ------------------------------------------------------- id -> index map */
static unsigned hash_int(int k) {
    unsigned x = (unsigned)k;
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static void map_init(IdMap *m, int cap) {
    m->cap = cap;
    m->keys = xmalloc(sizeof(int) * cap);
    m->vals = xmalloc(sizeof(int) * cap);
    for (int i = 0; i < cap; i++) m->keys[i] = -1;
}
static void map_put(IdMap *m, int key, int val);
static void map_grow(IdMap *m) {
    IdMap old = *m;
    map_init(m, old.cap * 2);
    for (int i = 0; i < old.cap; i++)
        if (old.keys[i] != -1) map_put(m, old.keys[i], old.vals[i]);
    free(old.keys); free(old.vals);
}
static void map_put(IdMap *m, int key, int val) {
    unsigned i = hash_int(key) & (unsigned)(m->cap - 1);
    while (m->keys[i] != -1 && m->keys[i] != key) i = (i + 1) & (unsigned)(m->cap - 1);
    m->keys[i] = key;
    m->vals[i] = val;
}
static int map_get(const IdMap *m, int key) {
    unsigned i = hash_int(key) & (unsigned)(m->cap - 1);
    while (m->keys[i] != -1) {
        if (m->keys[i] == key) return m->vals[i];
        i = (i + 1) & (unsigned)(m->cap - 1);
    }
    return -1;
}

/* ------------------------------------------------------------- lifecycle */
Graph *graph_new(void) {
    Graph *g = xmalloc(sizeof(Graph));
    g->n = 0; g->cap = 64; g->m = 0;
    g->adj = calloc((size_t)g->cap, sizeof(AdjList));
    g->ext_id = xmalloc(sizeof(int) * g->cap);
    g->name = xmalloc(sizeof(char *) * g->cap);
    map_init(&g->map, 128);
    return g;
}

void graph_free(Graph *g) {
    if (!g) return;
    for (int i = 0; i < g->n; i++) { free(g->adj[i].nbr); free(g->name[i]); }
    free(g->adj); free(g->ext_id); free(g->name);
    free(g->map.keys); free(g->map.vals);
    free(g);
}

/* ------------------------------------------------------------------ users */
int graph_find(const Graph *g, int user_id) { return user_id < 0 ? -1 : map_get(&g->map, user_id); }

int graph_add_user(Graph *g, int user_id, const char *name) {
    if (user_id < 0 || graph_find(g, user_id) != -1) return -1;
    if (g->n == g->cap) {
        g->cap *= 2;
        g->adj = xrealloc(g->adj, sizeof(AdjList) * g->cap);
        g->ext_id = xrealloc(g->ext_id, sizeof(int) * g->cap);
        g->name = xrealloc(g->name, sizeof(char *) * g->cap);
    }
    int i = g->n++;
    g->adj[i].nbr = NULL; g->adj[i].deg = 0; g->adj[i].cap = 0;
    g->ext_id[i] = user_id;
    char buf[32];
    if (!name || !*name) { snprintf(buf, sizeof buf, "user%d", user_id); name = buf; }
    g->name[i] = xstrdup(name);
    if (g->n * 2 > g->map.cap) map_grow(&g->map);
    map_put(&g->map, user_id, i);
    return i;
}

int graph_get_or_add(Graph *g, int user_id) {
    int i = graph_find(g, user_id);
    return i != -1 ? i : graph_add_user(g, user_id, NULL);
}

/* ------------------------------------------------------------ friendships */
static int lower_bound(const AdjList *a, int x) {
    int lo = 0, hi = a->deg;
    while (lo < hi) { int mid = (lo + hi) / 2; if (a->nbr[mid] < x) lo = mid + 1; else hi = mid; }
    return lo;
}
static int adj_insert(AdjList *a, int x) {
    int p = lower_bound(a, x);
    if (p < a->deg && a->nbr[p] == x) return 1;
    if (a->deg == a->cap) { a->cap = a->cap ? a->cap * 2 : 4; a->nbr = xrealloc(a->nbr, sizeof(int) * a->cap); }
    memmove(a->nbr + p + 1, a->nbr + p, sizeof(int) * (size_t)(a->deg - p));
    a->nbr[p] = x; a->deg++;
    return 0;
}
static int adj_erase(AdjList *a, int x) {
    int p = lower_bound(a, x);
    if (p >= a->deg || a->nbr[p] != x) return 1;
    memmove(a->nbr + p, a->nbr + p + 1, sizeof(int) * (size_t)(a->deg - p - 1));
    a->deg--;
    return 0;
}

int graph_has_edge(const Graph *g, int a, int b) {
    if (a < 0 || b < 0 || a >= g->n || b >= g->n) return 0;
    const AdjList *s = g->adj[a].deg <= g->adj[b].deg ? &g->adj[a] : &g->adj[b];
    int x = s == &g->adj[a] ? b : a;
    int p = lower_bound(s, x);
    return p < s->deg && s->nbr[p] == x;
}

int graph_add_edge(Graph *g, int a, int b) {
    if (a < 0 || b < 0 || a >= g->n || b >= g->n) return -1;
    if (a == b) return 2;
    if (adj_insert(&g->adj[a], b)) return 1;
    adj_insert(&g->adj[b], a);
    g->m++;
    return 0;
}

int graph_remove_edge(Graph *g, int a, int b) {
    if (a < 0 || b < 0 || a >= g->n || b >= g->n) return -1;
    if (a == b) return 2;
    if (adj_erase(&g->adj[a], b)) return 1;
    adj_erase(&g->adj[b], a);
    g->m--;
    return 0;
}

/* --------------------------------------------------------- recommendation */
const char *rec_method_name(RecMethod m) {
    switch (m) {
        case REC_COMMON: return "common-neighbours";
        case REC_JACCARD: return "jaccard";
        case REC_ADAMIC_ADAR: return "adamic-adar";
        case REC_POPULAR: return "popularity";
    }
    return "?";
}

static int rec_cmp(const void *x, const void *y) {
    const Rec *a = x, *b = y;
    if (a->score != b->score) return a->score < b->score ? 1 : -1;
    if (a->mutual != b->mutual) return b->mutual - a->mutual;
    return a->idx - b->idx;   /* deterministic tie-break */
}

/* Keep the k best in a small sorted buffer (k is small, typically 10). */
static void topk_push(Rec *out, int *len, int k, Rec r) {
    if (*len == k && rec_cmp(&r, &out[k - 1]) >= 0) return;
    int i = *len < k ? (*len)++ : k - 1;
    while (i > 0 && rec_cmp(&r, &out[i - 1]) < 0) { out[i] = out[i - 1]; i--; }
    out[i] = r;
}

int graph_recommend(const Graph *g, int u, RecMethod method, int k, Rec *out) {
    if (u < 0 || u >= g->n || k <= 0) return 0;
    int len = 0;

    if (method == REC_POPULAR) {               /* baseline: most-connected strangers */
        for (int v = 0; v < g->n; v++) {
            if (v == u || graph_has_edge(g, u, v)) continue;
            Rec r = { v, (double)g->adj[v].deg, 0 };
            topk_push(out, &len, k, r);
        }
        return len;
    }

    /* walk friends-of-friends once, accumulating per-candidate counters */
    double *score = calloc((size_t)g->n, sizeof(double));
    int *mutual = calloc((size_t)g->n, sizeof(int));
    int *touched = xmalloc(sizeof(int) * (size_t)g->n);
    int nt = 0;
    const AdjList *fu = &g->adj[u];
    for (int i = 0; i < fu->deg; i++) {
        int w = fu->nbr[i];
        const AdjList *fw = &g->adj[w];
        double aa = fw->deg > 1 ? 1.0 / log((double)fw->deg) : 0.0;
        for (int j = 0; j < fw->deg; j++) {
            int v = fw->nbr[j];
            if (v == u) continue;
            if (mutual[v] == 0) touched[nt++] = v;
            mutual[v]++;
            score[v] += aa;
        }
    }
    for (int t = 0; t < nt; t++) {
        int v = touched[t];
        if (graph_has_edge(g, u, v)) continue;   /* already friends */
        double s;
        if (method == REC_COMMON) s = mutual[v];
        else if (method == REC_JACCARD) s = (double)mutual[v] / (fu->deg + g->adj[v].deg - mutual[v]);
        else s = score[v];
        Rec r = { v, s, mutual[v] };
        topk_push(out, &len, k, r);
    }
    free(score); free(mutual); free(touched);
    return len;
}

/* -------------------------------------------------------- BFS & structure */
int graph_distance(const Graph *g, int a, int b, int *path, int path_cap, int *path_len) {
    if (path_len) *path_len = 0;
    if (a < 0 || b < 0 || a >= g->n || b >= g->n) return -1;
    if (a == b) { if (path && path_cap > 0) { path[0] = a; *path_len = 1; } return 0; }
    int *dist = xmalloc(sizeof(int) * (size_t)g->n), *prev = xmalloc(sizeof(int) * (size_t)g->n);
    int *queue = xmalloc(sizeof(int) * (size_t)g->n);
    for (int i = 0; i < g->n; i++) dist[i] = -1;
    int head = 0, tail = 0;
    dist[a] = 0; prev[a] = -1; queue[tail++] = a;
    while (head < tail && dist[b] == -1) {
        int x = queue[head++];
        for (int i = 0; i < g->adj[x].deg; i++) {
            int y = g->adj[x].nbr[i];
            if (dist[y] == -1) { dist[y] = dist[x] + 1; prev[y] = x; queue[tail++] = y; }
        }
    }
    int d = dist[b];
    if (d >= 0 && path && path_cap > d) {
        int x = b;
        for (int i = d; i >= 0; i--) { path[i] = x; x = prev[x]; }
        *path_len = d + 1;
    }
    free(dist); free(prev); free(queue);
    return d;
}

int graph_components(const Graph *g, int *largest) {
    int *seen = calloc((size_t)g->n, sizeof(int)), *stack = xmalloc(sizeof(int) * (size_t)(g->n + 1));
    int comps = 0, best = 0;
    for (int s = 0; s < g->n; s++) {
        if (seen[s]) continue;
        comps++;
        int top = 0, size = 0;
        stack[top++] = s; seen[s] = 1;
        while (top) {
            int x = stack[--top]; size++;
            for (int i = 0; i < g->adj[x].deg; i++) {
                int y = g->adj[x].nbr[i];
                if (!seen[y]) { seen[y] = 1; stack[top++] = y; }
            }
        }
        if (size > best) best = size;
    }
    free(seen); free(stack);
    if (largest) *largest = best;
    return comps;
}

/* -------------------------------------------------------------------- I/O */
static FILE *open_maybe_gz(const char *path, int *is_pipe) {
    size_t n = strlen(path);
    *is_pipe = 0;
    if (n > 3 && strcmp(path + n - 3, ".gz") == 0) {
        char cmd[1024];
        if (strchr(path, '\'')) return NULL;          /* refuse odd paths in the shell */
        snprintf(cmd, sizeof cmd, "gzip -dc '%s'", path);
        *is_pipe = 1;
        return popen(cmd, "r");
    }
    return fopen(path, "r");
}

long graph_load_edges(Graph *g, const char *path) {
    int is_pipe;
    FILE *f = open_maybe_gz(path, &is_pipe);
    if (!f) return -1;
    char line[256];
    long added = 0;
    while (fgets(line, sizeof line, f)) {
        int a, b;
        if (sscanf(line, "%d%*[,; \t]%d", &a, &b) != 2) continue;   /* skips header / blank lines */
        if (graph_add_edge(g, graph_get_or_add(g, a), graph_get_or_add(g, b)) == 0) added++;
    }
    if (is_pipe) pclose(f); else fclose(f);
    return added;
}

/* format:  U <id> <name>   for every user, then   E <id1> <id2>   per friendship */
int graph_save(const Graph *g, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < g->n; i++) fprintf(f, "U %d %s\n", g->ext_id[i], g->name[i]);
    for (int i = 0; i < g->n; i++)
        for (int j = 0; j < g->adj[i].deg; j++)
            if (i < g->adj[i].nbr[j]) fprintf(f, "E %d %d\n", g->ext_id[i], g->ext_id[g->adj[i].nbr[j]]);
    fclose(f);
    return 0;
}

int graph_load(Graph *g, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256], name[128];
    int a, b;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "U %d %127s", &a, name) == 2) graph_add_user(g, a, name);
        else if (sscanf(line, "E %d %d", &a, &b) == 2) graph_add_edge(g, graph_find(g, a), graph_find(g, b));
    }
    fclose(f);
    return 0;
}
