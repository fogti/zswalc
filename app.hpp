#pragma once
#include "3rdparty/FCgiIO.h"
#include <cgicc/Cgicc.h>
void handle_request(cgicc::FCgiIO &IO, cgicc::Cgicc &CGI);
void handle_error(cgicc::FCgiIO &IO, cgicc::Cgicc &CGI, const char *msg);
