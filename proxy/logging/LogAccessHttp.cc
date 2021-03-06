/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/***************************************************************************
 LogAccessHttp.cc

 This file defines the Http implementation of a LogAccess object, and
 implements the accessor functions using information about an Http state
 machine.


 ***************************************************************************/
#include "libts.h"
#include "Error.h"
#include "LogAccessHttp.h"
#include "http/HttpSM.h"
#include "MIME.h"
#include "HTTP.h"
#include "LogUtils.h"
#include "LogObject.h"
#include "LogConfig.h"
#include "Log.h"

/*-------------------------------------------------------------------------
  LogAccessHttp

  Initialize the private data members and assert that we got a valid state
  machine pointer.
  -------------------------------------------------------------------------*/

LogAccessHttp::LogAccessHttp(HttpSM * sm)
  : m_http_sm(sm), m_arena(), m_client_request(NULL), m_proxy_response(NULL), m_proxy_request(NULL),
    m_server_response(NULL), m_cache_response(NULL), m_client_req_url_str(NULL), m_client_req_url_len(0),
    m_client_req_url_canon_str(NULL), m_client_req_url_canon_len(0), m_client_req_unmapped_url_canon_str(NULL),
    m_client_req_unmapped_url_canon_len(-1), m_client_req_unmapped_url_path_str(NULL),
    m_client_req_unmapped_url_path_len(-1), m_client_req_unmapped_url_host_str(NULL),
    m_client_req_unmapped_url_host_len(-1), m_client_req_url_path_str(NULL), m_client_req_url_path_len(0),
    m_proxy_resp_content_type_str(NULL), m_proxy_resp_content_type_len(0)
{
  ink_assert(m_http_sm != NULL);
}

/*-------------------------------------------------------------------------
  LogAccessHttp::~LogAccessHttp

  Deallocate space for any strings allocated in the init routine.
  -------------------------------------------------------------------------*/

LogAccessHttp::~LogAccessHttp()
{
}

/*-------------------------------------------------------------------------
  LogAccessHttp::init

  Build some strings that will come in handy for processing later, such as
  URL.  This saves us from having to build the strings twice: once to
  compute their length and a second time to actually marshal them.  We also
  initialize local pointers to each of the 4 http headers.  However, there
  is no guarantee that these headers will all be valid, so we must always
  check the validity of these pointers before using them.
  -------------------------------------------------------------------------*/

#define HIDDEN_CONTENT_TYPE "@Content-Type"
#define HIDDEN_CONTENT_TYPE_LEN 13

void
LogAccessHttp::init()
{
  HttpTransact::HeaderInfo * hdr = &(m_http_sm->t_state.hdr_info);

  if (hdr->client_request.valid()) {
    m_client_request = &(hdr->client_request);

    // make a copy of the incoming url into the arena
    const char *url_string_ref = m_client_request->url_string_get_ref(&m_client_req_url_len);
    m_client_req_url_str = m_arena.str_alloc(m_client_req_url_len + 1);
    memcpy(m_client_req_url_str, url_string_ref, m_client_req_url_len);
    m_client_req_url_str[m_client_req_url_len] = '\0';

    m_client_req_url_canon_str = LogUtils::escapify_url(&m_arena, m_client_req_url_str, m_client_req_url_len,
                                                        &m_client_req_url_canon_len);
    m_client_req_url_path_str = m_client_request->path_get(&m_client_req_url_path_len);
  }

  if (hdr->client_response.valid()) {
    m_proxy_response = &(hdr->client_response);
    MIMEField *field = m_proxy_response->field_find(MIME_FIELD_CONTENT_TYPE, MIME_LEN_CONTENT_TYPE);
    if (field) {
      m_proxy_resp_content_type_str = (char *) field->value_get(&m_proxy_resp_content_type_len);
      //
      // here is the assert
      //
      //assert (m_proxy_resp_content_type_str[0] >= 'A' && m_proxy_resp_content_type_str[0] <= 'z');
      LogUtils::remove_content_type_attributes(m_proxy_resp_content_type_str, &m_proxy_resp_content_type_len);
    } else {
      // If Content-Type field is missing, check for @Content-Type
      field = m_proxy_response->field_find(HIDDEN_CONTENT_TYPE, HIDDEN_CONTENT_TYPE_LEN);
      if (field) {
        m_proxy_resp_content_type_str = (char *) field->value_get(&m_proxy_resp_content_type_len);
        LogUtils::remove_content_type_attributes(m_proxy_resp_content_type_str, &m_proxy_resp_content_type_len);
      }
    }
  }
  if (hdr->server_request.valid()) {
    m_proxy_request = &(hdr->server_request);
  }
  if (hdr->server_response.valid()) {
    m_server_response = &(hdr->server_response);
  }
  if (hdr->cache_response.valid()) {
    m_cache_response = &(hdr->cache_response);
  }
}

/*-------------------------------------------------------------------------
  The set routines ...

  These routines are used by the WIPE_FIELD_VALUE filter to replace the original req url
  strings with the WIPED req strings.
  -------------------------------------------------------------------------*/

void
LogAccessHttp::set_client_req_url(char *buf, int len)
{
  if (buf) {
    m_client_req_url_len = len;
    ink_strlcpy(m_client_req_url_str, buf, m_client_req_url_len + 1);
  }
}

void
LogAccessHttp::set_client_req_url_canon(char *buf, int len)
{
  if (buf) {
    m_client_req_url_canon_len = len;
    ink_strlcpy(m_client_req_url_canon_str, buf, m_client_req_url_canon_len + 1);
  }
}

void
LogAccessHttp::set_client_req_unmapped_url_canon(char *buf, int len)
{
  if (buf) {
    m_client_req_unmapped_url_canon_len = len;
    ink_strlcpy(m_client_req_unmapped_url_canon_str, buf, m_client_req_unmapped_url_canon_len + 1);
  }
}

void
LogAccessHttp::set_client_req_unmapped_url_path(char *buf, int len)
{
  if (buf) {
    m_client_req_unmapped_url_path_len = len;
    ink_strlcpy(m_client_req_unmapped_url_path_str, buf, m_client_req_unmapped_url_path_len + 1);
  }
}

void
LogAccessHttp::set_client_req_unmapped_url_host(char *buf, int len)
{
  if (buf) {
    m_client_req_unmapped_url_host_len = len;
    ink_strlcpy(m_client_req_unmapped_url_host_str, buf, m_client_req_unmapped_url_host_len + 1);
  }
}

void
LogAccessHttp::set_client_req_url_path(char *buf, int len)
{
  //?? use m_client_req_unmapped_url_path_str for now..may need to enhance later..
  if (buf) {
    m_client_req_url_path_len = len;
    ink_strlcpy(m_client_req_unmapped_url_path_str, buf, m_client_req_url_path_len + 1);
  }
}


/*-------------------------------------------------------------------------
  The marshalling routines ...

  We know that m_http_sm is a valid pointer (we assert so in the ctor), but
  we still need to check the other header pointers before using them in the
  routines.
  -------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
int
LogAccessHttp::marshal_plugin_identity_id(char *buf)
{
  if (buf) marshal_int(buf, m_http_sm->plugin_id);
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_plugin_identity_tag(char *buf)
{
  int len = INK_MIN_ALIGN;
  char const* tag = m_http_sm->plugin_tag;

  if (!tag) tag = "*";
  else len = LogAccess::strlen(tag);

  if (buf) marshal_str(buf, tag, len);

  return len;
}

int
LogAccessHttp::marshal_client_host_ip(char *buf)
{
  return marshal_ip(buf, &m_http_sm->t_state.client_info.addr.sa);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_host_port(char *buf)
{
  if (buf) {
    uint16_t port = ntohs(m_http_sm->t_state.client_info.addr.port());
    marshal_int(buf, port);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  user authenticated to the proxy (RFC931)
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_auth_user_name(char *buf)
{
  char *str = NULL;
  int len = INK_MIN_ALIGN;

  // Jira TS-40:
  // NOTE: Authentication related code and modules were removed/disabled.
  //       Uncomment code path below when re-added/enabled.
  /*if (m_http_sm->t_state.auth_params.user_name) {
    str = m_http_sm->t_state.auth_params.user_name;
    len = LogAccess::strlen(str);
    } */
  if (buf) {
    marshal_str(buf, str, len);
  }
  return len;
}


/*-------------------------------------------------------------------------
  Private utility function to validate m_client_req_unmapped_url_canon_str &
  m_client_req_unmapped_url_canon_len fields.
  -------------------------------------------------------------------------*/

void
LogAccessHttp::validate_unmapped_url(void)
{
  if (m_client_req_unmapped_url_canon_len < 0) {
    if (m_http_sm->t_state.pristine_url.valid()) {
      int unmapped_url_len;
      char *unmapped_url = m_http_sm->t_state.pristine_url.string_get_ref(&unmapped_url_len);

      if (unmapped_url && unmapped_url[0] != 0) {
        m_client_req_unmapped_url_canon_str =
        LogUtils::escapify_url(&m_arena, unmapped_url, unmapped_url_len, &m_client_req_unmapped_url_canon_len);
      }
    } else {
      m_client_req_unmapped_url_canon_len = 0;
    }
  }
}

/*-------------------------------------------------------------------------
  Private utility function to validate m_client_req_unmapped_url_path_str &
  m_client_req_unmapped_url_path_len fields.
  -------------------------------------------------------------------------*/

void
LogAccessHttp::validate_unmapped_url_path(void)
{
  int len;
  char *c;

  if (m_client_req_unmapped_url_path_len < 0 && m_client_req_unmapped_url_host_len < 0) {
    // Use unmapped canonical URL as default
    m_client_req_unmapped_url_path_str = m_client_req_unmapped_url_canon_str;
    m_client_req_unmapped_url_path_len = m_client_req_unmapped_url_canon_len;

    if (m_client_req_unmapped_url_path_len >= 6) {      // xxx:// - minimum schema size
      c = (char *) memchr((void *) m_client_req_unmapped_url_path_str, ':', m_client_req_unmapped_url_path_len - 1);
      if (c && (len = (int) (c - m_client_req_unmapped_url_path_str)) <= 5) {   // 5 - max schema size
        if (len + 2 <= m_client_req_unmapped_url_canon_len && c[1] == '/' && c[2] == '/') {
          len += 3;             // Skip "://"
          m_client_req_unmapped_url_host_str = &m_client_req_unmapped_url_canon_str[len];
          m_client_req_unmapped_url_host_len = m_client_req_unmapped_url_path_len - len;
          // Attempt to find first '/' in the path
          if (m_client_req_unmapped_url_host_len > 0 &&
              (c =
               (char *) memchr((void *) m_client_req_unmapped_url_host_str, '/',
                               m_client_req_unmapped_url_path_len)) != 0) {
            m_client_req_unmapped_url_host_len = (int) (c - m_client_req_unmapped_url_host_str);
            m_client_req_unmapped_url_path_str = &m_client_req_unmapped_url_host_str[m_client_req_unmapped_url_host_len];
            m_client_req_unmapped_url_path_len = m_client_req_unmapped_url_path_len - len - m_client_req_unmapped_url_host_len;
          }
        }
      }
    }
  }
}


/*-------------------------------------------------------------------------
  This is the method, url, and version all rolled into one.  Use the
  respective marshalling routines to do the job.
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_text(char *buf)
{
  int len = marshal_client_req_http_method(NULL) + marshal_client_req_url(NULL) + marshal_client_req_http_version(NULL);

  if (buf) {
    int offset = 0;
    offset += marshal_client_req_http_method(&buf[offset]);
    offset += marshal_client_req_url(&buf[offset]);
    offset += marshal_client_req_http_version(&buf[offset]);
    len = offset;
  }
  return len;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_http_method(char *buf)
{
  char *str = NULL;
  int alen = 0;
  int plen = INK_MIN_ALIGN;

  if (m_client_request) {
    str = (char *) m_client_request->method_get(&alen);

    // calculate the the padded length only if the actual length
    // is not zero. We don't want the padded length to be zero
    // because marshal_mem should write the DEFAULT_STR to the
    // buffer if str is nil, and we need room for this.
    //
    if (alen) {
      plen = round_strlen(alen + 1);    // +1 for trailing 0
    }
  }

  if (buf)
    marshal_mem(buf, str, alen, plen);
  return plen;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_url(char *buf)
{
  int len = round_strlen(m_client_req_url_len + 1);     // +1 for trailing 0

  if (buf) {
    marshal_mem(buf, m_client_req_url_str, m_client_req_url_len, len);
  }
  return len;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_url_canon(char *buf)
{
  int len = round_strlen(m_client_req_url_canon_len + 1);

  if (buf) {
    marshal_mem(buf, m_client_req_url_canon_str, m_client_req_url_canon_len, len);
  }
  return len;
}


/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_unmapped_url_canon(char *buf)
{
  int len = INK_MIN_ALIGN;

  validate_unmapped_url();
  if (0 == m_client_req_unmapped_url_canon_len) {
    // If the unmapped URL isn't populated, we'll fall back to the original
    // client URL. This helps for example server intercepts to continue to
    // log the requests, even when there is no remap rule for it.
    len = marshal_client_req_url_canon(buf);
  } else {
    len = round_strlen(m_client_req_unmapped_url_canon_len + 1);      // +1 for eos
    if (buf) {
      marshal_mem(buf, m_client_req_unmapped_url_canon_str, m_client_req_unmapped_url_canon_len, len);
    }
  }

  return len;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_unmapped_url_path(char *buf)
{
  int len = INK_MIN_ALIGN;

  validate_unmapped_url();
  validate_unmapped_url_path();

  if (0 == m_client_req_unmapped_url_path_len) {
    len = marshal_client_req_url_path(buf);
  } else {
    len = round_strlen(m_client_req_unmapped_url_path_len + 1);   // +1 for eos
    if (buf) {
      marshal_mem(buf, m_client_req_unmapped_url_path_str, m_client_req_unmapped_url_path_len, len);
    }
  }
  return len;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_unmapped_url_host(char *buf)
{
  int len = INK_MIN_ALIGN;

  validate_unmapped_url();
  validate_unmapped_url_path();

  len = round_strlen(m_client_req_unmapped_url_host_len + 1);      // +1 for eos
  if (buf) {
    marshal_mem(buf, m_client_req_unmapped_url_host_str, m_client_req_unmapped_url_host_len, len);
  }
  return len;
}

int
LogAccessHttp::marshal_client_req_url_path(char *buf)
{
  int len = round_strlen(m_client_req_url_path_len + 1);
  if (buf) {
    marshal_mem(buf, m_client_req_url_path_str, m_client_req_url_path_len, len);
  }
  return len;
}

int
LogAccessHttp::marshal_client_req_url_scheme(char *buf)
{
  char *str = NULL;
  int alen = 0;
  int plen = INK_MIN_ALIGN;

  str = (char *) m_client_request->scheme_get(&alen);

  // calculate the the padded length only if the actual length
  // is not zero. We don't want the padded length to be zero
  // because marshal_mem should write the DEFAULT_STR to the
  // buffer if str is nil, and we need room for this.
  //
  if (alen) {
    plen = round_strlen(alen + 1);    // +1 for trailing 0
  }

  if (buf) {
    marshal_mem(buf, str, alen, plen);
  }

  return plen;
}

/*-------------------------------------------------------------------------
  For this one we're going to marshal two INTs, one the first representing
  the major number and the second representing the minor.
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_http_version(char *buf)
{
  if (buf) {
    int64_t major = 0;
    int64_t minor = 0;
    if (m_client_request) {
      HTTPVersion versionObject = m_client_request->version_get();
      major = HTTP_MAJOR(versionObject.m_version);
      minor = HTTP_MINOR(versionObject.m_version);
    }
    marshal_int(buf, major);
    marshal_int((buf + INK_MIN_ALIGN), minor);
  }
  return (2 * INK_MIN_ALIGN);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_header_len(char *buf)
{
  if (buf) {
    int64_t len = 0;
    if (m_client_request) {
      len = m_client_request->length_get();
    }
    marshal_int(buf, len);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_client_req_body_len(char *buf)
{
  if (buf) {
    int64_t len = 0;
    if (m_client_request) {
      len = m_http_sm->client_request_body_bytes;
    }
    marshal_int(buf, len);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_client_finish_status_code(char *buf)
{
  if (buf) {
    int code = LOG_FINISH_FIN;
    HttpTransact::AbortState_t cl_abort_state = m_http_sm->t_state.client_info.abort;
    if (cl_abort_state == HttpTransact::ABORTED) {

      // Check to see if the abort is due to a timeout
      if (m_http_sm->t_state.client_info.state == HttpTransact::ACTIVE_TIMEOUT ||
          m_http_sm->t_state.client_info.state == HttpTransact::INACTIVE_TIMEOUT) {
        code = LOG_FINISH_TIMEOUT;
      } else {
        code = LOG_FINISH_INTR;
      }
    }
    marshal_int(buf, code);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_resp_content_type(char *buf)
{
  int len = round_strlen(m_proxy_resp_content_type_len + 1);
  if (buf) {
    marshal_mem(buf, m_proxy_resp_content_type_str, m_proxy_resp_content_type_len, len);
  }
  return len;
}

/*-------------------------------------------------------------------------
  Squid returns the content-length + header length as the total length.
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_resp_squid_len(char *buf)
{
  if (buf) {
    int64_t val = m_http_sm->client_response_hdr_bytes + m_http_sm->client_response_body_bytes;
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_resp_content_len(char *buf)
{
  if (buf) {
    int64_t val = m_http_sm->client_response_body_bytes;
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_resp_status_code(char *buf)
{
  if (buf) {
    HTTPStatus status;
    if (m_proxy_response && m_client_request) {
      if (m_client_request->version_get() >= HTTPVersion(1, 0)) {
        status = m_proxy_response->status_get();
      }
      // INKqa10788
      // For bad/incomplete request, the request version may be 0.9.
      // However, we can still log the status code if there is one.
      else if (m_proxy_response->valid()) {
        status = m_proxy_response->status_get();
      } else {
        status = HTTP_STATUS_OK;
      }
    } else {
      status = HTTP_STATUS_NONE;
    }
    marshal_int(buf, (int64_t) status);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_resp_header_len(char *buf)
{
  if (buf) {
    int64_t val = m_http_sm->client_response_hdr_bytes;
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_proxy_finish_status_code(char *buf)
{
  /* FIXME: Should there be no server transaction code if
     the result comes out of the cache.  Right now we default
     to FIN */
  if (buf) {
    int code = LOG_FINISH_FIN;
    if (m_http_sm->t_state.current.server) {
      switch (m_http_sm->t_state.current.server->state) {
      case HttpTransact::ACTIVE_TIMEOUT:
      case HttpTransact::INACTIVE_TIMEOUT:
        code = LOG_FINISH_TIMEOUT;
        break;
      case HttpTransact::CONNECTION_ERROR:
        code = LOG_FINISH_INTR;
        break;
      default:
        if (m_http_sm->t_state.current.server->abort == HttpTransact::ABORTED) {
          code = LOG_FINISH_INTR;
        }
        break;
      }
    }

    marshal_int(buf, code);
  }

  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
-------------------------------------------------------------------------*/
int
LogAccessHttp::marshal_proxy_host_port(char *buf)
{
  if (buf) {
    uint16_t port = m_http_sm->t_state.request_data.incoming_port;
    marshal_int(buf, port);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_cache_result_code(char *buf)
{
  if (buf) {
    SquidLogCode code = m_http_sm->t_state.squid_codes.log_code;
    marshal_int(buf, (int64_t) code);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_req_header_len(char *buf)
{
  if (buf) {
    int64_t val = 0;
    if (m_proxy_request) {
      val = m_proxy_request->length_get();
    }
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_req_body_len(char *buf)
{
  if (buf) {
    int64_t val = 0;
    if (m_proxy_request) {
      val = m_http_sm->server_request_body_bytes;
    }
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_proxy_req_server_name(char *buf)
{
  char *str = NULL;
  int len = INK_MIN_ALIGN;

  if (m_http_sm->t_state.current.server) {
    str = m_http_sm->t_state.current.server->name;
    len = LogAccess::strlen(str);
  }

  if (buf) {
    marshal_str(buf, str, len);
  }
  return len;
}

// TODO: Change marshalling code to support both IPv4 and IPv6 addresses.
int
LogAccessHttp::marshal_proxy_req_server_ip(char *buf)
{
  return marshal_ip(buf, m_http_sm->t_state.current.server != NULL ? &m_http_sm->t_state.current.server->addr.sa : 0);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_proxy_hierarchy_route(char *buf)
{
  if (buf) {
    SquidHierarchyCode code = m_http_sm->t_state.squid_codes.hier_code;
    marshal_int(buf, (int64_t) code);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

// TODO: Change marshalling code to support both IPv4 and IPv6 addresses.
int
LogAccessHttp::marshal_server_host_ip(char *buf)
{
  sockaddr const* ip = 0;
  ip = &m_http_sm->t_state.server_info.addr.sa;
  if (! ats_is_ip(ip)) {
    if (m_http_sm->t_state.current.server) {
      ip = &m_http_sm->t_state.current.server->addr.sa;
      if (! ats_is_ip(ip)) ip = 0;
    } else {
      ip = 0;
    }
  }
  return marshal_ip(buf, ip);
}


/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_server_host_name(char *buf)
{
  char const* str = NULL;
  int padded_len = INK_MIN_ALIGN;
  int actual_len = 0;

  if (m_client_request) {
    str = m_client_request->host_get(&actual_len);

    if (str)
      padded_len = round_strlen(actual_len + 1);        // +1 for trailing 0
  }
  if (buf) {
    marshal_mem(buf, str, actual_len, padded_len);
  }
  return padded_len;
}


/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_server_resp_status_code(char *buf)
{
  if (buf) {
    HTTPStatus status;
    if (m_server_response) {
      status = m_server_response->status_get();
    } else {
      status = HTTP_STATUS_NONE;
    }
    marshal_int(buf, (int64_t) status);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_server_resp_content_len(char *buf)
{
  if (buf) {
    int64_t val = 0;
    if (m_server_response) {
      val = m_http_sm->server_response_body_bytes;
    }
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_server_resp_header_len(char *buf)
{
  if (buf) {
    int64_t val = 0;
    if (m_server_response) {
      val = m_server_response->length_get();
    }
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_server_resp_http_version(char *buf)
{
  if (buf) {
    int64_t major = 0;
    int64_t minor = 0;
    if (m_server_response) {
      major = HTTP_MAJOR(m_server_response->version_get().m_version);
      minor = HTTP_MINOR(m_server_response->version_get().m_version);
    }
    marshal_int(buf, major);
    marshal_int((buf + INK_MIN_ALIGN), minor);
  }
  return (2 * INK_MIN_ALIGN);
}

/*-------------------------------------------------------------------------
-------------------------------------------------------------------------*/
int
LogAccessHttp::marshal_server_resp_time_ms(char *buf)
{
  if (buf) {
    ink_hrtime elapsed = m_http_sm->milestones.server_close - m_http_sm->milestones.server_connect;
    int64_t val = (int64_t)ink_hrtime_to_msec(elapsed);
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_server_resp_time_s(char *buf)
{
  if (buf) {
    ink_hrtime elapsed = m_http_sm->milestones.server_close - m_http_sm->milestones.server_connect;
    int64_t val = (int64_t)ink_hrtime_to_sec(elapsed);
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_cache_resp_status_code(char *buf)
{
  if (buf) {
    HTTPStatus status;
    if (m_cache_response) {
      status = m_cache_response->status_get();
    } else {
      status = HTTP_STATUS_NONE;
    }
    marshal_int(buf, (int64_t) status);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_cache_resp_content_len(char *buf)
{
  if (buf) {
    int64_t val = 0;
    if (m_cache_response) {
      val = m_http_sm->cache_response_body_bytes;
    }
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_cache_resp_header_len(char *buf)
{
  if (buf) {
    int64_t val = 0;
    if (m_cache_response) {
      val = m_http_sm->cache_response_hdr_bytes;
    }
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_cache_resp_http_version(char *buf)
{
  if (buf) {
    int64_t major = 0;
    int64_t minor = 0;
    if (m_cache_response) {
      major = HTTP_MAJOR(m_cache_response->version_get().m_version);
      minor = HTTP_MINOR(m_cache_response->version_get().m_version);
    }
    marshal_int(buf, major);
    marshal_int((buf + INK_MIN_ALIGN), minor);
  }
  return (2 * INK_MIN_ALIGN);
}


int
LogAccessHttp::marshal_client_retry_after_time(char *buf)
{
  if (buf) {
    int64_t crat = m_http_sm->t_state.congestion_control_crat;
    marshal_int(buf, crat);
  }
  return INK_MIN_ALIGN;
}

static LogCacheWriteCodeType
convert_cache_write_code(HttpTransact::CacheWriteStatus_t t)
{

  LogCacheWriteCodeType code;
  switch (t) {
  case HttpTransact::NO_CACHE_WRITE:
    code = LOG_CACHE_WRITE_NONE;
    break;
  case HttpTransact::CACHE_WRITE_LOCK_MISS:
    code = LOG_CACHE_WRITE_LOCK_MISSED;
    break;
  case HttpTransact::CACHE_WRITE_IN_PROGRESS:
    // Hack - the HttpSM doesn't record
    //   cache write aborts currently so
    //   if it's not complete declare it
    //   aborted
    code = LOG_CACHE_WRITE_LOCK_ABORTED;
    break;
  case HttpTransact::CACHE_WRITE_ERROR:
    code = LOG_CACHE_WRITE_ERROR;
    break;
  case HttpTransact::CACHE_WRITE_COMPLETE:
    code = LOG_CACHE_WRITE_COMPLETE;
    break;
  default:
    ink_assert(!"bad cache write code");
    code = LOG_CACHE_WRITE_NONE;
    break;
  }

  return code;
}


int
LogAccessHttp::marshal_cache_write_code(char *buf)
{

  if (buf) {
    int code = convert_cache_write_code(m_http_sm->t_state.cache_info.write_status);
    marshal_int(buf, code);
  }

  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_cache_write_transform_code(char *buf)
{

  if (buf) {
    int code = convert_cache_write_code(m_http_sm->t_state.cache_info.transform_write_status);
    marshal_int(buf, code);
  }

  return INK_MIN_ALIGN;
}


/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_transfer_time_ms(char *buf)
{
  if (buf) {
    ink_hrtime elapsed = m_http_sm->milestones.sm_finish - m_http_sm->milestones.sm_start;
    int64_t val = (int64_t)ink_hrtime_to_msec(elapsed);
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

int
LogAccessHttp::marshal_transfer_time_s(char *buf)
{
  if (buf) {
    ink_hrtime elapsed = m_http_sm->milestones.sm_finish - m_http_sm->milestones.sm_start;
    int64_t val = (int64_t)ink_hrtime_to_sec(elapsed);
    marshal_int(buf, val);
  }
  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  Figure out the size of the object *on origin*. This is somewhat tricky
  since there are many variations on how this can be calculated.
  -------------------------------------------------------------------------*/
int
LogAccessHttp::marshal_file_size(char *buf)
{
  if (buf) {
    MIMEField *fld;
    HTTPHdr *hdr = m_server_response ? m_server_response : m_cache_response;

    if (hdr && (fld = hdr->field_find(MIME_FIELD_CONTENT_RANGE, MIME_LEN_CONTENT_RANGE))) {
      int len;
      char *str = (char*)fld->value_get(&len);
      char *pos = (char*)memchr(str, '/', len); // Find the /

      // If the size is not /* (which means unknown) use it as the file_size.
      if (pos && !memchr(pos+1, '*', len - (pos + 1 - str))) {
        marshal_int(buf, ink_atoi64(pos+1, len - (pos + 1 - str)));
      }
    } else {
      // This is semi-broken when we serveq zero length objects. See TS-2213
      if (m_http_sm->server_response_body_bytes > 0)
        marshal_int(buf, m_http_sm->server_response_body_bytes);
      else if (m_http_sm->cache_response_body_bytes > 0)
        marshal_int(buf, m_http_sm->cache_response_body_bytes);
    }
  }
  // Else, we don't set the value at all (so, -)

  return INK_MIN_ALIGN;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
LogAccessHttp::marshal_http_header_field(LogField::Container container, char *field, char *buf)
{
  char *str = NULL;
  int padded_len = INK_MIN_ALIGN;
  int actual_len = 0;
  bool valid_field = false;
  HTTPHdr *header;

  switch (container) {
  case LogField::CQH:
    header = m_client_request;
    break;

  case LogField::PSH:
    header = m_proxy_response;
    break;

  case LogField::PQH:
    header = m_proxy_request;
    break;

  case LogField::SSH:
    header = m_server_response;
    break;

  case LogField::CSSH:
    header = m_cache_response;
    break;

  default:
    header = NULL;
    break;
  }

  if (header) {
    MIMEField *fld = header->field_find(field, (int)::strlen(field));
    if (fld) {
      valid_field = true;

      // Loop over dups, marshalling each one into the buffer and
      // summing up their length
      //
      int running_len = 0;
      while (fld) {
        str = (char *) fld->value_get(&actual_len);
        if (buf) {
          memcpy(buf, str, actual_len);
          buf += actual_len;
        }
        running_len += actual_len;
        fld = fld->m_next_dup;

        // Dups need to be comma separated.  So if there's another
        // dup, then add a comma and a space ...
        //
        if (fld != NULL) {
          if (buf) {
            memcpy(buf, ", ", 2);
            buf += 2;
          }
          running_len += 2;
        }
      }

      // Done with all dups.  Ensure that the string is terminated
      // and that the running_len is padded.
      //
      if (buf) {
        *buf = '\0';
        buf++;
      }
      running_len += 1;
      padded_len = round_strlen(running_len);

      // Note: marshal_string fills the padding to
      //  prevent purify UMRs so we do it here too
      //  since we always pass the unpadded length on
      //  our calls to marshal string
#ifdef DEBUG
      if (buf) {
        int pad_len = padded_len - running_len;
        for (int i = 0; i < pad_len; i++) {
          *buf = '$';
          buf++;
        }
      }
#endif
    }
  }

  if (valid_field == false) {
    padded_len = INK_MIN_ALIGN;
    if (buf) {
      marshal_str(buf, NULL, padded_len);
    }
  }

  return (padded_len);
}

int
LogAccessHttp::marshal_http_header_field_escapify(LogField::Container container, char *field, char *buf)
{
  char *str = NULL, *new_str = NULL;
  int padded_len = INK_MIN_ALIGN;
  int actual_len = 0, new_len = 0;
  bool valid_field = false;
  HTTPHdr *header;

  switch (container) {
  case LogField::ECQH:
    header = m_client_request;
    break;

  case LogField::EPSH:
    header = m_proxy_response;
    break;

  case LogField::EPQH:
    header = m_proxy_request;
    break;

  case LogField::ESSH:
    header = m_server_response;
    break;

  case LogField::ECSSH:
    header = m_cache_response;
    break;

  default:
    header = NULL;
    break;
  }

  if (header) {
    MIMEField *fld = header->field_find(field, (int)::strlen(field));
    if (fld) {
      valid_field = true;

      // Loop over dups, marshalling each one into the buffer and
      // summing up their length
      //
      int running_len = 0;
      while (fld) {
        str = (char *) fld->value_get(&actual_len);
        new_str = LogUtils::escapify_url(&m_arena, str, actual_len, &new_len);
        if (buf) {
          memcpy(buf, new_str, new_len);
          buf += new_len;
        }
        running_len += new_len;
        fld = fld->m_next_dup;

        // Dups need to be comma separated.  So if there's another
        // dup, then add a comma and a space ...
        //
        if (fld != NULL) {
          if (buf) {
            memcpy(buf, ", ", 2);
            buf += 2;
          }
          running_len += 2;
        }
      }

      // Done with all dups.  Ensure that the string is terminated
      // and that the running_len is padded.
      //
      if (buf) {
        *buf = '\0';
        buf++;
      }
      running_len += 1;
      padded_len = round_strlen(running_len);

      // Note: marshal_string fills the padding to
      //  prevent purify UMRs so we do it here too
      //  since we always pass the unpadded length on
      //  our calls to marshal string
#ifdef DEBUG
      if (buf) {
        int pad_len = padded_len - running_len;
        for (int i = 0; i < pad_len; i++) {
          *buf = '$';
          buf++;
        }
      }
#endif
    }
  }

  if (valid_field == false) {
    padded_len = INK_MIN_ALIGN;
    if (buf) {
      marshal_str(buf, NULL, padded_len);
    }
  }

  return (padded_len);
}
