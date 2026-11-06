#include <cstdio>
#include <cmath>
#include <vector>
#include "BlackfaceEngine.h"
int main(){ // sweeps oversampling x ratio x ALL x attack x drive; fails on NaN or GR ripple > 3 dB
  int bad=0, n=0; double worstRipple=0;
  for(int q=0;q<4;++q) for(int r=0;r<4;++r) for(int all=0;all<2;++all) for(double atk: {20.0,60.0,800.0}) for(double in: {10.0,40.0}){
    bf::Settings s; s.inputDb=in; s.outputDb=0; s.attackUs=atk; s.releaseMs=50; s.ratio=r; s.all=all; s.quality=q; s.color=0.5;
    bf::Engine e; e.prepare(48000,256); e.setSettings(s);
    const int total=48000*2; std::vector<float> a(256),b(256),oa(256),ob(256); double mn=1e9,mx=-1e9; bool fin=true;
    for(int st=0;st<total;st+=256){
      for(int i=0;i<256;++i){ a[i]=b[i]=0.7f*std::sin(2*bf::kPiD*180.0*(st+i)/48000.0);}
      float*ip[2]={a.data(),b.data()}; float*op[2]={oa.data(),ob.data()};
      e.process<float>(ip,op,2,256,nullptr,0);
      for(float v: oa) if(!std::isfinite(v)) fin=false;
      if(st>total/2){ mn=std::min(mn,e.meters().grDb); mx=std::max(mx,e.meters().grDb);} }
    ++n; double rip=mx-mn; worstRipple=std::max(worstRipple,rip);
    if(!fin||rip>3.0){ ++bad; std::printf("UNSTABLE q=%d r=%d all=%d atk=%.0f in=%.0f ripple=%.2f dB\n",q,r,all,atk,in,rip);} }
  std::printf("%d combos, %d suspicious, worst GR-meter ripple %.2f dB\n",n,bad,worstRipple); return bad?1:0;
}
