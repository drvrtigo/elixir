/* SPDX-License-Identifier: GPL-3.0-only
 * Original procedural generators and transformations; not Autechre transcriptions.
 * Runs only in the UI generator, never in the audio ISR.
 */
#ifndef HORTATOR_BEAT_ABSTRACT_H
#define HORTATOR_BEAT_ABSTRACT_H
typedef struct {uint8_t chaos,anchor,variation,prob;} bg_detail_t;
static bg_detail_t bd={0,100,0,0};
#ifndef BG_STORAGE
#define BG_STORAGE
#endif
static bg_pattern_t bt_work BG_STORAGE; /* UI-only scratch: avoids a 2 KiB stack copy. */
static uint32_t bt_random(const bg_settings_t *s,unsigned lane,unsigned step,unsigned kind)
{
    uint32_t v=s->seed ^ ((lane+1u)*0x9E3779B9u) ^ ((step+1u)*0x85EBCA6Bu) ^
               ((kind+1u)*0xC2B2AE35u) ^ ((s->style+1u)*0x27D4EB2Du);
    v^=v>>16;v*=0x7FEB352Du;v^=v>>15;v*=0x846CA68Bu;v^=v>>16;
    return v;
}
static unsigned bt_min(unsigned a,unsigned b){return a<b?a:b;}
static int bt_protected(const bg_settings_t *s,unsigned l,unsigned i)
{
    return bt_work.lane[l][i].on && bt_work.lane[l][i].acc &&
           bt_random(s,l,i,0)%100u<bd.anchor;
}
static void bt_shape(const bg_settings_t *s,bg_pattern_t *out)
{
    unsigned l,i;
    if(!s->energy){memset(out,0,sizeof *out);return;}
    bt_work=*out;
    for(l=0;l<BG_LANES;l++)for(i=0;i<s->length;i++){
        bg_hit_t *h=&out->lane[l][i];
        const bg_hit_t *base=&bt_work.lane[l][i];
        unsigned v=i>=16u?bd.variation:0u;
        unsigned disrupt=bt_min(100u,(unsigned)bd.chaos+v/2u);
        uint32_t a=bt_random(s,l,i,1),b=bt_random(s,l,i,2),c=bt_random(s,l,i,3);
        if(bt_protected(s,l,i))continue;
        if(base->on){
            if(a%100u<disrupt/4u){memset(h,0,sizeof *h);continue;}
            if(b%100u<disrupt*s->complexity/200u){
                int to=(int)i+((c&1u)?1:-1)*(s->complexity>75u?2:1);
                if(to>=0 && (unsigned)to<s->length && !out->lane[l][to].on &&
                   !bt_protected(s,l,(unsigned)to)){
                    out->lane[l][to]=*base;memset(h,0,sizeof *h);continue;
                }
            }
        }else if(!h->on && a%100u<disrupt*s->energy/500u){
            h->on=1;h->acc=0;h->rat=0;h->cond=0;
        }
    }
    /* Second pass: bounded ratchets / optional playback probability. */
    for(l=0;l<BG_LANES;l++)for(i=0;i<s->length;i++){
        bg_hit_t *h=&out->lane[l][i];
        if(bt_protected(s,l,i)){*h=bt_work.lane[l][i];continue;}
        if(!h->on)continue;
        if(l>=BG_CH && bt_random(s,l,i,4)%100u<(unsigned)bd.chaos*s->complexity/500u)
            h->rat=(uint8_t)(1u+bt_random(s,l,i,5)%3u);
        if(bd.prob && bt_random(s,l,i,6)%100u<bd.prob){
            unsigned pct=100u-(unsigned)bd.prob*60u/100u;
            unsigned cond=1u+pct/5u;
            h->cond=(uint8_t)(cond>20u?20u:cond);
        }
    }
    for(i=0;i<s->length;i++)if(out->lane[BG_CH][i].on && out->lane[BG_OH][i].on){
        unsigned drop=bt_protected(s,BG_CH,i) && !bt_protected(s,BG_OH,i)?BG_OH:BG_CH;
        memset(&out->lane[drop][i],0,sizeof(bg_hit_t));
    }
}
static void bt_interlock(const bg_settings_t *s,bg_pattern_t *out)
{
    static const uint8_t roles[8]={BG_KICK,BG_CH,BG_RIM,BG_SNARE,BG_TOM,BG_CH,BG_CLAP,BG_COWBELL};
    unsigned i,l,pulses=3u+s->energy/10u;
    unsigned rot=bt_random(s,0,0,10)%16u;
    memset(out,0,sizeof *out);
    if(!s->energy)return;
    for(i=0;i<s->length;i++){
        unsigned p=i%16u,b=i/16u;
        unsigned key=b && bt_random(s,0,b,11)%100u<bd.variation?i:p;
        unsigned pos=(p+rot)%16u;
        if((pos*pulses)%16u>=pulses)continue;
        l=roles[bt_random(s,0,key,12)%8u];
        out->lane[l][i].on=1;
        if(s->complexity>50 && bt_random(s,0,key,13)%100u<s->complexity-50u)
            out->lane[l][i].rat=(uint8_t)(1u+bt_random(s,l,key,14)%3u);
    }
    /* Shared anchors replace, rather than stack with, an allocated event. */
    for(i=0;i<s->length;i++)if(i%16u==0u || i%16u==12u){
        for(l=0;l<8;l++)memset(&out->lane[l][i],0,sizeof(bg_hit_t));
        l=i%16u==0u?BG_KICK:BG_SNARE;
        out->lane[l][i].on=1;out->lane[l][i].acc=1;
    }
}
static void bt_fracture(const bg_settings_t *s,bg_pattern_t *out)
{
    bg_hit_t cell[8][16];unsigned l,i;
    memset(cell,0,sizeof cell);memset(out,0,sizeof *out);
    if(!s->energy)return;
    for(l=0;l<8;l++)for(i=0;i<16;i++){
        unsigned hit=(l==BG_KICK?(i==0 || i==6 || i==8):
                      l==BG_SNARE?(i==4 || i==12):
                      l==BG_CH?!(i&1u):l==BG_RIM?(i==3 || i==11):0u);
        unsigned anchor=(l==BG_KICK && i==0)||(l==BG_SNARE && (i==4 || i==12));
        if(hit && (anchor || bt_random(s,l,i,20)%100u<s->energy)){
            cell[l][i].on=1;cell[l][i].acc=(uint8_t)anchor;
        }
    }
    for(i=0;i<s->length;i++){
        unsigned frag=(i%16u)/4u,offset=i%4u,b=i/16u,key=frag;
        if(b && bt_random(s,0,b,21)%100u<bd.variation)key+=b*4u;
        if(bt_random(s,0,key,22)%100u<s->complexity)
            frag=bt_random(s,0,key,23)%4u;
        if(s->complexity>70u && bt_random(s,0,key,24)%100u<s->complexity-70u)
            offset=3u-offset;
        for(l=0;l<8;l++)out->lane[l][i]=cell[l][frag*4u+offset];
    }
}
static int bt_procedural(const bg_settings_t *s,bg_pattern_t *out)
{
    if(s->style==BG_INTERLOCK)bt_interlock(s,out);else bt_fracture(s,out);
    return 1;
}
#endif
