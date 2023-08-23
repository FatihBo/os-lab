#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

#define __FORMAT_NORMAl (0)
#define __FORMAT_TRANSLATE (1)

/*
 * 将val转换为mod进制字符串
 */
static char *itoa(char *str, uint64_t val, int mod) {
	panic_on(str == NULL, "str is NULL");
	panic_on(mod < 1, "mod is invalid");


	char stack[40] = {0}, minus = 0;
	int sp = 0;

	if(val < 0) {
		minus = 1;
		val = -val;
	}


	while(val) {
		stack[sp++] = val % mod;
		val /= mod;
	}


	int idx = 0;
	if(minus) { str[idx++] = '-'; }

	if(sp) {
		while(sp--) {
			str[idx++] = ((stack[sp] < 10) ? '0' : ('a' - 10)) + stack[sp];
		}
	}else { str[idx++] = '0'; }

	str[idx] = 0;

	return str;
}


/*
 * 格式化手册
 * %%		: 转义
 * %d		: 10进制int
 * %D		: 10进制uint64_t
 * %x		: 16进制int
 * %X		: 16进制uint64_t
 * %c		: char
 * %s		： 输出字符串
 */




/*
 * 这里需要实现成中断程序/进程都可以任意调用的
 * 并且输出是原子的
 */
static int printf_lock = 0;
int printf(const char *fmt, ...) {
	/*
	 * 为了避免在中断程序和进程抢锁，从而导致死锁——则需要关闭中断
	 * 为了确保一次的输出是在一起的，需要上锁
	 */
	bool save_ienable = ienabled();
	iset(false);
	while(atomic_xchg(&printf_lock, 1) == 1) {;}


	va_list ap;
	int number = 0;
	char temp[40];
	unsigned char state = __FORMAT_NORMAl;


	va_start(ap, fmt);
	for(size_t i = 0; fmt[i]; ++i) {
		switch(state) {
			case __FORMAT_NORMAl:
				if(fmt[i] == '%') { state = __FORMAT_TRANSLATE; }
				else { putch(fmt[i]);}
				break;
			case __FORMAT_TRANSLATE:
				state = __FORMAT_NORMAl;
				switch(fmt[i]) {
					case '%':
						putch('%');
						break;
					case 'c':
						putch((char)va_arg(ap, unsigned int));
						++number;
						break;
					case 'D':
						putstr(itoa(temp, va_arg(ap, uint64_t), 10));
						++number;
						break;
					case 'd':
						putstr(itoa(temp, va_arg(ap, unsigned int), 10));
						++number;
						break;
					case 'X':
						putstr(itoa(temp, va_arg(ap, uint64_t), 16));
						++number;
						break;
					case 'x':
						putstr(itoa(temp, va_arg(ap, unsigned int), 16));
						++number;
						break;
					case 's':
						putstr(va_arg(ap, char*));
						++number;
						break;
					//这里default表示标准库未定义行为
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
	va_end(ap);


	/*
	 * 此时按照相反的顺序
	 * 释放锁，并且开中断即可
	 */
	panic_on(atomic_xchg(&printf_lock, 0) != 1, "error printf_lock");
	iset(save_ienable);

	return number;
}


static char *
sputch(char *out, const int ch)
{
	*(out++) = ch;
	*(out) = 0;
	return out;
}


static char *
sputstr(char *out, const char *in)
{
	while(*in) { out = sputch(out++, *(in++)); }
	return out;
}


/*
 * 对于下面这些字符串函数
 * 不进行上锁，关中断操作
 * 如果是共享数据，在调用这些函数前，程序员应该提前上锁
 */
int vsprintf(char *out, const char *fmt, va_list ap) {
	int number = 0;
	char temp[40];
	unsigned char state = __FORMAT_NORMAl;


	for(size_t i = 0; fmt[i]; ++i) {
		switch(state) {
			case __FORMAT_NORMAl:
				if(fmt[i] == '%') { state = __FORMAT_TRANSLATE; }
				else { out = sputch(out, fmt[i]);}
				break;
			case __FORMAT_TRANSLATE:
				state = __FORMAT_NORMAl;
				switch(fmt[i]) {
					case '%':
						out = sputch(out, '%');
						break;
					case 'c':
						out = sputch(out, (char)va_arg(ap, unsigned int));
						++number;
						break;
					case 'D':
						out = sputstr(out, itoa(temp, va_arg(ap, uint64_t), 10));
						++number;
						break;
					case 'd':
						out = sputstr(out, itoa(temp, va_arg(ap, unsigned int), 10));
						++number;
						break;
					case 'X':
						out = sputstr(out, itoa(temp, va_arg(ap, uint64_t), 16));
						++number;
						break;
					case 'x':
						out = sputstr(out, itoa(temp, va_arg(ap, unsigned int), 16));
						++number;
						break;
					case 's':
						out = sputstr(out, va_arg(ap, char*));
						++number;
						break;
					//这里default表示标准库未定义行为
					default:
						break;
				}
				break;
			default:
				break;
		}
	}

	return number;
}


int sprintf(char *out, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	int res = vsprintf(out, fmt, ap);

	va_end(ap);
	return res;
}


static char *
snputch(char *out, const int ch, size_t *n)
{
	if(*n >= 1) {
		--*n;
		*(out++) = ch;
		*(out) = 0;
	}
	return out;
}


static char *
snputstr(char *out, const char *in, size_t *n)
{
	while(*in && *n >= 1) { out = snputch(out++, *(in++), n); }
	return out;
}



int snprintf(char *out, size_t n, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	int res = vsnprintf(out, n, fmt, ap);

	va_end(ap);
	return res;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
	int number = 0;
	char temp[40];
	unsigned char state = __FORMAT_NORMAl;

	for(size_t i = 0; fmt[i] && n >= 1; ++i) {
		switch(state) {
			case __FORMAT_NORMAl:
				if(fmt[i] == '%') { state = __FORMAT_TRANSLATE; }
				else { out = snputch(out, fmt[i], &n);}
				break;
			case __FORMAT_TRANSLATE:
				state = __FORMAT_NORMAl;
				switch(fmt[i]) {
					case '%':
						out = snputch(out, '%', &n);
						break;
					case 'c':
						out = snputch(out, (char)va_arg(ap, unsigned int), &n);
						++number;
						break;
					case 'D':
						out = snputstr(out, itoa(temp, va_arg(ap, uint64_t), 10), &n);
						++number;
						break;
					case 'd':
						out = snputstr(out, itoa(temp, va_arg(ap, unsigned int), 10), &n);
						++number;
						break;
					case 'X':
						out = snputstr(out, itoa(temp, va_arg(ap, uint64_t), 16), &n);
						++number;
						break;
					case 'x':
						out = snputstr(out, itoa(temp, va_arg(ap, unsigned int), 16), &n);
						++number;
						break;
					case 's':
						out = snputstr(out, va_arg(ap, char*), &n);
						++number;
						break;
					//这里default表示标准库未定义行为
					default:
						break;
				}
				break;
			default:
				break;
		}
	}


	return number;
}

#endif