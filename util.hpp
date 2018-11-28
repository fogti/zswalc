#pragma once
#include <string>
#include <fcgio.h>

/**
 * Note this is not thread safe due to the static allocation of the
 * content.
 */
void get_request_content(std::string &content, const FCGX_Request &request);
