// SPDX-License-Identifier: GPL-2.0
/* Freestanding ARM64 /init for the third-update QEMU regression run. */
#include <linux/bpf.h>
#include <linux/btf.h>
#include "../../../include/linux/filter.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PTR(p) ((__u64)(unsigned long)(p))
static int failures;
static char verifier_log[16384];

void *memset(void *p, int c, unsigned long n)
{
	unsigned char *s = p;
	while (n--)
		*s++ = c;
	return p;
}

static long call(long nr, long a, long b, long c, long d, long e, long f)
{
	register long x0 asm("x0") = a;
	register long x1 asm("x1") = b;
	register long x2 asm("x2") = c;
	register long x3 asm("x3") = d;
	register long x4 asm("x4") = e;
	register long x5 asm("x5") = f;
	register long x8 asm("x8") = nr;
	asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3),
		     "r"(x4), "r"(x5), "r"(x8) : "memory", "cc");
	return x0;
}

static unsigned long length(const char *s)
{
	unsigned long n = 0;
	while (s[n])
		n++;
	return n;
}

static void say(const char *s)
{
	call(64, 1, (long)s, length(s), 0, 0, 0);
}

static void check(const char *name, int ok)
{
	say(ok ? "PASS: " : "FAIL: ");
	say(name);
	say("\n");
	failures += !ok;
}

static long bpf(int command, union bpf_attr *a)
{
	return call(280, command, (long)a, sizeof(*a), 0, 0, 0);
}

static void close_fd(long fd)
{
	if (fd >= 0)
		call(57, fd, 0, 0, 0, 0, 0);
}

static int run(const struct bpf_insn *insns, unsigned int count,
	       unsigned int expected, const unsigned char *packet)
{
	static const char license[] = "GPL";
	union bpf_attr a = {};
	struct bpf_prog_info info = {};
	long fd, ret;
	int ok;

	a.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
	a.insn_cnt = count;
	a.insns = PTR(insns);
	a.license = PTR(license);
	a.log_buf = PTR(verifier_log);
	a.log_size = sizeof(verifier_log);
	a.log_level = 1;
	fd = bpf(BPF_PROG_LOAD, &a);
	if (fd < 0) {
		say(verifier_log);
		return 0;
	}
	memset(&a, 0, sizeof(a));
	a.info.bpf_fd = fd;
	a.info.info_len = sizeof(info);
	a.info.info = PTR(&info);
	ret = bpf(BPF_OBJ_GET_INFO_BY_FD, &a);
	ok = ret == 0 && info.jited_prog_len > 0;
	memset(&a, 0, sizeof(a));
	a.test.prog_fd = fd;
	a.test.data_size_in = 64;
	a.test.data_in = PTR(packet);
	a.test.repeat = 1;
	ret = bpf(BPF_PROG_TEST_RUN, &a);
	ok &= ret == 0 && a.test.retval == expected;
	close_fd(fd);
	return ok;
}

static void test_bpf(void)
{
	unsigned char packet[64] = { 0, 0, 0, 1 };
	union bpf_attr a = {};
	unsigned int key = 0, value = 42, result = 0;
	long fd, ret;
	struct bpf_insn jump[] = {
		BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
		BPF_LD_ABS(BPF_W, 0),
		BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
		BPF_LD_IMM64(BPF_REG_2, 0x100000000ULL),
		BPF_ALU64_REG(BPF_OR, BPF_REG_1, BPF_REG_2),
		BPF_JMP32_IMM(BPF_JEQ, BPF_REG_1, 1, 2),
		BPF_MOV64_IMM(BPF_REG_0, 0), BPF_EXIT_INSN(),
		BPF_MOV64_IMM(BPF_REG_0, 1), BPF_EXIT_INSN(),
	};
	struct bpf_insn subprog[] = {
		BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, BPF_PSEUDO_CALL, 0, 1),
		BPF_EXIT_INSN(), BPF_MOV64_IMM(BPF_REG_0, 7), BPF_EXIT_INSN(),
	};
	struct bpf_insn boot_time[] = {
		BPF_EMIT_CALL(BPF_FUNC_ktime_get_boot_ns),
		BPF_MOV64_IMM(BPF_REG_0, 1), BPF_EXIT_INSN(),
	};

	check("ARM64 JMP32 execution and JIT", run(jump, COUNT(jump), 1, packet));
	jump[6] = BPF_JMP32_REG(BPF_JEQ, BPF_REG_1, BPF_REG_0, 2);
	check("ARM64 register JMP32 execution", run(jump, COUNT(jump), 1, packet));
	jump[6] = BPF_JMP32_REG(BPF_JSET, BPF_REG_1, BPF_REG_2, 2);
	check("ARM64 JSET ignores upper bits", run(jump, COUNT(jump), 0, packet));
	packet[0] = packet[1] = packet[2] = packet[3] = 255;
	jump[6] = BPF_JMP32_IMM(BPF_JSLT, BPF_REG_1, 0, 2);
	check("ARM64 signed JMP32 execution", run(jump, COUNT(jump), 1, packet));
	{
		static const struct {
			unsigned char op;
			unsigned char expected;
			const char *immediate, *reg;
		} cases[] = {
			{ BPF_JEQ, 0, "JMP32 immediate JEQ", "JMP32 register JEQ" },
			{ BPF_JGT, 1, "JMP32 immediate JGT", "JMP32 register JGT" },
			{ BPF_JLT, 0, "JMP32 immediate JLT", "JMP32 register JLT" },
			{ BPF_JGE, 1, "JMP32 immediate JGE", "JMP32 register JGE" },
			{ BPF_JLE, 0, "JMP32 immediate JLE", "JMP32 register JLE" },
			{ BPF_JNE, 1, "JMP32 immediate JNE", "JMP32 register JNE" },
			{ BPF_JSGT, 0, "JMP32 immediate JSGT", "JMP32 register JSGT" },
			{ BPF_JSLT, 1, "JMP32 immediate JSLT", "JMP32 register JSLT" },
			{ BPF_JSGE, 0, "JMP32 immediate JSGE", "JMP32 register JSGE" },
			{ BPF_JSLE, 1, "JMP32 immediate JSLE", "JMP32 register JSLE" },
			{ BPF_JSET, 1, "JMP32 immediate JSET", "JMP32 register JSET" },
		};
		struct bpf_insn matrix[] = {
			BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
			BPF_LD_ABS(BPF_W, 0),
			BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
			BPF_LD_IMM64(BPF_REG_2, 0x100000000ULL),
			BPF_ALU64_REG(BPF_OR, BPF_REG_1, BPF_REG_2),
			BPF_LD_IMM64(BPF_REG_3, 0x200000001ULL),
			BPF_JMP32_IMM(BPF_JEQ, BPF_REG_1, 1, 2),
			BPF_MOV64_IMM(BPF_REG_0, 0), BPF_EXIT_INSN(),
			BPF_MOV64_IMM(BPF_REG_0, 1), BPF_EXIT_INSN(),
		};
		unsigned int i;

		for (i = 0; i < COUNT(cases); i++) {
			matrix[8] = BPF_JMP32_IMM(cases[i].op, BPF_REG_1, 1, 2);
			check(cases[i].immediate,
			      run(matrix, COUNT(matrix), cases[i].expected, packet));
			matrix[8] = BPF_JMP32_REG(cases[i].op, BPF_REG_1, BPF_REG_3, 2);
			check(cases[i].reg,
			      run(matrix, COUNT(matrix), cases[i].expected, packet));
		}
	}
	check("BPF-to-BPF extra JIT pass", run(subprog, COUNT(subprog), 7, packet));
	check("ktime_get_boot_ns helper", run(boot_time, COUNT(boot_time), 1, packet));

	a.map_type = BPF_MAP_TYPE_ARRAY;
	a.key_size = sizeof(key);
	a.value_size = sizeof(value);
	a.max_entries = 1;
	fd = bpf(BPF_MAP_CREATE, &a);
	memset(&a, 0, sizeof(a));
	a.map_fd = fd;
	a.key = PTR(&key);
	a.value = PTR(&value);
	ret = bpf(BPF_MAP_UPDATE_ELEM, &a);
	a.value = PTR(&result);
	check("BPF array update and lookup", fd >= 0 && ret == 0 &&
	      bpf(BPF_MAP_LOOKUP_ELEM, &a) == 0 && result == value);
	close_fd(fd);

	memset(&a, 0, sizeof(a));
	a.map_type = BPF_MAP_TYPE_RINGBUF;
	a.max_entries = 4096;
	fd = bpf(BPF_MAP_CREATE, &a);
	check("BPF ring buffer creation", fd >= 0);
	if (fd >= 0) {
		struct bpf_insn ring[] = {
			BPF_LD_MAP_FD(BPF_REG_1, fd),
			BPF_MOV64_IMM(BPF_REG_2, 8),
			BPF_MOV64_IMM(BPF_REG_3, 0),
			BPF_EMIT_CALL(BPF_FUNC_ringbuf_reserve),
			BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 4),
			BPF_MOV64_REG(BPF_REG_1, BPF_REG_0),
			BPF_MOV64_IMM(BPF_REG_2, 0),
			BPF_EMIT_CALL(BPF_FUNC_ringbuf_discard),
			BPF_MOV64_IMM(BPF_REG_0, 1), BPF_EXIT_INSN(),
		};
		check("BPF ring buffer reserve/discard", run(ring, COUNT(ring), 1, packet));
	}
	close_fd(fd);

}

static void test_btf(void)
{
	struct {
		struct btf_header header;
		struct btf_type integer;
		__u32 encoding;
		char strings[5];
	} data = {
		.header = { .magic = BTF_MAGIC, .version = BTF_VERSION,
			.hdr_len = sizeof(struct btf_header), .type_len = 16,
			.str_off = 16, .str_len = 5 },
		.integer = { .name_off = 1, .info = BTF_KIND_INT << 24, .size = 4 },
		.encoding = (BTF_INT_SIGNED << 24) | 32,
		.strings = { 0, 'i', 'n', 't', 0 },
	};
	union bpf_attr a = {};
	long fd;

	a.btf = PTR(&data);
	a.btf_size = sizeof(struct btf_header) + 16 + 5;
	a.btf_log_buf = PTR(verifier_log);
	a.btf_log_size = sizeof(verifier_log);
	a.btf_log_level = 1;
	fd = bpf(BPF_BTF_LOAD, &a);
	if (fd < 0)
		say(verifier_log);
	check("BTF integer validation and load", fd >= 0);
	close_fd(fd);
}

static void test_cgroup(void)
{
	static const char license[] = "GPL";
	struct bpf_insn insns[] = { BPF_MOV64_IMM(BPF_REG_0, 1), BPF_EXIT_INSN() };
	union bpf_attr a = {};
	long cgroup, program, ret;
	unsigned int id = 0;
	int ok;

	call(34, -100, (long)"/cgroup", 0755, 0, 0, 0);
	ret = call(40, (long)"none", (long)"/cgroup", (long)"cgroup2", 0, 0, 0);
	cgroup = call(56, -100, (long)"/cgroup", 0, 0, 0, 0);
	a.prog_type = BPF_PROG_TYPE_CGROUP_SKB;
	a.insn_cnt = COUNT(insns);
	a.insns = PTR(insns);
	a.license = PTR(license);
	program = bpf(BPF_PROG_LOAD, &a);
	ok = ret == 0 && cgroup >= 0 && program >= 0;
	if (ok) {
		memset(&a, 0, sizeof(a));
		a.target_fd = cgroup;
		a.attach_bpf_fd = program;
		a.attach_type = BPF_CGROUP_INET_INGRESS;
		ok = bpf(BPF_PROG_ATTACH, &a) == 0;
		if (ok) {
			memset(&a, 0, sizeof(a));
			a.query.target_fd = cgroup;
			a.query.attach_type = BPF_CGROUP_INET_INGRESS;
			a.query.prog_cnt = 1;
			a.query.prog_ids = PTR(&id);
			ok = bpf(BPF_PROG_QUERY, &a) == 0 && a.query.prog_cnt == 1 && id != 0;
			memset(&a, 0, sizeof(a));
			a.target_fd = cgroup;
			a.attach_bpf_fd = program;
			a.attach_type = BPF_CGROUP_INET_INGRESS;
			ok &= bpf(BPF_PROG_DETACH, &a) == 0;
		}
	}
	check("cgroup BPF attach/query/detach", ok);
	close_fd(program);
	close_fd(cgroup);
}

static int contains(const char *s, const char *needle)
{
	unsigned long i, j;
	for (i = 0; s[i]; i++) {
		for (j = 0; needle[j] && s[i + j] == needle[j]; j++)
			;
		if (!needle[j])
			return 1;
	}
	return 0;
}

static void test_kmsg(void)
{
	const char *messages[] = {
		"<3>third-update-vm healthd\n", "<3>third-update-vm logd\n",
		"<3>third-update-vm DM_DEV_STATUS\n",
		"<3>third-update-vm Untracked pid\n",
		"<3>third-update-vm usb_gadget\n",
	};
	char record[8192];
	long reader = call(56, -100, (long)"/dev/kmsg", 2048, 0, 0, 0);
	long writer = call(56, -100, (long)"/dev/kmsg", 1, 0, 0, 0);
	unsigned int i;
	int ok = reader >= 0 && writer >= 0;

	if (ok)
		ok = call(62, reader, 0, 2, 0, 0, 0) >= 0;
	for (i = 0; i < COUNT(messages) && ok; i++) {
		int found = 0, tries;
		long n = call(64, writer, (long)messages[i], length(messages[i]), 0, 0, 0);
		ok &= n == (long)length(messages[i]);
		for (tries = 0; tries < 1000 && !found; tries++) {
			n = call(63, reader, (long)record, sizeof(record) - 1, 0, 0, 0);
			if (n > 0) {
				record[n] = 0;
				found = contains(record, messages[i] + 3);
			} else {
				call(124, 0, 0, 0, 0, 0, 0);
			}
		}
		ok &= found;
	}
	check("all five formerly filtered kmsg messages", ok);
	close_fd(reader);
	close_fd(writer);
}

void _start(void)
{
	if (call(172, 0, 0, 0, 0, 0, 0) != 1) {
		say("This test must run as /init in an isolated ARM64 VM.\n");
		call(93, 1, 0, 0, 0, 0, 0);
	}
	call(34, -100, (long)"/dev", 0755, 0, 0, 0);
	call(34, -100, (long)"/proc", 0755, 0, 0, 0);
	call(40, (long)"devtmpfs", (long)"/dev", (long)"devtmpfs", 0, 0, 0);
	call(40, (long)"proc", (long)"/proc", (long)"proc", 0, 0, 0);
	say("third-update ARM64 runtime regressions\n");
	test_bpf();
	test_btf();
	test_cgroup();
	test_kmsg();
	say(failures ? "THIRD_UPDATE_RUNTIME_FAIL\n" : "THIRD_UPDATE_RUNTIME_PASS\n");
	call(142, 0xfee1dead, 672274793, 0x4321fedc, 0, 0, 0);
	for (;;)
		call(93, failures != 0, 0, 0, 0, 0, 0);
}
