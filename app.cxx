#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <vector>
#include <pcrecpp.h>

#include "app.hpp"

#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTTPResponseHeader.h>
#include <cgicc/HTMLClasses.h>

using namespace std;
using namespace cgicc;

/* app invocation
 * GET without QUERY | := show=...  ---> chat display
 * GET Q := g=...                   ---> backend: get chat data
 * POST in=...                      ---> backend: append message
 */

auto get_cur_time() -> struct tm {
  time_t t = time(0);
  return *localtime(&t);
}

auto get_chattag(const string &ctfn) -> string {
  struct stat st;
  if(stat(ctfn.c_str(), &st)) return {};
  return to_string(st.st_mtime);
}

string get_chat_filename(const string &datadir, const struct tm &now) {
  return datadir + '/' + to_string(now.tm_year) + '_' + to_string(now.tm_mon + 1);
}

void handle_request(FCgiIO &IO, Cgicc &CGI) {
  auto &env = CGI.getEnvironment();
  const auto datadir = CgiInput(IO).getenv("ZSWA_DATADIR");

  if(datadir.empty()) {
    handle_error(IO, CGI, "no datadir given", false);
    return;
  }

  bool do_show = false;
  string err;
  string user = env.getRemoteUser();
  if(user.empty()) user = "&lt;anon&gt;";

  if(env.getRequestMethod() == "POST") {
    do_show = true;

    const auto it = CGI.getElement("in");
    if(it != (*CGI).end()) {
      // append to file
      fstream chatf;
      struct tm now = get_cur_time();
      {
        const string ctfn = get_chat_filename(datadir, now);
        chatf.open(ctfn.c_str(), fstream::out | fstream::app);
      }
      if(!chatf) {
        err = "Chatdatei konnte nicht ge&ouml;ffnet werden";
      } else if(!it->isEmpty()) {
        string msg = it->getStrippedValue();
        pcrecpp::RE("\\@([1-9][0-9]*|0)", pcrecpp::RE_Options().set_utf8(true))
          .GlobalReplace("<a href=\"#e${1}\">${0}</a>", &msg);

        char dtm[30];
        strftime(dtm, 30, " (%d.%m.%Y %H:%M): ", &now);
        chatf << user << dtm << msg << '\n';
        if(!chatf) err = "Chatdatei konnte nicht geschrieben werden";
      }
    }
  } else {
    const auto it = CGI.getElement("g");
    if(it != (*CGI).end()) {
      // get chat data
      int status = 200;
      const char *sttext = "OK";
      string chattag;
      vector<string> content;

      {
        bool found = false;
        fstream chatf;
        {
          string ct = it->getStrippedValue();
          pcrecpp::RE("[/\\.]").GlobalReplace("", &ct);
          if(!ct.empty()) {
            string ctfn = datadir + '/' + ct;
            if(ct == "cur") ctfn = get_chat_filename(datadir, get_cur_time());
            chattag = get_chattag(ctfn);
            if(!chattag.empty()) {
              found = true;
              const auto ctit = CGI.getElement("t");
              if(ctit != (*CGI).end() && ctit->getStrippedValue() == chattag) {
                status = 304;
                sttext = "Not Modified";
              } else {
                chatf.open(ctfn.c_str(), fstream::in);
              }
            }
          }
        }
        if(!found) {
          status = 404;
          sttext = "File Not Found";
        } else if(chatf) {
          string tmp;
          size_t i = 0;
          while(getline(chatf, tmp)) {
            const string tsi = to_string(i);
            content.emplace_back("<a name=\"e" + tsi + "\">[" + tsi + "]</a> " + tmp);
            ++i;
          }
        }
      }

      {
        IO << "Status: " << status << "\r\n";
        if(!chattag.empty()) IO << "X-ChatTag: " << chattag << "\r\n";
        IO << "\r\n";
        for(auto it = content.rbegin(); it != content.rend(); ++it)
          IO << *it << "<br />\n";
      }
    } else {
      do_show = true;
    }
  }

  if(do_show) {
    IO << HTTPHTMLHeader() << "<!doctype html>\n"
       << html() << '\n'
       << head() << '\n'
       << "  " << title("Chat") << '\n'
       << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
       << "  " << script().set("src", "/zswebapp/zlc.js") << script() << '\n';

    { // WARNING: JS INJECTION
      string show_chat;
      const auto it = CGI.getElement("show");
      if(it != (*CGI).end()) show_chat = it->getStrippedValue();
      if(!show_chat.empty())
        IO << "  " << script() << '\n'
           << "    document.show_chat = '" << show_chat << "';\n"
           << "  " << script() << '\n';
    }

    IO << head() << '\n'
       << body().set("onload", "loadchat()") << '\n'
       << "  " << h1("Chat") << '\n'
       << "  " << a("Hauptseite").set("href", "..") << '\n'
       << "  " << form().set("action", ".").set("method", "POST") << '\n'
       << "    " << user << ":\n"
       << "    "
         << input().set("type", "text").set("name", "in") << ' '
         << input().set("type", "submit").set("value", "Absenden") << '\n'
       << "  " << form() << '\n';

    if(!err.empty())
      IO << p().set("style", "color: red;") << "Fehler: " << err << p() << '\n';

    IO << "  " << hr() << "\n"
          "  <p id=\"chat\"></p>\n"
       << body() << '\n' << html() << '\n';
  }
}

void handle_error(FCgiIO &IO, Cgicc &CGI, const char *msg, const bool do_reset) {
  // reset all elements
  if(do_reset) {
    html::reset();      head::reset();          body::reset();
    title::reset();     h1::reset();            h4::reset();
    cgicc::div::reset(); p::reset();
    a::reset();         h2::reset();            h3::reset();
  }

  IO << HTTPHTMLHeader() << "<!doctype html>\n"
     << html() << '\n'

     << head(title("ERROR occured in chat app")) << '\n'

     << body() << '\n'
     << h1("ERROR in chat app") << '\n'
     << p() << "Error: " << msg << p() << '\n'
     << body() << '\n'

     << html() << '\n';
}
