#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>


#define MAX_AIR_ROUTES 5
#define COST_MIN 0
#define COST_MAX 100
#define INF (INT_MAX/4)
#define CACHE_SIZE 32768


typedef struct {
    int dest_idx;
} AirRoute;

typedef struct {
    int exit_cost;
    int air_route_count;
    AirRoute air_routes[MAX_AIR_ROUTES];
    int dist;
    int last_step;
} Hexagon;

typedef struct {
    int node;
    int dist;
} HeapNode;

typedef struct {
    HeapNode *a;
    int size;
    int cap;
} MinHeap;

typedef struct {
    int idx;
    int dist;
} QueueNode;

typedef struct {
    int source_idx;
    int dest_idx;
    int cost;
    int version;
} CacheEntry;

typedef struct {
    int cols;
    int rows;
    int N;
    Hexagon *grid;
    int cur_step;
    MinHeap heap;
    CacheEntry cache[CACHE_SIZE];
    int cache_version;
} HexMap;


int NEI[2][6][2] = {
    { {-1,-1}, {0,-1}, {-1,0}, {1,0}, {-1,1}, {0,1} },
    { {0,-1},  {1,-1}, {-1,0}, {1,0},  {0,1},  {1,1}  } 
};

int valid(HexMap *m, int x, int y){
    return (x >= 0 && x < m->cols && y >= 0 && y < m->rows);
}

int idx_of(HexMap *m, int x, int y){
    return y * m->cols + x;
}

Hexagon *hex_at(HexMap *m, int x, int y){
    return &m->grid[idx_of(m, x, y)];
}

Hexagon *hex_at_idx(HexMap *m, int idx){
    return &m->grid[idx];
}

int clamp(int v, int low, int high) {
    if (v < low) return low;
    if (v > high) return high;
    return v;
}

int floor_div_int(int a, int b){
    if (b <= 0) return 0;
    if (a >= 0) return a / b;
    return - (((-a) + (b-1)) / b);
}

int hex_neighbors(HexMap *m, int x, int y, int neighbors[6][2]){
    int (*OFFS)[2] = NEI[y % 2];
    int k = 0;
    for (int i = 0; i < 6; i++){
        int nx = x + OFFS[i][0];
        int ny = y + OFFS[i][1];
        if (valid(m, nx, ny)) {
            neighbors[k][0] = nx;
            neighbors[k][1] = ny;
            k++;
        }
    }
    return k;
}

void step_next(HexMap *m) {
    m->cur_step++;
}


void heap_init(MinHeap *h, int cap){
    if (cap <= 0) cap = 1;
    h->a = (HeapNode*)malloc(sizeof(HeapNode) * (size_t)cap);
    h->size = 0;
    h->cap = cap;
}

void heap_free(MinHeap *h) {
    free(h->a);
    h->a = NULL;
    h->size = 0;
    h->cap = 0;
}

void heap_clear(MinHeap *h){
    h->size = 0;
}

void heap_push(MinHeap *h, HeapNode it){
    if (h->size >= h->cap){
        int new_cap = h->cap * 2;
        if (new_cap <= 0) new_cap = 1;
        h->a = (HeapNode*)realloc(h->a, sizeof(HeapNode) * (size_t)new_cap);
        h->cap = new_cap;
    }

    int i = h->size++;
    while (i > 0){
        int p = (i - 1) / 2;
        if (h->a[p].dist <= it.dist) break;
        h->a[i] = h->a[p];
        i = p;
    }
    h->a[i] = it;
}

HeapNode heap_pop(MinHeap *h){
    HeapNode ret = h->a[0];
    HeapNode last = h->a[--h->size];

    int i = 0;
    for (;;){
        int l = 2 * i + 1;
        if (l >= h->size) break;
        int r = l + 1;
        int c = l;
        if (r < h->size && h->a[r].dist < h->a[l].dist) c = r;
        if (h->a[c].dist >= last.dist) break;
        h->a[i] = h->a[c];
        i = c;
    } 
    h->a[i] = last;
    return ret;
}

int heap_empty(MinHeap *h){
    return h->size == 0;
}


void map_free(HexMap *m){
    if (!m) return;
    free(m->grid);
    heap_free(&m->heap);
    free(m);
}

HexMap *init(int cols, int rows){
    if (cols <= 0 || rows <= 0) return NULL;
    HexMap *m = (HexMap*)malloc(sizeof(HexMap));
    if (!m) return NULL;

    m->cols = cols;
    m->rows = rows;
    m->N = cols * rows;

    m->grid = (Hexagon*)calloc((size_t)m->N, sizeof(Hexagon));
    if (!m->grid) {
        free(m);
        return NULL;
    }

    for (int y = 0; y < rows; y++){
        for (int x = 0; x < cols; x++){
            Hexagon *h = hex_at(m, x, y);
            h->exit_cost = 1;
            h->air_route_count = 0;
            h->dist = INF;
            h->last_step = 0;
        }
    }

    m->cur_step = 1;

    int heap_cap = (m->N > 0) ? m->N : 1;
    heap_init(&m->heap, heap_cap);

    for (int i = 0; i < CACHE_SIZE; i++){
        m->cache[i].version = 0;
    }
    m->cache_version = 1;

    return m;
}


int hash_function(int a, int b){
    int key = a * 31 + b;
    return key % CACHE_SIZE;
}

int cache_get(HexMap *m, int source_idx, int dest_idx, int *cost){
    int start = hash_function(source_idx, dest_idx);
    int idx = start;

    for (int lookups = 0; lookups < CACHE_SIZE; lookups++){
        CacheEntry *entry = &m->cache[idx];

        if (entry->version != m->cache_version){
            return 0;
        }

        if (entry->source_idx == source_idx && entry->dest_idx == dest_idx){
            *cost = entry->cost;
            return 1;
        }

        idx++;
        if (idx >= CACHE_SIZE) idx = 0;
    }

    return 0;
}

void cache_put(HexMap * m, int source_idx, int dest_idx, int cost){
    int start = hash_function(source_idx, dest_idx);
    int idx = start;

    for (int lookups = 0; lookups < CACHE_SIZE; lookups++){
        CacheEntry *entry = &m->cache[idx];
        
        if (entry->version != m->cache_version){
            entry->source_idx = source_idx;
            entry->dest_idx = dest_idx;
            entry->cost = cost;
            entry->version = m->cache_version;
            return;
        }

        if (entry->source_idx == source_idx && entry->dest_idx == dest_idx){
            entry->cost = cost;
            entry->version = m->cache_version;
            return;
        }

        idx++;
        if (idx >= CACHE_SIZE) idx = 0;
    }

    CacheEntry *entry = &m->cache[start];
    entry->source_idx = source_idx;
    entry->dest_idx = dest_idx;
    entry->cost = cost;
    entry->version = m->cache_version;
}

void cache_invalidate(HexMap *m){
    m->cache_version++;
}


bool change_cost(HexMap *m, int sx, int sy, int v, int raggio) {
    if (!m) return false;
    if (!valid(m, sx, sy)) return false;
    if (raggio <= 0) return false;
    if (v < -10 || v > 10) return false;

    step_next(m);
    int cur = m->cur_step;

    QueueNode *q = (QueueNode*)malloc(sizeof(QueueNode) * (size_t)m->N);
    int qhead = 0;
    int qtail = 0;

    int sidx = idx_of(m, sx, sy);
    Hexagon *hex_source = hex_at_idx(m, sidx);
    hex_source->last_step = cur;
    hex_source->dist = 0; 
    
    q[qtail].idx = sidx;
    q[qtail].dist = 0;
    qtail++;

    while (qhead < qtail){
        QueueNode curNode = q[qhead++];
        int uidx = curNode.idx;
        int d = curNode.dist;

        if (d >= raggio) continue;

        int ux = uidx % m->cols;
        int uy = uidx / m->cols;

        int num = raggio - d;
        if (num < 0) num = 0;
        int delta = floor_div_int(v * num, raggio);

        Hexagon *hu = hex_at_idx(m, uidx);
        int new_exit = hu->exit_cost + delta;
        hu->exit_cost = clamp(new_exit, COST_MIN, COST_MAX);

        if (d + 1 >= raggio) continue;

        int nb[6][2];
        int nk = hex_neighbors(m, ux, uy, nb);
        for (int i = 0; i < nk; i++){
            int vx = nb[i][0];
            int vy = nb[i][1];
            int vidx = idx_of(m, vx, vy);
            Hexagon *visited_hex = hex_at_idx(m, vidx);
            if (visited_hex->last_step != cur) {
                visited_hex->last_step = cur;
                q[qtail].idx = vidx;
                q[qtail].dist = d + 1;
                qtail++;
            }
        }
    }

    free(q);
    cache_invalidate(m);
    return true;
}


bool toggle_air_route(HexMap *m, int x1, int y1, int x2, int y2){
    if (!m) return false;
    if (!valid(m, x1, y1) || !valid(m, x2, y2)) return false;
    
    int didx = idx_of(m, x2, y2);
    Hexagon *h = hex_at(m, x1, y1);

    for (int i = 0; i < h->air_route_count; i++){
        if (h->air_routes[i].dest_idx == didx){
            int last = h->air_route_count - 1;
            if (i != last){
                h->air_routes[i] = h->air_routes[last];
            }
            h->air_route_count = last;
            cache_invalidate(m);
            return true;
        }
    }

    if (h->air_route_count >= MAX_AIR_ROUTES) return false;
    h->air_routes[h->air_route_count].dest_idx = didx;
    h->air_route_count++;

    cache_invalidate(m);
    return true;
}


int get_dist(HexMap *m, int idx){
    Hexagon *h = hex_at_idx(m, idx);
    if (h->last_step != m->cur_step) return INF;
    return h->dist;
}

void set_dist(HexMap *m, int idx, int v){
    Hexagon *h = hex_at_idx(m, idx);
    h->last_step = m->cur_step;
    h->dist = v;
}

int travel_cost(HexMap *m, int sx, int sy, int dx, int dy){
    if (!m) return -1;
    if (!valid(m, sx, sy) || !valid(m, dx, dy)) return -1;
    if (sx == dx && sy == dy) return 0;

    int source_idx = idx_of(m, sx, sy);
    int dest_idx = idx_of(m, dx, dy);

    int cached_cost;
    if (cache_get(m, source_idx, dest_idx, &cached_cost)){ 
        return cached_cost;
    }

    step_next(m);
    heap_clear(&m->heap);
    set_dist(m, source_idx, 0);
    heap_push(&m->heap, (HeapNode){ .node = source_idx, .dist = 0});

    while (!heap_empty(&m->heap)){
        HeapNode cur = heap_pop(&m->heap);
        int cur_node = cur.node;
        int cur_dist = cur.dist;

        int real_dist = get_dist(m, cur_node);
        if (real_dist != cur_dist) continue;

        if (cur_node == dest_idx){
            cache_put(m, source_idx, dest_idx, real_dist);
            return real_dist;
        }

        int cur_node_x = cur_node % m->cols;
        int cur_node_y = cur_node / m->cols;

        Hexagon *cur_hex = hex_at_idx(m, cur_node);

        if (cur_hex->exit_cost > 0){
            int nb[6][2];
            int nk = hex_neighbors(m, cur_node_x, cur_node_y, nb);
            for (int i = 0; i < nk; i++){
                int neighbor_x = nb[i][0];
                int neighbor_y = nb[i][1];
                int neighbor_idx = idx_of(m, neighbor_x, neighbor_y);

                int neighbor_new_dist = real_dist + cur_hex->exit_cost;
                int neighbor_cur_dist = get_dist(m, neighbor_idx);
                if (neighbor_new_dist < neighbor_cur_dist){
                    set_dist(m, neighbor_idx, neighbor_new_dist);
                    heap_push(&m->heap, (HeapNode){ .node = neighbor_idx, .dist = neighbor_new_dist });
                }
            }
        }

        for (int i = 0; i < cur_hex->air_route_count; i++){
            int air_dest_idx = cur_hex->air_routes[i].dest_idx;
            if (cur_hex->exit_cost <= 0) continue;
            int air_new_dist = real_dist + cur_hex->exit_cost;
            int air_cur_dist = get_dist(m, air_dest_idx);
            if (air_new_dist < air_cur_dist){
                set_dist(m, air_dest_idx, air_new_dist);
                heap_push(&m->heap, (HeapNode){ .node = air_dest_idx, .dist = air_new_dist });

            }
        }
    }

    cache_put(m, source_idx, dest_idx, -1);
    return -1;

}


int main(void) {
    HexMap *map = NULL;
    char cmd[64];

    while (scanf("%63s", cmd) == 1){
        if (strcmp(cmd, "init") == 0){
            int cols, rows;
            if (scanf("%d %d", &cols, &rows) != 2){
                puts("KO");
                continue;
            }
            if (map){
                map_free(map);
                map = NULL;
            }
            map = init(cols, rows);
            if (!map){ puts("KO"); continue; }
            puts("OK");
        }
        else if (strcmp(cmd, "change_cost") == 0){
            int x, y, v, r;
            if (scanf("%d %d %d %d", &x, &y, &v, &r) != 4){
                puts("KO");
                continue;
            }
            if (!map){ puts("KO"); continue; }
            bool ok = change_cost(map, x, y, v, r);
            puts(ok ? "OK" : "KO");
        }
        else if (strcmp(cmd, "toggle_air_route") == 0){
            int x1, y1, x2, y2;
            if (scanf("%d %d %d %d", &x1, &y1, &x2, &y2) != 4){
                puts("KO");
                continue;
            }
            if (!map){ puts("KO"); continue; }
            bool ok = toggle_air_route(map, x1, y1, x2, y2);
            puts(ok ? "OK" : "KO");
        }
        else if (strcmp(cmd, "travel_cost") == 0){
            int xs, ys, xd, yd;
            if (scanf("%d %d %d %d", &xs, &ys, &xd, &yd) != 4){
                puts("-1");
                continue;
            }
            if (!map){ puts("-1"); continue; }
            int res = travel_cost(map, xs, ys, xd, yd);
            if (res < 0) printf("-1\n"); else printf("%d\n", res);
        }
        else {
            puts("KO");
        }
    }

    if (map) map_free(map);
    return 0;
}
