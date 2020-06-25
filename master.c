#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

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
	int dev_fd, file_fd;
	struct timeval start, end;
	int ret, length, offset, temp_len, total_size = 0;
	double total_time;
	char *file_address, *kernel_address;

	/*	handle arguments	*/
	strncpy(buf, argv[1], BUF_SIZE);
	file_num = atoi(buf);
	if(file_num <= 0) {
		perror("master file number error\n");
		return file_num;
	}
	char file_name[file_num][MAX_NAME_LEN];
	for(int i = 0; i < file_num; i++) {
		strncpy(file_name[i], argv[2+i], MAX_NAME_LEN);
	}
	strncpy(method, argv[2+file_num], 16);

	/*	handle master device	*/
	if((dev_fd = open("/dev/master_device", O_RDWR)) < 0) {
		perror("failed to open /dev/master_device\n");
		return dev_fd;
	}
	if((ret = ioctl(dev_fd, IOCTL_CREATESOCKET)) < 0) {
		perror("failed to create master socket\n");
		return ret;
	}
	
	if ((kernel_address = mmap(NULL, BUF_SIZE, PROT_WRITE, MAP_SHARED, dev_fd, 0)) == (void*)-1) {
		perror("failed to mmap kernel memory\n");
		return -1;
	}

	/*	read from files and write into socket	*/
	gettimeofday(&start, NULL);
	for(int i = 0; i < file_num; i++) {
		if((file_fd = open(file_name[i], O_RDWR)) < 0) {
			perror("failed to open target file\n");
			return file_fd;
		}
		length = lseek(file_fd, (size_t)0, SEEK_END);
		lseek(file_fd, 0, SEEK_SET);
		sprintf(buf, "%010d", length);
		write(dev_fd, buf, DATA_LEN);
		// DEBUG
		if ((file_address = mmap(NULL, length, PROT_READ, MAP_SHARED, file_fd, 0)) == (void*)-1) {
			perror("slave failed to map file_address\n");
			return -1;
		}
		switch(method[0]) {
			case 'f':
				while(length > 0) {
					ret = read(file_fd, buf, BUF_SIZE);
					write(dev_fd, buf, ret);
					length -= ret;
					total_size += ret;
				}
				break;
			case 'm':
				// DEBUG
				//offset = DATA_LEN;
				offset = 0;
				while(length > 0) {
					temp_len = (length > BUF_SIZE? BUF_SIZE: length);
					// DEBUG
					/*
					if ((file_address = mmap(NULL, temp_len, PROT_READ, MAP_SHARED, file_fd, offset - DATA_LEN)) == (void*)-1) {
						perror("slave failed to map file_address\n");
						return -1;
					}
					memcpy(kernel_address, file_address, temp_len);
					*/
					memcpy(kernel_address, file_address + offset, temp_len);
					ioctl(dev_fd, IOCTL_MMAP, temp_len);

					offset += temp_len;
					length -= temp_len;
					total_size += temp_len;
					// DEBUG
					//munmap(file_address, temp_len);
				}
				break;
			default:
				perror("master method error\n");
				return -1;
		}
		// DEBUG
		munmap(file_address, length);
		close(file_fd);
	}
	if((ret = ioctl(dev_fd, IOCTL_EXIT) == -1) < 0) {
		perror("failed to exit master socket\n");
		return ret;
	}

	if(method[0] == 'm') {
		if((ret = ioctl(dev_fd, IOCTL_PAGE, kernel_address)) < 0) {
			perror("failed to print mmap page\n");
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
