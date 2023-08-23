#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
	size_t len = 0;
	while(s[len++]) {;}

	return len;
}

char *strcpy(char *dst, const char *src) {
	size_t len = 0;
	while(src[len]) {
		dst[len] = src[len];
		++len;
	}

	dst[len] = 0;
	return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
	size_t len = 0;
	while(src[len] && len < n) {
		dst[len] = src[len];
		++len;
	}

	while(len < n) { dst[len++] = 0; }
	return dst;
}

char *strcat(char *dst, const char *src) {
	size_t dstLen = 0, srcLen = 0;
	while(dst[dstLen++]) {;}

	while(src[srcLen]) { dst[dstLen++] = src[srcLen++]; }

	dst[dstLen] = 0;
	return dst;
}

int strcmp(const char *s1, const char *s2) {
	for(size_t i = 0; s1[i] || s2[i]; ++i) {
		if(s1[i] < s2[i]) { return -1; }
		else if(s1[i] > s2[i]) { return 1; }
	}
	return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
	for(size_t i = 0; i < n && (s1[i] || s2[i]); ++i) {
		if(s1[i] < s2[i]) { return -1; }
		else if(s1[i] > s2[i]) { return 1; }
	}
	return 0;
}

void *memset(void *s, int c, size_t n) {
	for(size_t i = 0; i != n; ++i) {
		((unsigned char*)s)[i] = (unsigned char)c;
	}
	return s;
}

void *memmove(void *dst, const void *src, size_t n) {
	if(dst <= src) {
		for(size_t i = 0; i != n; ++i) { ((unsigned char*)dst)[i] = ((const unsigned char*)src)[i]; }
	}else {
		for(size_t i = n - 1; i != -1; --i) { ((unsigned char*)dst)[i] = ((const unsigned char*)src)[i]; }
	}
	return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
	for(size_t i = 0; i != n; ++i) {
		((unsigned char*)out)[i] = ((const unsigned char*)in)[i];
	}
	return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
	for(size_t i = 0; i != n; ++i) {
		if(((unsigned char*)s1)[i] < ((unsigned char*)s2)[i]) { return -1; }
		else if(((unsigned char*)s1)[i] > ((unsigned char*)s2)[i]) { return 1; }
	}
	return 0;
}

#endif