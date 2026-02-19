#include <semaphore.h>
#include "toy_factory.h"

typedef struct
{
} assembler_thread_arg;

void *elf_assembler_thread(void *arg)
{
    assembler_thread_arg *ass_arg = (assembler_thread_arg *)arg;
    UNUSED(ass_arg);
    pthread_exit(NULL);
}

typedef struct
{
} wrapper_thread_arg;

void *elf_wrapper_thread(void *arg)
{
    wrapper_thread_arg *wrp_arg = (wrapper_thread_arg *)arg;
    UNUSED(wrp_arg);
    pthread_exit(NULL);
}

typedef struct
{
} santa_thread_arg;

void *santa_sleight_thread(void *arg)
{
    santa_thread_arg *stn_arg = (santa_thread_arg *)arg;
    UNUSED(stn_arg);
    pthread_exit(NULL);
}

int main()
{
    Gift *gift = GetNextGiftOrder();
    AssembleGift(gift);
    WrapGift(gift);
    MillGift(gift);

    Gift *assembly_line[ASSEMBLY_LINE_CAPACITY] = {0};
    Gift *sleight[SLEIGHT_CAPACITY] = {0};
    for (int i = 0; i < ASSEMBLY_LINE_CAPACITY; ++i)
    {
        assembly_line[i] = GetNextGiftOrder();
        AssembleGift(assembly_line[i]);
        WrapGift(assembly_line[i]);
        sleight[i] = assembly_line[i];
        assembly_line[i] = NULL;
    }
    DeliverGifts(sleight, ASSEMBLY_LINE_CAPACITY);

    return 0;
}
