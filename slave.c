#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#define BUF_SIZE 4096
#define MAX_NAME_LEN 64
#define DATA_LEN 10

#define IOCTL_PAGE 0x12345676
#define IOCTL_CREATESOCKET 0x12345677
#define IOCTL_MMAP 0x12345678
#define IOCTL_EXIT 0x12345679

int main(int argc, char *argv[]) {
	char buf[BUF_SIZE];
	unsigned int file_num;
	char method[16];
	char ip[16];
	int dev_fd, file_fd;
	ssize_t ret;
	char *file_address, *kernel_address;
	struct timeval start, end;
	double total_time;
	int total_size = 0, length, offset;

	/*	handle arguments	*/
	strncpy(buf, argv[1], BUF_SIZE);
	file_num = atoi(buf);
	if(file_num <= 0) {
		perror("slave file number error\n");
		return file_num;
	}
	char file_name[file_num][MAX_NAME_LEN];
	for(int i = 0; i < file_num; i++) {
		strncpy(file_name[i], argv[2+i], MAX_NAME_LEN);
	}
	strncpy(method, argv[2+file_num], 16);
	strncpy(ip, argv[3+file_num], 16);

	/*	handle slave device	*/
	if((dev_fd = open("/dev/slave_device", O_RDWR)) < 0) {
		perror("failed to open /dev/slave_device\n");
		return dev_fd;
	}

	if((ret = ioctl(dev_fd, IOCTL_CREATESOCKET, ip)) < 0) {
		perror("failed to create slave socket\n");
		return ret;
	}

	if((kernel_address = mmap(NULL, BUF_SIZE, PROT_WRITE, MAP_SHARED, dev_fd, 0)) == (void*)-1) {
		perror("slave failed to map kernel_address\n");
		return -1;
	}

	/*	read from socket and write into files	*/
	gettimeofday(&start, NULL);
	for(int i = 0; i < file_num; i++) {
		if((file_fd = open(file_name[i], O_RDWR | O_CREAT | O_TRUNC)) < 0) {
			perror("failed to open receive file");
			return file_fd;
		}
		read(dev_fd, buf, DATA_LEN);
		buf[DATA_LEN] = '\0';
		length = atoi(buf);
		// DEBUG
		if((file_address = mmap(NULL, length, PROT_WRITE, MAP_SHARED, file_fd, 0)) == (void*)-1) {
			perror("slave failed to map file_address\n");
			return -1;
		}
		ftruncate(file_fd, length);
		switch(method[0]) {
			case 'f':
				while(length > 0) {
					ret = read(dev_fd, buf, length > BUF_SIZE? BUF_SIZE: length);
					if(ret <= 0) {
						break;
					}
					write(file_fd, buf, ret);
					total_size += ret;
					length -= ret;
				}
				break;
			case 'm':
				// DEBUG
				//offset = DATA_LEN;
				offset = 0;
				while(length > 0) {
					ret = ioctl(dev_fd, IOCTL_MMAP, length > BUF_SIZE ? BUF_SIZE : length);
					if(ret <= 0) {
						break;
					}
					// DEBUG
					/*
					ftruncate(file_fd, offset - DATA_LEN + ret);
					if((file_address = mmap(NULL, ret, PROT_WRITE, MAP_SHARED, file_fd, offset - DATA_LEN)) == (void*)-1) {
						perror("slave failed to map file_address\n");
						return -1;
					}
					memcpy(file_address, kernel_address, ret);
					*/
					memcpy(file_address + offset, kernel_address, ret);
					total_size += ret;
					length -= ret;
					offset += ret;
					// DEBUG
					//munmap(file_address, ret);
				}
				break;
			default:
				perror("slave method error\n");
				return -1;
		}
		// DEBUG
		munmap(file_address, ret);
		close(file_fd);
	}
	if((ret = ioctl(dev_fd, IOCTL_EXIT)) < 0) {
		perror("failed to exit slave socket\n");
		return ret;
	}

	if(method[0] == 'm') {
		if((ret = ioctl(dev_fd, IOCTL_PAGE, kernel_address)) < 0) {
			perror("failed to print page descriptor\n");
			return ret;
		}
	}

	/*	results	*/
	gettimeofday(&end, NULL);
	total_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) * 0.0001;
	printf("Transmission time: %lf ms, Total File size: %d bytes\n", total_time, total_size);

	/*	final handle	*/
	close(dev_fd);
	return (0);
}
