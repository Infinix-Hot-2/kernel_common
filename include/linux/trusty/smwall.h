/*
 * Copyright (c) 2016 Google Inc. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __LINUX_TRUSTY_SMWALL_H
#define __LINUX_TRUSTY_SMWALL_H

/*
 * SM Wall buffer is formatted by secure side to contain the location of
 * objects it exports:
 *
 * In general it starts with sm_wall_toc header struct followed
 * by array of sm_wall_toc_item objects describing location of
 * individual objects within SM Wall buffer.
 */

#define SM_WALL_TOC_VER   1     /* current version of TOC structure */

struct sm_wall_toc_item {
	u32 id;       /* item id */
	u32 offset;   /* offset relative to appropriate base,
		       * global vs. individual cpu
		       */
	u32 size;     /* item size */
	u32 reserved; /* reserved: must be set to zero */
};

struct sm_wall_toc {
	u32 version;  /* current toc structure version */
	u32 cpu_num;  /* number ot cpus supported  */

	u32 per_cpu_num_items;   /* number of per cpu items registered */
	u32 per_cpu_region_size; /* size of each per cpu data region */
	u32 per_cpu_toc_offset;  /* offset of per_cpu item table */
	u32 per_cpu_base_offset; /* offset of per cpu data region */

	u32 global_num_items;    /* number of global items registered */
	u32 global_region_size;  /* size of global data region */
	u32 global_toc_offset;   /* offset of the start of global item table */
	u32 global_base_offset;  /* offset to the global data region */
};

#endif /* __LINUX_TRUSTY_SMWALL_H */

