#define __USE_LARGEFILE         // tells fseek we deal with >2G
#define _FILE_OFFSET_BITS 64    //we turn off_t into 64 bits
#define SECTOR 512
#define BLOCK_SECTOR_OFFSET(a) data_start512+((uint64_t)(a))*eff_block_size512
#define PRINT_ERROR(a,...)                                              \
        {                                                               \
        fprintf(stderr, "\nERROR -> " a "\n", ## __VA_ARGS__) ;         \
        exit(EXIT_FAILURE);                                             \
        }
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <libdevmapper.h>

const char *version = "0.2.2";

FILE *dev_urandom_ptr = NULL;

uint64_t data_start512;
uint64_t eff_block_size512;

int block_size;
int device_block_size = -1;
int total_nb_blocks;
int total_nb_devices = 0;
int nb_empty_blocks = 0;
int *empty_blocks = NULL;

struct device_structure {
        char            *name;
        FILE            *fp;
        int             last_block;
        int             total_nb_blocks;
        int             *blocks;
};
struct device_structure *device_structure = NULL;

struct block_header_structure {
        int64_t         zero;           //8bits
        int32_t         zero_or_one;    //4bits
        int32_t         number;         //4bits
        char            unused[496];    //512-8-4-4=496
};

const char *optstring="s:d:tohvlye";
struct option options[]=
        {// Long name,                  has_arg (0=no), FLAG, 'val'
        {"setsize",                     1, NULL, 's'},
        {"dmsetup",                     1, NULL, 'd'},
        {"table",                       0, NULL, 't'},
        {"offset",                      0, NULL, 'o'},
        {"help",                        0, NULL, 'h'},
        {"version",                     0, NULL, 'v'},
        {"list",			0, NULL, 'l'},
	{"yes",				0, NULL, 'y'},
	{"verbose",			0, NULL, 'e'},
        {NULL,                          0, NULL, 0}
        };

enum ACTIONS {
	TABLE,
	LIST_FREE_BLOCKS,
	SET_SIZE,
	OFFSET,
	DMSETUP
	};
