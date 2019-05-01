#include "app.hpp"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <deque>
#include <fstream>
#include <map>
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

static string get_chat_filename(const struct tm &now) {
  string ret;
  ret.reserve(6);
  ((ret  = to_string(now.tm_year))
        += '_')
        += to_string(now.tm_mon + 1);
  return ret;
}

typedef decltype(Cgicc().getElement("")) TCGIit;

static void handle_get_chat(FCgiIO &IO, Cgicc &CGI, const string &datadir, const TCGIit &it) {
  string chattag = it->getStrippedValue();
  pcrecpp::RE("[/\\.]").GlobalReplace({}, &chattag);

  if(chattag.empty()) {
    IO << "Status: 404\r\n\r\n";
    return;
  }

  // get chat data
  const bool is_cur = (chattag == "cur");
  string ctfn = datadir + (is_cur ? get_chat_filename(get_cur_time()) : chattag);
  int status = 200;
  struct stat st;
  deque<string> content;
  if(stat(ctfn.c_str(), &st)) {
    chattag.clear();
    status = 404;
  } else {
    chattag = to_string(st.st_mtime);
    const auto ctit = CGI.getElement("t");
    if(ctit != (*CGI).end() && ctit->getStrippedValue() == chattag)
      status = 304;
    else {
      ifstream chatf(ctfn.c_str());
      string line, tmp;
      for(size_t i = 0; getline(chatf, line); ++i) {
        const string tsi = to_string(i);
        tmp.reserve(26 + 2 * tsi.size() + tmp.size());
        tmp += "<a name=\"e";
        tmp += tsi;
        tmp += "\">[";
        tmp += tsi;
        tmp += "]</a> ";
        tmp += line;
        tmp += "<br />\n";
        content.emplace_back(move(tmp));
        if(is_cur && content.size() == 26) content.pop_front();
      }
    }
    chattag = "X-ChatTag: " + move(chattag) + "\r\n";
  }
  IO << "Status: " << status << "\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
     << chattag << "\r\n";
  for(auto cit = content.rbegin(); cit != content.rend(); ++cit)
    IO << *cit;
}

static auto post_msg(FCgiIO &IO, Cgicc &CGI, const string &datadir, const string &user, const TCGIit &it) -> const char * {
  // append to file
  mkdir(datadir.c_str(), 0750);
  fstream chatf;
  struct tm now = get_cur_time();
  {
    const string ctfn = datadir + get_chat_filename(now);
    chatf.open(ctfn.c_str(), fstream::out | fstream::app);
  }
  if(!chatf)
    return "Chatdatei konnte nicht ge&ouml;ffnet werden";

  if(!it->isEmpty()) {
    string msg = it->getStrippedValue();
    if(msg.empty()) return 0;

    const auto opts = pcrecpp::RE_Options().set_utf8(true);
    pcrecpp::RE("<", opts).GlobalReplace("&lt;", &msg);
    pcrecpp::RE(">", opts).GlobalReplace("&gt;", &msg);
    pcrecpp::RE("\\@([1-9][0-9]*|0)"                , opts).GlobalReplace("<a href=\"#e\\1\">\\0</a>", &msg);
    pcrecpp::RE("\\[a\\b\\s*([^]]*)\\](.*?)\\[/a\\]", opts).GlobalReplace("<a \\1>\\2</a>", &msg);
    pcrecpp::RE("\\[b\\](.*?)\\[/b\\]"              , opts).GlobalReplace("<b>\\1</b>", &msg);
    pcrecpp::RE("\\[i\\](.*?)\\[/i\\]"              , opts).GlobalReplace("<i>\\1</i>", &msg);

    char dtm[30];
    strftime(dtm, 30, " (%d.%m.%Y %H:%M): ", &now);
    chatf << user << dtm << msg << endl;
    if(!chatf) return "Chatdatei konnte nicht geschrieben werden";
  }
  return 0;
}

static bool redirect2dir(FCgiIO &IO, const CgiEnvironment &env, const map<string, string> &gvars) {
  const string pathi = env.getPathInfo();
  if(pathi.empty() || pathi.back() != '/' || env.getPathTranslated().empty()) {
    // redirect to correct path
    IO << "Status: 30" << (gvars.empty() ? '7' : '1')
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

static void decr_month(struct tm &x) noexcept {
  if(!x.tm_mon) {
    --x.tm_year;
    x.tm_mon = 11;
  } else {
    --x.tm_mon;
  }
}

static void incr_month(struct tm &x) noexcept {
  ++x.tm_mon;
  if(x.tm_mon > 11) {
    ++x.tm_year;
    x.tm_mon = 0;
  }
}

static void print_nav2(FCgiIO &IO, struct tm &x, const char *desc) {
  IO << " <a href=\"?show=" << x.tm_year << '_' << (x.tm_mon + 1) << "\">[" << desc << "]</a>";
}

void handle_request(FCgiIO &IO) {
  Cgicc CGI(&IO);
  auto &env = CGI.getEnvironment();
  const auto formEnd = (*CGI).end();
  string user = env.getRemoteUser(), datadir = env.getPathTranslated();
  if(user.empty()) user = "&lt;anon " + env.getRemoteAddr() + "&gt;";
  const char *err = 0;
  if(!datadir.empty() && datadir.back() != '/') datadir += '/';

  if(env.getRequestMethod() == "POST") {
    const auto it = CGI.getElement("in");
    if(it != formEnd) {
      if(redirect2dir(IO, env, {}))
        return;
      err = post_msg(IO, CGI, datadir, user, it);
    }
  } else {
    const auto it = CGI.getElement("g");
    if(it != formEnd) {
      if(!redirect2dir(IO, env, {{ "g", it->getStrippedValue() }}))
        handle_get_chat(IO, CGI, datadir, it);
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

  IO << "Content-Type: text/html; charset=utf-8\r\n\r\n"
        "<!doctype html><html><head>\n"
        "  <title>Chat</title>\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
        "  <script src=\"/zswebapp/zlc.js\"></script>\n";

  if(!show_chat.empty())
    IO << "  <script>document.show_chat = '" << show_chat << "';</script>\n";

  IO << "</head><body onload=\"loadchat()\">\n"
        "  <h1>Chat</h1>\n"
        "  <a href=\"https://github.com/zserik/zswalc/\">[source code]</a> <a href=\"." << (show_chat.empty()?".":"")
       << "\">[parent]</a>";

  {
    bool pr_next = false;
    struct tm prev = get_cur_time();
    size_t pos = show_chat.find('_');
    if(pos != string::npos) {
      prev.tm_year = stoi(show_chat.substr(0, pos));
      prev.tm_mon  = stoi(show_chat.substr(pos + 1)) - 1;
      pr_next = true;
    }
    decr_month(prev);
    print_nav2(IO, prev, "prev");
    if(pr_next) {
      incr_month(prev);
      incr_month(prev);
      print_nav2(IO, prev, "next");
    }
    IO << '\n';
  }

  if(show_chat.empty())
    IO << "  <form action=\".\" method=\"POST\">" << user <<
          ": <input type=\"text\" name=\"in\" />"
           " <input type=\"submit\" value=\"Absenden\" /></form>\n";

  if(err)
    IO << "  <p style=\"color: red;\"><b>Error: " << err << "</b></p>\n";

  IO << "  <hr /><p id=\"chat\"></p>\n"
        "</body></html>\n";
}
