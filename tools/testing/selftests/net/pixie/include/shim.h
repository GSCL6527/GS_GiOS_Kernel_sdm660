#ifndef PIXIE_TEST_SHIM
#define PIXIE_TEST_SHIM
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
typedef uint64_t u64; typedef uint32_t u32; typedef uint16_t u16;
#define U32_MAX UINT32_MAX
#define PAGE_SIZE 4096
#define USEC_PER_SEC 1000000
#define GFP_ATOMIC 1
#define __GFP_NOWARN 2
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x,v) ((x)=(v))
#define min_t(t,a,b) ({t _a=(a),_b=(b);_a<_b?_a:_b;})
#define max_t(t,a,b) ({t _a=(a),_b=(b);_a>_b?_a:_b;})
#define max(a,b) max_t(int,a,b)
#define clamp_t(t,v,lo,hi) min_t(t,max_t(t,v,lo),hi)
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))
#define div_u64(a,b) ((u64)(a)/(b))
#define div64_u64(a,b) ((u64)(a)/(b))
#define fls64(v) ((v)?64-__builtin_clzll(v):0)
#define __init
#define __exit
#define __read_mostly
#define THIS_MODULE NULL
#define ICSK_CA_PRIV_SIZE 104
#define TCP_INFINITE_SSTHRESH UINT32_MAX
#define TCP_CONG_NON_RESTRICTED 1
#define SK_PACING_NONE 0
#define SK_PACING_NEEDED 1
#define cmpxchg(p,a,b) __sync_val_compare_and_swap(p,a,b)
#define BUILD_BUG_ON(v) _Static_assert(!(v), #v)
#define module_param_cb(...)
#define module_init(...)
#define module_exit(...)
#define MODULE_LICENSE(...)
struct kernel_param { void *arg; };
struct kernel_param_ops {int (*set)(const char *,const struct kernel_param *); int (*get)(char *,const struct kernel_param *);};
static int kstrtoint(const char *s,unsigned int base,int *out){char *e;errno=0;long v=strtol(s,&e,base);if(errno||*e||v>INT_MAX||v<INT_MIN)return -EINVAL;*out=v;return 0;}
static int param_get_int(char *s,const struct kernel_param *p){return sprintf(s,"%d",*(int*)p->arg);}
static int fail_alloc; static size_t requested;
static void *kmalloc_array(size_t n,size_t s,int flags){requested=n*s;return fail_alloc?NULL:malloc(n*s);}
#define kfree free
struct tcp_sock {u64 tcp_mstamp;u32 srtt_us,mss_cache,snd_cwnd,snd_cwnd_clamp;};
struct sock {u64 sk_pacing_rate,sk_max_pacing_rate;int sk_pacing_status;struct tcp_sock tcp;_Alignas(8) unsigned char ca[ICSK_CA_PRIV_SIZE];};
#define tcp_sk(sk) (&(sk)->tcp)
#define inet_csk_ca(sk) ((void *)(sk)->ca)
struct rate_sample {int delivered;long interval_us;int losses;u32 acked_sacked;};
struct tcp_congestion_ops {int flags;const char *name;void *owner;void(*init)(struct sock*);void(*release)(struct sock*);void(*cong_control)(struct sock*,const struct rate_sample*);u32(*ssthresh)(struct sock*);u32(*undo_cwnd)(struct sock*);};
static int tcp_register_congestion_control(void*p){return 0;}
static void tcp_unregister_congestion_control(void*p){}
#endif
