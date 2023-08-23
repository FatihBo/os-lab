#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/*
 * 如果解析到相关的字段，则完成赋值即可
 */
#define true ((unsigned char)(1))
#define false ((unsigned char)(0))
static unsigned char show_pids = false, numeric_sort = false, show_version = false;
static void parse_arguements(int argc, char *argv[]) {
	/*
	 * 第一个是程序名称，因此不需要进行解析
	 */
	for(int i = 1; i < argc; ++i) {
		assert(argv[i]);

	/*
	 * 可以使用hash表进行优化
	 */
		if(!strcmp(argv[i], "-p") || !strcmp(argv[i], "--show-pids")) {
			show_pids = true;
		}else if(!strcmp(argv[i], "-n") || !strcmp(argv[i], "--numeric-sort")) {
			numeric_sort = true;
		}else if(!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
			show_version = true;
			break;
		}else {
			assert(false);
		}
	}
	assert(!argv[argc]);
}


/*
 * 版权字符串常量
 */
const char *version = "pstree (PSmisc) M1_4\n"
"Copyright (C) 1993-2020 H4wk1ns Jiawei\n"
"\n"
"PSmisc comes with ABSOLUTELY NO WARRANTY.\n"
"This is free software, and you are welcome to redistribute it under\n"
"the terms of the GNU General Public License.\n"
"For more information about these matters, see the files named COPYING.";





/*
 * 动态数组结构
 * 当number == capacity时，将其大小扩充一倍
 */
typedef struct ARRAY {
	void *arr;
	long int size, capacity;
} Array;

static Array *__ARRAY_INIT(unsigned int type_size, unsigned int array_capacity) {
	Array *array = (Array*)malloc(sizeof(Array));
	assert(array);

	array->arr = malloc(type_size * array_capacity);
	assert(array->arr);

	array->size = 0;
	array->capacity = array_capacity;
	return array;
}

static void __ARRAY_INSERT(Array *array, unsigned int type_size, void *element) {
	assert(array);

	if(array->size == array->capacity) { 
		array->capacity *= 2;
		array->arr = realloc(array->arr, type_size * array->capacity);
	}
	assert(array->arr);

	unsigned char *src = (unsigned char*)element, *dst = ((unsigned char*)array->arr) + type_size * (array->size++);
	for(int i = 0; i < type_size; ++i) { dst[i] = src[i]; }
}


static void* __ARRAY_FINI(Array *array) {
	if(array == NULL) { return NULL; }

	free(array->arr);
	free(array);
	return NULL;
}
#define Array_Init(type, capacity) (__ARRAY_INIT(sizeof(type), (capacity)))
#define Array_Insert(type, array, element) (__ARRAY_INSERT((array), sizeof(type), (element)))
#define Array_Get(type, array, idx) ((type)(((type*)((array)->arr))[(idx)]))
#define Array_Fini(array) (__ARRAY_FINI(array))


Array *pids = NULL;
const static char *procfs_dir = "/proc/";

/*
 * 如果是数字类型的子目录，则其为pid，返回pid的值
 * 如果不是，则返回-1
 */
static long Dirent_To_Pid(struct dirent *dirItem) {
	assert(dirItem);

	long pid = 0;
	if(dirItem->d_type == DT_DIR) {
		char *name = dirItem->d_name;
		for(int i = 0; name[i]; ++i) {
			if(name[i] > '9' || name[i] < '0') { return -1; }
			pid = pid * 10 + name[i] - '0';
		}
		return pid;
	}
	return -1;
}




/*
 * 动态插入pid
 * 如果大小不够了，调用realloc扩充一倍容量后继续插入
 */
static void Insert_Pid(long pid) {
	assert(pid > 0);
	Array_Insert(pid_t, pids, &pid);
}


/*
 * 获取系统中所有进程pid信息
 */
static void Get_Pids(const char *dirName) {
	pids = Array_Init(pid_t, 0x10);

	DIR *dir = opendir(dirName);
	assert(dir != NULL);

	struct dirent *dirItem = NULL;
	while((dirItem = readdir(dir)) != NULL) {
		long pid = Dirent_To_Pid(dirItem);
		if(pid > 0) { Insert_Pid(pid); }
	}

	closedir(dir);
}



/*
 * 邻接数组的结构, 每一个节点表示一个待输出的进程
 * pid表示当前节点所表示的进程的id信息
 * comm表示当前节点所执行的命令名称
 * adj是动态数组，存放所有子进程在邻接数组中的id信息, Array(int)类型
 */
typedef struct PNODE {
	pid_t pid;
	char *comm;
	Array *son;
} Pnode;


/*
 * 其为Pnode*的动态数组
 */
Array *pnodes = NULL;




/*
 * pnode是按照pid字段升序排列的
 * 使用二分查找，判断pid是否在pnode中
 * 如果不存在，返回-1
 * 如果存在，返回对应的下标
 */
static int Search_Pnode(pid_t pid) {
	if(pnodes == NULL) { return -1; }

	int left = 0, right = pnodes->size - 1;
	while(left <= right) {
		int middle = left + (right - left) / 2;
		if(Array_Get(Pnode*, pnodes, middle)->pid == pid) { return middle; }
		else if(Array_Get(Pnode*, pnodes, middle)->pid < pid) { left = middle + 1; }
		else { right = middle - 1; }
	}

	return -1;
}



/*
 * 读取/proc/[pid]/stat文件
 * 其ppid应该为已经添加过得进程
 * 根据上述文件内容生成PNODE,
 */
static void Get_Pnode(pid_t pid) {
	char pstat[24] = {0}, comm[17] = {0};
	int ppid = 0;
	sprintf(pstat, "/proc/%ld/stat", (long)pid);

	FILE *fstat = fopen(pstat, "r");
	if(!fstat) { return; }

	//读取/proc/[pid]/stat四个字段，第一个，第三个字段仅仅是占位使用，没有实际意义
	fscanf(fstat, "%d (%s %c %d", (int*)pstat, comm, pstat, &ppid);
	if((ppid = Search_Pnode(ppid)) >= 0) {
		Pnode *pnode = (Pnode*)malloc(sizeof(Pnode));
		pnode->pid = pid;
		pnode->son = NULL;

		//由于其第二个字段形式为 (comm) ,因此最后一个括号需要被消除
		comm[strlen(comm) - 1] = 0;
		pnode->comm = (char*)calloc(32, sizeof(char));
		if(show_pids) { sprintf(pnode->comm, "%s(%ld)", comm, (long)pid); }
		else { sprintf(pnode->comm, "%s", comm); }

		if(!Array_Get(Pnode*, pnodes, ppid)->son) { Array_Get(Pnode*, pnodes, ppid)->son = Array_Init(int, 1); }
		Array_Insert(int, Array_Get(Pnode*, pnodes, ppid)->son, &(pnodes->size));
		Array_Insert(Pnode*, pnodes, &pnode);
	}
	fclose(fstat);
}


/*
 * 初始化第一个进程信息
 * 然后从小到大开始遍历即可
 */
static void Get_Pnodes() {
	assert(pids && pids->arr);

	/*
	 * 初始化第一个进程信息
	 */
	pnodes = Array_Init(Pnode*, 1);
	char pstat[24] = {0}, comm[17] = {0};
	sprintf(pstat, "/proc/%ld/stat", (long)(Array_Get(pid_t, pids, 0)));

	FILE *fstat = fopen(pstat, "r");
	if(!fstat) { return; }

	//读取/proc/[pid]/stat两个字段，第一个仅仅是占位使用，没有实际意义
	fscanf(fstat, "%ld (%s", (long*)pstat, comm);
	Pnode *pnode = (Pnode*)malloc(sizeof(Pnode));
	pnode->pid = Array_Get(pid_t, pids, 0);
	pnode->son = NULL;

	//由于其第二个字段形式为 (comm) ,因此最后一个括号需要被消除
	comm[strlen(comm) - 1] = 0;
	pnode->comm = (char*)calloc(32, sizeof(char));
	if(show_pids) { sprintf(pnode->comm, "%s(%ld)", comm, (long)Array_Get(pid_t, pids, 0)); }
	else { sprintf(pnode->comm, "%s", comm); }

	Array_Insert(Pnode*, pnodes, &pnode);
	fclose(fstat);
	/*
	 * 下面开始自小向大遍历整个进程数组
	 */
	for(int i = 1; i < pids->size; ++i) { Get_Pnode(Array_Get(pid_t, pids, i)); }

	pids = Array_Fini(pids);
}



/*
 * 建树所需要的结构
 */
typedef struct PSTACKNODE {
	Pnode *pnode;
	int processedSon;
	unsigned char hasPrint;
} PStackNode;
Array *stack = NULL;



/*
 * 根据stack的信息，打印一/两行输出
 */
static void Print_Line(int line, pid_t leftPid) {
	assert(stack && stack->size);

	/*
	 * 首先执行步骤二，输出|信息
	 */
	if(line) {
		for(int i = 0; i < stack->size; ++i) {

			unsigned char hasPrint = Array_Get(PStackNode*, stack, i)->hasPrint;
			Pnode *pnode = Array_Get(PStackNode*, stack, i)->pnode;
			if(hasPrint) {
				//输出'|'，并用空格进行填充
				putchar('|');
				int len = strlen(pnode->comm);
				for(int j = 1; j < len; ++j) { putchar(' '); }
			}else {
				int len = strlen(pnode->comm);
				for(int j = 0; j < len; ++j) { putchar(' '); }
			}
			putchar(' ');
			putchar(' ');
		}
		putchar('\n');
	}


	/*
	 * 接着执行步骤三，输出+/命令信息
	 */
	unsigned char needDotLine = 0;
	for(int i = 0; i < stack->size; ++i) {
		PStackNode* pstacknode = Array_Get(PStackNode*, stack, i);
		Pnode *pnode = pstacknode->pnode;

		if(needDotLine) {
			putchar('-');
			putchar('-');
		}else if(i){
			putchar(' ');
			putchar(' ');
		}



		if(!pstacknode->hasPrint) {
			printf("%s", pnode->comm);
			pstacknode->hasPrint = 1;
			if(!line) { needDotLine = 1; }
		}
		else if(leftPid == pnode->pid) {
			int len = strlen(pnode->comm);
			putchar('+');
			for(int j = 1; j < len; ++j) { putchar('-'); }
			needDotLine = 1;
		}
		else {
			int len = strlen(pnode->comm);
			putchar('|');
			for(int j = 1; j < len; ++j) { putchar(' '); }
		}
	}
	putchar('\n');
}



/*
 * 即执行第四步，开始回退
 */
static void Go_Back() {
	while(stack->size) {
		PStackNode* p_stack_node = Array_Get(PStackNode*, stack, stack->size - 1);
		if(!p_stack_node->pnode->son || (p_stack_node->processedSon + 1 >= p_stack_node->pnode->son->size)) {
			Array_Fini(p_stack_node->pnode->son);
			free(p_stack_node->pnode->comm);
			free(p_stack_node->pnode);
			free(p_stack_node);
			--stack->size;
		}else if(++p_stack_node->processedSon < p_stack_node->pnode->son->size) {
			return;
		}


	}
}

/*
 * 开始构建进程树并输出
 *
 * 即初始化栈
 * 遍历栈即可
 */
static void Build_Print(void) {
	assert(pnodes && pnodes->arr);


	stack = Array_Init(PStackNode*, 1);
	//初始化进程号为1的进程
	PStackNode *initPStackNode = (PStackNode*)malloc(sizeof(PStackNode));
	initPStackNode->pnode = Array_Get(Pnode*, pnodes, 0);
	initPStackNode->processedSon = initPStackNode->hasPrint = 0;
	Array_Insert(PStackNode*, stack, &initPStackNode);

	for(int line = 0; stack->size; ++line) {
		PStackNode *pstacknode = Array_Get(PStackNode*, stack, stack->size - 1);
		pid_t leftPid = pstacknode->pnode->pid;

		//开始遍历进程，直到没有子进程
		do{
			int sonIdx = Array_Get(int, pstacknode->pnode->son, pstacknode->processedSon);
			pstacknode = (PStackNode*)malloc(sizeof(PStackNode));
			pstacknode->pnode = Array_Get(Pnode*, pnodes, sonIdx);
			pstacknode->processedSon = pstacknode->hasPrint = 0;
			Array_Insert(PStackNode*, stack, &pstacknode);
		}while(pstacknode->pnode->son && (pstacknode->processedSon < pstacknode->pnode->son->size));

		Print_Line(line, leftPid);
		Go_Back();
	}
}




int main(int argc, char *argv[]) {
	parse_arguements(argc, argv);

	if(show_version) {
		puts(version);
		return 0;
	}


	Get_Pids(procfs_dir);
	Get_Pnodes();
	Build_Print();
	return 0;
}
