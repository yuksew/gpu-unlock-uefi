// Sustained GEMM throughput and board power for one GPU, chosen by PCI bus ID.
// Ordinary Windows CUDA/NVML workload: no unlock, reset, clock or power-limit change.
//   power-benchmark.exe --pci 0000:05:00.0 [--seconds 15] [--warmup 5] [--idle 5] [--n 4096]
// Prints one JSON document. Unlike compute-benchmark.cu (30 short samples), each
// precision runs back to back for --seconds after a warmup, so the card reaches the
// clock it holds at its power limit, and NVML samples power, clocks, temperature,
// clock-event reasons and PCIe state on the same card while it does.
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#include <cublas_v2.h>
#include <nvml.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
static void cuda_check(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA: %s\n",cudaGetErrorString(e));exit(1);}}
static void blas_check(cublasStatus_t e){if(e!=CUBLAS_STATUS_SUCCESS){fprintf(stderr,"cuBLAS error %d\n",(int)e);exit(1);}}
static void nvml_check(nvmlReturn_t e,const char *what){if(e!=NVML_SUCCESS){fprintf(stderr,"NVML %s: %s\n",what,nvmlErrorString(e));exit(1);}}
template<class T> __global__ void ones(T *p,size_t n){size_t i=blockIdx.x*(size_t)blockDim.x+threadIdx.x;if(i<n)p[i]=T(1.0f);}
using Clock=std::chrono::steady_clock;
static double since(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}

// One NVML reading. Power is board power as the driver reports it, not wall power.
struct Sample {double t,watts;unsigned sm_mhz,mem_mhz,temp_c,pcie_gen,pcie_width,pcie_tx_kbs,pcie_rx_kbs;unsigned long long reasons;};
static nvmlDevice_t Nvml;
static std::atomic<bool> Recording{false},Quit{false};
static std::mutex Lock;static std::vector<Sample> Samples;static Clock::time_point Origin;
static void sampler() {
  while(!Quit) {
    if(Recording) {
      Sample s={since(Origin),0,0,0,0,0,0,0,0,0};unsigned mw=0;
      if(nvmlDeviceGetPowerUsage(Nvml,&mw)==NVML_SUCCESS)s.watts=mw/1000.0;
      nvmlDeviceGetClockInfo(Nvml,NVML_CLOCK_SM,&s.sm_mhz);nvmlDeviceGetClockInfo(Nvml,NVML_CLOCK_MEM,&s.mem_mhz);
      nvmlDeviceGetTemperature(Nvml,NVML_TEMPERATURE_GPU,&s.temp_c);
      nvmlDeviceGetCurrPcieLinkGeneration(Nvml,&s.pcie_gen);nvmlDeviceGetCurrPcieLinkWidth(Nvml,&s.pcie_width);
      // Link traffic over a 20 ms window: shows how little a resident GEMM moves over x1.
      nvmlDeviceGetPcieThroughput(Nvml,NVML_PCIE_UTIL_TX_BYTES,&s.pcie_tx_kbs);nvmlDeviceGetPcieThroughput(Nvml,NVML_PCIE_UTIL_RX_BYTES,&s.pcie_rx_kbs);
      nvmlDeviceGetCurrentClocksEventReasons(Nvml,&s.reasons);
      std::lock_guard<std::mutex> g(Lock);Samples.push_back(s);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}
// Energy over a window: the driver's cumulative counter when it has one, else the
// trapezoid of the samples. The JSON says which.
struct Window {std::vector<Sample> s;double seconds,joules;bool counter;};
static unsigned long long energy_mj(bool *ok){unsigned long long e=0;*ok=nvmlDeviceGetTotalEnergyConsumption(Nvml,&e)==NVML_SUCCESS;return e;}
static void begin_window(){std::lock_guard<std::mutex> g(Lock);Samples.clear();Recording=true;}
static Window end_window(double seconds,unsigned long long e0,bool ok0) {
  Recording=false;bool ok1;unsigned long long e1=energy_mj(&ok1);Window w;
  {std::lock_guard<std::mutex> g(Lock);w.s=Samples;}
  w.seconds=seconds;w.counter=ok0 && ok1 && e1>e0;w.joules=w.counter?(e1-e0)/1000.0:0;
  if(!w.counter)for(size_t i=1;i<w.s.size();i++)w.joules+=(w.s[i].t-w.s[i-1].t)*(w.s[i].watts+w.s[i-1].watts)/2;
  return w;
}
static double median(std::vector<double> v){if(v.empty())return 0;std::sort(v.begin(),v.end());size_t m=v.size()/2;return v.size()%2?v[m]:(v[m-1]+v[m])/2;}
template<class F> static double window_stat(const Window &w,F f,bool max=false){
  std::vector<double> v;for(auto &s:w.s)v.push_back(f(s));
  if(max)return v.empty()?0:*std::max_element(v.begin(),v.end());return median(v);}
static void print_window(const Window &w) {
  unsigned long long reasons=0;for(auto &s:w.s)reasons|=s.reasons;
  printf("\"seconds\":%.3f,\"energy_j\":%.3f,\"energy_source\":\"%s\",\"avg_power_w\":%.3f,\"median_power_w\":%.3f,\"max_power_w\":%.3f,"
    "\"median_sm_mhz\":%.0f,\"median_mem_mhz\":%.0f,\"max_temp_c\":%.0f,\"pcie_gen\":%.0f,\"pcie_width\":%.0f,"
    "\"median_pcie_tx_kbs\":%.0f,\"median_pcie_rx_kbs\":%.0f,\"clock_event_reasons\":\"0x%llx\",\"samples\":%zu",
    w.seconds,w.joules,w.counter?"nvml_total_energy":"sampled_power",w.seconds>0?w.joules/w.seconds:0,
    window_stat(w,[](const Sample &s){return s.watts;}),window_stat(w,[](const Sample &s){return s.watts;},true),
    window_stat(w,[](const Sample &s){return (double)s.sm_mhz;}),window_stat(w,[](const Sample &s){return (double)s.mem_mhz;}),
    window_stat(w,[](const Sample &s){return (double)s.temp_c;},true),window_stat(w,[](const Sample &s){return (double)s.pcie_gen;}),
    window_stat(w,[](const Sample &s){return (double)s.pcie_width;}),window_stat(w,[](const Sample &s){return (double)s.pcie_tx_kbs;}),
    window_stat(w,[](const Sample &s){return (double)s.pcie_rx_kbs;}),reasons,w.s.size());
}
static void idle(double seconds,const char *name,bool comma) {
  cuda_check(cudaDeviceSynchronize());bool ok;unsigned long long e0=energy_mj(&ok);begin_window();
  auto t=Clock::now();std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
  Window w=end_window(since(t),e0,ok);printf("%s\"%s\":{",comma?",":"",name);print_window(w);printf("}");
}

// kind 0 FP32 (no tensor cores), 1 FP16 in / FP32 accumulate, 2 BF16 in / FP32
// accumulate, 3 FP16 in / FP16 accumulate. 1 and 2 are the paths the unlock targets;
// 3 runs at full rate on GeForce already and is the control that should not move.
static const char *Names[]={"fp32_pedantic","fp16_tensor_fp32_accumulate","bf16_tensor_fp32_accumulate","fp16_tensor_fp16_accumulate_control"};
static void measure(cublasHandle_t handle,int kind,int n,double warmup,double seconds,double rest) {
  size_t count=(size_t)n*n;size_t in=count*(kind==0?sizeof(float):sizeof(__half)),out=count*(kind==3?sizeof(__half):sizeof(float));
  void *a,*b,*c;cuda_check(cudaMalloc(&a,in));cuda_check(cudaMalloc(&b,in));cuda_check(cudaMalloc(&c,out));
  unsigned blocks=(unsigned)((count+255)/256);
  if(kind==0){ones<<<blocks,256>>>((float *)a,count);ones<<<blocks,256>>>((float *)b,count);}
  else if(kind==2){ones<<<blocks,256>>>((__nv_bfloat16 *)a,count);ones<<<blocks,256>>>((__nv_bfloat16 *)b,count);}
  else {ones<<<blocks,256>>>((__half *)a,count);ones<<<blocks,256>>>((__half *)b,count);}
  cuda_check(cudaGetLastError());cuda_check(cudaDeviceSynchronize());
  blas_check(cublasSetMathMode(handle,kind?CUBLAS_TENSOR_OP_MATH:CUBLAS_PEDANTIC_MATH));
  auto gemm=[&](){
    if(kind==0){float alpha=1,beta=0;blas_check(cublasSgemm(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&alpha,(float *)a,n,(float *)b,n,&beta,(float *)c,n));}
    else if(kind==3){__half alpha=__float2half(1.0f),beta=__float2half(0.0f);
      blas_check(cublasGemmEx(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&alpha,a,CUDA_R_16F,n,b,CUDA_R_16F,n,&beta,c,CUDA_R_16F,n,CUBLAS_COMPUTE_16F,CUBLAS_GEMM_DEFAULT_TENSOR_OP));}
    else {float alpha=1,beta=0;cudaDataType_t type=kind==1?CUDA_R_16F:CUDA_R_16BF;
      blas_check(cublasGemmEx(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&alpha,a,type,n,b,type,n,&beta,c,CUDA_R_32F,n,CUBLAS_COMPUTE_32F,CUBLAS_GEMM_DEFAULT_TENSOR_OP));}
  };
  auto t=Clock::now();do{for(int i=0;i<4;i++)gemm();cuda_check(cudaDeviceSynchronize());}while(since(t)<warmup);
  // Measured window: GEMMs back to back in batches of 4, timed by events end to end.
  cudaEvent_t start,stop;cuda_check(cudaEventCreate(&start));cuda_check(cudaEventCreate(&stop));
  bool ok;unsigned long long e0=energy_mj(&ok);begin_window();long long gemms=0;
  t=Clock::now();cuda_check(cudaEventRecord(start));
  do{for(int i=0;i<4;i++)gemm();gemms+=4;cuda_check(cudaDeviceSynchronize());}while(since(t)<seconds);
  cuda_check(cudaEventRecord(stop));cuda_check(cudaEventSynchronize(stop));
  float ms;cuda_check(cudaEventElapsedTime(&ms,start,stop));Window w=end_window(ms/1000.0,e0,ok);
  // Reference: every element of an all-ones product equals n. Exact for FP32
  // accumulation; an FP16 accumulator cannot hold every integer above 2048, so the
  // control only has to land within 1% (a broken kernel is off by far more).
  float first,last;
  if(kind==3){__half h[2];cuda_check(cudaMemcpy(&h[0],c,2,cudaMemcpyDeviceToHost));cuda_check(cudaMemcpy(&h[1],(__half *)c+count-1,2,cudaMemcpyDeviceToHost));first=__half2float(h[0]);last=__half2float(h[1]);}
  else {cuda_check(cudaMemcpy(&first,c,4,cudaMemcpyDeviceToHost));cuda_check(cudaMemcpy(&last,(float *)c+count-1,4,cudaMemcpyDeviceToHost));}
  double tolerance=kind==3?n/100.0:0;
  if(!(fabs(first-n)<=tolerance) || !(fabs(last-n)<=tolerance)){fprintf(stderr,"Matrix result mismatch in %s: %g %g\n",Names[kind],first,last);exit(1);}
  double flops=2.0*n*n*(double)n*gemms,tflops=flops/(ms/1000.0)/1e12;
  double sm=window_stat(w,[](const Sample &s){return (double)s.sm_mhz;});
  printf("%s{\"name\":\"%s\",\"n\":%d,\"warmup_seconds\":%.1f,\"gemms\":%lld,\"tflops\":%.3f,\"tflops_per_ghz\":%.4f,"
    "\"gflops_per_w\":%.3f,\"j_per_gemm\":%.5f,\"reference_checked\":true,\"reference_tolerance\":%g,",kind?",":"",Names[kind],n,warmup,gemms,tflops,
    sm>0?tflops/(sm/1000.0):0,w.joules>0?flops/1e9/w.joules:0,w.joules/gemms,tolerance);
  print_window(w);printf("}");
  cuda_check(cudaEventDestroy(start));cuda_check(cudaEventDestroy(stop));cuda_check(cudaFree(a));cuda_check(cudaFree(b));cuda_check(cudaFree(c));
  std::this_thread::sleep_for(std::chrono::duration<double>(rest));
}
// Host link: pinned 256 MiB copies each way. On an x1 slot this, not the GPU, bounds
// model loading and anything that streams data between host and card.
static void transfer() {
  const size_t bytes=256u<<20;void *host,*dev;cuda_check(cudaMallocHost(&host,bytes));cuda_check(cudaMalloc(&dev,bytes));
  memset(host,1,bytes);cudaEvent_t start,stop;cuda_check(cudaEventCreate(&start));cuda_check(cudaEventCreate(&stop));
  printf(",\"pcie_transfer\":{\"bytes\":%zu",bytes);
  for(int dir=0;dir<2;dir++) {
    std::vector<double> gbs;bool ok;unsigned long long e0=energy_mj(&ok);begin_window();auto t=Clock::now();
    for(int i=0;i<9;i++) {
      cuda_check(cudaEventRecord(start));
      cuda_check(dir?cudaMemcpy(host,dev,bytes,cudaMemcpyDeviceToHost):cudaMemcpy(dev,host,bytes,cudaMemcpyHostToDevice));
      cuda_check(cudaEventRecord(stop));cuda_check(cudaEventSynchronize(stop));float ms;cuda_check(cudaEventElapsedTime(&ms,start,stop));
      gbs.push_back(bytes/(ms/1000.0)/1e9);
    }
    Window w=end_window(since(t),e0,ok);
    printf(",\"%s\":{\"median_gb_s\":%.3f,",dir?"device_to_host":"host_to_device",median(gbs));print_window(w);printf("}");
  }
  printf("}");
  cuda_check(cudaEventDestroy(start));cuda_check(cudaEventDestroy(stop));cuda_check(cudaFreeHost(host));cuda_check(cudaFree(dev));
}
static void escape(const char *s){for(;*s;s++){if(*s=='"'||*s=='\\')putchar('\\');if((unsigned char)*s>=32)putchar(*s);}}
int main(int argc,char **argv) {
  const char *want=nullptr;double seconds=15,warmup=5,rest=5;int n=4096;
  for(int i=1;i+1<argc;i+=2) {
    if(!strcmp(argv[i],"--pci"))want=argv[i+1];else if(!strcmp(argv[i],"--seconds"))seconds=atof(argv[i+1]);
    else if(!strcmp(argv[i],"--warmup"))warmup=atof(argv[i+1]);else if(!strcmp(argv[i],"--idle"))rest=atof(argv[i+1]);
    else if(!strcmp(argv[i],"--n"))n=atoi(argv[i+1]);else {fprintf(stderr,"unknown option %s\n",argv[i]);return 2;}
  }
  if(!want || (argc-1)%2 || seconds<=0 || warmup<0 || rest<0 || n<256 || n>8192 || n%256) {
    fprintf(stderr,"usage: %s --pci 0000:BB:DD.F [--seconds 15] [--warmup 5] [--idle 5] [--n 4096]\n",argv[0]);return 2;}
  // Exactly one CUDA device must sit at the requested address. Never fall back to
  // another card: this machine may hold several NVIDIA GPUs.
  int devices=0;cuda_check(cudaGetDeviceCount(&devices));int selected=-1;char bus[32]={0};
  for(int i=0;i<devices;i++){char id[32];cuda_check(cudaDeviceGetPCIBusId(id,sizeof(id),i));
    if(!_stricmp(id,want)){if(selected!=-1){fprintf(stderr,"two CUDA devices at %s\n",want);return 2;}selected=i;strcpy(bus,id);}}
  if(selected<0){fprintf(stderr,"no CUDA device at %s\n",want);return 2;}
  cudaDeviceProp p;cuda_check(cudaGetDeviceProperties(&p,selected));cuda_check(cudaSetDevice(selected));
  nvml_check(nvmlInit_v2(),"init");nvml_check(nvmlDeviceGetHandleByPciBusId_v2(bus,&Nvml),"handle");
  char vbios[64]={0},driver[96]={0};nvmlDeviceGetVbiosVersion(Nvml,vbios,sizeof(vbios));nvmlSystemGetDriverVersion(driver,sizeof(driver));
  unsigned limit=0,deflimit=0,maxgen=0,maxwidth=0;nvmlDeviceGetEnforcedPowerLimit(Nvml,&limit);nvmlDeviceGetPowerManagementDefaultLimit(Nvml,&deflimit);
  nvmlDeviceGetMaxPcieLinkGeneration(Nvml,&maxgen);nvmlDeviceGetMaxPcieLinkWidth(Nvml,&maxwidth);
  cublasHandle_t handle;blas_check(cublasCreate(&handle));int runtime=0,drv=0,blas=0;
  cuda_check(cudaRuntimeGetVersion(&runtime));cuda_check(cudaDriverGetVersion(&drv));blas_check(cublasGetVersion(handle,&blas));
  printf("{\"schema\":1,\"gpu\":\"");escape(p.name);printf("\",\"pci\":\"%s\",\"compute_capability\":\"%d.%d\",\"vbios\":\"",bus,p.major,p.minor);
  escape(vbios);printf("\",\"driver\":\"");escape(driver);
  printf("\",\"cuda_runtime\":%d,\"cuda_driver_api\":%d,\"cublas\":%d,\"power_limit_w\":%.3f,\"default_power_limit_w\":%.3f,"
    "\"pcie_max_gen\":%u,\"pcie_max_width\":%u,\"seconds\":%.1f,",runtime,drv,blas,limit/1000.0,deflimit/1000.0,maxgen,maxwidth,seconds);
  Origin=Clock::now();std::thread worker(sampler);
  idle(rest,"idle",false);
  printf(",\"benchmarks\":[");for(int kind=0;kind<4;kind++)measure(handle,kind,n,warmup,seconds,rest);printf("]");
  transfer();
  Quit=true;worker.join();blas_check(cublasDestroy(handle));nvmlShutdown();puts("}");return 0;
}
