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

static auto get_cur_time() -> struct tm {
  time_t t = time(0);
  return *localtime(&t);
}

static auto get_chattag(const string &ctfn) -> string {
  struct stat st;
  if(stat(ctfn.c_str(), &st)) return {};
  return to_string(st.st_mtime);
}

static string get_chat_filename(const string &datadir, const struct tm &now) {
  return datadir + '/' + to_string(now.tm_year) + '_' + to_string(now.tm_mon + 1);
}

void handle_request(FCgiIO &IO, Cgicc &CGI) {
  auto &env = CGI.getEnvironment();
  auto datadir = CgiInput(IO).getenv("ZSWA_DATADIR");

  if(datadir.empty()) {
    handle_error(IO, CGI, "no datadir given");
    return;
  }

  bool do_show = true;
  string err, user = env.getRemoteUser();
  if(user.empty()) user = "&lt;anon " + env.getRemoteHost() + "&gt;";

  {
    string subpath = env.getPathInfo();
    if(subpath == "/") subpath.clear();
    datadir += subpath;
    if(!subpath.empty()) mkdir(datadir.c_str(), 0750);
  }

  if(env.getRequestMethod() == "POST") {
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
      do_show = false;
      int status = 200;
      string chattag;
      vector<string> content;

      {
        bool found = false, is_cur = false;
        fstream chatf;
        {
          string ct = it->getStrippedValue();
          pcrecpp::RE("[/\\.]").GlobalReplace("", &ct);
          if(!ct.empty()) {
            string ctfn = datadir + '/' + ct;
            if(ct == "cur") {
              is_cur = true;
              ctfn = get_chat_filename(datadir, get_cur_time());
            }
            chattag = get_chattag(ctfn);
            if(!chattag.empty()) {
              found = true;
              const auto ctit = CGI.getElement("t");
              if(ctit != (*CGI).end() && ctit->getStrippedValue() == chattag)
                status = 304;
              else
                chatf.open(ctfn.c_str(), fstream::in);
            }
          }
        }
        if(!found) {
          status = 404;
        } else if(chatf) {
          string tmp;
          size_t i = 0;
          while(getline(chatf, tmp)) {
            const string tsi = to_string(i);
            content.emplace_back("<a name=\"e" + tsi + "\">[" + tsi + "]</a> " + tmp + "<br />\n");
            ++i;
          }
          if(is_cur && content.size() > 25)
            content.erase(content.begin(), content.end() - 25);
        }
      }

      IO << "Status: " << status << "\r\n";
      if(!chattag.empty()) IO << "X-ChatTag: " << chattag << "\r\n";
      IO << "\r\n";
      for(auto cit = content.rbegin(); cit != content.rend(); ++cit)
        IO << *cit;
    }
  }

  if(do_show) {
    IO << HTTPHTMLHeader() << "<!doctype html>\n"
          "<html>\n<head>\n"
          "  <title>Chat</title>\n"
          "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
       << "  <script src=\"/zswebapp/zlc.js\"></script>\n";

    { // WARNING: JS INJECTION
      string show_chat;
      const auto it = CGI.getElement("show");
      if(it != (*CGI).end()) show_chat = it->getStrippedValue();
      if(!show_chat.empty())
        IO << "  <script>document.show_chat = '" << show_chat << "';</script>\n";
    }

    IO << "</head>\n"
          "<body onload=\"loadchat()\">\n"
          "  <h1>Chat</h1>\n"
          "  <a href=\"..\">Hauptseite</a>\n"
          "  <form action=\".\" method=\"POST\">\n"
          "    " << user << ":\n"
          "    <input type=\"text\" name=\"in\" /> <input type=\"submit\" value=\"Absenden\" />\n"
          "  </form>\n";

    if(!err.empty())
      IO << "  <p style=\"color: red;\">Fehler: " << err << "</p>\n";

    IO << "  <hr />\n"
          "  <p id=\"chat\"></p>\n"
          "</body>\n</html>\n";
  }
}

void handle_error(FCgiIO &IO, Cgicc &CGI, const char *msg) {
  IO << HTTPHTMLHeader() << "<!doctype html>\n"
        "<html>\n"
        "<head><title>ERROR occured in chat app</title></head>\n"
        "<body>\n"
        "  <h1>ERROR in chat app</h1>\n"
        "  <p>Error: " << msg << "</p>\n"
        "</body>\n"
        "</html>\n";
}
