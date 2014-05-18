// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <pthread.h>
using std::string;
using std::map;

class lock_server {

 protected:
  int nacquire;
  //implement several locks with a lock and a map
  pthread_mutex_t ls_mutex;
  map<lock_protocol::lockid_t, int> ls_map;

 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







