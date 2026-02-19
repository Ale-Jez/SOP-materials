#include "lmap.h"

void usage(int argc, char** argv)
{
    fprintf(stderr, "USAGE: %s iterations bucket_count test_count worker_count output_file\n", argv[0]);
    exit(EXIT_FAILURE);
}
int main(int argc, char** argv)
{
    usage(argc, argv);
    return 0;
}
