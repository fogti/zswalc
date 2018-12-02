#include "app.hpp"
#include <stdlib.h>
#include <time.h>

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
using namespace std;

static atomic<bool> b_do_shutdown;

static void worker() {
  FCGX_Request request;
  FCGX_InitRequest(&request, 0, 0);

  while(!b_do_shutdown && !FCGX_Accept_r(&request)) {
    try {
      FCGX_SetExitStatus(0, request.out);
      cgicc::FCgiIO IO(request);
      try {
        handle_request(IO);
      } catch(const exception &e) {
        handle_error(IO, e.what());
      } catch(const char *e) {
        handle_error(IO, e);
      } catch(...) {
        handle_error(IO, "unknown exception occured");
      }
    } catch(...) {
      FCGX_SetExitStatus(1, request.out);
    }
    FCGX_Finish_r(&request);
  }

  FCGX_Free(&request, false);
}

int main(void) {
  FCGX_Init();

  // 1. prevent periodic stat(/etc/localtime)
  {
    ifstream tzf("/etc/timezone");
    string tmp;
    if(tzf) tzf >> tmp;
    if(!tmp.empty()) {
      tmp = ':' + tmp;
      setenv("TZ", tmp.c_str(), 0);
    }
  }
  tzset();

  // 2. spawn workers

  b_do_shutdown = false;
  vector<thread> workers;
  {
    const size_t imax = thread::hardware_concurrency();
    workers.reserve(imax);
    for(size_t i = 0; i < imax; ++i)
      workers.emplace_back(worker);
  }

  worker();

  // 3. cleanup
  b_do_shutdown = true;
  for(auto &i : workers)
    i.join();

  // NOTE: currently, the shutdown of zswalc is not always graceful,
  //       often one needs to call the app one more time to
  //       discontinue the 'accept' call

  return 0;
}
