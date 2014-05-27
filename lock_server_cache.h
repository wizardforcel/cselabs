#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <pthread.h>
#include <vector>

using std::string;
using std::map;
using std::vector;

struct lock_info
{
public:
  string cid;
  vector<string> q;

  lock_info() {}
};


class lock_server_cache {
 private:
  int nacquire;

  map<lock_protocol::lockid_t, lock_info> ls_map;
  pthread_mutex_t ls_mutex;

  void send_retry(const string &id, 
                  lock_protocol::lockid_t lid, int nd_return);

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
