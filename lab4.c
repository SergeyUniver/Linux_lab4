#define FUSE_USE_VERSION 26

#include <sys/types.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct directory
{
	int isFree;
	char path[255];
	char name[255];
};

int dirsfd, startPos = 0;
FILE* logfile;
struct directory dir;

static int getattr_callback(const char *path, struct stat *stbuf) 
{
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	lseek(dirsfd, startPos, SEEK_SET);
	while (read(dirsfd, &dir, sizeof(struct directory)) != 0) {
		fprintf(logfile, "getattr: \n\tpath:%s\n\tdir.path:%s\n\tdir.name:%s\n\tdir.isFree:%d\n",
			path, dir.path, dir.name, dir.isFree);
		if ((dir.isFree == 0) && (strcmp(dir.path, path) == 0)) {
			stbuf->st_mode = S_IFDIR | 0777;
			stbuf->st_nlink = 2;
			return 0;
		}
	}
	fprintf(logfile, "\n---------------------------\n");
	return -ENOENT;
}

static int mkdir_callback(const char* path, mode_t mode)
{
	lseek(dirsfd, startPos, SEEK_SET);
	while (read(dirsfd, &dir, sizeof(struct directory)) != 0) {
		fprintf(logfile, "mkdir: \n\tpath:%s\n\tdir.path:%s\n\tdir.name:%s\n\tdir.isFree:%d\n",
			path, dir.path, dir.name, dir.isFree);

		if (!dir.isFree && strcmp(path, dir.path) == 0)
			return -ENOENT;
	}
	lseek(dirsfd, startPos, SEEK_SET);
	int curpos = 0;
	while (read(dirsfd, &dir, sizeof(struct directory)) != 0) {
		if (dir.isFree) {
			dir.isFree = 0;
			strcpy(dir.path, path);
			int ps = strlen(path) - 1;
			while ((path[ps] != '/') && (ps >= 0))
				ps--;
			if (ps < 0)
				return -ENOENT;
			strcpy(dir.name, path + ps);
			fprintf(logfile, "dir to write:\n\tdir.name:%s\n\tdir.path:%s\n\tdir.isFree:%d\n", dir.name, dir.path, dir.isFree);
			//curpos = lseek(dirsfd, 0, SEEK_CUR);
			lseek(dirsfd, curpos, SEEK_SET);
			lseek(dirsfd, startPos + curpos * sizeof(struct directory), SEEK_SET);
			write(dirsfd, &dir, sizeof(struct directory));

			lseek(dirsfd, startPos, SEEK_SET);
			while(read(dirsfd, &dir, sizeof(struct directory)) != 0){
				fprintf(logfile, "mkdir to empty: \n\tpath:%s\n\tdir.path:%s\n\tdir.name:%s\n\tdir.isFree:%d\n",
					path, dir.path, dir.name, dir.isFree);
			}
			return 0;
		}
		curpos++;
	}
	dir.isFree = 0;
	int ps = strlen(path) - 1;
	while ((path[ps] != '/') && (ps >= 0))
		ps--;
	if (ps < 0)
		return -ENOENT;
	strcpy(dir.path, path);
	strcpy(dir.name, path + ps);
	write(dirsfd, &dir, sizeof(struct directory));
		lseek(dirsfd, startPos, SEEK_SET);
	return 0;
}

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi) 
{
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	lseek(dirsfd, startPos, SEEK_SET);
	fsync(dirsfd);
	while (read(dirsfd, &dir, sizeof(struct directory)) != 0) {
		if (dir.isFree == 0) {
			char s[600];
			strcpy(s, path);
			if (s[1] == '\0')
				s[0] = '\0';
			strcat(s, dir.name);
			if (strcmp(s, dir.path) == 0)
				filler(buf, dir.name + 1, NULL, 0);
		}
	}

	return 0;
}

static int rmdir_callback(const char *path)
{
	lseek(dirsfd, startPos, SEEK_SET);
	int curpos = 0;
	while (read(dirsfd, &dir, sizeof(struct directory)) != 0) {
		if (!dir.isFree && strncmp(dir.path, path, strlen(path)) == 0) {
			dir.isFree =  1;
			lseek(dirsfd, startPos + curpos * sizeof(struct directory), SEEK_SET);
			write(dirsfd, &dir, sizeof(struct directory));
		}
		curpos++;
	}
	lseek(dirsfd, startPos, SEEK_SET);
	while (read(dirsfd, &dir, sizeof(struct directory)) != 0) {
		fprintf(logfile, "rmdir after: \n\tpath:%s\n\tdir.path:%s\n\tdir.name:%s\n\tdir.isFree:%d\n",
			path, dir.path, dir.name, dir.isFree);
	}
	return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
	return 0;
}

static int read_callback(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {
	return 0;
}

static struct fuse_operations fuse_example_operations = {
	.getattr = getattr_callback,
	.open = open_callback,
	.read = read_callback,
	.readdir = readdir_callback,
	.mkdir = mkdir_callback,
	.rmdir = rmdir_callback,
};

int main(int argc, char *argv[])
{
	dirsfd = open("./directories", O_RDWR | O_CREAT, 0777);
	logfile = fopen("./log.txt", "w");
	int ret = fuse_main(argc, argv, &fuse_example_operations, NULL);
	close(dirsfd);
	fclose(logfile);
	return ret;
}