typedef int (*superproc_callback) (struct seq_file *seq);
extern int superproc_register(superproc_callback info,
			      superproc_callback value, int timeout);
extern int superproc_unregister(int handle);
