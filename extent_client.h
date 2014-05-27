// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "extent_server.h"
#include "pthread.h"
using std::string;
using std::map;

class extent_client
{
protected:
  rpcc *cl;

public:
  extent_client(string dst);
  virtual ~extent_client() {}
  virtual extent_protocol::status create(uint32_t type,
                                         extent_protocol::extentid_t &eid);
  virtual extent_protocol::status get(extent_protocol::extentid_t eid, string &buf);
  virtual extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				          extent_protocol::attr &a);
  virtual extent_protocol::status put(extent_protocol::extentid_t eid, string buf);
  virtual extent_protocol::status remove(extent_protocol::extentid_t eid);
};

////////////////////////////////////////////////////////////////////////////////////

class extent_client_cache : public extent_client
{
private:
  map<extent_protocol::extentid_t, string> cache_content;
  map<extent_protocol::extentid_t, extent_protocol::attr> cache_attr;
  pthread_mutex_t ec_mutex;

  extent_protocol::status
  modify_attr_get(extent_protocol::extentid_t eid, time_t tm);
  extent_protocol::status
  modify_attr_put(extent_protocol::extentid_t eid, size_t sz, time_t tm);

public:
  extent_client_cache(string dst);

  virtual extent_protocol::status create(uint32_t type,
                                         extent_protocol::extentid_t &eid);
  virtual extent_protocol::status get(extent_protocol::extentid_t eid, string &buf);
  virtual extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				          extent_protocol::attr &a);
  virtual extent_protocol::status put(extent_protocol::extentid_t eid, string buf);
  virtual extent_protocol::status remove(extent_protocol::extentid_t eid);

  void do_release(extent_protocol::extentid_t eid);
};


#endif 

