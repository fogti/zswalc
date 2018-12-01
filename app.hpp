#pragma once
#include "3rdparty/FCgiIO.h"
void handle_request(cgicc::FCgiIO &IO);
void handle_error(cgicc::FCgiIO &IO, const char *msg);
