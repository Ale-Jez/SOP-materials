#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define PATH_BUF_LEN 256
#define TEXT_BUF_LEN 256

ssize_t bulk_read(int fd, char *buf, size_t count)
{
	ssize_t c;
	ssize_t len = 0;
	do
	{
		c = TEMP_FAILURE_RETRY(read(fd, buf, count));
		if (c < 0)
			return c;
		if (c == 0)
			return len; // EOF
		buf += c;
		len += c;
		count -= c;
	} while (count > 0);
	return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
	ssize_t c;
	ssize_t len = 0;
	do
	{
		c = TEMP_FAILURE_RETRY(write(fd, buf, count));
		if (c < 0)
			return c;
		buf += c;
		len += c;
		count -= c;
	} while (count > 0);
	return len;
}

void write_stage2(const char *const path, const struct stat *const stat_buf)
{
	
}

void show_stage3(const char *const path, const struct stat *const stat_buf) {}

void walk_stage4(const char *const path, const struct stat *const stat_buf) {}

int interface_stage1()
{
	
}

int main()
{
	while (interface_stage1())
		;
	return EXIT_SUCCESS;
}