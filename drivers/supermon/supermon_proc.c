/*
	Copyright (C) 1999-2008 Ronald G. Minnich <rminnich@gmail.com>
	Copyright (C) 2006 Li-Ta Lo <ollie@lanl.gov>

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), Version 2 incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice. 

*/
/* ACL:license */
/*
This software and ancillary information (herein called "SOFTWARE")
called Supermon is made available under the terms described
here.  The SOFTWARE has been approved for release with associated
LA-CC Number LA-CC 99-51.

Unless otherwise indicated, this SOFTWARE has been authored by an
employee or employees of the University of California, operator of the
Los Alamos National Laboratory under Contract No.  W-7405-ENG-36 with
the U.S. Department of Energy.  The U.S. Government has rights to use,
reproduce, and distribute this SOFTWARE, and to allow others to do so.
The public may copy, distribute, prepare derivative works and publicly
display this SOFTWARE without charge, provided that this Notice and
any statement of authorship are reproduced on all copies.  Neither the
Government nor the University makes any warranty, express or implied,
or assumes any liability or responsibility for the use of this
SOFTWARE.

If SOFTWARE is modified to produce derivative works, such modified
SOFTWARE should be clearly marked, so as not to confuse it with the
version available from LANL.
*/
/* ACL:license */

/*
 * new supermon sysctl
 * Copyright (C) 2001 Ron Minnich
 * Covered under GPL
 */

#include <linux/autoconf.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/netdevice.h>
#include "supermon_proc.h"

MODULE_AUTHOR("Li-Ta Lo <ollie@lanl.gov>");
MODULE_DESCRIPTION("Supermon module for 2.6 kernel /proc");
MODULE_LICENSE("GPL");

static struct proc_dir_entry *proc_supermon;
static struct proc_dir_entry *proc_supermon_info, *proc_supermon_value;
extern void si_swapinfo(struct sysinfo *);
unsigned long *vmstat_start(int *vminfo_size);
extern unsigned long long xprt_transmit_number;
extern unsigned long long xprt_transmit_total_bytes;



static struct supermon_info {
	int ncpus;
	int nnetif;
} info;

static char *netfields[] = {
	"name",
	"rxbytes", "rxpackets", "rxerrs", "rxdrop", "rxfifo", "rxframe",
	"rxcompressed", "rxmulticast",
	"txbytes", "txpackets", "txerrs", "txdrop", "txfifo", "txcolls",
	"txcarrier", "txcompressed"
};

#ifdef CONFIG_ZONE_DMA
#define TEXT_FOR_DMA(xx) xx "_dma",
#else
#define TEXT_FOR_DMA(xx)
#endif

#ifdef CONFIG_ZONE_DMA32
#define TEXT_FOR_DMA32(xx) xx "_dma32",
#else
#define TEXT_FOR_DMA32(xx)
#endif

#ifdef CONFIG_HIGHMEM
#define TEXT_FOR_HIGHMEM(xx) xx "_high",
#else
#define TEXT_FOR_HIGHMEM(xx)
#endif

#define TEXTS_FOR_ZONES(xx) TEXT_FOR_DMA(xx) TEXT_FOR_DMA32(xx) xx "_normal", \
                                        TEXT_FOR_HIGHMEM(xx) xx "_movable",

static const char * const vmstat_text[] = {
        /* Zoned VM counters */
        "nr_free_pages",
        "nr_inactive",
        "nr_active",
        "nr_anon_pages",
        "nr_mapped",
        "nr_file_pages",
        "nr_dirty",
        "nr_writeback",
        "nr_slab_reclaimable",
        "nr_slab_unreclaimable",
        "nr_page_table_pages",
        "nr_unstable",
        "nr_bounce",
        "nr_vmscan_write",

#ifdef CONFIG_NUMA
        "numa_hit",
        "numa_miss",
        "numa_foreign",
        "numa_interleave",
        "numa_local",
        "numa_other",
#endif

#ifdef CONFIG_VM_EVENT_COUNTERS
        "pgpgin",
        "pgpgout",
        "pswpin",
        "pswpout",

        TEXTS_FOR_ZONES("pgalloc")

        "pgfree",
        "pgactivate",
        "pgdeactivate",

        "pgfault",
        "pgmajfault",

        TEXTS_FOR_ZONES("pgrefill")
        TEXTS_FOR_ZONES("pgsteal")
        TEXTS_FOR_ZONES("pgscan_kswapd")
        TEXTS_FOR_ZONES("pgscan_direct")

        "pginodesteal",
        "slabs_scanned",
        "kswapd_steal",
        "kswapd_inodesteal",
        "pageoutrun",
        "allocstall",

        "pgrotated",
#endif
};

unsigned long *vmstat_start(int *vminfo_size)
{
        unsigned long *v;
	#ifdef CONFIG_VM_EVENT_COUNTERS
	        unsigned long *e;
	#endif
	        int i;
	
	#ifdef CONFIG_VM_EVENT_COUNTERS
	        v = kmalloc(NR_VM_ZONE_STAT_ITEMS * sizeof(unsigned long)
	                        + sizeof(struct vm_event_state), GFP_KERNEL);
	#else
	        v = kmalloc(NR_VM_ZONE_STAT_ITEMS * sizeof(unsigned long),
	                        GFP_KERNEL);
	#endif
//	        if (!v)
//	                return ERR_PTR(-ENOMEM);
	        for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
	                v[i] = global_page_state(i);
	#ifdef CONFIG_VM_EVENT_COUNTERS
	        e = v + NR_VM_ZONE_STAT_ITEMS;
	        all_vm_events(e);
	        e[PGPGIN] /= 2;         /* sectors -> kbytes */
	        e[PGPGOUT] /= 2;
	#endif
	*vminfo_size = NR_VM_ZONE_STAT_ITEMS * sizeof(unsigned long) + sizeof(struct vm_event_state);
	return v;
}

static int
supermon_meta_info(struct supermon_info *info, struct seq_file *seq)
{
	int i, vmstat_size;
	PRINTIT("(");
	PRINTIT("(cpuinfo 'U cpu user nice system)");
	PRINTIT("(time 'U timestamp jiffies)");
	PRINTIT("(netinfo 'U ");
	for (i = 0; i < sizeof(netfields) / sizeof(netfields[0]); i++) {
		PRINTIT("%s ", netfields[i]);
	}
	PRINTIT(")"); // End netinfo
#ifdef ON_CLUSTER
	PRINTIT("(rdma 'U xprt_transmit_number xprt_transmit_total_bytes)");
        {
        PRINTIT(" (balance_dirty_pages nr_reclaimable bdi_nr_reclaimable nr_writeback bdi_nr_writeback background_thresh_dwrudis dirty_thresh_dwrudis bdi_thresh pages_written write_chunk) ");
        }
#endif
	PRINTIT
	    ("(meminfo 'U pagesize totalram sharedram freeram bufferram totalhigh freehigh mem_unit)");
//      PRINTIT("(swapinfo 'U "
//              "(pagesize %lu) (total used pagecache))", PAGE_SIZE);
#if 0
	vmstat_start(&vmstat_size);
	PRINTIT("(vmstat 'U ");
        for (i = 0; i < vmstat_size  / sizeof(unsigned long); i++) {
		PRINTIT("%s ", vmstat_text[i]);
        }
	PRINTIT(")"); // End vmstat
#endif
	PRINTIT(")\n"); // End #
	return 0;
}

static int supermon_proc_info_seq_show(struct seq_file *seq, void *offset)
{
	supermon_meta_info(&info, seq);
	return 0;
}

static int
supermon_proc_info_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, supermon_proc_info_seq_show,
			   PDE(inode)->data);
}

struct file_operations proc_supermon_info_ops = {
	.open = supermon_proc_info_open_fs,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int
supermon_convert_net_value(struct supermon_info *info,
			   struct seq_file *seq)
{
	struct net_device *dev;

	PRINTIT("(netinfo ");
	read_lock(&dev_base_lock);
	for_each_netdev(&init_net, dev) {
		struct net_device_stats *stats =
		    (dev->get_stats ? dev->get_stats(dev) : NULL);
		if (!stats)
			continue;

		PRINTIT("(%s ", dev->name);
		PRINTIT(" %lu", stats->rx_bytes);
		PRINTIT(" %lu", stats->rx_packets);
		PRINTIT(" %lu", stats->rx_errors);
		PRINTIT(" %lu",
			stats->rx_dropped + stats->rx_missed_errors);
		PRINTIT(" %lu", stats->rx_fifo_errors);
		PRINTIT(" %lu",
			stats->rx_length_errors +
			stats->rx_over_errors +
			stats->rx_crc_errors + stats->rx_frame_errors);
		PRINTIT(" %lu", stats->rx_compressed);
		PRINTIT(" %lu", stats->multicast);
		PRINTIT(" %lu", stats->tx_bytes);
		PRINTIT(" %lu", stats->tx_packets);
		PRINTIT(" %lu", stats->tx_errors);
		PRINTIT(" %lu", stats->tx_dropped);
		PRINTIT(" %lu", stats->tx_fifo_errors);
		PRINTIT(" %lu", stats->collisions);
		PRINTIT(" %lu",
			stats->tx_carrier_errors +
			stats->tx_aborted_errors +
			stats->tx_window_errors +
			stats->tx_heartbeat_errors);
		PRINTIT(" %lu", stats->tx_compressed);
		PRINTIT(")"); // End netinfo
	}
#ifdef ON_CLUSTER
	PRINTIT("(rdma %llu %llu)", xprt_transmit_number, xprt_transmit_total_bytes);

	read_unlock(&dev_base_lock);
	// My patch
        {
        extern long nr_reclaimable, bdi_nr_reclaimable;
        extern long nr_writeback, bdi_nr_writeback;
        extern long background_thresh_dwrudis;
        extern long dirty_thresh_dwrudis;
        extern long bdi_thresh;
        extern unsigned long pages_written;
        extern unsigned long write_chunk;
        PRINTIT(" (balance_dirty_pages %li %li %li %li %li %li %li %lu %lu) ", nr_reclaimable, bdi_nr_reclaimable, nr_writeback, bdi_nr_writeback, background_thresh_dwrudis, dirty_thresh_dwrudis, bdi_thresh, pages_written, write_chunk);
        }
	PRINTIT(")");
#endif

	return 0;

}

static int
supermon_convert_value(struct supermon_info *info, struct seq_file *seq)
{
	int n, i, vmstat_size;
	struct timeval now;
	struct sysinfo meminfo;
	unsigned long *vmstat_info = 0;
	PRINTIT("(");
	PRINTIT("(cpuinfo ");
	for_each_online_cpu(n) {
		PRINTIT("%d ", n);
		for (i = 0; i < 3; i++) {
			if (i == 0) {
				PRINTIT(" %lld",
					kstat_cpu(n).cpustat.user);
			} else if (i == 1) {
				PRINTIT(" %lld",
					kstat_cpu(n).cpustat.nice);
			} else {
				PRINTIT(" %lld",
					kstat_cpu(n).cpustat.system);
			}
		}
	}
	PRINTIT(")"); // Close cpuinfo
	do_gettimeofday(&now);
	PRINTIT("(time 0x%lx %lu)", now.tv_sec * 1000 + now.tv_usec / 1000, jiffies);
	supermon_convert_net_value(info, seq);
	si_meminfo(&meminfo);
//      si_swapinfo(&meminfo);
	PRINTIT
	    ("(meminfo %lu %lu %lu %lu %lu %lu %lu %u) ", PAGE_SIZE, meminfo.totalram << 2,
	     meminfo.sharedram << 2, meminfo.freeram << 2,
	     meminfo.bufferram << 2, meminfo.totalhigh << 2,
	     meminfo.freehigh << 2, meminfo.mem_unit << 2);
#if 0
	vmstat_info = vmstat_start(&vmstat_size);
	PRINTIT("(vmstat ");
        for (i = 0; i < vmstat_size  / sizeof(vmstat_info[0]); i++) {
		PRINTIT("%lu ", vmstat_info[i]);
        }
	PRINTIT(")"); // End vmstat
#endif
	PRINTIT(")\n"); // End S
	return 0;
}

static int supermon_proc_value_seq_show(struct seq_file *seq, void *offset)
{
	supermon_convert_value(&info, seq);
	return 0;
}

static int
supermon_proc_value_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, supermon_proc_value_seq_show,
			   PDE(inode)->data);
}

static struct file_operations proc_supermon_value_ops = {
	.open = supermon_proc_value_open_fs,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init supermon_proc_init(void)
{
	proc_supermon =
	    create_proc_entry("supermon", S_IFDIR | S_IRUGO | S_IXUGO,
			      NULL);
	proc_supermon_info = create_proc_entry("#", S_IRUGO | S_IXUGO,
					       proc_supermon);
	proc_supermon_info->proc_fops = &proc_supermon_info_ops;
	proc_supermon_value = create_proc_entry("S", S_IRUGO | S_IXUGO,
						proc_supermon);
	proc_supermon_value->proc_fops = &proc_supermon_value_ops;
	return 0;
}

static void __exit supermon_proc_exit(void)
{
	remove_proc_entry("S", proc_supermon);
	remove_proc_entry("#", proc_supermon);
	remove_proc_entry("supermon", NULL);
}

module_init(supermon_proc_init);
module_exit(supermon_proc_exit);


