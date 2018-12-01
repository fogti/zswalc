#include "app.hpp"

using namespace std;

int main(void) {
  FCGX_Request request;

  FCGX_Init();
  FCGX_InitRequest(&request, 0, 0);

  while(!FCGX_Accept_r(&request)) {
    try {
      cgicc::FCgiIO IO(request);
      try {
        cgicc::Cgicc CGI(&IO);
        handle_request(IO, CGI);
      } catch(const exception &e) {
        handle_error(IO, e.what());
      } catch(const char *e) {
        handle_error(IO, e);
      } catch(...) {
        handle_error(IO, "unknown exception occured");
      }
    } catch(...) {
      // do nothing
    }
    FCGX_Finish_r(&request);
  }

  return 0;
}
