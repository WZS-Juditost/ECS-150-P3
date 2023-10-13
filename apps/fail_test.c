#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fs.h>

#define ASSERT(cond, func)                               \
do {                                                     \
	if (!(cond)) {                                       \
		fprintf(stderr, "Function '%s' failed\n", func); \
		exit(EXIT_FAILURE);                              \
	}                                                    \
} while (0)

#define ASSERT_FAIL(cond, func)                          \
do {                                                     \
	if (cond < 0) {                                       \
		fprintf(stderr, "Function '%s' failed\n", func); \
	}                                                    \
} while (0)

void disk_errors(int ret, char* diskname, int fd)
{
    printf("Disk errors:\n");
	/* FAIL Mount disk */
	
	/* Causes segfault later */
/* 	ret = fs_mount("4096");
	ASSERT_FAIL(ret, "fs_mount");
	ret = fs_umount(); */
	
	/* FAIL Mount disk */
	ret = fs_mount("diskname");
	ASSERT_FAIL(ret, "fs_mount");

    /* FAIL info */
    ret = fs_info();
	ASSERT_FAIL(ret, "fs_info");

    /* FAIL umount */
    ret = fs_umount();
	ASSERT_FAIL(ret, "fs_umount");

    /* FAIL info */
    ret = fs_create(diskname);
	ASSERT_FAIL(ret, "fs_create");

    /* FAIL info */
    ret = fs_delete(diskname);
	ASSERT_FAIL(ret, "fs_delete");

    /* FAIL info */
    ret = fs_ls();
	ASSERT_FAIL(ret, "fs_ls");

    /* FAIL info */
    ret = fs_open(diskname);
	ASSERT_FAIL(ret, "fs_open");

    /* FAIL info */
    ret = fs_close(10);
	ASSERT_FAIL(ret, "fs_close");

    /* FAIL info */
    ret = fs_stat(10);
	ASSERT_FAIL(ret, "fs_stat");

    /* FAIL info */
    ret = fs_lseek(fd, fd);
	ASSERT_FAIL(ret, "fs_lseek");

    /* FAIL info */
    ret = fs_write(fd, diskname, fd);
	ASSERT_FAIL(ret, "fs_write");

    /* FAIL info */
    ret = fs_read(fd, diskname, fd);
	ASSERT_FAIL(ret, "fs_read");

    printf("-----------------------------------------------\n");
}

int main(int argc, char *argv[])
{
	int ret;
	char *diskname;
	int fd;
	char data[26] = "abcdefghijklmnopqrstuvwxyz";

	if (argc < 1) {
		printf("Usage: %s <diskimage>\n", argv[0]);
		exit(1);
	}
    

    fd = 0;
    ret = 0;
    diskname = argv[1];
    disk_errors(ret, diskname, fd);

    /* Mount disk */
	ret = fs_mount(diskname);
	ASSERT(!ret, "fs_mount");

    

	/* Create file and open */
	ret = fs_create("non_existent");
	ASSERT_FAIL(ret, "fs_create");

	fd = fs_open("myfile");
	ASSERT(fd >= 0, "fs_open");

	/* Write some data */
	ret = fs_write(fd, data, sizeof(data));
	ASSERT(ret == sizeof(data), "fs_write");

	/* Close file and unmount */
	fs_close(fd);
	fs_umount();

	return 0;
}
