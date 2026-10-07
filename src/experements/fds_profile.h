#ifndef FDS_PROFILE_H
#define FDS_PROFILE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef FDS_PROFILE_ENABLE
    #define FDS_PROFILE_INIT() fds_profile_init()
    #define FDS_PROFILE_BEGIN(name) fds_profile_begin(name)
    #define FDS_PROFILE_END() fds_profile_end()
    #define FDS_PROFILE_DUMP(filepath) fds_profile_dump(filepath)
#else
    #define FDS_PROFILE_INIT()
    #define FDS_PROFILE_BEGIN(name)
    #define FDS_PROFILE_END()
    #define FDS_PROFILE_DUMP(filepath)
#endif

void fds_profile_init(void);
void fds_profile_begin(const char *name);
void fds_profile_end(void);
void fds_profile_dump(const char *filepath);

#endif // FDS_PROFILE_H


/* =====================================================================
 * РЕАЛІЗАЦІЯ
 * ===================================================================== */
#ifdef FDS_PROFILE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <time.h>
#endif

#define FDS_MAX_PROFILE_NODES 1024

typedef struct FdsProfileNode FdsProfileNode;

struct FdsProfileNode {
    const char *name;
    uint64_t start_time;
    uint64_t total_time;
    uint64_t min_time;  // Додано: Мінімальний час виконання
    uint64_t max_time;  // Додано: Максимальний час виконання
    uint32_t call_count;
    
    FdsProfileNode *parent;
    FdsProfileNode *first_child;
    FdsProfileNode *next_sibling;
};

typedef struct {
    FdsProfileNode nodes[FDS_MAX_PROFILE_NODES];
    size_t node_count;
    
    FdsProfileNode *current;
    FdsProfileNode *root;
    
    uint64_t timer_frequency;
} FdsProfileContext;

static FdsProfileContext g_prof_ctx = {0};

static uint64_t fds_get_ticks(void) {
#if defined(_WIN32)
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

void fds_profile_init(void) {
    memset(&g_prof_ctx, 0, sizeof(g_prof_ctx));
#if defined(_WIN32)
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    g_prof_ctx.timer_frequency = freq.QuadPart;
#else
    g_prof_ctx.timer_frequency = 1000000000ULL;
#endif
}

void fds_profile_begin(const char *name) {
    FdsProfileNode *node = NULL;

    if (g_prof_ctx.current) {
        FdsProfileNode *child = g_prof_ctx.current->first_child;
        while (child) {
            if (child->name == name || strcmp(child->name, name) == 0) {
                node = child;
                break;
            }
            child = child->next_sibling;
        }
    } else {
        node = g_prof_ctx.root;
        while (node) {
            if (node->name == name || strcmp(node->name, name) == 0) break;
            node = node->next_sibling;
        }
    }

    if (!node) {
        if (g_prof_ctx.node_count >= FDS_MAX_PROFILE_NODES) return;
        
        node = &g_prof_ctx.nodes[g_prof_ctx.node_count++];
        node->name = name;
        node->parent = g_prof_ctx.current;

        if (g_prof_ctx.current) {
            node->next_sibling = g_prof_ctx.current->first_child;
            g_prof_ctx.current->first_child = node;
        } else {
            node->next_sibling = g_prof_ctx.root;
            g_prof_ctx.root = node;
        }
    }

    node->start_time = fds_get_ticks();
    g_prof_ctx.current = node;
}

void fds_profile_end(void) {
    if (!g_prof_ctx.current) return;
    
    uint64_t end_time = fds_get_ticks();
    uint64_t elapsed = end_time - g_prof_ctx.current->start_time;
    
    g_prof_ctx.current->total_time += elapsed;
    
    // Оновлення MIN та MAX
    if (g_prof_ctx.current->call_count == 0) {
        g_prof_ctx.current->min_time = elapsed;
        g_prof_ctx.current->max_time = elapsed;
    } else {
        if (elapsed < g_prof_ctx.current->min_time) g_prof_ctx.current->min_time = elapsed;
        if (elapsed > g_prof_ctx.current->max_time) g_prof_ctx.current->max_time = elapsed;
    }
    
    g_prof_ctx.current->call_count++;
    g_prof_ctx.current = g_prof_ctx.current->parent;
}

static void fds_print_node(FILE *f, FdsProfileNode *node, int depth) {
    while (node) {
        double time_ms = (double)(node->total_time) * 1000.0 / (double)g_prof_ctx.timer_frequency;
        double avg_ms  = time_ms / (double)node->call_count;
        double min_ms  = (double)(node->min_time) * 1000.0 / (double)g_prof_ctx.timer_frequency;
        double max_ms  = (double)(node->max_time) * 1000.0 / (double)g_prof_ctx.timer_frequency;

        char indent[64] = {0};
        for (int i = 0; i < depth && i < 30; i++) {
            strcat(indent, "  ");
        }
        if (depth > 0) strcat(indent, "|-");

        // ВИПРАВЛЕННЯ: Тепер відступи об'єднуються з іменем перед друком
        char formatted_name[64];
        snprintf(formatted_name, sizeof(formatted_name), "%s%s", indent, node->name);

        fprintf(f, "%-30s | %8u | %10.3f ms | %10.3f ms | %10.3f ms | %10.3f ms\n", 
                formatted_name, node->call_count, time_ms, avg_ms, min_ms, max_ms);

        if (node->first_child) {
            fds_print_node(f, node->first_child, depth + 1);
        }
        node = node->next_sibling;
    }
}

void fds_profile_dump(const char *filepath) {
    FILE *f = fopen(filepath, "w");
    if (!f) return;

    fprintf(f, "=================================================================================================\n");
    fprintf(f, "                                       FDS PROFILER REPORT                                       \n");
    fprintf(f, "=================================================================================================\n");
    fprintf(f, "%-30s | %8s | %13s | %13s | %13s | %13s\n", "NAME", "CALLS", "TOTAL TIME", "AVG TIME", "MIN TIME", "MAX TIME");
    fprintf(f, "-------------------------------------------------------------------------------------------------\n");

    fds_print_node(f, g_prof_ctx.root, 0);

    fprintf(f, "=================================================================================================\n");
    fclose(f);
}

#endif // FDS_PROFILE_IMPLEMENTATION