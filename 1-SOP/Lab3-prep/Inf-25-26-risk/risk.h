#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_NEIGHBORS 6
#define FRUSTRATION_LIMIT 3
#define MOVE_MS 140
#define SHOW_MS 500
/**
 * @struct region
 * @brief The unit of a playing board
 */
typedef struct region
{
    int8_t owner;                    /* Symbol of the player that controls this region */
    int8_t neighbors[MAX_NEIGHBORS]; /* Array of indexes of neighboring regions */
    int8_t num_neighbors;            /* The number of neighboring regions */
} region_t;

/**
 * @brief Loads a playing board from a file
 *
 * Parses the board file format and initializes all regions' owner to '-'
 *
 * @param file The file to load the board from
 * @param num_regions The value under this pointer will be set to the number of regions in the returned array
 * @return An array containing all regions described in the file
 */
region_t* load_regions(char* file, int* num_regions)
{
    FILE* f = fopen(file, "r");
    if (!f)
        ERR("fopen");
    *num_regions = 0;
    {
        int c;
        while ((c = fgetc(f)) != EOF)
            if (c == '\n')
                (*num_regions)++;
        if (fseek(f, 0, SEEK_SET) == -1)
            ERR("fseek");
    }
    region_t* regions = malloc(sizeof(region_t) * *num_regions);
    if (!regions)
        ERR("malloc");
    memset(regions, 0, sizeof(region_t) * *num_regions);

    char* line = NULL;
    size_t capacity = 0;
    int length = 0;
    int i_region = 0;
    while ((length = getline(&line, &capacity, f)) != -1)
    {
        region_t* r = &regions[i_region];
        char* cur = strtok(line, ";");
        r->owner = '-';
        if (*cur != '\n')
            while (cur != NULL)
            {
                if (r->num_neighbors >= MAX_NEIGHBORS)
                {
                    fprintf(stderr, "Exceeded max neighbor count on line %d\n", i_region);
                    exit(EXIT_FAILURE);
                }
                r->neighbors[r->num_neighbors] = atoi(cur);
                cur = strtok(NULL, ";");
                r->num_neighbors++;
            }
        i_region++;
    }
    if (!feof(f))
        ERR("getline");
    free(line);
    fclose(f);
    return regions;
}

void ms_sleep(unsigned int ms_time)
{
    struct timespec ts = {0, ms_time * 1000000};
    while (nanosleep(&ts, &ts))
        ;
}
