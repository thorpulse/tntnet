////////////////////////////////////////////////////////////////////////
// static.cpp
//

#include "static.h"
#include <tnt/http.h>
#include <tnt/log.h>
#include <fstream>
#include <cxxtools/thread.h>
#include <sys/types.h>
#include <sys/stat.h>

static Mutex mutex;
static tnt::component* theComponent = 0;
static unsigned refs = 0;

////////////////////////////////////////////////////////////////////////
// external functions
//

bool config_static(const tnt::tntconfig::name_type& key,
  const tnt::tntconfig::config_value_type& values)
{
  if (key == "DocumentRoot" && values.size() >= 1)
  {
    tntcomp::staticcomp::setDocumentRoot(values[0]);
    return true;
  }
  return false;
}

tnt::component* create_static(const tnt::compident& ci,
  const tnt::urlmapper& um, tnt::comploader& cl)
{
  MutexLock lock(mutex);
  if (theComponent == 0)
  {
    theComponent = new tntcomp::staticcomp();
    refs = 1;
  }
  else
    ++refs;

  return theComponent;
}

namespace tntcomp
{
  //////////////////////////////////////////////////////////////////////
  // componentdefinition
  //

  std::string staticcomp::document_root;

  unsigned staticcomp::operator() (tnt::httpRequest& request,
    tnt::httpReply& reply, query_params& qparams)
  {
    if (!tnt::httpRequest::checkUrl(request.getPathInfo()))
      throw tnt::httpError(HTTP_BAD_REQUEST, "illegal url");

    std::string file;
    if (!document_root.empty())
      file = document_root + '/';
    file += request.getPathInfo();

    log_debug("file: " << file);

    struct stat st;
    if (stat(file.c_str(), &st) != 0)
    {
      log_warn("error in stat for file \"" << file << "\"");
      reply.throwNotFound(request.getPathInfo());
    }

    if (!S_ISREG(st.st_mode))
    {
      log_warn("no regular file \"" << file << "\"");
      reply.throwNotFound(request.getPathInfo());
    }

    std::ifstream in(file.c_str());

    if (!in)
    {
      log_warn("file \"" << file << "\" not found");
      reply.throwNotFound(request.getPathInfo());
    }

    // set Content-Type
    if (request.getArgs().size() > 0)
      reply.setContentType(request.getArg(0));

    // set Content-Length
    reply.setContentLengthHeader(st.st_size);

    // set Keep-Alive
    if (request.keepAlive())
      reply.setHeader(tnt::httpMessage::Connection,
                      tnt::httpMessage::Connection_Keep_Alive);

    // send datea
    reply.setDirectMode();
    reply.out() << in.rdbuf();

    return HTTP_OK;
  }

  bool staticcomp::drop()
  {
    MutexLock lock(mutex);
    if (--refs == 0)
    {
      delete this;
      theComponent = 0;
      return true;
    }
    else
      return false;
  }

}
