/*
 * graph.h — social graph + friend recommendation library.
 *
 * Users are vertices, friendships are undirected edges. Each user keeps a
 * SORTED adjacency array, so "are A and B friends?" is a binary search
 * (O(log d)) and recommendation can walk friends-of-friends in O(sum of
 * degrees of the user's friends).
 */
#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>

typedef struct {
    int *nbr;      /* sorted internal indices of friends */
    int deg;
    int cap;
} AdjList;

typedef struct {
    int *keys;     /* external user id, or -1 = empty slot */
    int *vals;     /* internal index */
    int cap;       /* power of two */
} IdMap;

typedef struct {
    int n, cap;
    long m;            /* number of friendships */
    AdjList *adj;
    int *ext_id;       /* internal index -> user id */
    char **name;       /* internal index -> display name */
    IdMap map;         /* user id -> internal index */
} Graph;

typedef enum { REC_COMMON = 0, REC_JACCARD = 1, REC_ADAMIC_ADAR = 2, REC_POPULAR = 3 } RecMethod;

typedef struct {
    int idx;           /* internal index of the suggested user */
    double score;      /* higher = stronger suggestion */
    int mutual;        /* number of mutual friends */
} Rec;

/* lifecycle */
Graph *graph_new(void);
void graph_free(Graph *g);

/* users: returns internal index, or -1 if the id already exists / is invalid */
int graph_add_user(Graph *g, int user_id, const char *name);
int graph_find(const Graph *g, int user_id);          /* -1 if unknown */
int graph_get_or_add(Graph *g, int user_id);          /* used by loaders */

/* friendships: 0 = added/removed, 1 = already friends / not friends, 2 = self, -1 = bad index */
int graph_add_edge(Graph *g, int a, int b);
int graph_remove_edge(Graph *g, int a, int b);
int graph_has_edge(const Graph *g, int a, int b);

/* recommendation: fills out[0..k-1], returns how many were written */
int graph_recommend(const Graph *g, int u, RecMethod method, int k, Rec *out);
const char *rec_method_name(RecMethod m);

/* degrees of separation: hops from a to b (-1 = not connected).
   If path != NULL it receives up to path_cap indices a..b and *path_len. */
int graph_distance(const Graph *g, int a, int b, int *path, int path_cap, int *path_len);

int graph_components(const Graph *g, int *largest);

/* I/O: CSV edge list "id_1,id_2" (header optional, .gz read through gzip) */
long graph_load_edges(Graph *g, const char *path);
int graph_save(const Graph *g, const char *path);     /* users + edges, reloadable */
int graph_load(Graph *g, const char *path);

#endif
