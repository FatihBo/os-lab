#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#define KB * (1024ll)
#define MB * (1024ll KB)
/*
 * 环境变量数组
 */
extern char **environ;

void static_load_elf(char *exec, int exec_argc, char *exec_argv[], char **exec_environ); /* 载入64位的静态链接程序exec的各个segment */
void static_load_segments(int exec_fd, Elf64_Ehdr *elf64_ehdr);
/*
 * 初始化程序的堆栈结构，设置argc，argv和envp
 */
void *static_init_stack(int exec_argc, char *exec_argv[], char **exec_environ);

void static_init_registers(Elf64_Ehdr *elf64_ehdr, void *stack);



void static_load_elf(char *exec, int exec_argc, char *exec_argv[], char **exec_environ) {
	int exec_fd = open(exec, O_RDONLY);

	if(exec_fd < 0) { exit(EXIT_FAILURE); }

	Elf64_Ehdr *elf64_ehdr = (Elf64_Ehdr*)mmap(
			NULL,		//addr，即映射的虚拟地址
			4 KB, 		//length，映射的字节长度
			PROT_READ,	//prot，映射内存的属性
			MAP_SHARED,	//flags，映射内存的标志
			exec_fd,	//fd，映射内存的文件描述符
			0		//offset，文件描述符的起始偏移
	);
	if(elf64_ehdr == MAP_FAILED) { exit(EXIT_FAILURE); }

#ifdef DEBUG
	printf("elf64_ehdr address => %p\n", elf64_ehdr);
	printf("elf64_ehdr->e_phentsize => %d\n", elf64_ehdr->e_phentsize);
	printf("elf64_ehdr->e_phnum => %d\n", elf64_ehdr->e_phnum);
	printf("elf64_ehdr->e_phoff => %ld\n", elf64_ehdr->e_phoff);
#endif

	static_load_segments(exec_fd, elf64_ehdr);
	void *stack = static_init_stack(exec_argc, exec_argv, exec_environ);

	static_init_registers(elf64_ehdr, stack);
}



void static_load_segments(int exec_fd, Elf64_Ehdr *elf64_ehdr) {

	//首先读取segments描述符数组，即Program header数组
	Elf64_Phdr *elf64_phdrs = (Elf64_Phdr*)(((uintptr_t)(elf64_ehdr)) + elf64_ehdr->e_phoff);
#ifdef DEBUG
	printf("elf64_phdrs address => %p\n", elf64_phdrs);
	printf("prot: PROT_READ => %#x PROT_WRITE => %#x PROT_EXEC => %#x\n", PROT_READ, PROT_WRITE, PROT_EXEC);
	printf("Elf64_Phdr->p_flags: PF_R => %#x PF_W => %#x PF_X => %#x\n", PF_R, PF_W, PF_X);
#endif


	for(int i = 0; i < elf64_ehdr->e_phnum; ++i) {
		Elf64_Phdr *elf64_phdr = &elf64_phdrs[i];

		if(elf64_phdr->p_type == PT_LOAD) {
			uint64_t prot = 0;

			if(elf64_phdr->p_flags & PF_R) { prot |= PROT_READ; }
			if(elf64_phdr->p_flags & PF_W) { prot |= PROT_WRITE; }
			if(elf64_phdr->p_flags & PF_X) { prot |= PROT_EXEC; }

			void *elf64_phdr_vaddr_align = (void*)((uintptr_t)elf64_phdr->p_vaddr & (~((uintptr_t)elf64_phdr->p_align - 1)));
			size_t elf64_phdr_length_align = elf64_phdr->p_memsz + ((uintptr_t)elf64_phdr->p_vaddr - (uintptr_t)elf64_phdr_vaddr_align);
			off_t elf64_phdr_offset_align = elf64_phdr->p_offset & (~((uintptr_t)elf64_phdr->p_align - 1));

#ifdef DEBUG
			printf("elf64_phdrs[%d] elf64_phdr_vaddr_align => %p, elf64_phdr_length_align => %#lx, elf64_phdr_offset_align => %#lx prots => %#lx\n", i, elf64_phdr_vaddr_align, elf64_phdr_length_align, elf64_phdr_offset_align, prot);
#endif
			void *address = mmap(
					elf64_phdr_vaddr_align,		//addr，即映射的虚拟地址
					elf64_phdr_length_align, 	//length，映射的字节长度
					prot,				//prot，映射内存的属性
					MAP_PRIVATE | MAP_FIXED,	//flags，映射内存的标志
					exec_fd,			//fd，映射内存的文件描述符
					elf64_phdr_offset_align		//offset，文件描述符的起始偏移
			);

			memset(((char*)elf64_phdr->p_vaddr) + elf64_phdr->p_filesz, 0, elf64_phdr->p_memsz - elf64_phdr->p_filesz);

#ifdef DEBUG
			printf("elf64_phdrs[%d] address => %p p_vaddr => %#lx; p_memsz => %#lx; p_flags => %#x p_filesz => %#lx p_align => %#lx\n\n", i, address, elf64_phdr->p_vaddr, elf64_phdr->p_memsz, elf64_phdr->p_flags, elf64_phdr->p_filesz, elf64_phdr->p_align);
#endif
		}
	}
}



void *static_init_stack(int exec_argc, char *exec_argv[], char **exec_environ) {

	void *stack = mmap(
			NULL,				//addr，即映射的虚拟地址
			1 MB, 				//length，映射的字节长度
			PROT_READ | PROT_WRITE,		//prot，映射内存的属性
			MAP_PRIVATE | MAP_ANONYMOUS,	//flags，映射内存的标志
			-1,				//fd，映射内存的文件描述符
			0				//offset，文件描述符的起始偏移
	);


	uintptr_t *sp = (uintptr_t*)((uintptr_t)(stack) + 1 MB - 4 KB);

	//这里设置当前栈帧的顶部
	void *res = (void*)sp;

	*sp++ = exec_argc;

	//压入exec_argv
	for(int i = 0; exec_argv[i]; ++i) {*sp++ = (uintptr_t)exec_argv[i];}
	*sp++ = 0;

	//压入exec_environ
	int environ_idx = 0;
	for(; exec_environ[environ_idx]; ++environ_idx) {
		if(strchr(exec_environ[environ_idx], '_') != exec_environ[environ_idx]) { *sp++ = (uintptr_t)exec_environ[environ_idx]; }
		else {
			char *environ = (char*)malloc(strlen(exec_argv[0] + 3));
			sprintf(environ, "_=%s", exec_argv[0]);
			*sp++ = (uintptr_t)environ;
		}
	}
	//压入exec_environ结束的0
	*sp++ = 0;


	*((Elf64_auxv_t*)sp) = (Elf64_auxv_t){ .a_type = AT_RANDOM, .a_un.a_val = (uintptr_t)(stack) + 1 MB - 16};
	sp = (uintptr_t*)((Elf64_auxv_t*)sp + 1);
	*((Elf64_auxv_t*)sp) = (Elf64_auxv_t){ .a_type = AT_NULL};
	


#ifdef DEBUG
	printf("stack => %p\n", res);
	int argc = *(int*)res, i;
	printf("argc => %d\n", argc);

	char **argv = ((char**)res) + 1;
	for(i = 0; i < argc; ++i) { printf("argv[%d] = %s\n", i, argv[i]); }

	char **environ = argv + argc + 1;
	for(i = 0; environ[i]; ++i) { printf("environ[%d] = %s\n", i, environ[i]); }

	Elf64_auxv_t *elf64_auxv_t = (Elf64_auxv_t*)(environ + i + 1);
	for(i = 0; elf64_auxv_t[i].a_type != AT_NULL; ++i) { printf("auxiliary[%d].a_type = %#lx, auxiliary[%d].a_un.a_val = %#lx\n", i, elf64_auxv_t[i].a_type, i, elf64_auxv_t[i].a_un.a_val); }

#endif

	return res;
}




void static_init_registers(Elf64_Ehdr *elf64_ehdr, void *stack){

	__asm__ volatile(
			"mov %0, %%rsp;"	//将rsp设置为之前初始化的栈的位置
			"xor %%rdx, %%rdx;"	//将rdx设置为atexit部分
			"jmp *%1"		//将待跳转的目标地址存到寄存器中，然后间接跳转
			:
			: "r"(stack), "r"(elf64_ehdr->e_entry)
			: "rdx");
}


int main(int argc, char *argv[]) {

	if(argc == 1) {
		printf("Usage: static-loader file [args...]\n");
		exit(EXIT_SUCCESS);
	}

	/*
	 * argv[0] = "./static-loader"
	 * argv[1] = 待载入程序路径
	 * argv[2 : argc] = 待载入程序参数
	 * argv[argc] = 0
	 */
	static_load_elf(argv[1], argc - 1, argv + 1, environ);

	exit(EXIT_FAILURE);
}