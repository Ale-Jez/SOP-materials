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
#include <ctype.h>

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
	int fd;

	if (!S_ISREG(stat_buf->st_mode)) //if a file is not reg file, print an error and return
	{
		fprintf(stderr, "%s is not a regular file", path);
		return;
	}

	if(unlink(path)) //if it is a reg file, remove it and create it again
		ERR("unlink");
	if((fd = open(path, O_CREAT | O_WRONLY, 0777)) == -1)
		ERR("open");
		
	char buffer[TEXT_BUF_LEN];

	while(1)
	{
		if(fgets(buffer, TEXT_BUF_LEN, stdin) != NULL); //wait for user input

		if (strlen(buffer) == 1) // if the line read is empty, close the file and return
		{
			close(fd);
			return;
		}

		for(int i = 0 ; i < strlen(buffer) ; i++) //replace lowercase letters to uppercase
		{
			if(toupper(buffer[i]) < 0)
				ERR("toupper");
		}

		if(bulk_write(fd, buffer, strlen(buffer)) < 0) //write each line into newly created file
			ERR("bulk_write");
	}

	close(fd);
	return;
}

void show_stage3(const char *const path, const struct stat *const stat_buf) 
{
	DIR* directory;
	struct dirent *dp;

	if(S_ISDIR(stat_buf->st_mode)) // if dir, print names of all the files in dir
	{
		if((directory = opendir(path)) == NULL)
			ERR("opendir");

		do
		{
			errno = 0;
			if((dp = readdir(directory)) != NULL)
			{
				printf("%s\n", dp->d_name);
			}
		} while (dp != NULL);	

		closedir(directory);
	}

	/* NOT A LOW LEVEL SOLUTION
	FILE* file;

	char text[TEXT_BUF_LEN];

	if(S_ISREG(stat_buf->st_mode))
	{
		if((file = fopen(path, "r")) == NULL)
			ERR("fopen");

		while(fgets(text, TEXT_BUF_LEN, file))
		{
			fputs(text, stdout);
		}

		printf("SIZE\n: %ld", stat_buf->st_size);
		printf("UID\n: %d", stat_buf->st_uid);
		printf("GID\n: %d", stat_buf->st_gid);
	}

	return;
	*/

	// LOW LEVEL SOLUTION
	int fd;
	char text[TEXT_BUF_LEN];

	if (S_ISREG(stat_buf->st_mode))
	{
		if ((fd = open(path, O_RDONLY)) == -1)
			ERR("open");
		
		while(bulk_read(fd, text, TEXT_BUF_LEN))
		{
			bulk_write(1, text, strlen(text));
		}

		printf("SIZE: %ld bytes\n", stat_buf->st_size);
		printf("UID: %d\n", stat_buf->st_uid);
		printf("GID: %d\n", stat_buf->st_gid);

		close(fd);
	}
}

int walk(const char *name, const struct stat *s, int type, struct FTW *f)
{
	if (type == FTW_F) // for a regular file N + 1 spaces
	{
		for(int i = 0; i <= f->level + 1; i++)
		{
			printf(" ");
		}
	}

	if (type == FTW_D) // for a dir, N spaces and a +
	{
		for(int i = 0; i <= f->level; i++)
		{
			printf(" ");
		}
		printf("+");
	}

	printf("%s\n", name + f->base);
	return FTW_CONTINUE;
}

void walk_stage4(const char *const path, const struct stat *const stat_buf)
{

	DIR* dir;

	if((dir = opendir(path)) == NULL)
	{	
		if(errno == ENOTDIR)
		{
			perror(path);
			return;		
		}

		ERR("opendir");
	}


	if (nftw(path, walk, FTW_F, FTW_PHYS) == -1)
		ERR("nftw");
	
	closedir(dir);
}

int interface_stage1()
{
	char path[PATH_BUF_LEN];
	struct stat filestat;

	//The application should display the following commands
	if(printf("A. write\nB. show\nC. walk\nD. exit\n") < 0)
		ERR("printf");

	
	if(fgets(path, PATH_BUF_LEN, stdin) == NULL)
		ERR("fgets");
	char option = path[0]; //If we do getchar or scanf("%c", option) it doesn't work idk why

	switch (option)
	{
	//Lower and upper case letters should be allowed
	case 'A':
	case 'B':
	case 'C':

	case 'a':
	case 'b':
	case 'c':

		if(fgets(path, PATH_BUF_LEN, stdin) == NULL)
			ERR("fgets");
		int len = strlen(path);
		path[len - 1] = '\0'; //since we enter a newline and we want a path without a newline

		if(stat(path, &filestat) == -1)
		{
			if(errno == ENOENT)
			{
				perror(path);
				return 1;
			}
			else
				ERR("stat");
		}

		if (option == 'A' || option == 'a') // write
		{
			write_stage2(path, &filestat);
			return 1;
		}
		else if (option == 'B' || option == 'b') // show
		{
			show_stage3(path, &filestat);
			return 1;
		}
		else if (option == 'C' || option == 'c') // walk
		{
			walk_stage4(path, &filestat);
			return 1;
		}

	case 'D':
	case 'd':
		
		return 0; // the command exit [...] return 0 immediately

	case '?':
	default: // any other letter
		fputs("Invalid command", stderr);
		return 1;
	}
	return 0;
}

int main()
{
	while (interface_stage1());
	return EXIT_SUCCESS;
}