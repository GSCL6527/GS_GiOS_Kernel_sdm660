// SPDX-License-Identifier: GPL-2.0
/* Host regression harness for the actual Pixie implementation. */
#include "../../../../../net/ipv4/tcp_pixie.c"
static unsigned int checks;
#define CHECK(x) do { assert(x); checks++; } while (0)
static struct sock socket_init(void){struct sock s={.sk_max_pacing_rate=UINT64_MAX,.tcp={.srtt_us=800000,.mss_cache=1460,.snd_cwnd=10,.snd_cwnd_clamp=100000}};return s;}
static void check_ring(struct pixie *p){u64 a=0,l=0;CHECK(p->count<=PIXIE_SAMPLES);CHECK(p->start<PIXIE_SAMPLES);for(int i=0;i<p->count;i++){struct sample *s=&p->samples[(p->start+i)&PIXIE_MASK];a+=s->acked;l+=s->losses;}CHECK(a==p->curr_acked);CHECK(l==p->curr_losses);}
int main(void){
 struct kernel_param r={&rate},f={&feedback};
 CHECK(pixie_set_positive("0",&r)==-EINVAL);CHECK(pixie_set_positive("-1",&r)==-EINVAL);CHECK(pixie_set_positive("65",&f)==-EINVAL);CHECK(pixie_set_positive("0",&f)==-EINVAL);CHECK(pixie_set_positive("2147483647",&r)==0);
 struct sock s=socket_init();pixie_init(&s);struct pixie *p=inet_csk_ca(&s);CHECK(requested==3072);CHECK(p->samples!=NULL);
 struct rate_sample rs={.delivered=1,.interval_us=1000,.losses=3,.acked_sacked=0};
 pixie_main(&s,&rs);CHECK(s.sk_pacing_rate==(u64)INT_MAX);CHECK(s.tcp.snd_cwnd>=2);check_ring(p);
 s.tcp.tcp_mstamp=UINT32_MAX-100000;
 for(unsigned i=0;i<100000;i++){
  s.tcp.tcp_mstamp+=1000;rs.acked_sacked=(i%31)?20:0;rs.losses=i%7;
  if(i%199==0){rs.acked_sacked=UINT32_MAX;rs.losses=INT_MAX;}
  pixie_main(&s,&rs);CHECK(s.tcp.snd_cwnd>=2);CHECK(s.tcp.snd_cwnd<=s.tcp.snd_cwnd_clamp);CHECK(s.sk_pacing_rate>=p->rate);if(i%97==0)check_ring(p);
 }
 check_ring(p);
 s.tcp.tcp_mstamp+=2000000;rs.acked_sacked=1;rs.losses=0;pixie_main(&s,&rs);CHECK(p->count==1);CHECK(p->curr_acked==1);CHECK(p->curr_losses==0);
 s.sk_max_pacing_rate=12345;pixie_main(&s,&rs);CHECK(s.sk_pacing_rate==12345);
 rs.losses=-1;u64 old=p->curr_acked;pixie_main(&s,&rs);CHECK(p->curr_acked==old);
 pixie_release(&s);CHECK(p->samples==NULL);
 fail_alloc=1;s=socket_init();pixie_init(&s);CHECK(((struct pixie*)inet_csk_ca(&s))->samples==NULL);rs.losses=0;rs.acked_sacked=0;pixie_main(&s,&rs);CHECK(s.tcp.snd_cwnd>=2);pixie_release(&s);
 fail_alloc=0;rate=150000000;
 for(unsigned i=0;i<1000;i++){s=socket_init();pixie_init(&s);rs.acked_sacked=i;pixie_main(&s,&rs);pixie_release(&s);}
 printf("PASS: %u assertions; 100000 samples, wrap/eviction, zero ACK, overflow, pacing cap, invalid parameters, allocation failure, 1000 socket lifetimes\n",checks);
}
