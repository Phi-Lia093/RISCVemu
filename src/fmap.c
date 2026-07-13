#include <emu.h>

void *map_file(char *path, size_t *length, int pm)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		warn("could not open file %s", path);
		return NULL;
	}
	struct stat st;
	if (fstat(fd, &st) < 0) {
		warn("fstat failed");
		close(fd);
		return NULL;
	}

	if (st.st_size == 0) {
		warn("file %s is empty", path);
		close(fd);
		return NULL;
	}

	void *mapped = mmap(NULL, st.st_size, pm, MAP_PRIVATE, fd, 0);
	if (mapped == MAP_FAILED) {
		warn("mmap failed");
		close(fd);
		return NULL;
	}
	if (length != NULL) {
		length[0] = st.st_size;
	}
	close(fd);
	return mapped;
}

void unmap_file(void *addr, size_t length)
{
	if (munmap(addr, length) != 0) {
		warn("failed to unmap @ addr %p", addr);
	}
}