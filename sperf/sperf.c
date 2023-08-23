#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <regex.h>
#include <assert.h>

regex_t regex_syscall, regex_time;

#define re_syscall ("^[^\\()+]")
#define re_time ("[0-9]+\\.[0-9]+>$")

#define MAX_STRACE_OUTPUT_SIZE (4096)
#define PATH_SPLIT (':')
#define STRACE_EXECUTE ("/strace")
#define TIME_INTERVAL_SECONDS (1)

char *STRACE_SHOW_TIME = "-T", *STRACE_OUTPUT = "-o";

extern char **environ;


#define SYSCALL_SIZE	(400)
typedef struct SYSCALL_INFO {
	const char *syscall;
	int syscall_name_size;
	long long time;
} Syscall_Info;
Syscall_Info *syscall_info_sort_by_name[SYSCALL_SIZE] = {NULL}, *syscall_info_sort_by_time[SYSCALL_SIZE] = {NULL};
int syscall_info_number = 0;
long long syscall_time_total = 0;



#define SYSCALL_INFO_SHOW_SIZE (5)

#define SYSCALL_INFO_WINDOW_HEIGHT (20)
#define SYSCALL_INFO_WINDOW_WIDTH (40)

#define syscall_info_show_format(color) ("\e["#color";37m%s\e[0m")
const char *syscall_info_show_formats[SYSCALL_INFO_SHOW_SIZE] = {syscall_info_show_format(42), syscall_info_show_format(45), syscall_info_show_format(43), syscall_info_show_format(44), syscall_info_show_format(46)};
#define syscall_info_show(idx, str) (fprintf(stderr, syscall_info_show_formats[(idx)], (str)))

#define syscall_info_show_move(opcode) (fprintf(stderr, "\e[1"#opcode))
void syscall_info_show_move_up(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(A); }
}

void syscall_info_show_move_down(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(B); }
}

void syscall_info_show_move_left(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(D); }
}

void syscall_info_show_move_right(int idx) {
	for(int i = 0; i < idx; ++i) { syscall_info_show_move(C); }
}

#define syscall_info_show_position_init() (fprintf(stderr, "\e[0;0H"))



void syscall_info_display() {
	
	syscall_info_show_position_init();
	int left_top_row = 0, left_top_col = 0, syscall_info_idx = 0, height = 0, width = 0;
	long long syscall_info_show_time_total = 0;
	for(int i = 0; i < SYSCALL_INFO_SHOW_SIZE && i < syscall_info_number; ++i) { syscall_info_show_time_total += syscall_info_sort_by_time[i]->time; }

	for(; syscall_info_idx + 1 < SYSCALL_INFO_SHOW_SIZE && syscall_info_idx + 1 < syscall_info_number; ++syscall_info_idx) {

		if(syscall_info_idx & 1) {
			width = SYSCALL_INFO_WINDOW_WIDTH - left_top_col;
			height = (SYSCALL_INFO_WINDOW_HEIGHT - left_top_row) * (syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_info_show_time_total);
		}else {
			height = SYSCALL_INFO_WINDOW_HEIGHT - left_top_row;
			width = (SYSCALL_INFO_WINDOW_WIDTH - left_top_col) * (syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_info_show_time_total);
		}
		syscall_info_show_time_total -= syscall_info_sort_by_time[syscall_info_idx]->time;
		int row_end = left_top_row + height, col_end = left_top_col + width;
		for(int row = left_top_row; row < row_end; ++row) {
			for(int col = left_top_col; col < col_end; ++col) { syscall_info_show(syscall_info_idx, " "); }
			syscall_info_show_move_down(1);
			syscall_info_show_move_left(width);
		}
		
		syscall_info_show_move_up(height);
		syscall_info_show(syscall_info_idx, syscall_info_sort_by_time[syscall_info_idx]->syscall);

		syscall_info_show_move_down(1);
		syscall_info_show_move_left(syscall_info_sort_by_time[syscall_info_idx]->syscall_name_size);

		char percentage[10] = {0};
		sprintf(percentage, "%2.0lf%%", syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_time_total * 100);
		syscall_info_show(syscall_info_idx, percentage);

		if(syscall_info_idx & 1) {
			syscall_info_show_move_down(height - 1);	//即left_top_row + height - (left_top_row + 1)
			syscall_info_show_move_left(strlen(percentage));			//即left_top_col + strlen(percentage) - left_top_col

			left_top_row += height;
		}else {
			syscall_info_show_move_up(1);			//即left_top_row + 1 - left_top_row
			syscall_info_show_move_right(width - 3);	//即left_top_col + width - (left_top_col + strlen(percentage))
			left_top_col += width;
		}

	}

	height = SYSCALL_INFO_WINDOW_HEIGHT - left_top_row;
	width = SYSCALL_INFO_WINDOW_WIDTH - left_top_col;

	for(int row = left_top_row; row < SYSCALL_INFO_WINDOW_HEIGHT; ++row) {
		for(int col = left_top_col; col < SYSCALL_INFO_WINDOW_WIDTH; ++col) {syscall_info_show(syscall_info_idx, " "); }
		syscall_info_show_move_down(1);
		syscall_info_show_move_left(width);
	}

	syscall_info_show_move_up(height);

	syscall_info_show(syscall_info_idx, syscall_info_sort_by_time[syscall_info_idx]->syscall);
	syscall_info_show_move_down(1);
	syscall_info_show_move_left(syscall_info_sort_by_time[syscall_info_idx]->syscall_name_size);

	char percentage[10] = {0};
	sprintf(percentage, "%2.0lf%%", syscall_info_sort_by_time[syscall_info_idx]->time / (double)syscall_time_total * 100);
	syscall_info_show(syscall_info_idx, percentage);
	
	syscall_info_show_move_down(SYSCALL_INFO_WINDOW_HEIGHT - left_top_row - 1);	
	syscall_info_show_move_left(left_top_col + strlen(percentage));				
	for(int i = 0; i < 80; ++i) { fprintf(stderr, "%c", '\x00'); }
	fflush(stderr);
}



int syscall_info_find_idx_by_name(const char *name) {
	int left = 0, right = syscall_info_number - 1;
	while(left <= right) {
		int middle = left + (right - left) / 2, cmp = strcmp(syscall_info_sort_by_name[middle]->syscall, name);
		if(cmp == 0) { return middle; }
		else if(cmp < 0) { left = middle + 1; }
		else { right = middle - 1; }
	}
	return -1;
}



#define syscall_info_sort_by_name_sort() _syscall_info_qsort(syscall_info_sort_by_name, 0, syscall_info_number - 1, syscall_info_sort_by_name_cmp)
#define syscall_info_sort_by_time_sort() _syscall_info_qsort(syscall_info_sort_by_time, 0, syscall_info_number - 1, syscall_info_sort_by_time_cmp)
void _syscall_info_qsort(Syscall_Info **base, int left, int right, int (*cmp)(Syscall_Info **base, int i, int j)) {
	if(left >= right) { return; }

	int leftIndex = left, rightIndex = right + 1;
	Syscall_Info *temp = NULL;

	while(1) {
		while((*cmp)(base, left, ++leftIndex) > 0) {
			if(leftIndex == right) { break; }
		}

		while((*cmp)(base, left, --rightIndex) < 0) {;}

		if(leftIndex >= rightIndex) { break; }

		temp = base[leftIndex];
		base[leftIndex] = base[rightIndex];
		base[rightIndex] = temp;
	}


	temp = base[left];
	base[left] = base[rightIndex];
	base[rightIndex] = temp;

	_syscall_info_qsort(base, left, rightIndex - 1, cmp);
	_syscall_info_qsort(base, rightIndex +1, right, cmp);
}


int syscall_info_sort_by_name_cmp(Syscall_Info **base, int i, int j) {
	return strcmp(base[i]->syscall, base[j]->syscall);
}

int syscall_info_sort_by_time_cmp(Syscall_Info **base, int i, int j) {
	if(base[i]->time == base[j]->time) { return 0; }
	else if(base[i]->time > base[j]->time) { return -1; }
	return 1;
}



void syscall_info_insert_and_sort(const char *name, long long time) {

	syscall_time_total += time;

	int syscall_info_idx = 0;
	if((syscall_info_idx = syscall_info_find_idx_by_name(name)) == -1) {
		Syscall_Info *syscall_info = (Syscall_Info*)malloc(sizeof(Syscall_Info));
		syscall_info->syscall = name;
		syscall_info->syscall_name_size = strlen(name);
		syscall_info->time = time;

		syscall_info_sort_by_name[syscall_info_number] = syscall_info;
		syscall_info_sort_by_time[syscall_info_number++] = syscall_info;


		syscall_info_sort_by_name_sort();
		syscall_info_sort_by_time_sort();
	}else {
		syscall_info_sort_by_name[syscall_info_idx]->time += time;
		syscall_info_sort_by_time_sort();
	}
}



void parse_strace_output_init(void){
    if (regcomp(&regex_syscall,re_syscall,REG_EXTENDED | REG_NEWLINE)){
        fprintf(stderr,"regcomp(&regex_syscall, reg_syscall, REG_EXTENDED)\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    if (regcomp(&regex_time, re_time, REG_EXTENDED | REG_NEWLINE)){
        fprintf(stderr, "regcomp(&regex_time, re_time, REG_EXTENDED)\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
}



#define NS_TO_LONGLONG (1000000)
int parse_strace_output(char *buf, int buf_size) {
	if(buf_size == 0) { return 0; }

	double time = 0;
	regmatch_t pmatch[1];
	char *regex_syscall_so = buf, *regex_syscall_eo = buf, *regex_time_so = buf, *regex_time_eo = buf, regex_matched = 0;
	while(1) {
		if(regexec(&regex_syscall, regex_time_eo, sizeof(pmatch) / sizeof(pmatch[0]), pmatch, 0)) { break;}
		regex_syscall_so = regex_time_eo + pmatch[0].rm_so;
		regex_syscall_eo = regex_time_eo + pmatch[0].rm_eo;


		if(regexec(&regex_time, regex_syscall_eo, sizeof(pmatch) / sizeof(pmatch[0]), pmatch, 0)) { break;}
		regex_time_so = regex_syscall_eo + pmatch[0].rm_so;
		regex_time_eo = regex_syscall_eo + pmatch[0].rm_eo;


		regex_matched = 1;
		int syscall_name_size = regex_syscall_eo - regex_syscall_so;
		char *syscall = (char*)malloc(sizeof(char) * (1 + syscall_name_size));
		memcpy(syscall, regex_syscall_so, syscall_name_size);
		syscall[syscall_name_size] = 0;
		sscanf(regex_time_so, "%lf", &time);
		syscall_info_insert_and_sort(syscall, time * NS_TO_LONGLONG);
	}

	if(regex_matched) { syscall_info_display(); }
	int remain_size = buf + buf_size - regex_time_eo;
	memmove(buf, regex_time_eo, remain_size);
	buf[remain_size] = 0;

	return remain_size;
}



char **parser_args_environ(int argc, char *argv[]){
    char **exec_arg = (char**)malloc(sizeof(char*) * (argc + 4));
    assert(exec_arg && argv);

    exec_arg[1] = STRACE_SHOW_TIME;
    exec_arg[2] = STRACE_OUTPUT;
    for (int i = 1; i <= argc; ++i){
        exec_arg[i + 3] = argv[i];
    }
    return exec_arg;
}


int pipefd[2] = {0};


void child(int argc, char *argv[]){
    char fd_path[20] = {0}, strace_path[MAX_STRACE_OUTPUT_SIZE] = {0};
    //close read side;
    close(pipefd[0]);
    
    char **exec_arg = parser_args_environ(argc,argv);

    sprintf(fd_path,"/proc/%d/fd/%d",getpid(),pipefd[1]);
    exec_arg[3] = fd_path;
    
    exec_arg[0] = strace_path;
    int pathBegin = 0, i = 0;
    char *path = getenv("PATH");

    while (path[i])
    {
        while (path[i] && path[i] != PATH_SPLIT){++i;}
        strncpy(strace_path,path + pathBegin,i - pathBegin);
        strncpy(strace_path + i - pathBegin, STRACE_EXECUTE,sizeof(STRACE_EXECUTE) + 1);

        execve(strace_path, exec_arg, environ);
        pathBegin = ++i;
    }
    fprintf(stderr, "%s\n", "execve() could not find strace");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

void parent(void){
    char buf[MAX_STRACE_OUTPUT_SIZE] = {0};
    int read_result = 0, buf_available = 0;
    close(pipefd[1]);

    if (fcntl(pipefd[0],F_SETFD, fcntl(pipefd[0],F_GETFD) | O_NONBLOCK) == -1){
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    parse_strace_output_init();

    while (1)
    {
        switch (read_result = read(pipefd[0],buf + buf_available, MAX_STRACE_OUTPUT_SIZE - buf_available))
        {
        case -1:
            break;
        case 0:
            exit(EXIT_SUCCESS);
            break;
        default:
            buf[buf_available + read_result] = 0;
            buf_available = parse_strace_output(buf, buf_available + read_result);
            break;
        }
        sleep(TIME_INTERVAL_SECONDS);
    }
}








int main(int argc, char *argv[]) {

  if (pipe(pipefd) == -1){
      perror("pipe");
      exit(EXIT_FAILURE);
  }

  switch (fork())
  {
  case -1:
    perror("fork");
    exit(EXIT_FAILURE);
    break;
  case 0:
    child(argc, argv);
    break;
  default:
    parent();
  }


  fprintf(stderr, "%s\n","sperf wrong return");
  fflush(stderr);
  exit(EXIT_FAILURE);

  // char *exec_argv[] = { "strace", "ls", NULL, };
  // char *exec_envp[] = { "PATH=/bin", NULL, };
  // execve("strace",          exec_argv, exec_envp);
  // execve("/bin/strace",     exec_argv, exec_envp);
  // execve("/usr/bin/strace", exec_argv, exec_envp);
  // perror(argv[0]);
  // exit(EXIT_FAILURE);
}
