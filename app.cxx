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

static string get_chat_filename(const string &datadir, const struct tm &now) {
  string ret = datadir;
  ret.reserve(ret.size() + 6);
  ret += to_string(now.tm_year);
  ret += '_';
  ret += to_string(now.tm_mon + 1);
  return ret;
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
      pcrecpp::RE("[/\\.]").GlobalReplace({}, &chattag);
      if(!chattag.empty()) {
        string ctfn;
        if(chattag == "cur") {
          is_cur = true;
          ctfn = get_chat_filename(datadir, get_cur_time());
        } else {
          ctfn = datadir + chattag;
        }
        { // chattag = get_chattag(ctfn);
          struct stat st;
          if(stat(ctfn.c_str(), &st)) chattag.clear();
          else chattag = to_string(st.st_mtime);
        }
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

  IO << "Status: " << status << "\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n";
  if(!chattag.empty()) IO << "X-ChatTag: " << chattag << "\r\n";
  IO << "\r\n";
  for(auto cit = content.rbegin(); cit != content.rend(); ++cit)
    IO << *cit;
}

static auto post_msg(FCgiIO &IO, Cgicc &CGI, const string &datadir, const string &user, const TCGIit &it) -> const char * {
  // append to file
  mkdir(datadir.c_str(), 0750);
  fstream chatf;
  struct tm now = get_cur_time();
  {
    const string ctfn = get_chat_filename(datadir, now);
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

  string prev_link;
  {
    struct tm prev = get_cur_time();
    size_t pos = show_chat.find('_');
    if(pos != string::npos) {
      prev.tm_year = stoi(show_chat.substr(0, pos));
      prev.tm_mon  = stoi(show_chat.substr(pos + 1)) - 1;
    }
    if(!prev.tm_mon) {
      prev.tm_year--;
      prev.tm_mon = 12;
    }
    prev_link += " <a href=\"?show=";
    prev_link += to_string(prev.tm_year) + '_' + to_string(prev.tm_mon);
    prev_link += "\">[prev]</a>";
  }

  IO << "  <a href=\"https://github.com/zserik/zswalc/\">[source code]</a> ";
  if(show_chat.empty()) {
    IO << "<a href=\"..\">[parent]</a>" << prev_link << "\n"
          "  <form action=\".\" method=\"POST\">\n"
          "    " << user << ":\n"
          "    <input type=\"text\" name=\"in\" /> <input type=\"submit\" value=\"Absenden\" />\n"
          "  </form>\n";
  } else {
    IO << "<a href=\".\">[parent]</a>" << prev_link << '\n';
  }

  if(err)
    IO << "  <p style=\"color: red;\"><b>Error: " << err << "</b></p>\n";

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
