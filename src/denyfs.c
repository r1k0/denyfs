#include "denyfs.h"

const char *dm_device = NULL;
int yes = 0, verbose= 0;
int nb_allocated_blocks;
int device_nb;

void usage(const char *strings) {
        if ((strings && *strings) != '\0')
                printf("%s\n", strings);

        printf("Usage:\n"
                "  denyfs [options]\n\n"
                "Where [options] are as follow:\n"
                "  -t, --table\t[dev1]...\t\t\t\tDisplay devices block table\n"
                "  -l, --list\t[dev1]...\t\t\t\tList free blocks of a set of devices\n"
                "  -o, --offset\t[dev]\t\t\t\t\tPrint offsets of blocks of a single device\n"
                "  -s, --setsize\t[dev nb],[nb of blocks]\t[dev1]...\tSet size of device nb [int] with [int] block\n"
                "  -d, --dmsetup\t[new dev]\t\t\t\tDmsetup device binding\n"
                "  -y, --yes\t\t\t\t\t\tPassed with --setsize it will not ask for resize\n"
                "  -e, --verbose\t\t\t\t\t\tBe verbose\n"
                "  -v, --version\t\t\t\t\t\tProgram version\n"
                "  -h, --help\t\t\t\t\t\tThis plus quick examples.\n\n"
                );
//      exit(strings?1:0);
}

// checks
void *check_memory(size_t size) {
        void *ptr;
        if (!(ptr = calloc(size, 1)))
        PRINT_ERROR("out of memory");
        return ptr;
}

void check_access(int nb_devices, char **devices, int verbose) {
    struct stat statbuf;
    struct device_structure *d;
    int i, size = -1;

    // allocate memory for all devices
    device_structure = check_memory(sizeof(*device_structure)*nb_devices);

    if (verbose == 1) printf("  devices structure\t");

    for (i = 0; i < nb_devices; i++) {
        if (verbose == 1) {
            printf(".");
        }

        d = &device_structure[i];
        d->name = devices[i];
        int file_descriptor, thissize, thisblocksize;

        // check file status
        if (stat(d->name, &statbuf))
            PRINT_ERROR("device %d: stat %s: %s\n", i+1, d->name, strerror(errno));

        // check if it's a block device
        if (!S_ISBLK(statbuf.st_mode))
            PRINT_ERROR("device %d: %s is not a block device\n", i+1, d->name);

        // check if we can open the device
        if (!(device_structure[i].fp = fopen(d->name, "r+")))
            PRINT_ERROR("device %d: error opening %s: %s\n"
            "  You might want to increase open files limits see ulimit", i+1, d->name, strerror(errno));

        // map device_structure name to fd
        if ((file_descriptor = fileno(device_structure[i].fp)) == -1)
            PRINT_ERROR("device %d: impossible: %s\n", i+1, strerror(errno));

        // return device size
        if (ioctl(file_descriptor, BLKGETSIZE, &thissize) == -1)
            PRINT_ERROR("device %d: ioctl on %s: %s\n", i+1, d->name, strerror(errno));
        if (size == -1)
            size = thissize;
        else if (size != thissize)
            PRINT_ERROR("device %d: cannot set device size\n", i+1);

        // get block device logical sector size
        if (ioctl(file_descriptor, BLKBSZGET, &thisblocksize) == -1)
                PRINT_ERROR("device %d: ioctl on %s: %s\n", i+1, d->name, strerror(errno));
        if (device_block_size == -1)
                device_block_size = thisblocksize;
        else if (device_block_size != thisblocksize)
                PRINT_ERROR("device %d: cannot set block device size\n", i+1);
    }
    if (verbose == 1) printf("\n");

    // found int of all devices
    total_nb_devices = nb_devices;

    // make room for worst case padding
    size = size - device_block_size/SECTOR + 1;

    i = 0;
    while ((total_nb_blocks = size/(block_size=1+(device_block_size<<i))) > 1024) 
    i++;
    block_size *= SECTOR;
    eff_block_size512 = (block_size - SECTOR)/SECTOR;
    data_start512 = (total_nb_blocks + device_block_size/SECTOR - 1);

    // allocate mem for blocks of all devices
    for (i = 0; i < total_nb_devices; i++) {
            int j;
            d = &device_structure[i];
            d->last_block = -1;
            d->blocks = check_memory(sizeof(struct block_s*)*total_nb_blocks);
            for (j = 0; j < total_nb_blocks; j++)
            // fill with -1
            d->blocks[j] = -1;
    }

    // allocate mem for empty_blocks
    empty_blocks = check_memory(sizeof(int)*total_nb_blocks);
    for (i = 0; i < total_nb_blocks; i++)
            empty_blocks[i] = -1;

    // accessing /dev/urandom
    if (!(dev_urandom_ptr = fopen("/dev/urandom", "r")))
        PRINT_ERROR("Cannot access /dev/urandom %s\n", strerror(errno));
    dev_urandom_ptr = fopen("/dev/urandom", "r");
}

void seek_header(int block, struct device_structure *dev) {
        if (fseek(dev->fp, block*SECTOR, SEEK_SET) == -1)
            PRINT_ERROR("fseek to %d on %s: %s\n", block*SECTOR, dev->name, strerror(errno));
}

void read_header(struct block_header_structure *header2, int block, struct device_structure *dev) {
        seek_header(block, dev);

        if (fread(header2, sizeof(*header2), 1, dev->fp) != 1)
            PRINT_ERROR("fread %lu bytes from %s: %s\n", sizeof(*header2), dev->name, strerror(errno));
}

// check block device headers
void check_header(int block) {
        int i;
        struct block_header_structure header;
       
    for (i = 0; i < total_nb_devices; i++) {
        struct device_structure *d = &device_structure[i];
        
        read_header(&header, block, d);

        if (header.zero == 0 && header.zero_or_one>>1 == 0) { //padd to the right
            if (d->blocks[header.number] != -1)
                    PRINT_ERROR("block already claimed.\n");
            if (header.number >= total_nb_blocks)
                    PRINT_ERROR("illegal block number\n");
            // add the padded block
            d->total_nb_blocks++;

            d->blocks[header.number] = block;
            if (header.zero_or_one == 1)
                d->last_block = block;
// DEBUG        printf("block %d claimed by device %d\n", block, i);
            return;
        }
    }
    // store nb_empty_blocks
    empty_blocks[nb_empty_blocks++] = block;
}

void check_device(int verbose) {
    int i, j, blocks_seen = 0;
    int *bools = NULL;

    bools = check_memory(sizeof(int)*total_nb_blocks);

    if (verbose == 1) printf("  visible blocks\t");

        for (i = 0; i < total_nb_devices; i++) {
            if (verbose == 1) printf(".");

            struct device_structure *d = &device_structure[i];
            int adder = 0;
            blocks_seen += device_structure[i].total_nb_blocks;
            for (j = 0; j < d->total_nb_blocks; j++) {
                while (d->blocks[j + adder] == -1) {
                    adder++;
                }
                bools[d->blocks[j+adder]] = 1;
            }
        }
    if (verbose == 1) printf("\n");

    blocks_seen += nb_empty_blocks;
    for (j = 0; j < nb_empty_blocks; j++) bools[empty_blocks[j]] = 1;

    int k, l;
    if (verbose == 1) printf("  blocks continuity\t");
    
    for ( k = 0; k < total_nb_devices; k++) {
        if (verbose == 1) printf(".");

            struct device_structure *d = &device_structure[k];
            if (d->last_block == -1 && d->total_nb_blocks > 0)
                PRINT_ERROR("there is no last block while %s not empty\n", d->name); // password mismatch
            for (l = 0; l < d->total_nb_blocks - 1; l++) {
                if (d->blocks[l] == -1)
                    PRINT_ERROR("block %d of %s missing\n", l, d->name); // password mismatch
                if (d->blocks[l] == d->last_block)
                    PRINT_ERROR("middle block %d of %s marked as last block\n", l, d->name);
            }
            if (d->blocks[l] != d->last_block)
                PRINT_ERROR("last block of %s not marked as last block or missing\n", d->name);
    }
    if (verbose == 1) printf("\n");
}

int select_random_block(void) {
    uint32_t rd;
    
    if (fread(&rd, sizeof(rd), 1, dev_urandom_ptr) != 1)
        PRINT_ERROR("reading from /dev/urandom: %s\n", strerror(errno));
    
    return (int)(((double)nb_empty_blocks)*rd/(UINT32_MAX+1.0));
}

void write_header(struct block_header_structure *header2, int block, struct device_structure *dev) {
    seek_header(block, dev);
    if (fwrite(header2, sizeof(*header2), 1, dev->fp) != 1)
        PRINT_ERROR("fwrite %lu bytes to %s: %s\n", sizeof(*header2), dev->name, strerror(errno));
}

void allocate_single_block(struct device_structure *d) {
    struct block_header_structure old, new;
    int block = select_random_block();
    
    printf("Empty block %d becomes block %d of %s\n", empty_blocks[block], d->total_nb_blocks, d->name);
    
    new.zero = old.zero = 0;
    old.zero_or_one = 0;
    new.zero_or_one = 1;
    old.number = d->total_nb_blocks - 1;
    new.number = d->total_nb_blocks;
    
    // adding block to device list
    d->last_block = d->blocks[d->total_nb_blocks++] = empty_blocks[block];
    // removing block from empty list
    empty_blocks[block] = empty_blocks[--nb_empty_blocks];
    // marking old 'last block' (if any) a 'non last block'
    if (d->total_nb_blocks > 1)
        write_header(&old, d->blocks[old.number], d);
    // marking new block as 'last block'
    write_header(&new, d->blocks[new.number], d);
}

void destroy_block(struct device_structure *d) {
    struct block_header_structure header;
    int i, block;
    
    block = d->blocks[d->total_nb_blocks - 1];
    
    if (fread(&header, sizeof(header), 1, dev_urandom_ptr) != 1)
        PRINT_ERROR("error reading /dev/urandom");
    
    write_header(&header, block, d);
    
    for (i = 0; i < eff_block_size512; i++) {
        if (fread(&header, sizeof(header), 1, dev_urandom_ptr) != 1)
            PRINT_ERROR("error reading /dev/urandom: %s\n", strerror(errno));
        if (fseeko(d->fp, (BLOCK_SECTOR_OFFSET(block) + i)*SECTOR, SEEK_SET) == -1)
            PRINT_ERROR("fseeko to %lu on %s: %s\n", (BLOCK_SECTOR_OFFSET(block) + i)*SECTOR, d->name, strerror(errno));
    
        if (fwrite(&header, sizeof(header), 1, d->fp) != 1)
            PRINT_ERROR("error writing to %s: %s\n", d->name, strerror(errno));
    }
}

void remove_block(struct device_structure *d) {
    struct block_header_structure last;
    
    printf("Freeing block n*%d indexed as %dth block from %s\n", d->blocks[d->total_nb_blocks-1],d->total_nb_blocks - 1, d->name);
    destroy_block(d);
    
    last.zero = 0;
    last.zero_or_one = 1;
    last.number = --d->total_nb_blocks - 1;
    empty_blocks[nb_empty_blocks++] = d->blocks[d->total_nb_blocks];
    d->blocks[d->total_nb_blocks] = -1;
    if (d->total_nb_blocks == 0) {
        d->last_block = -1;
        return;
    }
    d->last_block = d->blocks[d->total_nb_blocks - 1];
    
    write_header(&last, d->last_block, d);
}

void enlarge(struct device_structure *d, int allocated_blocks) {
    while (allocated_blocks--)
        allocate_single_block(d);
}

void shrink(struct device_structure *d, int allocated_blocks) {
    while (allocated_blocks++)
        remove_block(d);
}

// commands
void print_offset() {
    struct device_structure *d;
    int i;
    d = &device_structure[0];
    
    if (d->total_nb_blocks == 0)
        PRINT_ERROR("%s is zero sized\n", d->name);
    for (i = 0; i < d->total_nb_blocks; i++)
        printf("%lu %lu linear %s %lu\n", i*eff_block_size512, eff_block_size512,d->name, BLOCK_SECTOR_OFFSET(d->blocks[i]));
}

void print_table() {
    int i;
    
    printf("Nb of blocks\tDevice\t\t\tDevice nb\tSize\n");
    for (i = 0; i < total_nb_devices; i++)
            printf("+    %d\t\t%s\t\t   %d\t\t%dMb\n", device_structure[i].total_nb_blocks, device_structure[i].name, i+1, device_structure[i].total_nb_blocks*2);
    printf("+    %d\t<-- Total nb of free blocks\n", nb_empty_blocks);
    printf("-----------");
    printf("\n=    %d\t<-- Total nb of blocks\n", total_nb_blocks);
    printf("space ratio: %d%% free\n", nb_empty_blocks*100/total_nb_blocks);
}

void list_free_blocks(int block) {
    int i;
    struct block_header_structure header;
    for (i = 0; i < total_nb_devices; i++)
        {
        struct device_structure *d = &device_structure[i];
        read_header(&header, block, d);
        if (header.zero == 0 && header.zero_or_one >> 1 == 0) {
            // add block to table
            if (d->blocks[header.number] != -1)
                PRINT_ERROR("block already claimed!\n");
            if (header.number >= total_nb_blocks)
                PRINT_ERROR("illegal block number\n");
            d->total_nb_blocks++;
    
            d->blocks[header.number] = block;
            if (header.zero_or_one == 1)
                d->last_block = block;
            printf("block %d claimed by device %d\n", block, i+1);
            return;
        }
    }
    empty_blocks[nb_empty_blocks++] = block;
}

void set_size(int yes) {
    struct device_structure *d;
    int allocated_blocks;
    
    if (device_nb < 0 || device_nb > total_nb_devices)
        PRINT_ERROR("illegal device %d\n", device_nb);
    d = &device_structure[device_nb];
    allocated_blocks = nb_allocated_blocks - d->total_nb_blocks;
    if (allocated_blocks > nb_empty_blocks)
        PRINT_ERROR("requested new size too large, not enough unclaimed blocks\n");
    if (allocated_blocks == 0)
        return;
    if (allocated_blocks < 0) {
        if ( yes == 0 ) {
            printf("Do you intend to resize? (CTRL-C to  abort) ");
            getchar();
        }
        printf("Shrinking ...\n");
        shrink(d, allocated_blocks);
    }
    if (allocated_blocks > 0) {
        if ( yes == 0 ) {
            printf("Do you intend to resize? (CTRL-C to  abort) ");
            getchar();
        }
        printf("Enlarging ...\n");
        enlarge(d, allocated_blocks);
    }
}

void dmsetup_add() {
    struct device_structure *d;
    int i;
    char buf[128];

    struct dm_task *task = dm_task_create(DM_DEVICE_CREATE);
    d = &device_structure[0];
    
    if (d->total_nb_blocks == 0)
        PRINT_ERROR("%s is zero sized\n", d->name);
    if (!task)
        PRINT_ERROR("unable to create dm_task for %s\n", dm_device);
    if (!dm_task_set_name(task, dm_device))
        PRINT_ERROR("dm_task_set_name to %s failed\n", dm_device);
    for (i = 0; i < d->total_nb_blocks; i++) {
        snprintf(buf, 128, "%s %lu", d->name, BLOCK_SECTOR_OFFSET(d->blocks[i]));
        if (!dm_task_add_target(task, i*eff_block_size512, eff_block_size512, "linear", buf))
            PRINT_ERROR("dm_task_add_target to %s failed\n", dm_device);
    }
    if (!dm_task_run(task))
        PRINT_ERROR("dm_task_run to create %s failed\n", dm_device);
    dm_task_destroy(task);
    dm_lib_release();
    dm_lib_exit();
}

// main
int main(int argc, char **argv) {
    int i, c, j, h;
    enum ACTIONS command = TABLE;
    char *tmp;
    
    while ( (c = getopt_long(argc, argv, optstring, options, NULL) ) != -1) 
        switch (c) {
            case 's':
                command = SET_SIZE;
                j = strtol(optarg, &tmp, 10);
                device_nb = j - 1;
                if (*tmp != ',') {
                    usage("error with the device number\n");
                    exit(1);
                }
                optarg = tmp + 1;
                nb_allocated_blocks = strtol(optarg, &tmp, 10);
                if (*tmp != '\0') {
                    usage("error with the number of blocks\n");
                    exit(1);
                }
                break;
            case 'd':
                dm_device = optarg;
                command = DMSETUP;
                break;
            case 't':
                command = TABLE;
                break;
            case 'o':
                command = OFFSET;
                break;
            case 'l':
                command = LIST_FREE_BLOCKS;
                break;
            case 'y':
                yes = 1;
                break;
            case 'e':
                verbose = 1;
                break;
            case 'h':
                usage(NULL);
                printf( "Examples:\n"
                    "  denyfs --offset      /dev/mapper/fs1\n"
                    "  denyfs --table       /dev/mapper/fs1 /dev/mapper/fs2\n"
                    "  denyfs --setsize     1,12    /dev/mapper/fs1\n"
                    "  denyfs --setsize     2,2     /dev/mapper/fs1 /dev/mapper/fs2 -y\n"
                    "  denyfs --list        /dev/mapper/fs1 /dev/mapper/fs2 /dev/mapper/fs3\n"
                    "  denyfs --dmsetup     fs1_new /dev/mapper/fs1\n\n"
                    );
                exit(0);
                break;
            case 'v':
                printf("%s\n", version);
                exit(0);
                break;
            default:
                usage(NULL);
                exit(1);
        }
    
    if (argc-optind == 0) {
        usage("no devices specified\n");
        exit(1);
    }
    if ((command == DMSETUP) && argc - optind != 1) {
        usage("only one device may be specifed with option -m\n");
        exit(1);
    }
    if ((command == OFFSET ) && argc - optind != 1) {
        usage("use a single [crypsetup device]1,2,3.. to display offset\n");
        exit(1);
    }
    
    if (verbose == 1) {
        printf("Checking ...\n");
    }
    
    check_access(argc - optind, argv + optind, verbose); // (int, char**)

    for (i=0; i<total_nb_blocks; i++) 
        check_header(i);
    check_device(verbose);
    
    switch (command) {
        case TABLE:
            print_table();
            break;
        case LIST_FREE_BLOCKS:
            printf("Listing free blocks:\n");
            for (h = 0; h < total_nb_blocks; h++) 
                list_free_blocks(h);
            break;
        case SET_SIZE:
            set_size(yes);
            break;
        case OFFSET:
            print_offset();
            break;
        case DMSETUP:
            dmsetup_add();
            break;
    }
    exit(0);
}
