#include <stdlib.h>
#include "util.hpp"

using namespace std;

// Maximum bytes
static const size_t STDIN_MAX = 1000000;

/**
 * Note this is not thread safe due to the static allocation of the
 * content.
 */
void get_request_content(string &content, const FCGX_Request &request) {
  const char *clhst = "CONTENT_LENGTH";
  const char *content_length_str = FCGX_GetParam(clhst, request.envp);
  size_t content_length = 0;

  // Do not read from stdin if CONTENT_LENGTH is missing
  if(content_length_str) {
    char *eocl = 0;
    content_length = strtol(content_length_str, &eocl, 10);
    if(*eocl) {
      cerr << "parsing error in " << clhst << "='"
           << content_length_str
           << "'. consuming stdin up to " << STDIN_MAX << endl;
    }

    if(content_length > STDIN_MAX) content_length = STDIN_MAX;
  }

  content.resize(content_length);
  cin.read(&content.front(), content.size());

  // ignore() doesn't set the eof bit in some versions of glibc++
  // so use gcount() instead of eof()...
  do cin.ignore(1024); while(cin.gcount() == 1024);
}
