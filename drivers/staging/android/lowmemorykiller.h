#ifndef __LOWMEMORYKILLER_H
#define __LOWMEMORYKILLER_H

/* The lowest score LMK is using */
#define LMK_SCORE_THRESHOLD 0

extern u32 lowmem_debug_level;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

#endif
