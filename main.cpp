#include <geGL/geGL.h>
#include <geGL/StaticCalls.h>
#include <SDL2CPP/MainLoop.h>
#include <SDL2CPP/Window.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <sstream>

using namespace ge::gl;
using namespace std;

shared_ptr<Program> getProgram(size_t workGroupSize,size_t floatsPerThread = 1,size_t registersPerThread = 0){
  static size_t wgs = 0;
  static size_t fpt = 0;
  static size_t rpt = 0;
  static std::shared_ptr<Program>program = nullptr;
  if(wgs == workGroupSize && fpt == floatsPerThread && rpt == registersPerThread)
    return program;
  std::cerr << "need to recompile program" << std::endl;
  wgs = workGroupSize;
  fpt = floatsPerThread;
  rpt = registersPerThread;

  stringstream ss;
  ss << "#version 450" << endl;
  ss << "#line " << __LINE__ << endl;
  ss << "layout(local_size_x=" << workGroupSize << ")in;" << endl;
  ss << "#define FLOATS_PER_THREAD " << floatsPerThread << endl;
  ss << "#define REGISTERS_PER_THREAD " << registersPerThread << endl;
  ss << "#define REGISTER_CHUNKS (FLOATS_PER_THREAD / REGISTERS_PER_THREAD)" << endl;
  ss << "#define WORKGROUP_SIZE " << workGroupSize << endl;
  ss << R".(

  layout(binding=0,std430)buffer Data{float data[];};

  void main(){
    uint lid  = gl_LocalInvocationID .x;
    uint gid  = gl_GlobalInvocationID.x;
    uint wgs  = gl_WorkGroupSize     .x;
    uint wid  = gl_WorkGroupID       .x;
    uint nwgs = gl_NumWorkGroups     .x;
    const uint workGroupOffset = wid*FLOATS_PER_THREAD*WORKGROUP_SIZE;
    float accumulator = 0.f;

    #if REGISTERS_PER_THREAD != 0

    float registers[REGISTERS_PER_THREAD];
    for(uint r=0;r<REGISTERS_PER_THREAD;++r)
      registers[r] = 0.f;

    for(uint f=0;f<REGISTER_CHUNKS;++f)
      for(uint r=0;r<REGISTERS_PER_THREAD;++r)
        registers[r] += data[lid + (f*REGISTERS_PER_THREAD+r)*wgs + workGroupOffset];
    for(uint r=0;r<REGISTERS_PER_THREAD;++r)
      accumulator += registers[r];

    #else

    for(uint f=0;f<FLOATS_PER_THREAD;++f)
      accumulator += data[lid + f*wgs + workGroupOffset];

    #endif

    if(accumulator == 1.337f)
      data[gid] = 0.f;
  }
  ).";
  program = make_shared<Program>(make_shared<Shader>(GL_COMPUTE_SHADER,ss.str()));
  return program;
}

shared_ptr<Buffer> getBuffer(size_t workGroupSize,size_t nofWorkGroups,size_t floatsPerThread){
  static size_t wgs = 0;
  static size_t nw  = 0;
  static size_t fpt = 0;
  static shared_ptr<Buffer>buffer = nullptr;
  if(wgs == workGroupSize && nw == nofWorkGroups && fpt == floatsPerThread)
    return buffer;
  std::cerr << "need to reallocate buffer" << std::endl;
  wgs = workGroupSize;
  nw = nofWorkGroups;
  fpt = floatsPerThread;

  size_t const bufferSize = workGroupSize * nofWorkGroups * floatsPerThread;
  buffer = make_shared<Buffer>(bufferSize * sizeof(float));
  return buffer;
}

int main(int argc,char*argv[]){
  auto mainLoop = make_shared<sdl2cpp::MainLoop>();
  auto window   = make_shared<sdl2cpp::Window  >();
  window->createContext("rendering");
  ge::gl::init();
  mainLoop->addWindow("mainWindow",window);
  auto imgui = std::make_unique<imguiSDL2OpenGL::Imgui>(window->getWindow());
  mainLoop->setEventHandler([&](SDL_Event const&event){
    return imgui->processEvent(&event);
  });

  GLuint query;
  glCreateQueries(GL_TIME_ELAPSED,1,&query);

  uint64_t const nanoSecondsInSecond = 1e9;
  uint64_t const gigabyte = 1024*1024*1024;

  size_t nofMX              = 28  ;
  size_t coresPerMX         = 128 ;
  size_t warp               = 32  ;
  size_t workGroupSize      = 128 ;
  size_t nofWorkGroups      = 28  ;
  size_t floatsPerThread    = 1024;
  size_t registersPerThread = 0   ;

  size_t minWorkGroupSize      = 1        ;
  size_t maxWorkGroupSize      = 1024     ;
  size_t minFloatsPerThread    = 1        ;
  size_t maxFloatsPerThread    = 100000   ;
  size_t minNofMX              = 1        ;
  size_t maxNofMX              = 256      ;
  size_t minCorePerMX          = 32       ;
  size_t maxCorePerMX          = 1024     ;
  size_t minNofWorkGroups      = 1        ;
  size_t maxNofWorkGroups      = 1024*1024;
  size_t minRegistersPerThread = 0        ;
  size_t maxRegistersPerThread = 4096     ;


  double time = 0.;
  double bandwidthInGigabytes = 0.;
  bool show_demo_window = true;
  mainLoop->setIdleCallback([&]{
    glClearColor(1.f,0.1f,0.1f,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    imgui->newFrame(window->getWindow());
    auto program = getProgram(workGroupSize,floatsPerThread,registersPerThread);
    auto buffer  = getBuffer(workGroupSize,nofWorkGroups,floatsPerThread);
    buffer->clear(GL_R32F,GL_RED,GL_FLOAT);
    program->use();
    program->bindBuffer("data",buffer);
    glFinish();
    glBeginQuery(GL_TIME_ELAPSED, query);
    program->dispatch(nofWorkGroups);
    glFinish();
    uint64_t timeInNanoseconds;
    glEndQuery(GL_TIME_ELAPSED);
    glGetQueryObjectui64v(query,GL_QUERY_RESULT,&timeInNanoseconds);
    time = static_cast<double>(timeInNanoseconds) / static_cast<double>(nanoSecondsInSecond);
    auto const bufferSize = nofMX * workGroupSize * floatsPerThread * sizeof(float);
    auto const bandwidth = bufferSize / time;
    bandwidthInGigabytes = bandwidth / static_cast<double>(gigabyte);

#if 0
    auto d = (float*)buffer->map();
    for(size_t i=0;i<10;++i)
      std::cout << d[i] << std::endl;
    buffer->unmap();
#endif

    ImGui::Begin("vars");
    ImGui::DragScalar("floatsPerThread"     ,ImGuiDataType_U64,&floatsPerThread   ,1,&minFloatsPerThread   ,&maxFloatsPerThread   );
    ImGui::DragScalar("number of SMX"       ,ImGuiDataType_U64,&nofMX             ,1,&minNofMX             ,&maxNofMX             );
    ImGui::DragScalar("cores per SMX"       ,ImGuiDataType_U64,&coresPerMX        ,1,&minCorePerMX         ,&maxCorePerMX         );
    ImGui::DragScalar("workGroupSize"       ,ImGuiDataType_U64,&workGroupSize     ,1,&minWorkGroupSize     ,&maxWorkGroupSize     );
    ImGui::DragScalar("number of workgroups",ImGuiDataType_U64,&nofWorkGroups     ,1,&minNofWorkGroups     ,&maxNofWorkGroups     );
    ImGui::DragScalar("registers per thread",ImGuiDataType_U64,&registersPerThread,1,&minRegistersPerThread,&maxRegistersPerThread);
    ImGui::Text("Buffer Size: %f [GB]",static_cast<uint32_t>(workGroupSize * nofWorkGroups * floatsPerThread * sizeof(float))/static_cast<float>(gigabyte));
    ImGui::Text("Time: %lf [s]",time);
    ImGui::Text("bandwidth: %lf [GB/s]",bandwidthInGigabytes);
    ImGui::End();
    imgui->render(window->getWindow(), window->getContext("rendering"));
    window->swap();
  });
  (*mainLoop)();
  imgui = nullptr;
  return 0;
}
