// lock client interface.

#ifndef lock_client_cache_h
#define lock_client_cache_h

#include <string>
#include <map>
#include <pthread.h>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "extent_client.h"
using std::string;
using std::map;

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class i_lock_release_user
{
public:
  virtual void do_release(lock_protocol::lockid_t) = 0;
  virtual ~i_lock_release_user() {};
};

class lock_release_user : public i_lock_release_user
{
private:
  extent_client_cache *ec;

public:
  lock_release_user(extent_client_cache *_ec)
    : ec(_ec) {}

  virtual void do_release(lock_protocol::lockid_t lid)
  { ec->do_release(lid); }
};

/////////////////////////////////////////////////////////////////////////

enum lock_status
{ UNALLOC, WAIT, AVAIL, OCCUPY };

struct lock_info
{
public:
  lock_status status;
  bool nd_return;
  int last_release;

  lock_info() : status(UNALLOC), nd_return(false), last_release(0) {}
};

class lock_client_cache : public lock_client
{
private:
  i_lock_release_user *lu;
  int rlock_port;
  string hostname;
  string id;

  map<lock_protocol::lockid_t, lock_info> lc_map;
  pthread_mutex_t lc_mutex;
  pthread_t tid;

  static void *thread_func(void *);

public:
  static int last_port;
  lock_client_cache(string xdst, i_lock_release_user *_lu = 0);
  virtual ~lock_client_cache();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, int nd_return, int &);

  void call_do_release(lock_protocol::lockid_t lid)
  { if(lu != 0) lu->do_release(lid); }
};


#endif
