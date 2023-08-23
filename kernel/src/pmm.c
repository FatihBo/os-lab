#include <pmm.h>

static void
lock_init(int *lock)
{
  atomic_xchg(lock, PMMUNLOCKED);
}


static void
lock_acquire(int *lock)
{
  while(atomic_xchg(lock, PMMLOCKED) == PMMLOCKED) {;}
}



static void
lock_release(int *lock)
{
  panic_on(atomic_xchg(lock, PMMUNLOCKED) != PMMLOCKED, "lock is not acquire");
}



static int
lock_try_acquire(int *lock)
{
  return atomic_xchg(lock, PMMLOCKED);
}

/*
 * 计算ceil(log2(n))
 * 0 < n <= 2 ^ (sizeof(uintptr_t) * 8 - 1)
 */
static uintptr_t
log_ceil(uintptr_t n)
{
  panic_on(n <= 0, "log error n");

  if(n == 1) {return 0;}

  uintptr_t left = 0, right = sizeof(uintptr_t) * 8 - 1;

  --n;
  while(left <= right) {
    uintptr_t middle = left + (right - left) / 2;

    if(n >> middle) { left = middle + 1; }
    else { right = middle - 1; }
  }

  return left;
}



/*
 * 将申请的内存大小向上对齐到最接近的幂
 * 如果小于MINSIZE的内存统一以MINSIZE为主
 */
static size_t
request_size2mem_size(size_t size)
{
  size = ((size_t)1) << log_ceil(size);

  panic_on(size > MAXSIZE, "size is too big");

  return size < MINSIZE ? MINSIZE : size;
}


uintptr_t *chunks, chunks_size = -1;


static int buddys_size = -1, slabs_size = -1;


static Chunk *buddys = NULL;


static Chunk **slabs = NULL;


static uintptr_t chunks_base = 0;



static void
list_insert(Chunk *chunk)
{
  chunk->un.fence = FENCE;

  Chunk *head = NULL;
  
  switch(CHUNKS_GET_FLAG(chunk)) {
    case CHUNKS_FLAG_BUDDY:
      panic_on(CHUNKS_GET_IDX(chunk) >= buddys_size, "error idx");
      head = &(buddys[CHUNKS_GET_IDX(chunk)]);
      break;

    case CHUNKS_FLAG_SLAB:
      panic_on(CHUNKS_GET_IDX(chunk) >= slabs_size, "error idx");
      head = &slabs[chunk->slabs_cpu_belongs_to][CHUNKS_GET_IDX(chunk)];
      break;

    default:
      panic("error flag");
  }

  panic_on(lock_try_acquire(&(head->un.lock)) != PMMLOCKED, "don't have the lock");

  CHUNK_CHECK_LIST(head);
  Chunk *bck = head, *fwd = bck->fd;

  /*
   * 将chunk插入到head和head->fd之间
   */
  chunk->bk = bck;
  chunk->fd = fwd;
  bck->fd = chunk;
  fwd->bk = chunk;

  CHUNK_CHECK_FENCE(chunk);
  CHUNK_CHECK_LIST(chunk);
}



static void
list_remove(Chunk *chunk)
{
  Chunk *head = NULL;
  switch(CHUNKS_GET_FLAG(chunk)) {

    case CHUNKS_FLAG_BUDDY:
      panic_on(CHUNKS_GET_IDX(chunk) >= buddys_size, "error idx");
      head = &buddys[CHUNKS_GET_IDX(chunk)];

      break;

    case CHUNKS_FLAG_SLAB:
      panic_on(CHUNKS_GET_IDX(chunk) >= slabs_size, "error idx");
      head = &slabs[chunk->slabs_cpu_belongs_to][CHUNKS_GET_IDX(chunk)];

      break;

    default:
      panic("error flag");
  }

  panic_on(lock_try_acquire(&(head->un.lock)) != PMMLOCKED, "don't have the lock");

  CHUNK_CHECK_FENCE(chunk);
  CHUNK_CHECK_LIST(chunk);

  Chunk *fwd = chunk->fd, *bck = chunk->bk;
  fwd->bk = bck;
  bck->fd = fwd;
}



static void
buddys_init(uint64_t start, uint64_t end)
{
  panic_on(start % MAXSIZE, "error start");
  panic_on(end % MAXSIZE, "error end");

  chunks_base = start;
  for(int i = 0; i < buddys_size; ++i) {
    buddys[i].fd = buddys[i].bk = &buddys[i];
    lock_init(&(buddys[i].un.lock));
  }

  for(uintptr_t iter = start; iter < end; iter += MAXSIZE) {
    CHUNKS_SET_IDX(iter, buddys_size - 1);
    CHUNKS_SET_STATUS(iter, CHUNKS_STATUS_UNUSE);
    CHUNKS_SET_FLAG(iter, CHUNKS_FLAG_BUDDY);
    
    lock_acquire(&(buddys[buddys_size - 1].un.lock));
    list_insert((Chunk*)iter);
    lock_release(&(buddys[buddys_size - 1].un.lock));
  }

  printf("buddys_init(%X, %X), MAXSIZE:%X\n", start, end, (uint64_t)MAXSIZE);
}



static Chunk *
buddys_alloc(size_t size)
{
  int idx = BUDDY_CHUNK_SIZE2IDX(size);
  uintptr_t res = 0;

  panic_on(size < PAGESIZE, "size is too small");
  panic_on(size > MAXSIZE, "size is too big");
  panic_on(BUDDY_IDX2CHUNK_SIZE(idx) != size, "size is invalid");

  int iter = idx;
  while(iter < buddys_size) {

    Chunk *head = &(buddys[iter]);
    lock_acquire(&(head->un.lock));

    if(head->fd != head) {
      res = (uintptr_t)head->fd;

      panic_on(CHUNKS_GET_IDX(res) != iter, "error idx");
      panic_on(CHUNKS_GET_STATUS(res) != CHUNKS_STATUS_UNUSE, "error status");
      panic_on(CHUNKS_GET_FLAG(res) != CHUNKS_FLAG_BUDDY, "error flag");

      CHUNKS_SET_STATUS(res, CHUNKS_STATUS_INUSE);
      list_remove((Chunk*)res);

      lock_release(&(head->un.lock));
      break;
    }

    lock_release(&(head->un.lock));
    ++iter;
  }

  if(res == 0) { return NULL; }

  while(iter-- > idx) {

    Chunk *chunk = (Chunk*)(uintptr_t)(res + BUDDY_IDX2CHUNK_SIZE(iter)), *head = &(buddys[iter]);

    CHUNKS_SET_FLAG(chunk, CHUNKS_FLAG_BUDDY);
    CHUNKS_SET_STATUS(chunk, CHUNKS_STATUS_UNUSE);
    CHUNKS_SET_IDX(chunk, iter);

    lock_acquire(&(head->un.lock));
    list_insert(chunk);
    lock_release(&(head->un.lock));
  }

  CHUNKS_SET_IDX(res, idx);
  CHUNKS_SET_STATUS(res, CHUNKS_STATUS_INUSE);

  debug_pmm("buddys_alloc(%X) res:%X", (uint64_t)size, (uint64_t)res);

  return (Chunk*)res;
}



static void
buddys_free(Chunk *chunk)
{
  panic_on(CHUNKS_GET_IDX(chunk) >= chunks_size, "error idx");
  panic_on(CHUNKS_GET_STATUS(chunk) != CHUNKS_STATUS_INUSE, "error status");
  panic_on(CHUNKS_GET_FLAG(chunk) != CHUNKS_FLAG_BUDDY, "error flag");

  int idx = CHUNKS_GET_IDX(chunk);
  Chunk *head = &(buddys[idx]);

  lock_acquire(&(head->un.lock));

  while(idx < buddys_size - 1) {
    uintptr_t size = BUDDY_IDX2CHUNK_SIZE(idx);
    Chunk *another_chunk = (Chunk*)(((uintptr_t)chunk) ^ size);

    /*
     * 如果不满足以下条件
     * 则说明另一个chunk不能进行合并
     */
    if((CHUNKS_GET_IDX(another_chunk) != idx) || (CHUNKS_GET_STATUS(another_chunk) != CHUNKS_STATUS_UNUSE) || (CHUNKS_GET_FLAG(another_chunk) != CHUNKS_FLAG_BUDDY)) { break;}

    CHUNKS_SET_STATUS(another_chunk, CHUNKS_STATUS_INUSE);
    list_remove(another_chunk);
    lock_release(&(head->un.lock));


    chunk = chunk < another_chunk ? chunk : another_chunk;
    idx += 1;
    
    
    head = &(buddys[idx]);
    lock_acquire(&(head->un.lock));
  }


  CHUNKS_SET_IDX(chunk, idx);
  CHUNKS_SET_STATUS(chunk, CHUNKS_STATUS_UNUSE);

  debug_pmm("buddys_free(%X) size:%X", (uint64_t)(uintptr_t)chunk, (uint64_t)(BUDDY_IDX2CHUNK_SIZE(CHUNKS_GET_IDX(chunk))));

  list_insert(chunk);
  lock_release(&(head->un.lock));
}



static void
slabs_init(void)
{
  for(int cpu = 0; cpu < cpu_count(); ++cpu) {
    for(int idx = 0; idx < slabs_size; ++idx) {
      lock_init(&(slabs[cpu][idx].un.lock));
      slabs[cpu][idx].fd = slabs[cpu][idx].bk = &(slabs[cpu][idx]);
      

      printf("slabs[%D][%D]: %X; slabs[%D][%D].fd: %X; slabs[%D][%D].bk: %X\n", (uint64_t)cpu, (uint64_t)idx, (uint64_t)(uintptr_t)&(slabs[cpu][idx]),
        (uint64_t)cpu, (uint64_t)idx, (uint64_t)(uintptr_t)(slabs[cpu][idx].fd),
        (uint64_t)cpu, (uint64_t)idx, (uint64_t)(uintptr_t)(slabs[cpu][idx].bk)
      );
    }
  }

  printf("slabs_init\n");
}


static Chunk *
slabs_alloc(size_t size)
{
  int idx = SLAB_CHUNK_SIZE2IDX(size);
  uintptr_t res = 0;

  panic_on(size < MINSIZE, "size is too small");
  panic_on(size >= PAGESIZE, "size is too big");
  panic_on(SLAB_IDX2CHUNK_SIZE(idx) != size, "size is invalid");

  int cpu = cpu_current();
  Chunk *head = &(slabs[cpu][idx]);
  do{

    if(cpu == cpu_current()) { lock_acquire(&(head->un.lock)); }
    else {
      if(lock_try_acquire(&(head->un.lock)) == PMMLOCKED) { goto PREPARE_BEFORE_NEXT;}
    }

    if(head->fd != head) {
      res = (uintptr_t)head->fd;

      panic_on(CHUNKS_GET_IDX(res) != idx, "error idx");
      panic_on(CHUNKS_GET_STATUS(res) != CHUNKS_STATUS_INUSE, "error status");
      panic_on(CHUNKS_GET_FLAG(res) != CHUNKS_FLAG_SLAB, "error flag");

      list_remove((Chunk*)res);
      lock_release(&(head->un.lock));
      return (Chunk*)res;
    }
    lock_release(&(head->un.lock));

    PREPARE_BEFORE_NEXT:
      cpu = (cpu + 1) % cpu_count();
      head = &(slabs[cpu][idx]);

  }while(cpu != cpu_current());


  panic_on(res != 0, "error res");
  if((res = (uintptr_t)buddys_alloc(PAGESIZE)) == 0) { return NULL; }
  panic_on(BUDDY_IDX2CHUNK_SIZE(CHUNKS_GET_IDX(res)) != PAGESIZE, "error idx");
  panic_on(CHUNKS_GET_STATUS(res) != CHUNKS_STATUS_INUSE, "error status");
  panic_on(CHUNKS_GET_FLAG(res) != CHUNKS_FLAG_BUDDY, "error flag");

  CHUNKS_SET_FLAG(res, CHUNKS_FLAG_SLAB);
  CHUNKS_SET_IDX(res, idx);

  uintptr_t gap = SLAB_IDX2CHUNK_SIZE(idx);
  for(uintptr_t iter = gap; iter < PAGESIZE; iter += gap) {
    Chunk *chunk = (Chunk*)(iter + res);
    chunk->slabs_cpu_belongs_to = cpu;
    lock_acquire(&(head->un.lock));
    list_insert(chunk);
    lock_release(&(head->un.lock));
  }

  debug_pmm("slabs_alloc(%X) res:%X", (uint64_t)size, (uint64_t)res);

  return (Chunk*)res;
}




static void
slabs_free(Chunk *chunk)
{
  panic_on(CHUNKS_GET_IDX(chunk) >= slabs_size, "error idx");
  panic_on(CHUNKS_GET_STATUS(chunk) != CHUNKS_STATUS_INUSE, "error status");
  panic_on(CHUNKS_GET_FLAG(chunk) != CHUNKS_FLAG_SLAB, "error flag");

  debug_pmm("slabs_free(%X) size:%X", (uint64_t)(uintptr_t)chunk, (uint64_t)(SLAB_IDX2CHUNK_SIZE(CHUNKS_GET_IDX(chunk))));

  chunk->slabs_cpu_belongs_to = cpu_current();
  Chunk *head = &(slabs[cpu_current()][CHUNKS_GET_IDX(chunk)]);
  lock_acquire(&(head->un.lock));
  list_insert(chunk);
  lock_release(&(head->un.lock));
}



static void *
kalloc(size_t size)
{
  if(size > MAXSIZE) { return NULL; }
  if(size < MINSIZE) { size = MINSIZE; }

  size_t size_align = request_size2mem_size(size);

  panic_on(size_align < MINSIZE, "size is too small");
  panic_on(size_align > MAXSIZE, "size is too big");

  void *res = size_align < PAGESIZE ? slabs_alloc(size_align) : buddys_alloc(size_align);

  debug_pmm("kalloc(%X) = %X", (uint64_t)size, (uint64_t)(uintptr_t)res);

  return res;
}


static void
kfree(void *ptr)
{
  if(ptr == NULL) {return;}
  panic_on((uintptr_t)ptr > (uintptr_t)heap.end, "invalid ptr");
  panic_on((uintptr_t)ptr < (uintptr_t)chunks_base, "invalid ptr");

  switch (CHUNKS_GET_FLAG(ptr))
  {
  case CHUNKS_FLAG_BUDDY:
    buddys_free(ptr);
    break;
  
  case CHUNKS_FLAG_SLAB:
    slabs_free(ptr);
    break;

  default:
    panic("error flag");
  }

  debug_pmm("kfree(%X)", (uint64_t)(uintptr_t)ptr);
}

static void pmm_init() {

  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%X, %X)\n", (int)(pmsize >> 20), (uint64_t)(uintptr_t)heap.start, (uint64_t)(uintptr_t)heap.end);

  chunks = (uintptr_t*)heap.start;
  chunks_size = (((uintptr_t)heap.end) - ((uintptr_t)heap.start) + PAGESIZE - 1) / PAGESIZE;
  printf("chunks: [%X, %X), chunks_size: %D\n", (uint64_t)(uintptr_t)chunks, (uint64_t)(uintptr_t)(chunks + chunks_size), (uint64_t)chunks_size);

  #ifdef TESTpmm
  //首先测试其log_ceil函数
    uint64_t n = 0;

    n = 1 << 0;
    panic_on(log_ceil(n) != 0, "log_ceil(1 << 0) != 0");

    n = 1 << 1;
    panic_on(log_ceil(n) != 1, "log_ceil(1 << 1) != 1");

    n = (1 << 1) + 1;
    panic_on(log_ceil(n) != 2, "log_ceil((1 << 1) + 1) != 2");

    n = (1 << 2) + 1;
    panic_on(log_ceil(n) != 3, "log_ceil((1 << 2) + 1) != 3");

    n = ((uint64_t)1) << (sizeof(uintptr_t) * 8 - 1);
    panic_on(log_ceil(n) != (sizeof(uintptr_t) * 8 - 1), "log_ceil(((uint64_t)1) << (sizeof(uintptr_t) * 8 - 1)) != (sizeof(uintptr_t) * 8 - 1)");

    n = (((uint64_t)1) << (sizeof(uintptr_t) * 8 - 2)) + 1;
    panic_on(log_ceil(n) != (sizeof(uintptr_t) * 8 - 1), "log_ceil((((uint64_t)1) << (sizeof(uintptr_t) * 8 - 2)) + 1) != (sizeof(uintptr_t) * 8 - 1)");


    //其次测试相关的CHUNKS_宏
    for(int i = 0; i < 0x40; ++i) {
      uintptr_t idx = rand() & CHUNKS_IDX_MASK, status = rand() % 2, flag = rand() % 2;
      uintptr_t ptr = rand() << 8;
      CHUNKS_VAL_SET_IDX(&ptr, idx);
      CHUNKS_VAL_SET_STATUS(&ptr, status);
      CHUNKS_VAL_SET_FLAG(&ptr, flag);
      panic_on(idx != CHUNKS_VAL_GET_IDX(ptr), "error CHUNKS_VAL_GET_IDX");
      panic_on(status != CHUNKS_VAL_GET_STATUS(ptr), "error CHUNKS_VAL_GET_STATUS");
      panic_on(flag != CHUNKS_VAL_GET_FLAG(ptr), "error CHUNKS_VAL_GET_FLAG");
      panic_on(ptr != (((flag << (CHUNKS_IDX_SIZE + CHUNKS_STATUS_SIZE)) | (status << (CHUNKS_IDX_SIZE)) | (idx))), "error CHUNKS_VAL_SET_FLAG or CHUNKS_VAL_SET_IDX");

      flag ^= 1;
      CHUNKS_VAL_SET_FLAG(&ptr, flag);
      panic_on(flag != CHUNKS_VAL_GET_FLAG(ptr), "error CHUNKS_VAL_GET_FLAG");

      status ^= 1;
      CHUNKS_VAL_SET_STATUS(&ptr, status);
      panic_on(status != CHUNKS_VAL_GET_STATUS(ptr), "error CHUNKS_VAL_GET_FLAG");

      idx = rand() & CHUNKS_IDX_MASK;
      CHUNKS_VAL_SET_IDX(&ptr, idx);
      panic_on(idx != CHUNKS_VAL_GET_IDX(ptr), "error CHUNKS_VAL_GET_IDX");

      printf("val: %X flag: %X status: %x idx:%X\n", (uint64_t)ptr, (uint64_t)flag, (uint64_t)status, (uint64_t)idx);


      ptr = chunks_base +  (rand() % chunks_size) * PAGESIZE;
      CHUNKS_SET_IDX((Chunk*)ptr, idx);
      panic_on(idx != CHUNKS_GET_IDX((Chunk*)ptr), "error CHUNKS_GET_IDX");
      CHUNKS_SET_STATUS(ptr, status);
      panic_on(status != CHUNKS_GET_STATUS((Chunk*)ptr), "error CHUNKS_GET_STATUS");
      CHUNKS_SET_FLAG((Chunk*)ptr, flag);
      panic_on(flag != CHUNKS_GET_FLAG((Chunk*)ptr), "error CHUNKS_GET_FLAG");
      panic_on(chunks[(ptr - chunks_base) / PAGESIZE] != (((flag << (CHUNKS_IDX_SIZE + CHUNKS_STATUS_SIZE)) | (status << (CHUNKS_IDX_SIZE)) | (idx))), "error CHUNKS_SET_");
    }


    //将chunks所有的值填充为CHUNKS_FLAG_SLAB | CHUNKS_STATUS_INUSE | CHUNKS_IDX_MASK，方便提前检查错误
    for(uintptr_t iter = chunks_base; iter < (uintptr_t)(heap.end); iter += PAGESIZE) {
      CHUNKS_SET_IDX(iter, CHUNKS_IDX_MASK);
      CHUNKS_SET_STATUS(iter, CHUNKS_STATUS_INUSE);
      CHUNKS_SET_FLAG(iter, CHUNKS_FLAG_SLAB);
    }
  #endif

  
  buddys = (Chunk*)(chunks + chunks_size);
  buddys_size = log_ceil(MAXSIZE / PAGESIZE) + 1;
  printf("buddys: [%X, %X), chunks_size: %D\n", (uint64_t)(uintptr_t)buddys, (uint64_t)(uintptr_t)(buddys + buddys_size), (uint64_t)buddys_size);

  slabs = (Chunk**)(buddys + buddys_size);
  printf("slabs: [%X, %X), size: %D\n", (uint64_t)(uintptr_t)slabs, (uint64_t)(uintptr_t)(slabs + cpu_count()), (uint64_t)cpu_count());
  slabs_size = log_ceil(PAGESIZE / 2 / MINSIZE) + 1;
  for(int i = 0; i < cpu_count(); ++i) {
    slabs[i] = ((Chunk*)(slabs + cpu_count())) + i * slabs_size;
    printf("slabs[%d]: [%X, %X), size: %D\n", i, (uint64_t)(uintptr_t)slabs[i], (uint64_t)(uintptr_t)(slabs[i] + slabs_size), (uint64_t)slabs_size);
  }


  buddys_init((((uint64_t)(uintptr_t)(slabs[cpu_count() - 1] + slabs_size)) + MAXSIZE - 1) & (~(MAXSIZE - 1)), ((uint64_t)(uintptr_t)heap.end) & (~(MAXSIZE - 1)));
  slabs_init();
}


MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};



#if defined TESTbuddy
  //测试buddys方式
  void test_pmm() {
    int size = 0;
    #define CAPACITY (500)
    char *array[CAPACITY] = {NULL};
    int array_size[CAPACITY] = {0};

    while (1) {
      switch (rand() % 2)
      {
        case 0:
          if(size < CAPACITY) {
            array_size[size] = BUDDY_IDX2CHUNK_SIZE(rand() % buddys_size);
            array[size] = (char*)buddys_alloc(array_size[size]);

            if(array[size] != NULL) {
              printf("cpu%d:buddys_alloc(%X) = %X\n", (int)cpu_current(), (uint64_t)(uintptr_t)array_size[size], (uint64_t)(uintptr_t)array[size]);

              //填充，方便进行调试
              for(int i = 0; i < array_size[size]; ++i) { array[size][i] = (char)array_size[size];}
              ++size;
            }
          }
          break;

        case 1:
          if(size) {
            --size;
            for(int i = 0; i < array_size[size]; ++i) {
              panic_on(array[size][i] != (char)array_size[size], "corrupted");
            }
            buddys_free((Chunk*)array[size]);
            printf("cpu%d:buddys_free(%X)\n", (int)cpu_current(), (uint64_t)(uintptr_t)array[size]);
          }
          break;
      }
    }
  }
#elif defined TESTslab
  //测试slabs方式
  void test_pmm() {
    int size = 0;
    #define CAPACITY (500)
    char *array[CAPACITY] = {NULL};
    int array_size[CAPACITY] = {0};
    while (1) {
      switch (rand() % 2)
      {
        case 0:
          if(size < CAPACITY) {
            array_size[size] = SLAB_IDX2CHUNK_SIZE(rand() % slabs_size);
            array[size] = (char*)slabs_alloc(array_size[size]);

	          panic_on(array[size] == NULL, "not enough space");
            printf("cpu%d:slabs_alloc(%X) = %X\n", (int)cpu_current(), (uint64_t)(uintptr_t)array_size[size], (uint64_t)(uintptr_t)array[size]);

            //填充，方便进行调试
            for(int i = 0; i < array_size[size]; ++i) { array[size][i] = (char)array_size[size];}
            ++size;
          }
          break;

        case 1:
          if(size) {
            --size;
            for(int i = 0; i < array_size[size]; ++i) {
              panic_on(array[size][i] != (char)array_size[size], "corrupted");
            }
            slabs_free((Chunk*)array[size]);
            printf("cpu%d:slabs_free(%X)\n", (int)cpu_current(), (uint64_t)(uintptr_t)array[size]);
          }
          break;
      }
    }
  }
#else
  void test_pmm() {
    int size = 0;
    #define CAPACITY (500)
    char *array[CAPACITY] = {NULL};
    int array_size[CAPACITY] = {0};
    int total = 0, counts[3] = {80, 19, 1};
    uintptr_t BOUNDARY1 = 128, BOUNDARY2 = 32 KB, BOUNDARY3 = MAXSIZE;
    while (1) {
      switch (rand() % 2)
      {
        case 0:
          if(size < CAPACITY) {
            for(int mode = rand() % 3; ; mode = (mode + 1) % 3) {
              if(counts[mode]) {
                --counts[mode];
                switch(mode) {
                  case 0:
                    array_size[size] = 1 + (rand() % BOUNDARY1);
                    break;
                  case 1:
                    array_size[size] = PAGESIZE * (1 + (rand() % (BOUNDARY2 / PAGESIZE)));
                    break;
                  case 2:
                    array_size[size] = BUDDY_IDX2CHUNK_SIZE(BUDDY_CHUNK_SIZE2IDX(BOUNDARY2) + 1 + (rand() % (BUDDY_CHUNK_SIZE2IDX(BOUNDARY3) - BUDDY_CHUNK_SIZE2IDX(BOUNDARY2))));
                    break;
                }
                break;
              }
            }
            if(++total == 100) {
              total = 0;
              counts[0] = 80;
              counts[1] = 19;
              counts[2] = 1;
            }

            array[size] = (char*)pmm->alloc(array_size[size]);
            panic_on(array[size] == NULL, "not enough space");

            printf("cpu%d:pmm->alloc(%X) = %X\n", (int)cpu_current(), (uint64_t)(uintptr_t)array_size[size], (uint64_t)(uintptr_t)array[size]);

            //填充，方便进行调试
            for(int i = 0; i < array_size[size]; ++i) { array[size][i] = (char)array_size[size];}
            ++size;
          }
          break;

        case 1:
          if(size) {
            --size;
            for(int i = 0; i < array_size[size]; ++i) {
              panic_on(array[size][i] != (char)array_size[size], "corrupted");
            }
            pmm->free((Chunk*)array[size]);
            printf("cpu%d:pmm->free(%X)\n", (int)cpu_current(), (uint64_t)(uintptr_t)array[size]);
            array[size] = NULL;
            array_size[size] = 0;
          }
          break;
      }
    }
  }
#endif
