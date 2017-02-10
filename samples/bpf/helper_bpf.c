/*
 * Compile:
 * gcc -I ../../usr/include -I ../../tools/lib -I ../../tools/include \
 * -I ./samples/bpf -Wall sock_example.c ../../tools/lib/bpf/bpf.c -o \
 * helper_bpf
 *
 * TEST:
 *  iptables -A OUTPUT -t raw -p tcp -d 127.0.0.3 -j MARK --set-mark 4
 *  netperf -4 -t TCP_STREAM -H 127.0.0.3 
 */

#define _GNU_SOURCE

#define offsetof(type, member)	__builtin_offsetof(type, member)

#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <bpf/bpf.h>
#include "libbpf.h"

/* the struct used to save the 64 bit tagUid pairs as well as a counter set
currently the counter set is always 0*/
struct stats {
    uint32_t uid;
    uint64_t packets;
    uint64_t bytes;
};

static int map_fd, prog_fd;

static void maps_create(void)
{
    map_fd = bpf_create_map(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(struct stats), 10, 0);
    if(map_fd < 0 ) {
        printf("map create failed: %s\n", strerror(errno));
    }

}

static void prog_load(void)
{
	static char log_buf[1 << 16];

	struct bpf_insn prog[] = {
      /*0. move sk_buff to r6*/
      BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
      /*1. get socket cookie from sk_buff*/
      BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_socket_cookie),
      /*2. load the socket cookie to stack*/
      BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -8),
      /*3-4.load the socket cookie address in the stack to r7*/
      BPF_MOV64_REG(BPF_REG_7, BPF_REG_10),
      BPF_ALU64_IMM(BPF_ADD, BPF_REG_7, -8),
      /*5. load map_fd of the socketCookie to uidtag map, the load instruction
      takes two step in program counter*/
      BPF_LD_MAP_FD(BPF_REG_1, map_fd),
      /*7-8. load socket cookie to r2 and lookup the uidtag value in the map*/
      BPF_MOV64_REG(BPF_REG_2, BPF_REG_7),
      BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
      /*9. if r0 != 0x0, go to pc+16, since we have the cookie stored already*/
      BPF_JMP_IMM(BPF_AND, BPF_REG_0, 0, 14),
      /*10-24 get the uid and use the socket cookie number as tag to update a
      new data entry in the cookie to uidtag map.*/
      BPF_MOV64_REG(BPF_REG_1, BPF_REG_6),
      BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_get_socket_uid),
      BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_0, -32 + offsetof(struct stats, uid)),
      BPF_ST_MEM(BPF_DW, BPF_REG_10, -32 + offsetof(struct stats, packets), 1),
      BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_6, offsetof(struct __sk_buff, len)),
      BPF_STX_MEM(BPF_DW, BPF_REG_10, BPF_REG_1, -32 + offsetof(struct stats, bytes)),
      BPF_LD_MAP_FD(BPF_REG_1, map_fd), /* 19-20. load mapfd, pc+=2*/
      BPF_MOV64_REG(BPF_REG_2, BPF_REG_7),
      BPF_MOV64_REG(BPF_REG_3, BPF_REG_10),
      BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, -32),
      BPF_MOV64_IMM(BPF_REG_4, 0),
      BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_update_elem),
      BPF_JMP_IMM(BPF_JA, 0, 0, 5),
      BPF_MOV64_REG(BPF_REG_9, BPF_REG_0),
      BPF_MOV64_IMM(BPF_REG_1, 1),
      BPF_RAW_INSN(BPF_STX | BPF_XADD | BPF_DW, BPF_REG_9, BPF_REG_1,
      				offsetof(struct stats, packets), 0),
      BPF_LDX_MEM(BPF_W, BPF_REG_1, BPF_REG_6, offsetof(struct __sk_buff, len)),
      BPF_RAW_INSN(BPF_STX | BPF_XADD | BPF_DW, BPF_REG_9, BPF_REG_1,
      				offsetof(struct stats, bytes), 0),      
      BPF_LDX_MEM(BPF_W, BPF_REG_0, BPF_REG_6, offsetof(struct __sk_buff, len)),
      BPF_EXIT_INSN(),
    };

    prog_fd = bpf_load_program(BPF_PROG_TYPE_SOCKET_FILTER, prog, sizeof(prog)/sizeof(prog[0]),
                            "GPL", 0, log_buf, sizeof(log_buf));
    if (prog_fd < 0) {
        printf("failed to load prog '%s'\n", log_buf);
    }
}

static void prog_attach_iptables(void)
{	
	if (bpf_obj_pin(prog_fd, "/mnt/bpf/iptables"))
		error(1, errno, "bpf_obj_pin");

	system("iptables -A INPUT -m bpf --object-pinned /mnt/bpf/iptables");
}

static void print_table(void)
{
	struct stats curEntry;
    //read out the data stored in map.
    uint32_t curN = 0xffffffff;
    uint32_t nextN, res;
    while ( bpf_map_get_next_key(map_fd, &curN, &nextN) > -1) {
        curN = nextN;
        res = bpf_map_lookup_elem(map_fd, &curN, &curEntry);
        if(res < 0 ) {
            printf("fail to get entry value: %s\n", strerror(errno));
        } else {
          printf("the result socket cookie is: %u\n"
          		 "uid: 0x%x Pakcet Count: %lu, Bytes Count: %lu\n",
                 curN, curEntry.uid, curEntry.packets, curEntry.bytes);
        }
    }
}

int main(int argc, char **argv)
{	
	maps_create();
	prog_load();
	prog_attach_iptables();

	for(int i = 0; i < 10; i++) {
		print_table();
		sleep(1);
	};

	return 0;
}

