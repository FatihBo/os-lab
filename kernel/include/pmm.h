#ifndef __PMM_H__
#define __PMM_H__

    #include <common.h>


    #define B * ((uint64_t)1)
    #define KB * (1024)
    #define MB * (1024 KB)
    #define GB * (1024 MB)

    #define MINSIZE     (((uint64_t)4) * sizeof(uintptr_t))
    #define MAXSIZE     (((uint64_t)16) MB)
    #define PAGESIZE    (((uint64_t)4096) B)


    #define PMMLOCKED (1)
    #define PMMUNLOCKED (0)

    #define FENCE (0x17377199)

    typedef struct CHUNK {
        struct CHUNK *fd, *bk;

        union
        {
            int lock;
            int fence;
        } un;
        int slabs_cpu_belongs_to;
    } Chunk;



    //检查Chunk结构指针的ptr的链表完整性
    #define CHUNK_CHECK_LIST(ptr) panic_on(((Chunk*)(ptr))->fd->bk != (ptr) || ((Chunk*)(ptr))->bk->fd != (ptr), "chunk list is corrupted")
    //检查Chunk结构指针的ptr的fence正确性
    #define CHUNK_CHECK_FENCE(ptr) panic_on(((Chunk*)(ptr))->un.fence != FENCE, "chunk is corrupted");


    //将获取buddy[idx]中所有chunk的大小
    #define BUDDY_IDX2CHUNK_SIZE(idx) ((((uintptr_t)1) << (idx)) * PAGESIZE)
    //根据chunk的大小，获取buddy中的下标
    #define BUDDY_CHUNK_SIZE2IDX(size) (log_ceil((size) / PAGESIZE))


    //将获取slabs[X][idx]中所有chunk的大小
    #define SLAB_IDX2CHUNK_SIZE(idx) ((((uintptr_t)1) << (idx)) * MINSIZE)
    //根据chunk的大小，获取slabs中的下标
    #define SLAB_CHUNK_SIZE2IDX(size) (log_ceil((size) / MINSIZE))


    #define CHUNKS_FLAG_SIZE                (1)
    #define CHUNKS_FLAG_BUDDY               (0)
    #define CHUNKS_FLAG_SLAB                (1)

    #define CHUNKS_STATUS_SIZE              (1)
    #define CHUNKS_STATUS_INUSE             (0)
    #define CHUNKS_STATUS_UNUSE             (1)

    #define CHUNKS_IDX_SIZE                 (sizeof(uintptr_t) * 8 - CHUNKS_STATUS_SIZE - CHUNKS_FLAG_SIZE)

    #define CHUNKS_IDX_MASK                 ((((uintptr_t)1) << (CHUNKS_IDX_SIZE)) - 1)
    #define CHUNKS_STATUS_MASK              ((((uintptr_t)1) << (CHUNKS_IDX_SIZE + CHUNKS_STATUS_SIZE)) - 1 - CHUNKS_IDX_MASK)
    #define CHUNKS_FLAG_MASK                ((~((uintptr_t)0)) - CHUNKS_IDX_MASK - CHUNKS_STATUS_MASK)


    #define CHUNKS_VAL_GET_IDX(val)         (((uintptr_t)(val)) & CHUNKS_IDX_MASK)
    #define CHUNKS_VAL_GET_STATUS(val)      ((((uintptr_t)(val)) & CHUNKS_STATUS_MASK) >> (CHUNKS_IDX_SIZE))
    #define CHUNKS_VAL_GET_FLAG(val)        ((((uintptr_t)(val)) & CHUNKS_FLAG_MASK) >> (CHUNKS_IDX_SIZE + CHUNKS_STATUS_SIZE))

    #define CHUNKS_VAL_SET_IDX(ptr, val) \
        (*((uintptr_t*)(ptr))) &= (~CHUNKS_IDX_MASK); \
        (*((uintptr_t*)(ptr))) |= (((uintptr_t)(val)) & CHUNKS_IDX_MASK)
    #define CHUNKS_VAL_SET_STATUS(ptr, val) \
        (*((uintptr_t*)(ptr))) &= (~CHUNKS_STATUS_MASK); \
        (*((uintptr_t*)(ptr))) |= ((((uintptr_t)(val)) << (CHUNKS_IDX_SIZE)) & CHUNKS_STATUS_MASK)
    #define CHUNKS_VAL_SET_FLAG(ptr, val) \
        (*((uintptr_t*)(ptr))) &= (~CHUNKS_FLAG_MASK); \
        (*((uintptr_t*)(ptr))) |= ((((uintptr_t)(val)) << (CHUNKS_IDX_SIZE + CHUNKS_STATUS_SIZE)) & CHUNKS_FLAG_MASK)

    #define CHUNKS_GET_IDX(addr)                (CHUNKS_VAL_GET_IDX(chunks[(((uintptr_t)(addr)) - chunks_base) / PAGESIZE]))
    #define CHUNKS_GET_STATUS(addr)             (CHUNKS_VAL_GET_STATUS(chunks[(((uintptr_t)(addr)) - chunks_base) / PAGESIZE]))
    #define CHUNKS_GET_FLAG(addr)               (CHUNKS_VAL_GET_FLAG(chunks[(((uintptr_t)(addr)) - chunks_base) / PAGESIZE]))
    #define CHUNKS_SET_IDX(addr, idx)           CHUNKS_VAL_SET_IDX(chunks + ((((uintptr_t)(addr)) - chunks_base) / PAGESIZE), (idx))
    #define CHUNKS_SET_STATUS(addr, status)     CHUNKS_VAL_SET_STATUS(chunks + ((((uintptr_t)(addr)) - chunks_base) / PAGESIZE), (status))
    #define CHUNKS_SET_FLAG(addr, flag)         CHUNKS_VAL_SET_FLAG(chunks + ((((uintptr_t)(addr)) - chunks_base) / PAGESIZE), (flag))



    #ifdef DEBUGpmm
        #define debug_pmm(fmt, ...) \
            printf("[pmm cpu%d] %s:%s,%d:\t"fmt"\n", cpu_current(),  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
    #else
        #define debug_pmm(fmt, ...) (panic_on(false, fmt))
    #endif

#endif