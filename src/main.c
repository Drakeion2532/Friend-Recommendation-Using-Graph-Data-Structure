/*
 * friendrec — interactive friend recommender (menu-driven, like the original
 * coursework version, now backed by the graph library).
 *
 *   ./friendrec                      start with an empty network
 *   ./friendrec data/demo_network.txt  start from a saved network
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

#define RESET "\x1b[0m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define BLUE "\x1b[1;34m"
#define YELLOW "\x1b[0;33m"

/* Safe line-based input: never overflows, survives non-numeric input. */
static int read_line(const char *prompt, char *buf, size_t n) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = 0;
    return 1;
}
static int read_int(const char *prompt, int *out) {
    char buf[64], *end;
    if (!read_line(prompt, buf, sizeof buf)) return 0;
    long v = strtol(buf, &end, 10);
    if (end == buf) { printf(RED "Please enter a number.\n" RESET); return 0; }
    *out = (int)v;
    return 1;
}
static int read_user(Graph *g, const char *prompt) {
    int id;
    if (!read_int(prompt, &id)) return -1;
    int u = graph_find(g, id);
    if (u == -1) printf(RED "Error: user %d not found.\n" RESET, id);
    return u;
}

static void print_menu(void) {
    printf("\n+---------------------------------------+\n");
    printf(BLUE "|          FRIEND RECOMMENDER           |\n" RESET);
    printf("+---------------------------------------+\n");
    printf("|  1. Add user                          |\n");
    printf("|  2. Add friendship                    |\n");
    printf("|  3. Remove friendship                 |\n");
    printf("|  4. Recommend friends                 |\n");
    printf("|  5. List users                        |\n");
    printf("|  6. Show a user's friends             |\n");
    printf("|  7. Display graph (adjacency list)    |\n");
    printf("|  8. Degrees of separation (BFS path)  |\n");
    printf("|  9. Network statistics                |\n");
    printf("| 10. Load edge list (CSV / CSV.gz)     |\n");
    printf("| 11. Save network                      |\n");
    printf("|  0. Exit                              |\n");
    printf("+---------------------------------------+\n");
}

static void recommend(Graph *g) {
    int u = read_user(g, "User ID: ");
    if (u < 0) return;
    int m = 2;
    printf("Method: 0 common-neighbours, 1 jaccard, 2 adamic-adar (default), 3 popularity: ");
    char buf[16];
    if (fgets(buf, sizeof buf, stdin) && buf[0] >= '0' && buf[0] <= '3') m = buf[0] - '0';
    Rec out[10];
    int k = graph_recommend(g, u, (RecMethod)m, 10, out);
    printf("\nTop suggestions for %s (user %d) by %s:\n", g->name[u], g->ext_id[u], rec_method_name((RecMethod)m));
    if (k == 0) { printf(YELLOW "  No suggestions yet: add some friends first.\n" RESET); return; }
    printf("  %-4s %-10s %-20s %-8s %s\n", "#", "User ID", "Name", "Mutual", "Score");
    for (int i = 0; i < k; i++)
        printf("  %-4d %-10d %-20s %-8d %.3f\n", i + 1, g->ext_id[out[i].idx], g->name[out[i].idx], out[i].mutual, out[i].score);
}

static void list_users(Graph *g) {
    int show = g->n < 50 ? g->n : 50;
    printf("\n  %-10s %-20s %s\n", "User ID", "Name", "Friends");
    for (int i = 0; i < show; i++) printf("  %-10d %-20s %d\n", g->ext_id[i], g->name[i], g->adj[i].deg);
    if (g->n > show) printf("  ... %d more users\n", g->n - show);
}

static void show_friends(Graph *g) {
    int u = read_user(g, "User ID: ");
    if (u < 0) return;
    printf("\nFriends of %s (user %d): %d\n", g->name[u], g->ext_id[u], g->adj[u].deg);
    for (int i = 0; i < g->adj[u].deg && i < 50; i++) {
        int v = g->adj[u].nbr[i];
        printf("  - %s (user %d)\n", g->name[v], g->ext_id[v]);
    }
}

static void display_graph(Graph *g) {
    int show = g->n < 30 ? g->n : 30;
    for (int i = 0; i < show; i++) {
        printf("%s (%d)\n", g->name[i], g->ext_id[i]);
        for (int j = 0; j < g->adj[i].deg && j < 10; j++)
            printf("  |--- %s (%d)\n", g->name[g->adj[i].nbr[j]], g->ext_id[g->adj[i].nbr[j]]);
    }
    if (g->n > show) printf("... %d more users (graph too large to print)\n", g->n - show);
}

static void separation(Graph *g) {
    int a = read_user(g, "From user ID: ");
    if (a < 0) return;
    int b = read_user(g, "To user ID: ");
    if (b < 0) return;
    int path[64], len;
    int d = graph_distance(g, a, b, path, 64, &len);
    if (d < 0) { printf(YELLOW "Not connected.\n" RESET); return; }
    printf(GREEN "%d degree(s) of separation: " RESET, d);
    for (int i = 0; i < len; i++) printf("%s%s", g->name[path[i]], i + 1 < len ? " -> " : "\n");
}

static void stats(Graph *g) {
    int largest, comps = graph_components(g, &largest);
    int maxdeg = 0;
    for (int i = 0; i < g->n; i++) if (g->adj[i].deg > maxdeg) maxdeg = g->adj[i].deg;
    printf("\nUsers: %d   Friendships: %ld   Average friends: %.2f   Max friends: %d\n",
           g->n, g->m, g->n ? 2.0 * g->m / g->n : 0.0, maxdeg);
    printf("Connected groups: %d   Largest group: %d users\n", comps, largest);
}

int main(int argc, char **argv) {
    Graph *g = graph_new();
    if (argc > 1) {
        if (graph_load(g, argv[1]) == 0) printf(GREEN "Loaded %d users, %ld friendships from %s\n" RESET, g->n, g->m, argv[1]);
        else printf(RED "Could not open %s\n" RESET, argv[1]);
    }
    for (;;) {
        print_menu();
        int choice;
        if (!read_int("Enter your choice: ", &choice)) { if (feof(stdin)) break; continue; }
        switch (choice) {
        case 1: {
            int id; char name[64];
            if (!read_int("User ID: ", &id)) break;
            if (!read_line("Name (no spaces): ", name, sizeof name)) break;
            for (char *p = name; *p; p++) if (*p == ' ') *p = '_';
            if (id < 1) printf(RED "Error: user ID must be a positive integer.\n" RESET);
            else if (graph_add_user(g, id, name) < 0) printf(RED "Error: user %d already exists.\n" RESET, id);
            else printf(GREEN "User added.\n" RESET);
            break;
        }
        case 2: case 3: {
            int a = read_user(g, "First user ID: ");
            if (a < 0) break;
            int b = read_user(g, "Second user ID: ");
            if (b < 0) break;
            int r = choice == 2 ? graph_add_edge(g, a, b) : graph_remove_edge(g, a, b);
            const char *msg[] = { GREEN "Done." RESET, YELLOW "Nothing to change (already friends / not friends)." RESET,
                                  RED "A user cannot befriend themselves." RESET };
            printf("%s\n", r >= 0 && r <= 2 ? msg[r] : "Error");
            break;
        }
        case 4: recommend(g); break;
        case 5: list_users(g); break;
        case 6: show_friends(g); break;
        case 7: display_graph(g); break;
        case 8: separation(g); break;
        case 9: stats(g); break;
        case 10: {
            char path[512];
            if (!read_line("Path to edge list: ", path, sizeof path)) break;
            long n = graph_load_edges(g, path);
            if (n < 0) printf(RED "Could not open %s\n" RESET, path);
            else printf(GREEN "Added %ld friendships. Network now has %d users.\n" RESET, n, g->n);
            break;
        }
        case 11: {
            char path[512];
            if (!read_line("Save to file: ", path, sizeof path)) break;
            printf(graph_save(g, path) == 0 ? GREEN "Saved.\n" RESET : RED "Could not write file.\n" RESET);
            break;
        }
        case 0:
            graph_free(g);
            printf(GREEN "Goodbye.\n" RESET);
            return 0;
        default:
            printf(RED "Invalid choice.\n" RESET);
        }
    }
    graph_free(g);
    return 0;
}
