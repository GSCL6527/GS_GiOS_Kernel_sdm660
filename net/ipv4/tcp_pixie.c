// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <net/tcp.h>

/* Keep the feedback window in bounded time buckets, not one entry per ACK. */
#define PIXIE_SAMPLES 256
#define PIXIE_MASK (PIXIE_SAMPLES - 1)

static int rate = 900000000;
static int feedback = 2;

static int pixie_set_positive(const char *val, const struct kernel_param *kp)
{
	int value, ret = kstrtoint(val, 0, &value);

	if (ret)
		return ret;
	if (value <= 0 || (kp->arg == &feedback && value > 64))
		return -EINVAL;
	WRITE_ONCE(*(int *)kp->arg, value);
	return 0;
}

static const struct kernel_param_ops pixie_param_ops = {
	.set = pixie_set_positive,
	.get = param_get_int,
};
module_param_cb(rate, &pixie_param_ops, &rate, 0644);
module_param_cb(feedback, &pixie_param_ops, &feedback, 0644);

struct sample {
	u32 acked;
	u32 losses;
	u32 tstamp_us;
};

struct pixie {
	u64 rate;
	u64 curr_acked;
	u64 curr_losses;
	struct sample *samples;
	u16 start;
	u16 count;
};

static void pixie_remove_oldest(struct pixie *pixie)
{
	struct sample *sample = &pixie->samples[pixie->start];

	pixie->curr_acked -= sample->acked;
	pixie->curr_losses -= sample->losses;
	pixie->start = (pixie->start + 1) & PIXIE_MASK;
	pixie->count--;
}

static void pixie_record(struct pixie *pixie, const struct rate_sample *rs,
			u32 now, u32 window)
{
	struct sample *sample;
	u32 bucket = DIV_ROUND_UP(window, PIXIE_SAMPLES - 1);

	while (pixie->count &&
	       (u32)(now - pixie->samples[pixie->start].tstamp_us) > window)
		pixie_remove_oldest(pixie);

	if (pixie->count) {
		sample = &pixie->samples[(pixie->start + pixie->count - 1) &
				       PIXIE_MASK];
		if ((u32)(now - sample->tstamp_us) < bucket &&
		    rs->acked_sacked <= U32_MAX - sample->acked &&
		    rs->losses <= U32_MAX - sample->losses)
			goto accumulate;
	}

	if (pixie->count == PIXIE_SAMPLES)
		pixie_remove_oldest(pixie);
	sample = &pixie->samples[(pixie->start + pixie->count) & PIXIE_MASK];
	sample->acked = 0;
	sample->losses = 0;
	sample->tstamp_us = now;
	pixie->count++;

accumulate:
	sample->acked += rs->acked_sacked;
	sample->losses += rs->losses;
	pixie->curr_acked += rs->acked_sacked;
	pixie->curr_losses += rs->losses;
}

static void pixie_main(struct sock *sk, const struct rate_sample *rs)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct pixie *pixie = inet_csk_ca(sk);
	u64 pacing = pixie->rate, cwnd;
	u32 rtt = tp->srtt_us >> 3;

	if (rs->delivered < 0 || rs->interval_us <= 0 || rs->losses < 0)
		return;

	if (pixie->samples) {
		u32 window = clamp_t(u64,
			(u64)READ_ONCE(feedback) * tp->srtt_us / 2,
			1, U32_MAX / 2);

		pixie_record(pixie, rs, (u32)tp->tcp_mstamp, window);
		if (pixie->curr_acked) {
			u64 total = pixie->curr_acked + pixie->curr_losses;
			unsigned int shift = max(fls64(total) - 32, 0);
			u64 acked = max_t(u64, pixie->curr_acked >> shift, 1);

			/* Bound the product to 63 bits even for extreme samples. */
			pacing = div64_u64(pixie->rate * (total >> shift), acked);
		}
	}

	WRITE_ONCE(sk->sk_pacing_rate,
		   min_t(u64, pacing, READ_ONCE(sk->sk_max_pacing_rate)));
	if (!rtt || !tp->mss_cache)
		return;

	/* Widen before multiplying the BDP; never collapse the window to zero. */
	cwnd = min_t(u64, div_u64(pacing, tp->mss_cache), U32_MAX);
	cwnd = div_u64(cwnd * rtt, USEC_PER_SEC);
	if (!pixie->samples)
		cwnd *= 2;
	tp->snd_cwnd = min_t(u64, max_t(u64, cwnd, 2), tp->snd_cwnd_clamp);
}

static void pixie_init(struct sock *sk)
{
	struct pixie *pixie = inet_csk_ca(sk);

	pixie->rate = READ_ONCE(rate);
	pixie->start = 0;
	pixie->count = 0;
	pixie->curr_acked = 0;
	pixie->curr_losses = 0;
	/* Single-page-sized request; allocation failure has a safe fallback. */
	pixie->samples = kmalloc_array(PIXIE_SAMPLES, sizeof(struct sample),
				      GFP_ATOMIC | __GFP_NOWARN);
	cmpxchg(&sk->sk_pacing_status, SK_PACING_NONE, SK_PACING_NEEDED);
}

static void pixie_release(struct sock *sk)
{
	struct pixie *pixie = inet_csk_ca(sk);

	kfree(pixie->samples);
	pixie->samples = NULL;
}

static u32 pixie_ssthresh(struct sock *sk)
{
	return TCP_INFINITE_SSTHRESH;
}

static u32 pixie_undo_cwnd(struct sock *sk)
{
	return tcp_sk(sk)->snd_cwnd;
}

static struct tcp_congestion_ops tcp_pixie_cong_ops __read_mostly = {
	.flags = TCP_CONG_NON_RESTRICTED,
	.name = "pixie",
	.owner = THIS_MODULE,
	.init = pixie_init,
	.release = pixie_release,
	.cong_control = pixie_main,
	.ssthresh = pixie_ssthresh,
	.undo_cwnd = pixie_undo_cwnd,
};

static int __init pixie_register(void)
{
	BUILD_BUG_ON(sizeof(struct pixie) > ICSK_CA_PRIV_SIZE);
	BUILD_BUG_ON(PIXIE_SAMPLES & PIXIE_MASK);
	BUILD_BUG_ON(PIXIE_SAMPLES * sizeof(struct sample) > PAGE_SIZE);
	return tcp_register_congestion_control(&tcp_pixie_cong_ops);
}

static void __exit pixie_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_pixie_cong_ops);
}

module_init(pixie_register);
module_exit(pixie_unregister);
MODULE_LICENSE("GPL");
