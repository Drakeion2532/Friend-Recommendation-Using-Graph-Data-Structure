#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/graph.h"

static int passed = 0;
#define CHECK(cond) do { assert(cond); passed++; } while (0)

static Graph *demo(void) {
    /* Alice(1) - Bob(2) - Carol(3), Alice - Dave(4) - Carol, Eve(5) - Carol, Frank(6) alone */
    Graph *g = graph_new();
    const char *names[] = { "", "Alice", "Bob", "Carol", "Dave", "Eve", "Frank" };
    for (int i = 1; i <= 6; i++) graph_add_user(g, i, names[i]);
    int e[][2] = { {1, 2}, {2, 3}, {1, 4}, {4, 3}, {5, 3} };
    for (int i = 0; i < 5; i++) graph_add_edge(g, graph_find(g, e[i][0]), graph_find(g, e[i][1]));
    return g;
}

int main(void) {
    Graph *g = demo();
    int A = graph_find(g, 1), C = graph_find(g, 3), E = graph_find(g, 5), F = graph_find(g, 6);

    /* users */
    CHECK(g->n == 6);
    CHECK(graph_add_user(g, 1, "dup") == -1);          /* duplicate id rejected */
    CHECK(graph_add_user(g, -3, "neg") == -1);         /* negative id rejected */
    CHECK(graph_find(g, 99) == -1);

    /* friendships */
    CHECK(g->m == 5);
    CHECK(graph_has_edge(g, A, graph_find(g, 2)));
    CHECK(!graph_has_edge(g, A, C));
    CHECK(graph_add_edge(g, A, graph_find(g, 2)) == 1);  /* already friends */
    CHECK(graph_add_edge(g, A, A) == 2);                /* self-friendship blocked (bug in v1) */
    CHECK(g->m == 5);

    /* recommendations: Carol shares 2 mutual friends with Alice -> first, no duplicates (bug in v1) */
    Rec out[10];
    int k = graph_recommend(g, A, REC_COMMON, 10, out);
    CHECK(k == 1);
    CHECK(out[0].idx == C && out[0].mutual == 2);
    k = graph_recommend(g, E, REC_ADAMIC_ADAR, 10, out);   /* Eve -> Bob, Dave via Carol */
    CHECK(k == 2);
    CHECK(fabs(out[0].score - 1.0 / log(3.0)) < 1e-9);
    k = graph_recommend(g, F, REC_COMMON, 10, out);        /* isolated user: nothing */
    CHECK(k == 0);
    k = graph_recommend(g, A, REC_JACCARD, 10, out);       /* |N(A) n N(C)| / |N(A) u N(C)| = 2/3 */
    CHECK(fabs(out[0].score - 2.0 / 3.0) < 1e-9);

    /* BFS degrees of separation */
    int path[10], len;
    CHECK(graph_distance(g, A, E, path, 10, &len) == 3);
    CHECK(len == 4 && path[0] == A && path[3] == E);
    CHECK(graph_distance(g, A, F, NULL, 0, NULL) == -1);

    /* components */
    int largest;
    CHECK(graph_components(g, &largest) == 2 && largest == 5);

    /* removal */
    CHECK(graph_remove_edge(g, A, graph_find(g, 2)) == 0);
    CHECK(graph_remove_edge(g, A, graph_find(g, 2)) == 1);
    CHECK(g->m == 4);

    /* save / load round trip */
    CHECK(graph_save(g, "/tmp/friendrec_test.txt") == 0);
    Graph *h = graph_new();
    CHECK(graph_load(h, "/tmp/friendrec_test.txt") == 0);
    CHECK(h->n == g->n && h->m == g->m);
    graph_free(h);

    /* growth beyond the old 100-user limit */
    Graph *big = graph_new();
    for (int i = 0; i < 5000; i++) graph_add_user(big, i + 1, NULL);
    for (int i = 1; i < 5000; i++) graph_add_edge(big, 0, i);
    CHECK(big->n == 5000 && big->adj[0].deg == 4999);
    graph_free(big);

    graph_free(g);
    printf("all %d checks passed\n", passed);
    return 0;
}
