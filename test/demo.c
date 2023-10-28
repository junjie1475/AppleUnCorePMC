#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
//#include <sys/guarded.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioccom.h>
#include <limits.h>
#include <unistd.h>

typedef __uint64_t guardid_t;
int guarded_open_np(const char *path, const guardid_t *guard, u_int guardflags, int flags, ...);
#define GUARD_CLOSE             (1u << 0)
#define GUARD_DUP               (1u << 1)
#define GUARD_WRITE             (1u << 4)


#define SREG_READ(SR)                                                          \
  ({                                                                           \
    uint64_t VAL = 0;                                                          \
    __asm__ volatile("isb \r\n mrs %0, " SR " \r\n isb \r\n" : "=r"(VAL));     \
    VAL;                                                                       \
  })

#define MACHDEP_TPIDR_CPUNUM_SHIFT     0
#define MACHDEP_TPIDR_CPUNUM_MASK      0x0000000000000fff
#define MACHDEP_TPIDR_CLUSTERID_SHIFT  12
#define MACHDEP_TPIDR_CLUSTERID_MASK   0x00000000000ff000

static int get_current_core() {
    return (SREG_READ("TPIDR_EL0") & MACHDEP_TPIDR_CPUNUM_MASK) >> MACHDEP_TPIDR_CPUNUM_SHIFT;
}
static int get_current_cluster() {
    return( SREG_READ("TPIDR_EL0") & MACHDEP_TPIDR_CLUSTERID_MASK) >> MACHDEP_TPIDR_CLUSTERID_SHIFT;
}

#define UNCORE_DEV_PATH "/dev/monotonic/uncore"

#define REF_CYCLE_EVENT 0x2
#define REF_TIMEBASE_EVENT 0x3
#define CTRS_MAX 128

#define MT_IOC(x) _IO('m', (x))
#define MT_IOC_RESET MT_IOC(0)
#define MT_IOC_ADD MT_IOC(1)
#define MT_IOC_ENABLE MT_IOC(2)
#define MT_IOC_COUNTS MT_IOC(3)
#define MT_IOC_GET_INFO MT_IOC(4)

struct monotonic_config {
	uint64_t event;
	uint64_t allowed_ctr_mask;
	uint64_t cpu_mask;
};

union monotonic_ctl_add {
	struct {
		struct monotonic_config config;
	} in;

	struct {
		uint32_t ctr;
	} out;
};

union monotonic_ctl_enable {
	struct {
		bool enable;
	} in;
};

union monotonic_ctl_counts {
	struct {
		uint64_t ctr_mask;
	} in;

	struct {
		uint64_t counts[1];
	} out;
};

union monotonic_ctl_info {
	struct {
		unsigned int nmonitors;
		unsigned int ncounters;
	} out;
};

uint8_t event_mapping[16];

typedef struct event_count_map {
    uint8_t event;
    uint64_t count;
} event_count_map_t;

event_count_map_t event_count_mapping[256] = {0};
int top = 0;

static void
uncore_counts(int fd, uint64_t ctr_mask, uint64_t *counts)
{
	int r;
	union monotonic_ctl_counts *cts_ctl;

	cts_ctl = (union monotonic_ctl_counts *)counts;
	cts_ctl->in.ctr_mask = ctr_mask;

	r = ioctl(fd, MT_IOC_COUNTS, cts_ctl);
}

static uint32_t
uncore_add(int fd, uint64_t event, uint64_t allowed_ctrs)
{
	int save_errno;
	int r;
	uint32_t ctr;
	union monotonic_ctl_add add_ctl;

	add_ctl.in.config.event = event;
    add_ctl.in.config.allowed_ctr_mask = allowed_ctrs;
	add_ctl.in.config.cpu_mask = UINT64_MAX; // here we recieve events from all the cores

    r = sysctlbyname("kern.uncore_add", NULL, NULL, &add_ctl, sizeof(union monotonic_ctl_add));
    
    if(r != 0) {
        printf("uncore add event failed, error = %d\n", r);
        exit(0);
    }
	ctr = add_ctl.out.ctr;
    
    // Insert event to the mapping table
    event_mapping[ctr] = event;
	return ctr;
}

static void
uncore_enable(int fd)
{
    int r;
	union monotonic_ctl_enable en_ctl = {
		.in = { .enable = true }
	};

	r = ioctl(fd, MT_IOC_ENABLE, &en_ctl);
    if(r != 0) {
        printf("uncore enable failed, error = %d\n", r);
        exit(0);
    }
}

static void
uncore_reset(int fd) {
    int r;
    r = ioctl(fd, MT_IOC_RESET);
    
    if(r != 0) {
        printf("uncore reset failed, error = %d\n", r);
        exit(0);
    }
    
    // Clear event from the mapping table
    memset(event_mapping, 0, 16);
}

int main() {
	guardid_t guard;
	int fd;
    int r;
    
    uint64_t ctr_mask = UINT64_MAX;
	guard = 0xa5adcafe;
    uint64_t counts[2][CTRS_MAX];
    
    
	fd = guarded_open_np(UNCORE_DEV_PATH, &guard,
	    GUARD_CLOSE | GUARD_DUP | GUARD_WRITE, O_CLOEXEC | O_EXCL);

	if (fd < 0 && errno == ENOENT) {
		printf("uncore counters are unsupported\n");
		exit(0);
	}
    
    for (int i = 0; i < 16; i++) {
        
        uncore_reset(fd);
        // add event to counter 0 - 15
        for (int j = 0; j < 16; j++) {
            int ctr = uncore_add(fd, i * 16 + j, UINT64_MAX);
            printf("Added event %d to UMPC%d\n", i * 16 + j, ctr);
        }
        
        uncore_enable(fd);
        
        // get current cluster code running on
        int cluster = get_current_cluster();
        uncore_counts(fd, ctr_mask, counts[0]);
        int i = 100000;
        while(i--) {
            ;
        }
        uncore_counts(fd, ctr_mask, counts[1]);
        
        if(cluster != get_current_cluster()) {
            printf("Warning: Code runs on different clusters!\n");
        }

        for (int k = 0; k < 16; k++) {
            uint64_t count = counts[1][k + cluster * 16] - counts[0][k + cluster * 16];
            if(count != 0) {
                event_count_mapping[top].event = event_mapping[k];
                event_count_mapping[top].count = count;
                top++;
            }
        }
    }
    printf("All nonzero uncore counters\n");
    for (int i = 0; i < top; i++) {
        printf("Event: %d, Count: %llu\n", event_count_mapping[i].event, event_count_mapping[i].count);
    }
    top = 0;
}
