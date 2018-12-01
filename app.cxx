#include "app.hpp"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <map>
#include <vector>
#include <pcrecpp.h>
#include <cgicc/Cgicc.h>

using namespace std;
using namespace cgicc;

/* app invocation
 * GET without QUERY | := show=...  ---> chat display
 * GET Q := g=...                   ---> backend: get chat data
 * POST in=...                      ---> backend: append message
 */

static auto get_cur_time() noexcept -> struct tm {
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

typedef decltype(Cgicc().getElement("")) TCGIit;

static void handle_get_chat(FCgiIO &IO, Cgicc &CGI, const string &datadir, const TCGIit &it) {
  // get chat data
  int status = 200;
  string chattag;
  vector<string> content;

  {
    bool found = false, is_cur = false;
    fstream chatf;
    {
      chattag = it->getStrippedValue();
      pcrecpp::RE("[/\\.]").GlobalReplace("", &chattag);
      if(!chattag.empty()) {
        string ctfn = datadir + '/' + chattag;
        if(chattag == "cur") {
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

static string post_msg(FCgiIO &IO, Cgicc &CGI, const string &datadir, const string &user, const TCGIit &it) {
  // append to file
  mkdir(datadir.c_str(), 0750);
  fstream chatf;
  struct tm now = get_cur_time();
  {
    const string ctfn = get_chat_filename(datadir, now);
    chatf.open(ctfn.c_str(), fstream::out | fstream::app);
  }
  if(!chatf) {
    return "Chatdatei konnte nicht ge&ouml;ffnet werden";
  } else if(!it->isEmpty()) {
    string msg = it->getStrippedValue();
    if(msg.empty()) return {};

    const auto opts = pcrecpp::RE_Options().set_utf8(true);
    pcrecpp::RE("<", opts).GlobalReplace("&lt;", &msg);
    pcrecpp::RE(">", opts).GlobalReplace("&gt;", &msg);
    pcrecpp::RE("\\@([1-9][0-9]*|0)"                , opts).GlobalReplace("<a href=\"#e\\1\">\\0</a>", &msg);
    pcrecpp::RE("\\[a\\b\\s*([^]]*)\\](.*?)\\[/a\\]", opts).GlobalReplace("<a \\1>\\2</a>", &msg);
    pcrecpp::RE("\\[b\\](.*?)\\[/b\\]"              , opts).GlobalReplace("<b>\\1</b>", &msg);

    char dtm[30];
    strftime(dtm, 30, " (%d.%m.%Y %H:%M): ", &now);
    chatf << user << dtm << msg << '\n';
    if(!chatf) return "Chatdatei konnte nicht geschrieben werden";
  }
  return {};
}

static bool redirect2dir(FCgiIO &IO, const CgiEnvironment &env, const map<string, string> &gvars) {
  const string pathi = env.getPathInfo();
  if(pathi.empty() || pathi.back() != '/' || env.getPathTranslated().empty()) {
    // redirect to correct path
    IO << "Status: " << (gvars.empty() ? "307" : "301")
       << "\r\nLocation: " << env.getScriptName() << pathi << '/';
    bool fi = true;
    for(const auto &i : gvars) {
      if(i.second.empty()) continue;
      IO << (fi ? '?' : '&') << i.first << '=' << i.second;
      fi = false;
    }
    IO << "\r\n\r\n";
    return true;
  }
  return false;
}

void handle_request(FCgiIO &IO) {
  Cgicc CGI(&IO);
  auto &env = CGI.getEnvironment();
  const auto formEnd = (*CGI).end();
  string user = env.getRemoteUser();
  if(user.empty()) user = "&lt;anon " + env.getRemoteAddr() + "&gt;";

  if(env.getRequestMethod() == "POST") {
    const auto it = CGI.getElement("in");
    if(it != formEnd) {
      if(redirect2dir(IO, env, {}))
        return;
      post_msg(IO, CGI, env.getPathTranslated(), user, it);
    }
  } else {
    const auto it = CGI.getElement("g");
    if(it != formEnd) {
      if(!redirect2dir(IO, env, {{ "g", it->getStrippedValue() }}))
        handle_get_chat(IO, CGI, env.getPathTranslated(), it);
      return;
    }
  }

  string show_chat;
  {
    const auto it = CGI.getElement("show");
    if(it != (*CGI).end()) {
      show_chat = it->getStrippedValue();
      if(show_chat == "cur" || (!show_chat.empty() && show_chat.find("'") != string::npos))
        show_chat.clear();
    }

    if(redirect2dir(IO, env, {{ "show", show_chat }}))
      return;
  }

  IO << "Content-Type: text/html\r\n\r\n"
        "<!doctype html>\n"
        "<html>\n<head>\n"
        "  <title>Chat</title>\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
        "  <script src=\"/zswebapp/zlc.js\"></script>\n";

  if(!show_chat.empty())
    IO << "  <script>document.show_chat = '" << show_chat << "';</script>\n";

  IO << "</head>\n"
        "<body onload=\"loadchat()\">\n"
        "  <h1>Chat</h1>\n";

  if(show_chat.empty()) {
    IO << "  <a href=\"..\">Hauptseite</a>\n"
          "  <form action=\".\" method=\"POST\">\n"
          "    " << user << ":\n"
          "    <input type=\"text\" name=\"in\" /> <input type=\"submit\" value=\"Absenden\" />\n"
          "  </form>\n";
  } else {
    IO << "  <a href=\".\">Hauptseite</a>\n";
  }

  IO << "  <hr />\n"
        "  <p id=\"chat\"></p>\n"
        "</body>\n</html>\n";
}

void handle_error(FCgiIO &IO, const char *msg) {
  IO << "Content-Type: text/html\r\n\r\n"
        "<!doctype html>\n<html>\n"
        "<head><title>ERROR occured in chat app</title></head>\n"
        "<body>\n"
        "  <h1>ERROR in chat app</h1>\n"
        "  <p style=\"color: red;\">Error: " << msg << "</p>\n"
        "</body>\n</html>\n";
}
